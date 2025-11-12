#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>

struct WindowInfo {
    Window xid;
    std::string title;
    GLuint tex;
    int texW, texH;
    bool capturable;
};

Display* x_display = nullptr;
Window x_root = 0;
std::vector<WindowInfo> g_windows;
int g_selectedIndex = 0;

int winW = 1280;
int winH = 720;
bool isFullscreen = true;

// ---------------- Manejo de errores X ----------------
static int trapped_error_code = 0;

int x_error_handler(Display*, XErrorEvent* error) {
    trapped_error_code = error->error_code;
    return 0;
}

void start_xerror_trap() {
    trapped_error_code = 0;
    XSync(x_display, False);
    XSetErrorHandler(x_error_handler);
}

bool end_xerror_trap() {
    XSync(x_display, False);
    XSetErrorHandler(nullptr);
    return trapped_error_code != 0;
}

// ---------------- Utilidades X11 ----------------
static std::string get_window_title(Display* dpy, Window w) {
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_type = XInternAtom(dpy, "UTF8_STRING", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;
    std::string title;

    if (XGetWindowProperty(dpy, w, net_wm_name, 0, (~0L), False, utf8_type,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
        title = reinterpret_cast<char*>(prop);
        XFree(prop);
        return title;
    }

    XTextProperty tp;
    if (XGetWMName(dpy, w, &tp) && tp.value) {
        title = reinterpret_cast<char*>(tp.value);
        if (tp.value) XFree(tp.value);
        return title;
    }
    return "[Sin título]";
}

void enumerate_windows() {
    g_windows.clear();
    Window root_return, parent_return;
    Window* children = nullptr;
    unsigned int nchildren = 0;

    if (!XQueryTree(x_display, x_root, &root_return, &parent_return, &children, &nchildren)) {
        if (children) XFree(children);
        return;
    }

    for (unsigned int i = 0; i < nchildren; ++i) {
        Window w = children[i];
        XWindowAttributes attr;
        if (!XGetWindowAttributes(x_display, w, &attr)) continue;
        if (attr.map_state != IsViewable) continue; // solo ventanas visibles

        WindowInfo info{};
        info.xid = w;
        info.title = get_window_title(x_display, w);
        info.tex = 0;
        info.texW = info.texH = 0;
        info.capturable = false;
        g_windows.push_back(info);
    }

    if (children) XFree(children);
}

// ---------------- Captura segura ----------------
static void ensure_texture(WindowInfo &info) {
    XWindowAttributes wa;
    if (!XGetWindowAttributes(x_display, info.xid, &wa)) {
        info.capturable = false;
        return;
    }

    info.capturable = (wa.width > 0 && wa.height > 0);
    if (!info.capturable) return;

    start_xerror_trap();
    XImage* img = XGetImage(x_display, info.xid, 0, 0, wa.width, wa.height, AllPlanes, ZPixmap);
    bool failed = end_xerror_trap();

    if (failed || !img) {
        info.capturable = false;
        return;
    }

    int width = wa.width;
    int height = wa.height;
    unsigned char* pixels = new unsigned char[width * height * 3];

    unsigned long rmask = img->red_mask;
    unsigned long gmask = img->green_mask;
    unsigned long bmask = img->blue_mask;
    int rshift = 0; while (!((rmask >> rshift) & 1) && rshift < 32) rshift++;
    int gshift = 0; while (!((gmask >> gshift) & 1) && gshift < 32) gshift++;
    int bshift = 0; while (!((bmask >> bshift) & 1) && bshift < 32) bshift++;

    for (int y = 0; y < height; ++y) {
        int ty = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            unsigned long p = XGetPixel(img, x, y);
            unsigned char r = ((p & rmask) >> rshift) & 0xFF;
            unsigned char g = ((p & gmask) >> gshift) & 0xFF;
            unsigned char b = ((p & bmask) >> bshift) & 0xFF;
            int idx = (ty * width + x) * 3;
            pixels[idx] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
        }
    }

    if (info.tex == 0)
        glGenTextures(1, &info.tex);

    glBindTexture(GL_TEXTURE_2D, info.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, pixels);

    info.texW = width;
    info.texH = height;

    delete[] pixels;
    XDestroyImage(img);
}

// ---------------- Dibujo ----------------
void display() {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);

    if (g_windows.empty()) {
        glutSwapBuffers();
        return;
    }

    WindowInfo &sel = g_windows[g_selectedIndex];
    ensure_texture(sel);
    if (sel.capturable && sel.tex) {
        float winAspect = (float)winW / winH;
        float texAspect = (float)sel.texW / sel.texH;
        float sx = 1.0f, sy = 1.0f;
        if (texAspect > winAspect) sy = winAspect / texAspect;
        else sx = texAspect / winAspect;

        glBindTexture(GL_TEXTURE_2D, sel.tex);
        glBegin(GL_QUADS);
            glTexCoord2f(0,0); glVertex2f(-sx, -sy);
            glTexCoord2f(1,0); glVertex2f( sx, -sy);
            glTexCoord2f(1,1); glVertex2f( sx,  sy);
            glTexCoord2f(0,1); glVertex2f(-sx,  sy);
        glEnd();
    }

    glutSwapBuffers();
    glutPostRedisplay();
}

// ---------------- Eventos ----------------
void keyboard(unsigned char key, int, int) {
    if (key >= '1' && key - '1' < (int)g_windows.size()) {
        g_selectedIndex = key - '1';
        printf("Mostrando ventana %d: %s\n", g_selectedIndex, g_windows[g_selectedIndex].title.c_str());
    } else if (key == 27) { // ESC
        for (auto &w : g_windows)
            if (w.tex) glDeleteTextures(1, &w.tex);
        if (x_display) XCloseDisplay(x_display);
        exit(0);
    }
}

void special_key(int key, int, int) {
    if (key == GLUT_KEY_F4) {
        if (isFullscreen) {
            glutReshapeWindow(1280, 720);
            isFullscreen = false;
        } else {
            glutFullScreen();
            isFullscreen = true;
        }
    }
}

void reshape(int w, int h) {
    winW = w;
    winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ---------------- main ----------------
int main(int argc, char** argv) {
    XInitThreads();
    x_display = XOpenDisplay(nullptr);
    if (!x_display) {
        fprintf(stderr, "No se pudo abrir X display\n");
        return 1;
    }

    x_root = DefaultRootWindow(x_display);
    enumerate_windows();
    if (!g_windows.empty()) g_selectedIndex = 0;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Ventana Visible X11 - Fullscreen Toggle");
    glutFullScreen();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_key);

    glEnable(GL_TEXTURE_2D);
    glClearColor(0,0,0,1);

    glutMainLoop();
    return 0;
}

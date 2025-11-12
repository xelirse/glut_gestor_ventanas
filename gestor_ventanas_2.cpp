#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
int g_selectedIndex = -1;

int winW = 1280;
int winH = 720;
const float PANEL_RATIO = 0.12f;
const int GRID_ROWS = 1;
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

    // Conversión y corrección de color (Aseguramos RGB correcto)
    // Nota: mantenemos el flip vertical (ty = height - 1 - y) para que la textura se vea con orientación correcta.
    for (int y = 0; y < height; ++y) {
        int ty = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            unsigned long p = XGetPixel(img, x, y);
            unsigned char r = ((p & rmask) >> rshift) & 0xFF;
            unsigned char g = ((p & gmask) >> gshift) & 0xFF;
            unsigned char b = ((p & bmask) >> bshift) & 0xFF;

            int idx = (ty * width + x) * 3;
            // ---> CORRECCIÓN: escribir en orden R, G, B (sin intercambio)
            pixels[idx]     = r;
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
static void drawTexturedQuad(GLuint tex, float x1, float y1, float x2, float y2) {
    if (tex == 0) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(x1, y1);
        glTexCoord2f(1, 0); glVertex2f(x2, y1);
        glTexCoord2f(1, 1); glVertex2f(x2, y2);
        glTexCoord2f(0, 1); glVertex2f(x1, y2);
    glEnd();
}

void display() {
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);

    float panelH = PANEL_RATIO;
    float panelTopY = -1.0f + 2.0f * panelH;

    if (g_windows.empty()) {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.4f,0.4f,0.4f);
        glBegin(GL_QUADS);
            glVertex2f(-0.8f,-0.05f);
            glVertex2f(0.8f,-0.05f);
            glVertex2f(0.8f,0.05f);
            glVertex2f(-0.8f,0.05f);
        glEnd();
        glutSwapBuffers();
        return;
    }

    if (g_selectedIndex >= 0 && g_selectedIndex < (int)g_windows.size()) {
        WindowInfo &sel = g_windows[g_selectedIndex];
        ensure_texture(sel);
        if (sel.capturable && sel.tex)
            drawTexturedQuad(sel.tex, -1.0f, panelTopY, 1.0f, 1.0f);
        else {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.25f,0.25f,0.25f);
            glBegin(GL_QUADS);
                glVertex2f(-1.0f,panelTopY);
                glVertex2f(1.0f,panelTopY);
                glVertex2f(1.0f,1.0f);
                glVertex2f(-1.0f,1.0f);
            glEnd();
            glEnable(GL_TEXTURE_2D);
        }
    } else g_selectedIndex = 0;

    glDisable(GL_TEXTURE_2D);
    glColor3f(0.07f,0.07f,0.07f);
    glBegin(GL_QUADS);
        glVertex2f(-1,-1);
        glVertex2f(1,-1);
        glVertex2f(1,panelTopY);
        glVertex2f(-1,panelTopY);
    glEnd();
    glEnable(GL_TEXTURE_2D);

    int total = g_windows.size();
    int rows = GRID_ROWS;
    int cols = (total + rows - 1) / rows;
    float thumbW = 2.0f / cols;
    float thumbH = (2.0f * panelH) / rows;

    int idx = 0;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (idx >= total) break;
            float x1 = -1.0f + col * thumbW;
            float x2 = x1 + thumbW;
            float y_top = panelTopY - row * thumbH;
            float y_bottom = y_top - thumbH;

            WindowInfo &wi = g_windows[idx];
            ensure_texture(wi);

            if (wi.capturable && wi.tex)
                drawTexturedQuad(wi.tex, x1, y_bottom, x2, y_top);
            else {
                glDisable(GL_TEXTURE_2D);
                glColor3f(idx == g_selectedIndex ? 0.5f : 1.0f, 1.0f, 1.0f);
                glBegin(GL_QUADS);
                    glVertex2f(x1, y_bottom);
                    glVertex2f(x2, y_bottom);
                    glVertex2f(x2, y_top);
                    glVertex2f(x1, y_top);
                glEnd();
                glEnable(GL_TEXTURE_2D);
            }
            ++idx;
        }
    }

    glutSwapBuffers();
    glutPostRedisplay();
}

// ---------------- Eventos ----------------
void toggle_fullscreen() {
    if (isFullscreen) { glutReshapeWindow(1024, 700); isFullscreen = false; }
    else { glutFullScreen(); isFullscreen = true; }
}

void special_key(int key, int, int) {
    if (key == GLUT_KEY_F4) toggle_fullscreen();
}

void keyboard(unsigned char key, int, int) {
    if (key == 27) {
        for (auto &w : g_windows)
            if (w.tex) glDeleteTextures(1, &w.tex);

        if (x_display) {
            XCloseDisplay(x_display);
            x_display = nullptr;
        }

        glutDestroyWindow(glutGetWindow());
        exit(0);
    }
}

void mouse_click(int button, int state, int mx, int my) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    float fx = (2.0f * mx) / winW - 1.0f;
    float fy = 1.0f - (2.0f * my) / winH;
    float panelTopY = -1.0f + 2.0f * PANEL_RATIO;

    if (fy < panelTopY) {
        int total = g_windows.size();
        int rows = GRID_ROWS;
        int cols = (total + rows - 1) / rows;
        float thumbW = 2.0f / cols;
        float thumbH = (2.0f * PANEL_RATIO) / rows;

        int col = (int)((fx + 1.0f) / thumbW);
        int row = (int)((panelTopY - fy) / thumbH);
        int idx = row * cols + col;
        if (idx >= 0 && idx < total)
            g_selectedIndex = idx;
    }
}

void reshape(int w, int h) {
    winW = w; winH = h;
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
    glutCreateWindow("Gestor de Ventanas - Live");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutFullScreen();
    isFullscreen = true;

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_key);
    glutMouseFunc(mouse_click);

    glClearColor(0,0,0,1);
    glEnable(GL_TEXTURE_2D);
    glutMainLoop();
    return 0;
}

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

Display* x_display = nullptr;
Window g_textureWindow; // ventana activa a mostrar
GLuint g_textureID = 0;
int g_textureWidth = 0, g_textureHeight = 0;
std::vector<Window> windows; // lista de ventanas para cambiar

void captureWindowAsTexture(Window window, int width, int height, bool init) {
    XImage* image = XGetImage(x_display, window, 0, 0, width, height, AllPlanes, ZPixmap);
    if (!image) return;

    unsigned char* pixels = new unsigned char[width * height * 3];

    unsigned long red_mask = image->red_mask;
    unsigned long green_mask = image->green_mask;
    unsigned long blue_mask = image->blue_mask;
    int r_shift = 0; while (!((red_mask >> r_shift) & 1) && r_shift < 32) r_shift++;
    int g_shift = 0; while (!((green_mask >> g_shift) & 1) && g_shift < 32) g_shift++;
    int b_shift = 0; while (!((blue_mask >> b_shift) & 1) && b_shift < 32) b_shift++;

    for (int y = 0; y < height; ++y) {
        int target_y = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            unsigned long pix = XGetPixel(image, x, y);
            unsigned char r = ((pix & red_mask) >> r_shift) & 0xFF;
            unsigned char g = ((pix & green_mask) >> g_shift) & 0xFF;
            unsigned char b = ((pix & blue_mask) >> b_shift) & 0xFF;
            int idx = (target_y * width + x) * 3;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
        }
    }

    glBindTexture(GL_TEXTURE_2D, g_textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (init) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, pixels);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                        GL_RGB, GL_UNSIGNED_BYTE, pixels);
    }

    delete[] pixels;
    XDestroyImage(image);
}

void display() {
    captureWindowAsTexture(g_textureWindow, g_textureWidth, g_textureHeight, false);

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_textureID);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f( 1, -1);
    glTexCoord2f(1, 1); glVertex2f( 1,  1);
    glTexCoord2f(0, 1); glVertex2f(-1,  1);
    glEnd();

    glutSwapBuffers();
    glutPostRedisplay();
}

// función para cambiar ventana según tecla
void keyboard(unsigned char key, int x, int y) {
    if (key >= '0' && key - '0' < windows.size()) {
        g_textureWindow = windows[key - '0'];
        printf("Ventana seleccionada: %d\n", key - '0');
    }
}

int main(int argc, char** argv) {
    x_display = XOpenDisplay(nullptr);
    if (!x_display) {
        fprintf(stderr, "No se pudo abrir el display X\n");
        return 1;
    }

    Window root = DefaultRootWindow(x_display);
    g_textureWindow = root; // por defecto, escritorio completo

    // obtener tamaño del root window
    XWindowAttributes gwa;
    XGetWindowAttributes(x_display, root, &gwa);
    g_textureWidth = gwa.width;
    g_textureHeight = gwa.height;
    printf("Resolución detectada: %dx%d\n", g_textureWidth, g_textureHeight);

    // listar ventanas hijas del root
    Window root_return, parent_return;
    Window* children;
    unsigned int nchildren;
	if (XQueryTree(x_display, root, &root_return, &parent_return, &children, &nchildren)) {
		windows.push_back(root); // índice 0 -> escritorio completo
		for (unsigned int i = 0; i < nchildren; ++i) {
			XWindowAttributes attr;
			if (XGetWindowAttributes(x_display, children[i], &attr) && attr.map_state == IsViewable) {
				windows.push_back(children[i]); // solo visibles
			}
		}
		if (children) XFree(children);
	}

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(g_textureWidth, g_textureHeight);
    glutCreateWindow("Captura X11 con cambio de ventana");

    glViewport(0, 0, g_textureWidth, g_textureHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glGenTextures(1, &g_textureID);
    captureWindowAsTexture(g_textureWindow, g_textureWidth, g_textureHeight, true);

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutMainLoop();

    return 0;
}

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>

Display* x_display = NULL;
Window x_root;
GLuint g_textureID = 0;
int g_textureWidth = 800, g_textureHeight = 600;

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
        // Solo actualiza contenido, sin recrear textura
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                        GL_RGB, GL_UNSIGNED_BYTE, pixels);
    }

    delete[] pixels;
    XDestroyImage(image);
}

void display() {
    captureWindowAsTexture(x_root, g_textureWidth, g_textureHeight, false);

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
    glutPostRedisplay(); // fuerza refresco continuo
}

int main(int argc, char** argv) {
    x_display = XOpenDisplay(NULL);
    x_root = DefaultRootWindow(x_display);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(g_textureWidth, g_textureHeight);
    glutCreateWindow("Captura en tiempo real");

    glGenTextures(1, &g_textureID);
    captureWindowAsTexture(x_root, g_textureWidth, g_textureHeight, true);

    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}

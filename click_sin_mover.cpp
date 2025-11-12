#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

struct WindowInfo {
    Window id;
    std::string title;
};

std::string getWindowTitle(Display* dpy, Window w) {
    Atom prop = XInternAtom(dpy, "WM_NAME", False);
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char* prop_ret = nullptr;

    if (XGetWindowProperty(dpy, w, prop, 0, (~0L), False, AnyPropertyType,
                           &type, &format, &nitems, &bytes_after, &prop_ret) == Success) {
        if (prop_ret) {
            std::string title(reinterpret_cast<char*>(prop_ret));
            XFree(prop_ret);
            return title;
        }
    }
    return "";
}

void listWindowsRec(Display* dpy, Window root, std::vector<WindowInfo>& windows) {
    Window root_ret, parent_ret;
    Window* children;
    unsigned int nchildren;
    if (XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; i++) {
            std::string title = getWindowTitle(dpy, children[i]);
            if (!title.empty())
                windows.push_back({ children[i], title });
            listWindowsRec(dpy, children[i], windows);
        }
        if (children)
            XFree(children);
    }
}

void sendClick(Display* dpy, Window w, int x, int y) {
    XEvent event;
    memset(&event, 0, sizeof(event));

    event.xbutton.type = ButtonPress;
    event.xbutton.button = Button1;
    event.xbutton.same_screen = True;
    event.xbutton.x = x;
    event.xbutton.y = y;
    event.xbutton.window = w;

    XSendEvent(dpy, w, True, ButtonPressMask, &event);

    event.xbutton.type = ButtonRelease;
    XSendEvent(dpy, w, True, ButtonReleaseMask, &event);
    XFlush(dpy);
}

int main() {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        std::cerr << "❌ No se pudo abrir la pantalla X11.\n";
        return 1;
    }

    Window root = DefaultRootWindow(dpy);
    std::vector<WindowInfo> windows;
    listWindowsRec(dpy, root, windows);

    if (windows.empty()) {
        std::cerr << "No se encontraron ventanas visibles.\n";
        XCloseDisplay(dpy);
        return 1;
    }

    std::cout << "🪟 Ventanas abiertas:\n";
    for (size_t i = 0; i < windows.size(); ++i)
        std::cout << i << ": " << windows[i].title << "\n";

    int choice;
    std::cout << "\nElige el número de la ventana destino: ";
    std::cin >> choice;

    if (choice < 0 || choice >= (int)windows.size()) {
        std::cerr << "Opción inválida.\n";
        XCloseDisplay(dpy);
        return 1;
    }

    int x, y;
    std::cout << "Coordenada X dentro de la ventana: ";
    std::cin >> x;
    std::cout << "Coordenada Y dentro de la ventana: ";
    std::cin >> y;

    sendClick(dpy, windows[choice].id, x, y);
    std::cout << "✅ Click enviado a \"" << windows[choice].title
              << "\" en (" << x << "," << y << ").\n";

    XCloseDisplay(dpy);
    return 0;
}

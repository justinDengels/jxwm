#include "jxwm.hpp"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <X11/Xatom.h>
#include <vector>

/*
 Bugs/Errors/Todos:
 when running in xephyr killing the first window ends xephyr session
 add proper tagging support
 add parseable config file
 add proper external bar support
*/
bool JXWM::otherWM = false;

int JXWM::Init()
{
    disp = XOpenDisplay(NULL);

    if (disp == NULL)
    {
        std::cout << "Error: Failed to open X Display" << std::endl;
        return 1;
    }

    root = DefaultRootWindow(disp);
    XSetErrorHandler(&OnOtherWMDetected);
    rootMask = SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask;
    XSelectInput(disp, root, rootMask);
    SetWindowLayout(&JXWM::MasterStack);
    GetExistingWindows();
    GrabHandlers();
    //temp, TODO: make config file parser
    arg* arg1 = new arg{.spawn = "xterm"};
    arg* arg2 = new arg{.tag = 1};
    arg* arg3 = new arg{.tag = 2};
    keybinding keb {Mod4Mask, XStringToKeysym("o"), &JXWM::Spawn, arg1};
    keybinding keb2 {Mod4Mask, XStringToKeysym("i"), &JXWM::KillWindow, nullptr};
    keybinding keb3 {Mod4Mask, XStringToKeysym("1"), &JXWM::ChangeTag, arg2};
    keybinding keb4 {Mod4Mask, XStringToKeysym("2"), &JXWM::ChangeTag, arg3};
    keybindings.push_back(keb);
    keybindings.push_back(keb2);
    keybindings.push_back(keb3);
    keybindings.push_back(keb4);
    //end temp
    GrabKeys();
    GetAtoms();

    if (otherWM) { return 1; }

    XSetErrorHandler(&OnError);
    XSync(disp, false);
    quit = false;
    return 0;
}

void JXWM::GetAtoms()
{
    WM_PROTOCOLS = XInternAtom(disp, "WM_PROTOCOLS", false);
    WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", false);
    NET_ACTIVE_WINDOW = XInternAtom(disp, "_NET_ACTIVE_WINDOW", false);
    NET_CLOSE_WINDOW = XInternAtom(disp, "_NET_CLOSE_WINDOW", false);
    NET_SUPPORTED = XInternAtom(disp, "_NET_SUPPORTED", false);

    NET_NUMBER_OF_DESKTOPS = XInternAtom(disp, "_NET_NUMBER_OF_DESKTOPS", false);
    tags = 2;
    XChangeProperty(disp, root, NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&tags, 1);

    NET_CURRENT_DESKTOP = XInternAtom(disp, "_NET_CURRENT_DESKTOP", false);
    currentTag = 1;
    XChangeProperty(disp, root, NET_CURRENT_DESKTOP, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&currentTag, 1);

    //TODO: NET_DESKTOP_NAMES 


    Atom atoms[] = { WM_PROTOCOLS, WM_DELETE_WINDOW, NET_ACTIVE_WINDOW, NET_CLOSE_WINDOW, NET_SUPPORTED, NET_NUMBER_OF_DESKTOPS, NET_CURRENT_DESKTOP };
    XChangeProperty(disp, root, NET_SUPPORTED, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, sizeof(atoms) / sizeof(Atom));
}

void JXWM::GetExistingWindows()
{
    XGrabServer(disp);
    Window rootr, pReturn, *children;
    uint nChildren;
    XQueryTree(disp, root, &rootr, &pReturn, &children, &nChildren);
    for (uint i = 0; i < nChildren; i++) { Clients.push_back({children[i]}); }
    XUngrabServer(disp);
    XFree(children);
    Arrange();
}

void JXWM::Run()
{
    XEvent e;
    while (!quit)
    {
        XNextEvent(disp, &e);
        if (handlers[e.type]) { (this->*handlers[e.type])(&e); }
    }
}

int JXWM::OnOtherWMDetected(Display* disp, XErrorEvent* xee)
{
    otherWM = true;
    std::cout << "Other window manager detected" << std::endl;
    return 0;
}

int JXWM::OnError(Display* disp, XErrorEvent* xee)
{
    std::cout << "An error occurred" << std::endl;
    char text[512];
    XGetErrorText(disp, xee->error_code, text, sizeof(text));
    std::cout << text << "\nCode: " << xee->error_code
        << "\nType: " << xee->type << "\nSerial: " << xee->serial
        << "\nMinor: " << xee->minor_code << "\nResource ID: " << xee->resourceid
        << "\nRequest Code: " << xee->request_code << std::endl;
    return 0;
}

void JXWM::GrabKeys() 
{
    XUngrabKey(disp, AnyKey, AnyModifier, root);

    for (auto kb: keybindings)
    {
        KeyCode kc = XKeysymToKeycode(disp, kb.sym);
        if (kc) { XGrabKey(disp, kc, kb.mod, root, True, GrabModeAsync, GrabModeAsync); }
    }
}

void JXWM::GrabHandlers()
{
    handlers[MapRequest] = &JXWM::OnMapRequest;
    handlers[UnmapNotify] = &JXWM::OnUnmapNotify;
    handlers[ConfigureRequest] = &JXWM::OnConfigureRequest;
    handlers[KeyPress] = &JXWM::OnKeyPress;
    handlers[ClientMessage] = &JXWM::OnClientMessage;
    handlers[DestroyNotify] = &JXWM::OnDestroyNotify;
    handlers[EnterNotify] = &JXWM::OnWindowEnter; 
}

void JXWM::OnMapRequest(XEvent* e) 
{
    std::cout << "In OnMapRequest" << std::endl;
    Client* c = GetClientFromWindow(e->xmaprequest.window);
    if (c != nullptr) { return; }
    Client newClient = { e->xmaprequest.window, currentTag };
    XMapWindow(disp, newClient.window);
    XSelectInput(disp, newClient.window, EnterWindowMask | LeaveWindowMask);
    Clients.push_back(newClient);
    FocusClient(newClient);
    Arrange();
}

void JXWM::OnConfigureRequest(XEvent* e)
{
    std::cout << "In OnConfigureRequest" << std::endl;
    XConfigureRequestEvent xcre = e->xconfigurerequest;
    XWindowChanges xwc;
    xwc.x = xcre.x;
    xwc.y = xcre.y;
    xwc.width = xcre.width;
    xwc.height = xcre.height;
    xwc.sibling = xcre.above;
    xwc.stack_mode = xcre.detail;
    xwc.border_width = xcre.border_width;
    XConfigureWindow(disp, xcre.window, xcre.value_mask, &xwc);
}

void JXWM::OnKeyPress(XEvent* e)
{
    std::cout << "In OnKeyPress" << std::endl;
    XKeyEvent xke = e->xkey;

    for (auto kb: keybindings)
    {
        if ((xke.keycode == XKeysymToKeycode(disp, kb.sym)) && ((kb.mod & xke.state))) 
        { 
            (this->*kb.func)(kb.argument); 
        }
    }
}

void JXWM::Spawn(arg* arg)
{
    std::cout << "In Spawn function" << std::endl;
    if (fork())
    {
        if (disp) { close(ConnectionNumber(disp)); }
        setsid();
        execl("/bin/sh", "sh", "-c", arg->spawn, 0);
        exit(0);
    }
}

void JXWM::KillWindow(arg*)
{
    std::cout << "In KillWindow function" << std::endl;
    if (focused.window == root) { return; }
    Atom* retAtoms;
    int numAtoms;
    if (XGetWMProtocols(disp, focused.window, &retAtoms, &numAtoms))
    {
        for (int i = 0; i < numAtoms; i++)
        {
            if (retAtoms[i] == WM_DELETE_WINDOW)
            {
                XEvent killEvent;
                memset(&killEvent, 0, sizeof(killEvent));
                killEvent.xclient.type = ClientMessage;
                killEvent.xclient.window = focused.window;
                killEvent.xclient.message_type = WM_PROTOCOLS;
                killEvent.xclient.format = 32;
                killEvent.xclient.data.l[0] = WM_DELETE_WINDOW;
                killEvent.xclient.data.l[1] = CurrentTime;
                XSendEvent(disp, focused.window, False, NoEventMask, &killEvent);
                XFree(retAtoms);
                RemoveClient(focused);
                Arrange();
                return;
            }
        }
        XFree(retAtoms);
    }
    std::cout << "Could not get delete window atom" << std::endl;
    XKillClient(disp, focused.window);
    RemoveClient(focused);
    Arrange();
}

void JXWM::OnClientMessage(XEvent* e)
{
    std::cout << "In OnClientMessage function" << std::endl;
    XClientMessageEvent xcme = e->xclient;
    if (xcme.message_type == NET_NUMBER_OF_DESKTOPS)
    {
        tags = xcme.data.l[0];
        XChangeProperty(disp, root, NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&tags, 1);
        std::cout << "Got client message to change number of virtual desktops to " << tags << std::endl;
    }
    else if (xcme.message_type == NET_CURRENT_DESKTOP)
    {
        XChangeProperty(disp, root, NET_CURRENT_DESKTOP, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&xcme.data.l[0], 1);
        std::cout << "Got client message to change current tag to tag " << currentTag << std::endl;
        arg tmp = { .tag = (int)xcme.data.l[0] };
        ChangeTag(&tmp);
    }
    else if (xcme.message_type == NET_CLOSE_WINDOW)
    {
        Client* c = GetClientFromWindow(xcme.window);
        if (c == nullptr)
        {
            Clients.push_back({xcme.window});
            c = &Clients[Clients.size() - 1];
        }
        std::cout << "Got client message to kill a window, attempting to kill it" << std::endl;
        Client tmpFocused = focused;
        focused = *c;
        KillWindow(nullptr);
        focused = tmpFocused;
    }
}

void JXWM::OnDestroyNotify(XEvent* e)
{
    std::cout << "In OnDestroyNotify" << std::endl;
    XDestroyWindowEvent xdwe = e->xdestroywindow;
    Client* c = GetClientFromWindow(xdwe.window);
    if (c == nullptr || c->window == root) { return; }
    if (focused.window == c->window) 
    {
        Window w = AttemptToGetFocusedWindow();
        Client* next = GetClientFromWindow(w);
        if (next == nullptr) { return; }
        focused = *next;
    }
    RemoveClient(*c);
}


void JXWM::FocusClient(Client& client)
{
    unsigned long FBG_COLOR = 0x0000ff;
    unsigned long UBG_COLOR = 0xff0000;
    XSetWindowBorder(disp, focused.window, UBG_COLOR);
    focused = client;
    XSetWindowBorder(disp, focused.window, FBG_COLOR);
    XSetInputFocus(disp, client.window, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(disp, client.window);
    XChangeProperty(disp, root, NET_ACTIVE_WINDOW, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&client.window, 1);
}

Client* JXWM::GetClientFromWindow(Window w)
{
    for (int i = 0; i < Clients.size(); i++) 
    {
        if (Clients[i].window == w) { return &Clients[i]; }
    }

    return nullptr;
}

void JXWM::RemoveClient(Client& c) 
{
    for (int i = 0; i < Clients.size(); i++)
    {
        if (Clients[i].window == c.window) 
        {
            Clients.erase(Clients.begin() + i);
            Arrange();
            if (c.window == focused.window) { if (Clients.size() > 0) { FocusClient(Clients[0]); } }
            return;
        }
    }
}

Window JXWM::AttemptToGetFocusedWindow()
{
    Window w;
    int ignore;
    XGetInputFocus(disp, &w, &ignore);
    return w;
}

void JXWM::MasterStack()
{
    std::cout << "In master stack" << std::endl;
    // TODO: get current tag
    // TODO: get workable area from struts
    int waX, waY, waW, waH;
    //tmp
    int snum = DefaultScreen(disp);
    waW = DisplayWidth(disp, snum);
    waH = DisplayHeight(disp, snum);
    waX = waY = 0;
    //end tmp
    std::vector<Client> visibleClients;
    for (auto c: Clients)
    {
        if (c.tag == currentTag) { visibleClients.push_back(c); }
    }

    int numClients = visibleClients.size();
    if (numClients == 0) { return; }
    if (numClients == 1)
    {
        XMoveResizeWindow(disp, visibleClients[0].window, waX, waY, waW, waH);
        return;
    }

    Client master = visibleClients[0];
    XMoveResizeWindow(disp, master.window, waX, waY, waW/2, waH);
    int yGap = waH / (numClients - 1);
    int yOffSet = 0;
    for (int i = 1; i < numClients; i++)
    {
        XMoveResizeWindow(disp, visibleClients[i].window, waW / 2, yOffSet, waW / 2, yGap);
        yOffSet += yGap;
    }

}

void JXWM::SetWindowLayout(void(JXWM::*func)(void)) { layout = func; }
void JXWM::Arrange() { (this->*layout)(); }

void JXWM::OnWindowEnter(XEvent* e)
{
    std::cout << "In OnWindowEnter function" << std::endl;
    Client* c = GetClientFromWindow(e->xcrossing.window);
    if (c != nullptr) { FocusClient(*c); }
}

void JXWM::OnUnmapNotify(XEvent* e)
{
    std::cout << "In unmap notify function" << std::endl;
    XUnmapEvent xume = e->xunmap;
    Client* c = GetClientFromWindow(xume.window);
    if (c != nullptr && c->window != root) { RemoveClient(*c); }
}

void JXWM::ChangeTag(arg* arg)
{
    std::cout << "In ChangeTag Function" << std::endl;
    int tagToChange = arg->tag;
    if (currentTag == tagToChange) { return; }
    for (auto c: Clients)
    {
        if (c.tag == currentTag) { XUnmapWindow(disp, c.window); }
        else if (c.tag == tagToChange) { XMapWindow(disp, c.window); }
    }
    currentTag = tagToChange;
    std::cout << "Changed to tag " << currentTag << std::endl;
    Arrange();

}

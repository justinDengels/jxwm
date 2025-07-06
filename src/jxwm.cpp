#include "jxwm.hpp"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <string.h>
#include <X11/Xatom.h>
#include <vector>
#include <fstream>
#include <sstream>
/*
 Bugs/Errors/Todos:
 when running in xephyr killing the first window ends xephyr session
 add proper tagging support
 add parseable config file
 add proper external bar support
*/
bool JXWM::otherWM = false;

JXWM::JXWM() 
{
    disp = nullptr;
    borderWidth = 3;
    currentTag = 0;
    tags = 9;
} //init variables later to get rid of warnings

JXWM::~JXWM()
{
    std::cout << "Quiting JXWM..." << std::endl;
    XCloseDisplay(disp);
    //pkill -KILL -u $USER
}

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
    long rootMask = SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask;
    XSelectInput(disp, root, rootMask);
    SetWindowLayout(&JXWM::MasterStack);
    GetAtoms();
    GetExistingWindows();
    GrabHandlers();
    const char* HOME = std::getenv("HOME");
    std::string configPath = std::string(HOME) + "/.config/jxwm/jxwm.conf";
    ReadConfigFile(configPath);
    GrabKeys();

    XSync(disp, false);
    if (otherWM) { return 1; }

    XSetErrorHandler(&OnError);
    quit = false;
    screenArea.x = screenArea.y = usableArea.x = usableArea.y = 0;
    screenum = DefaultScreen(disp);
    screenArea.w = usableArea.w = DisplayWidth(disp, screenum);
    screenArea.h = usableArea.h = DisplayHeight(disp, screenum);
    return 0;
}

void JXWM::GetAtoms()
{
    WM_PROTOCOLS = XInternAtom(disp, "WM_PROTOCOLS", false);
    WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", false);
    NET_ACTIVE_WINDOW = XInternAtom(disp, "_NET_ACTIVE_WINDOW", false);
    NET_CLOSE_WINDOW = XInternAtom(disp, "_NET_CLOSE_WINDOW", false);
    NET_SUPPORTED = XInternAtom(disp, "_NET_SUPPORTED", false);
    NET_WM_STRUT_PARTIAL = XInternAtom(disp, "_NET_WM_STRUT_PARTIAL", false);

    NET_NUMBER_OF_DESKTOPS = XInternAtom(disp, "_NET_NUMBER_OF_DESKTOPS", false);
    XChangeProperty(disp, root, NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&tags, 1);

    NET_CURRENT_DESKTOP = XInternAtom(disp, "_NET_CURRENT_DESKTOP", false);
    XChangeProperty(disp, root, NET_CURRENT_DESKTOP, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&currentTag, 1);

    //TODO: NET_DESKTOP_NAMES 


    Atom atoms[] = { WM_PROTOCOLS, WM_DELETE_WINDOW, NET_ACTIVE_WINDOW, NET_CLOSE_WINDOW, NET_SUPPORTED, NET_NUMBER_OF_DESKTOPS, NET_CURRENT_DESKTOP, NET_WM_STRUT_PARTIAL };
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
        if (handlers[e.type]) { (this->*handlers[e.type])(e); }
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
    //first, grab tag keybindings
    for (int i = 0; i < 9; i++)
    { 
        keybindings.push_back({Mod4Mask, XK_1 + i, &JXWM::ChangeTag, {.tag = i}});
        keybindings.push_back({Mod4Mask | ShiftMask, XK_1 + i, &JXWM::ChangeClientTag, {.tag = i}});
    }
    
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
    handlers[CreateNotify] = &JXWM::OnCreateNotify;
}

void JXWM::OnMapRequest(const XEvent& e) 
{
    std::cout << "In OnMapRequest" << std::endl;
    Window w = e.xmaprequest.window;
    Client* c = GetClientFromWindow(w);
    if (c != nullptr) 
    {
        XMapWindow(disp, c->window); 
        return; 
    }
    static XWindowAttributes wa;
    if (!XGetWindowAttributes(disp, w, &wa)) { return; }
    if (wa.override_redirect)
    {
        XMapWindow(disp, w);
        return;
    }
    static Strut struts;
    if (IsPager(w, struts))
    {
        UpdateStruts(struts);
        XMapWindow(disp, w);
        Arrange();
    }
    else
    {
        Client newClient = { w, currentTag };
        XMapWindow(disp, newClient.window);
        XSelectInput(disp, newClient.window, EnterWindowMask | LeaveWindowMask);
        Clients.push_back(newClient);
        FocusClient(newClient);
        Arrange();
    }
}

void JXWM::OnConfigureRequest(const XEvent& e)
{
    std::cout << "In OnConfigureRequest" << std::endl;
    XConfigureRequestEvent xcre = e.xconfigurerequest;
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

void JXWM::OnKeyPress(const XEvent& e)
{
    std::cout << "In OnKeyPress" << std::endl;
    XKeyEvent xke = e.xkey;

    for (auto kb: keybindings)
    {
        if ((xke.keycode == XKeysymToKeycode(disp, kb.sym)) && ((kb.mod & xke.state) == kb.mod)) 
        { 
            (this->*kb.func)(&kb.argument); 
        }
    }
}

void JXWM::Spawn(arg* arg) { Spawn(arg->spawn); }

void JXWM::Spawn(const char* spawn)
{
    std::cout << "In Spawn function" << std::endl;
    if (fork())
    {
        if (disp) { close(ConnectionNumber(disp)); }
        setsid();
        execl("/bin/sh", "sh", "-c", spawn, 0);
        exit(0);
    }
}

void JXWM::KillWindow(arg* arg) { KillWindow(focused.window); }

void JXWM::KillWindow(Window w)
{
    std::cout << "In KillWindow function" << std::endl;
    if (w == root) { return; }
    Atom* retAtoms;
    int numAtoms;
    if (XGetWMProtocols(disp, w, &retAtoms, &numAtoms))
    {
        for (int i = 0; i < numAtoms; i++)
        {
            if (retAtoms[i] == WM_DELETE_WINDOW)
            {
                XEvent killEvent;
                memset(&killEvent, 0, sizeof(killEvent));
                killEvent.xclient.type = ClientMessage;
                killEvent.xclient.window = w;
                killEvent.xclient.message_type = WM_PROTOCOLS;
                killEvent.xclient.format = 32;
                killEvent.xclient.data.l[0] = WM_DELETE_WINDOW;
                killEvent.xclient.data.l[1] = CurrentTime;
                XSendEvent(disp, w, False, NoEventMask, &killEvent);
                XFree(retAtoms);
                RemoveClient(*GetClientFromWindow(w));
                Arrange();
                return;
            }
        }
        XFree(retAtoms);
    }
    std::cout << "Could not get delete window atom" << std::endl;
    XKillClient(disp, w);
    RemoveClient(*GetClientFromWindow(w));
    Arrange();
}

void JXWM::OnClientMessage(const XEvent& e)
{
    std::cout << "In OnClientMessage function" << std::endl;
    XClientMessageEvent xcme = e.xclient;
    if (xcme.message_type == NET_NUMBER_OF_DESKTOPS)
    {
        tags = xcme.data.l[0];
        XChangeProperty(disp, root, NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&tags, 1);
        std::cout << "Got client message to change number of virtual desktops to " << tags << std::endl;
    }
    else if (xcme.message_type == NET_CURRENT_DESKTOP)
    {
        int clientTag = xcme.data.l[0];
        XChangeProperty(disp, root, NET_CURRENT_DESKTOP, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&clientTag, 1);
        std::cout << "Got client message to change current tag to tag " << currentTag << std::endl;
        ChangeTag(clientTag);
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
        KillWindow(c->window);
    }
}

void JXWM::OnDestroyNotify(const XEvent& e)
{
    std::cout << "In OnDestroyNotify" << std::endl;
    XDestroyWindowEvent xdwe = e.xdestroywindow;
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
    XSetWindowBorderWidth(disp, client.window, borderWidth);
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
    // TODO: get workable area from struts
    std::vector<Client> visibleClients;
    for (auto c: Clients)
    {
        if (c.tag == currentTag) { visibleClients.push_back(c); }
    }

    int numClients = visibleClients.size();
    if (numClients == 0) { return; }
    if (numClients == 1)
    {
        JMoveResizeClient(visibleClients[0], usableArea.x, usableArea.y, usableArea.w, usableArea.h);
        return;
    }

    Client master = visibleClients[0];
    JMoveResizeClient(master, usableArea.x, usableArea.y, usableArea.w/2, usableArea.h);
    int yGap = usableArea.h / (numClients - 1);
    int yOffSet = usableArea.y;
    for (int i = 1; i < numClients; i++)
    {
        JMoveResizeClient(visibleClients[i], usableArea.w / 2, yOffSet, usableArea.w / 2, yGap);
        yOffSet += yGap;
    }

}

void JXWM::SetWindowLayout(void(JXWM::*func)(void)) { layout = func; }

void JXWM::Arrange() { (this->*layout)(); }

void JXWM::OnWindowEnter(const XEvent& e)
{
    std::cout << "In OnWindowEnter function" << std::endl;
    Client* c = GetClientFromWindow(e.xcrossing.window);
    if (c != nullptr) { FocusClient(*c); }
}

void JXWM::OnUnmapNotify(const XEvent& e)
{
    std::cout << "In unmap notify function" << std::endl;
    /*XUnmapEvent xume = e->xunmap;
    Client* c = GetClientFromWindow(xume.window);
    if (c != nullptr && c->window != root) { RemoveClient(*c); }*/
}

void JXWM::ChangeTag(arg* arg) { ChangeTag(arg->tag); }

void JXWM::ChangeTag(int tagToChange)
{
    std::cout << "In ChangeTag Function" << std::endl;
    if (currentTag == tagToChange) { return; }
    for (auto c: Clients)
    {
        if (c.tag == currentTag) { XUnmapWindow(disp, c.window); }
        else if (c.tag == tagToChange) { XMapWindow(disp, c.window); }
    }
    currentTag = tagToChange;
    std::cout << "Changed to tag " << currentTag << std::endl;
    XChangeProperty(disp, root, NET_CURRENT_DESKTOP, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&currentTag, 1);
    Arrange();
}

void JXWM::OnCreateNotify(const XEvent& e)
{
    std::cout << "In OnCreateNotifyFunction" << std::endl;
}

void JXWM::Quit(arg* arg)
{
    std::cout << "In Quit function" << std::endl;
    quit = true;
}

void JXWM::ReadConfigFile(const std::string& configFile)
{
    std::ifstream file(configFile);
    if (!file.is_open())
    {
        std::cout << "Could not open config file " << configFile << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string item;
        char delimiter = ','; 
        std::vector<std::string> split;
        while (std::getline(ss, item, delimiter)) { split.push_back(item); }
        if (split[0] == "run") 
        {
            arg a = {.spawn = split[1].c_str()}; 
            Spawn(&a);
            Arrange(); 
        }
        else if(split[0] == "kb")
        {
            //for now assume all mod 4
            //KeySym sym = std::stoul(split[2]);
            if (split[3] == "spawn")
            {
                keybindings.push_back({Mod4Mask, XStringToKeysym(split[2].c_str()), &JXWM::Spawn, {.spawn = strdup(split[4].c_str())}});
            }
            else if (split[3] == "killwindow")
            {
                keybindings.push_back({Mod4Mask, XStringToKeysym(split[2].c_str()), &JXWM::KillWindow, nullptr});
            }
            else if (split[3] == "quit")
            {
                keybindings.push_back({Mod4Mask, XStringToKeysym(split[2].c_str()), &JXWM::Quit, nullptr});
            }
            else if(split[3] == "reload")
            {
                keybindings.push_back({Mod4Mask, XStringToKeysym(split[2].c_str()), &JXWM::ReloadConfig, nullptr});
            }
        }
        //else if()
    }
}

void JXWM::ReloadConfig(arg* arg)
{
    std::cout << "Reloading config..." << std::endl;
    keybindings.clear();
    ReadConfigFile("test.conf");
    GrabKeys();
}

bool JXWM::IsPager(Window w, Strut& strutsRet)
{
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    long* prop = nullptr;

    if (XGetWindowProperty(disp, w, NET_WM_STRUT_PARTIAL, 0, 12, False, XA_CARDINAL, &actualType, &actualFormat, &nItems, &bytesAfter, (unsigned char**)&prop) == Success && prop)
   {
       if (nItems == 12) 
       {
           strutsRet.left = prop[0];
           strutsRet.right = prop[1];
           strutsRet.top = prop[2];
           strutsRet.bottom = prop[3];
           XFree(prop);
           return true;
       }
        XFree(prop);
       return true;            
   }
    else { return false; }
}

void JXWM::UpdateStruts(Strut& struts)
{
    usableArea.x = struts.left;    
    usableArea.y = struts.top;
    usableArea.w = screenArea.w - struts.left - struts.right;
    usableArea.h = screenArea.h - struts.top - struts.bottom;
    std::cout << "Old area:\nx= " << screenArea.x << "\ny= " << screenArea.y << "\nw= " << screenArea.w << "\nh = " << screenArea.h << std::endl;
    std::cout << "New area:\nx= " << usableArea.x << "\ny= " << usableArea.y << "\nw= " << usableArea.w << "\nh = " << usableArea.h << std::endl;
}

void JXWM::ChangeClientTag(arg* arg) { ChangeClientTag(focused.window, arg->tag); }

void JXWM::ChangeClientTag(Window w, int tag)
{
    std::cout << "In ChangeClientTag Function" << std::endl;
    Client* c = GetClientFromWindow(w);
    if (c == nullptr) { return; }
    if (c->tag == tag) { return; }
    c->tag = tag;
    ChangeTag(tag);
}

int JXWM::JMoveResizeClient(Client& c, int x, int y, uint w, uint h)
{
    return XMoveResizeWindow(disp, c.window, x + borderWidth, y + borderWidth, w - (2*borderWidth), h - (2*borderWidth));
}

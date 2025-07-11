#include "jxwm.hpp"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>
#include <string.h>
#include <X11/Xatom.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "logger.hpp"

#define PRINTHERE std::cout << __LINE__ << std::endl;
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
    focused = nullptr;
    borderWidth = 3;
    currentTag = 0;
    tags = 9;
    
    JLogger::SetLogType(JLogger::LOGBOTH);
    JLogger::SetLogLevel(JLogger::LOGTRACE);
    JLogger::SetLogPath(std::string(getenv("HOME")) + "/.config/jxwm/jxwm.log");
    
} //init variables later to get rid of warnings

JXWM::~JXWM()
{
    JLogger::Info("Quiting JXWM...");
    XCloseDisplay(disp);
    //pkill -KILL -u $USER
}

int JXWM::Init()
{
    disp = XOpenDisplay(NULL);
    if (disp == NULL)
    {
        JLogger::Error("Failed to open X Display");
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
    for (uint i = 0; i < nChildren; i++) { Clients[currentTag].push_back({children[i]}); }
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
    JLogger::Error("Other window manager detected");
    return 0;
}

int JXWM::OnError(Display* disp, XErrorEvent* xee)
{
    JLogger::Error("An error occurred");
    char text[512];
    XGetErrorText(disp, xee->error_code, text, sizeof(text));
    std::stringstream ss;
    ss << text << "\nCode: " << xee->error_code
        << "\nType: " << xee->type << "\nSerial: " << xee->serial
        << "\nMinor: " << xee->minor_code << "\nResource ID: " << xee->resourceid
        << "\nRequest Code: " << xee->request_code;
    JLogger::Error(ss.str());
    return 0;
}

void JXWM::GrabKeys() 
{
    XUngrabKey(disp, AnyKey, AnyModifier, root);
    //first, grab tag keybindings
    for (int i = 0; i < 9; i++)
    { 
        keybindings.push_back({Mod4Mask | ShiftMask, XK_1 + i, &JXWM::ChangeClientTag, {.tag = i}});
        keybindings.push_back({Mod4Mask, XK_1 + i, &JXWM::ChangeTag, {.tag = i}});
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
    JLogger::Trace("In OnMapRequest");
    Window w = e.xmaprequest.window; 
    Client* c = GetClientFromWindow(w); 
    if (c != nullptr) 
    {
        XMapWindow(disp, c->window); 
        Arrange();
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
        return;
    }
    XMapWindow(disp, w); 
    XSelectInput(disp, w, EnterWindowMask | LeaveWindowMask); 
    Clients[currentTag].push_back({w}); 
    FocusClient(GetClientFromWindow(w));
    Arrange(); 
}

void JXWM::OnConfigureRequest(const XEvent& e)
{
    JLogger::Trace("In OnConfigureRequest");
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
    JLogger::Trace("In OnKeyPress");
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
    JLogger::Trace("In Spawn function");
    if (fork())
    {
        if (disp) { close(ConnectionNumber(disp)); }
        setsid();
        execl("/bin/sh", "sh", "-c", spawn, 0);
        exit(0);
    }
}

void JXWM::KillWindow(arg* arg) { KillWindow(focused); }

void JXWM::KillWindow(Client* c)
{
    JLogger::Trace("In KillWindow function");
    if (c == nullptr) { return; }
    Atom* retAtoms;
    int numAtoms;
    if (XGetWMProtocols(disp, c->window, &retAtoms, &numAtoms))
    {
        for (int i = 0; i < numAtoms; i++)
        {
            if (retAtoms[i] == WM_DELETE_WINDOW)
            {
                XEvent killEvent;
                memset(&killEvent, 0, sizeof(killEvent));
                killEvent.xclient.type = ClientMessage;
                killEvent.xclient.window = c->window;
                killEvent.xclient.message_type = WM_PROTOCOLS;
                killEvent.xclient.format = 32;
                killEvent.xclient.data.l[0] = WM_DELETE_WINDOW;
                killEvent.xclient.data.l[1] = CurrentTime;
                XSendEvent(disp, c->window, False, NoEventMask, &killEvent);
                XFree(retAtoms);
                RemoveClient(c);
                Arrange();
                return;
            }
        }
        XFree(retAtoms);
    }
    JLogger::Warning("Could not get delete window atom");
    XKillClient(disp, c->window);
    RemoveClient(c);
    Arrange();
}

void JXWM::OnClientMessage(const XEvent& e)
{
    JLogger::Trace("In OnClientMessage function");
    XClientMessageEvent xcme = e.xclient;
    if (xcme.message_type == NET_NUMBER_OF_DESKTOPS)
    {
        tags = (int)xcme.data.l[0];
        XChangeProperty(disp, root, NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&tags, 1);
        JLogger::Info("Got client message to change number of virtual desktops to " + std::to_string(tags));
    }
    else if (xcme.message_type == NET_CURRENT_DESKTOP)
    {
        int clientTag = (int)xcme.data.l[0];
        XChangeProperty(disp, root, NET_CURRENT_DESKTOP, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&clientTag, 1);
        JLogger::Info("Got client message to change current tag to tag " + std::to_string(currentTag));
        ChangeTag(clientTag);
    }
    else if (xcme.message_type == NET_CLOSE_WINDOW)
    {
        JLogger::Info("Got client message to kill a window, attempting to kill it");
        Client* c = GetClientFromWindow(xcme.window);
        KillWindow(c);
    }
    else
    {
        JLogger::Info("Got unknonw client message " + std::string(XGetAtomName(disp, xcme.message_type)));
    }
}

void JXWM::OnDestroyNotify(const XEvent& e)
{
    JLogger::Trace("In OnDestroyNotify");
    XDestroyWindowEvent xdwe = e.xdestroywindow;
    Client* c = GetClientFromWindow(xdwe.window);
    if (c == nullptr) { return; }
    RemoveClient(c);
}

void JXWM::FocusClient(Client* c)
{
    if (c == nullptr) { return; }
    unsigned long FBG_COLOR = 0x0000ff;
    unsigned long UBG_COLOR = 0xff0000;
    XSetWindowBorderWidth(disp, c->window, borderWidth);
    if (focused != nullptr) { XSetWindowBorder(disp, focused->window, UBG_COLOR); }
    focused = c;
    XSetWindowBorder(disp, focused->window, FBG_COLOR);
    XSetInputFocus(disp, c->window, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(disp, c->window);
    XChangeProperty(disp, root, NET_ACTIVE_WINDOW, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&c->window, 1);
}

Client* JXWM::GetClientFromWindow(Window w)
{
    for (int i = 0; i < Clients.size(); i++) 
    {
        for (int j = 0; j < Clients[i].size(); j++) 
        { 
            if (Clients[i][j].window == w) { return &Clients[i][j]; }
        }
    }
    return nullptr;
}

void JXWM::RemoveClient(Client* c) 
{
    if (c == nullptr) { return; }
    for (auto i = 0; i < Clients.size(); i++)
    {
        for (auto it = Clients[i].begin(); it != Clients[i].end(); it++)
        {
            if (it->window == c->window)
            {
                if (c->window == focused->window) { focused = nullptr; }
                Clients[i].erase(it);
                Arrange();
                return;
            }
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
    JLogger::Trace("In master stack");

    int numClients = Clients[currentTag].size();
    if (numClients == 0) { return; }
    if (numClients == 1)
    {
        JMoveResizeClient(Clients[currentTag][0], usableArea.x, usableArea.y, usableArea.w, usableArea.h);
        return;
    }

    Client master = Clients[currentTag][0];
    JMoveResizeClient(master, usableArea.x, usableArea.y, usableArea.w/2, usableArea.h);
    int yGap = usableArea.h / (numClients - 1);
    int yOffSet = usableArea.y;
    for (int i = 1; i < numClients; i++)
    {
        JMoveResizeClient(Clients[currentTag][i], usableArea.w / 2, yOffSet, usableArea.w / 2, yGap);
        yOffSet += yGap;
    }

}

void JXWM::SetWindowLayout(void(JXWM::*func)(void)) { layout = func; }

void JXWM::Arrange() { (this->*layout)(); }

void JXWM::OnWindowEnter(const XEvent& e)
{
    JLogger::Trace("In OnWindowEnter function");
    Client* c = GetClientFromWindow(e.xcrossing.window);
    if (c != nullptr) { FocusClient(c); }
}

void JXWM::OnUnmapNotify(const XEvent& e)
{
    JLogger::Trace("In unmap notify function");
}

void JXWM::ChangeTag(arg* arg) { ChangeTag(arg->tag); }

void JXWM::ChangeTag(int tagToChange)
{
    JLogger::Trace("In ChangeTag Function");
    if (currentTag == tagToChange) { return; }
    for (auto c: Clients[currentTag]) { XUnmapWindow(disp, c.window); }
    currentTag = tagToChange;
    for (auto c: Clients[currentTag]) { XMapWindow(disp, c.window); }
    JLogger::Info("Changed to tag " + std::to_string(currentTag));
    XChangeProperty(disp, root, NET_CURRENT_DESKTOP, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&currentTag, 1);
    Arrange();
}

void JXWM::OnCreateNotify(const XEvent& e)
{
    JLogger::Trace("In OnCreateNotifyFunction");
}

void JXWM::Quit(arg* arg)
{
    JLogger::Trace("In Quit function");
    quit = true;
}

void JXWM::ReadConfigFile(const std::string& configFile)
{
    std::ifstream file(configFile);
    if (!file.is_open())
    {
        JLogger::Error("Could not open config file " + configFile);
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
            Spawn(split[1].c_str());
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
            else if (split[3] == "logout")
            {
                keybindings.push_back({Mod4Mask, XStringToKeysym(split[2].c_str()), &JXWM::Logout, nullptr});
            }
            else if(split[3] == "reload")
            {
                keybindings.push_back({Mod4Mask, XStringToKeysym(split[2].c_str()), &JXWM::ReloadConfig, nullptr});
            }
            else if (split[3] == "move_left")
            {
                keybindings.push_back({Mod4Mask, XStringToKeysym(split[2].c_str()), &JXWM::MoveClientLeft, nullptr});
            }
            else if (split[3] == "move_right")
            {
                keybindings.push_back({Mod4Mask, XStringToKeysym(split[2].c_str()), &JXWM::MoveClientRight, nullptr});
            }
        }
        else if ( split[0] == "set")
        {
            if (split[1] == "bw") { borderWidth = std::stoi(split[2]); }
        }
    }
}

void JXWM::ReloadConfig(arg* arg)
{
    JLogger::Info("Reloading config...");
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
    JLogger::Info("Updated workable area");
}

void JXWM::ChangeClientTag(arg* arg) { ChangeClientTag(focused, arg->tag); }

void JXWM::ChangeClientTag(Client* c, int tag)
{
    JLogger::Trace("In ChangeClientTag Function");
    if (c == nullptr || currentTag == tag || tag < 0 || tag >= tags) { return; }
    auto& from = Clients[currentTag];
    auto& to = Clients[tag];
    for (auto it = from.begin(); it != from.end(); it++)
    {
        if (it->window == c->window)
        {
            to.push_back(std::move(*it));
            from.erase(it);
            break;
        }
    }
    ChangeTag(tag);
}

int JXWM::JMoveResizeClient(Client& c, int x, int y, uint w, uint h)
{
    return XMoveResizeWindow(disp, c.window, x + borderWidth, y + borderWidth, w - (2*borderWidth), h - (2*borderWidth));
}

void JXWM::MoveClientLeft(arg* arg) { MoveClient(focused, -1); }

void JXWM::MoveClientRight(arg* arg) { MoveClient(focused, 1); }

void JXWM::MoveClient(Client* c, int amount)
{
    JLogger::Trace("In MoveClient function");
    if (c == nullptr || Clients[currentTag].size() <= 1) { return; }
    int idx;
    for (auto i = 0; i < Clients[currentTag].size(); i++)
    {
        if (Clients[currentTag][i].window == c->window)
        {
            idx = i;
            break;
        }
    }
    int other = (idx + amount) % Clients[currentTag].size();
    std::swap(Clients[currentTag][idx], Clients[currentTag][other]);
    Arrange();
}

void JXWM::Logout(arg* arg) { Spawn("pkill -KILL -u $USER"); }

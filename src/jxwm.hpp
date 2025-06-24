#pragma once

#include <X11/X.h>
#include <X11/Xlib.h>
#include <vector>

 typedef struct 
{
    Window window;
    int tag;
} Client;


class JXWM
{
public:
    JXWM() = default;
    ~JXWM() = default;
    int Init();
    void Run();

    

private:
    Window root;
    Display* disp;
    long rootMask;
    static bool otherWM;
    bool quit;
    int tags;
    int currentTag;
    static int OnOtherWMDetected(Display* disp, XErrorEvent* xee);
    static int OnError(Display* disp, XErrorEvent* xee);
    void GetAtoms();
   
    Client focused;
    std::vector<Client> Clients;
    Client* GetClientFromWindow(Window w);
    void RemoveClient(Client& c);

    void GetExistingWindows();
    typedef union 
    {
        const char* spawn;
        Client* c;
        int tag;
    } arg;

    typedef struct 
    {
        unsigned mod;
        KeySym sym;
        void (JXWM::*func)(arg*);
        arg* argument;
    } keybinding;
    std::vector<keybinding> keybindings;

    void GrabKeys();

    void (JXWM::*handlers[LASTEvent])(XEvent*);
    void GrabHandlers();
    void OnMapRequest(XEvent* e);
    void OnUnmapNotify(XEvent* e);
    void OnConfigureRequest(XEvent* e);
    void OnKeyPress(XEvent* e);
    void OnClientMessage(XEvent* e);
    void OnDestroyNotify(XEvent* e);
    void OnWindowEnter(XEvent* e);

    void Spawn(arg* arg);
    void KillWindow(arg*);
    void ChangeTag(arg* arg);

    void FocusClient(Client& client);
    Window AttemptToGetFocusedWindow();
    Atom WM_DELETE_WINDOW;
    Atom WM_PROTOCOLS;
    Atom NET_SUPPORTED;
    Atom NET_ACTIVE_WINDOW;
    Atom NET_NUMBER_OF_DESKTOPS;
    Atom NET_CURRENT_DESKTOP;
    Atom NET_CLOSE_WINDOW;

    void Arrange();
    void (JXWM::*layout)(void);
    void SetWindowLayout(void(JXWM::*func)(void));
    void MasterStack();
};

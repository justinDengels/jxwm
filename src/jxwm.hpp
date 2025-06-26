#pragma once

#include <X11/X.h>
#include <X11/Xlib.h>
#include <vector>
#include <string>

typedef struct 
{
    Window window;
    int tag;
} Client;


class JXWM
{
public:
    JXWM();
    ~JXWM();
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
    void ReadConfigFile(const std::string& configFile);
   
    Client focused;
    std::vector<Client> Clients;
    Client* GetClientFromWindow(Window w);
    void RemoveClient(Client& c);

    void GetExistingWindows();
    typedef union 
    {
        const char* spawn; //wish to make this a std::string if possible
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

    void (JXWM::*handlers[LASTEvent])(XEvent*); //debating if this should be XEvent* or XEvent&
    void GrabHandlers();
    void OnMapRequest(XEvent* e);
    void OnUnmapNotify(XEvent* e);
    void OnConfigureRequest(XEvent* e);
    void OnKeyPress(XEvent* e);
    void OnClientMessage(XEvent* e);
    void OnDestroyNotify(XEvent* e);
    void OnWindowEnter(XEvent* e);
    void OnCreateNotify(XEvent* e);

    void Spawn(arg* arg);
    void KillWindow(arg*);
    void ChangeTag(arg* arg);
    void Quit(arg* arg);
    void ReloadConfig(arg* arg);

    void FocusClient(Client& client);
    Window AttemptToGetFocusedWindow(); //Probably useless
    Atom WM_DELETE_WINDOW;
    Atom WM_PROTOCOLS;
    Atom NET_SUPPORTED;
    Atom NET_ACTIVE_WINDOW;
    Atom NET_NUMBER_OF_DESKTOPS;
    Atom NET_CURRENT_DESKTOP;
    Atom NET_CLOSE_WINDOW;
    //Should I make these in an array?

    void Arrange();
    void (JXWM::*layout)(void);
    void SetWindowLayout(void(JXWM::*func)(void));
    void MasterStack();
};

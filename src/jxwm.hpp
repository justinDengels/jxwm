#pragma once

#include <X11/X.h>
#include <X11/Xlib.h>
#include <vector>
#include <string>
#include <array>

typedef struct 
{
    Window window;
    int tag;
} Client;

typedef struct 
{
    int x;
    int y;
    uint w;
    uint h;
} Rect;

typedef struct 
{
    long left;
    long right;
    long top;
    long bottom;
} Strut;

typedef union 
{
    const char* spawn; //wish to make this a std::string if possible
    int tag;
} arg;

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
    int screenum;
    Rect screenArea;
    Rect usableArea;
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
    

    typedef struct 
    {
        unsigned mod;
        KeySym sym;
        void (JXWM::*func)(arg*);
        arg* argument;
    } keybinding;
    std::vector<keybinding> keybindings;

    void GrabKeys();

    void (JXWM::*handlers[LASTEvent])(const XEvent&); 
    void GrabHandlers();
    void OnMapRequest(const XEvent& e);
    void OnUnmapNotify(const XEvent& e);
    void OnConfigureRequest(const XEvent& e);
    void OnKeyPress(const XEvent& e);
    void OnClientMessage(const XEvent& e);
    void OnDestroyNotify(const XEvent& e);
    void OnWindowEnter(const XEvent& e);
    void OnCreateNotify(const XEvent& e);

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
    Atom NET_WM_STRUT_PARTIAL;
    //Should I make these in an array?
    bool IsPager(Window w, Strut& strutsRet);
    void UpdateStruts(Strut& struts);
    void Arrange();
    void (JXWM::*layout)(void);
    void SetWindowLayout(void(JXWM::*func)(void));
    void MasterStack();
};

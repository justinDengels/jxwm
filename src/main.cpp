#include "jxwm.hpp"

int main()
{
    JXWM* wm = new JXWM();
    int res = wm->Init();
    if (res != 0) 
    {
        delete wm;
        return res; 
    }

    wm->Run();
    delete wm;
    return 0;
}

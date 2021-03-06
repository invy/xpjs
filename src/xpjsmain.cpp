
#include <iostream>

#include <XPLMDefs.h>
#include <XPLMPlanes.h>
#include <XPLMDataAccess.h>
#include <XPLMProcessing.h>
#include "XPLMUtilities.h"

#include "xpdataaccess.h"
#include "jsengine.h"

JSEngine* g_js = nullptr;
XPDataAccess g_xpdata;

XPLMFlightLoopID callbackId;

static std::string getDirSeparator()
{
    std::string sep = XPLMGetDirectorySeparator();
    if(sep == std::string(":")) {
        return std::string("/");
    } else {
        return sep;
    }
}

std::string getAircraftDir()
{
    char model[512], path[512];
    XPLMGetNthAircraftModel(0, model, path);
    XPLMExtractFileAndPath(path);

    return std::string(path) + getDirSeparator();
}


PLUGIN_API int XPluginStart(
                                                char *          outName,
                                                char *          outSig,
                                                char *          outDesc)
{
    strcpy(outName, "JS Script Engine for XPlane");
    strcpy(outSig, "xpjs.script.engine");
    strcpy(outDesc, "The plugin allows to control XPlane using JavaScript language.");
    return 1;
}

PLUGIN_API void XPluginStop(void)
{
}

PLUGIN_API int XPluginEnable(void)
{
    ::g_js = new JSEngine();
    if(!g_js->setup(getAircraftDir())) {
        delete ::g_js;
        return false;
    }
    g_js->callJsOnEnable();
    return true;
}

PLUGIN_API void XPluginDisable(void)
{
    g_js->callJsOnDisable();

    if(::g_js)
        delete ::g_js;
}

PLUGIN_API void XPluginReceiveMessage(
                    XPLMPluginID        /*inFromWho*/,
                    long                        /*inMessage*/,
                    void *                      /*inParam*/)
{
}



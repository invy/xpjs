#ifndef JSENGINE_H
#define JSENGINE_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <jsapi.h>
#include <jsdbgapi.h>

#include "jsrdbg.h"

#include "xpcallbacks.h"

static JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
};

class FlightLoopCallbackObject;

class JSEngine
{
    // Component responsible for loading script's source code if the JS engine cannot provide it.
    class ScriptLoader : public JSR::IJSScriptLoader {
        JSEngine& engine;
    public:

        ScriptLoader(JSEngine &e) : engine(e) { }

        ~ScriptLoader() { }

        int load( JSContext *cx, const std::string &path, std::string &script ) {
            if( path == "avionics.js" ) {
                script = engine.getScript();
                return JSR_ERROR_NO_ERROR;
            }
            return JSR_ERROR_FILE_NOT_FOUND;
        }

    };

private:
    JSRuntime *rt;
    JSContext *cx;
    JSAutoRequest *ar;
    JS::RootedObject *global;
    JSAutoCompartment *ac;
    std::unique_ptr<JSR::JSRemoteDebugger> dbg;
    std::string scriptFileName;
    std::string scriptContent;
    ScriptLoader loader;
public:
    std::vector<FlightLoopCallbackObject> callbacks;
public:
    bool InitDebugger();
    std::string& getScript();
    JSEngine();
    ~JSEngine();
    bool setup(const std::string scriptPath);
    void callJsUpdate();
    void callJsOnEnable();
    void callJsOnDisable();
    void onError(const std::string& error, JSErrorReport *report);
    FlightLoopCallbackObject* addJSFlightLoopCallback(const std::string &callbackName);
    void unregisterAllCallbacks();
    static void dispatchError(JSContext* ctx, const char* message, JSErrorReport*);
};

#endif // JSENGINE_H

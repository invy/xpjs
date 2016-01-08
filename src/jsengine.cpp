#include <fstream>
#include <string>
#include <sstream>
#include <iterator>

#include <jsapi.h>
#include <jsdbgapi.h>


#include "XPLMUtilities.h"
#include "jsfuncwrapper.h"
#include "jsengine.h"

#include "jsrdbg.h"


JSEngine::JSEngine() :
    loader(*this)
{
    rt = JS_NewRuntime(64L * 1024 * 1024, JS_USE_HELPER_THREADS);
    cx = JS_NewContext(rt, 8192);
    ar = new JSAutoRequest(cx);
    global = new JS::RootedObject(cx, JS_NewGlobalObject(cx, &global_class, nullptr));
    ac = new JSAutoCompartment(cx, *global);


    JS_SetNativeStackQuota(rt, 1024 * 1024);
    JS_SetGCParameter(rt, JSGC_MAX_BYTES, 0xffffffff);

    JS_InitStandardClasses(cx, *global);

    JS_DefineFunctions(cx, *global, js_mappedFunctions);

    JS_SetRuntimePrivate(rt, this);
    JS_SetErrorReporter(cx, &JSEngine::dispatchError);

}


// Initializes debugger and runs script into its scope.
bool JSEngine::InitDebugger() {

    // Initialize debugger.

    // Configure remote debugger.
    JSR::JSRemoteDebuggerCfg cfg;
    cfg.setTcpHost(JSR_DEFAULT_TCP_BINDING_IP);
    cfg.setTcpPort(JSR_DEFAULT_TCP_PORT);
    cfg.setScriptLoader(&loader);

    // Configure debugger engine.
    JSR::JSDbgEngineOptions dbgOptions;
    // Suspend script just after starting it.
    dbgOptions.continueWhenNoConnections();

    this->dbg.reset(new JSR::JSRemoteDebugger( cfg ));

    if( dbg->install(cx, "avionics-JS", dbgOptions) != JSR_ERROR_NO_ERROR ) {
        XPLMDebugString("Cannot install debugger.\n");
        return false;
    }

    if( dbg->start() != JSR_ERROR_NO_ERROR ) {
        dbg->uninstall( cx );
        XPLMDebugString("Cannot start debugger.\n");
        return false;
    }

    return true;
}

JSEngine::~JSEngine() {
    dbg->stop();
    dbg->uninstall(cx);

    delete global;
    delete ac;
    delete ar;

    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
}

void JSEngine::callJsUpdate() {
    JS::RootedValue rval(cx);
    JS::AutoValueVector argv(cx);
    argv.resize(0);
    JS_CallFunctionName(this->cx, *(this->global), "update", 0, argv.begin(), rval.address());
}

void JSEngine::callJsOnEnable() {
    JS::RootedValue rval(cx);
    JS::AutoValueVector argv(cx);
    argv.resize(0);
    JS_CallFunctionName(this->cx, *(this->global), "onEnable", 0, argv.begin(), rval.address());
}

void JSEngine::callJsOnDisable() {
    JS::RootedValue rval(cx);
    JS::AutoValueVector argv(cx);
    argv.resize(0);
    JS_CallFunctionName(this->cx, *(this->global), "onDisable", 0, argv.begin(), rval.address());
}



bool JSEngine::setup(const std::string scriptPath) {

    this->scriptFileName = scriptPath + "avionics.js";

    if( !InitDebugger() ) {
        XPLMDebugString("Application failed.\n");
        return false;
    }

    XPLMDebugString("Script path: ");
    XPLMDebugString(scriptFileName.c_str());
    XPLMDebugString("\n");

    std::ifstream scriptFile(scriptFileName, std::ifstream::in);
    scriptContent.assign((std::istreambuf_iterator<char>(scriptFile)), (std::istreambuf_iterator<char>()));
    int lineno = 1;
    JS::RootedValue rval(cx);


    // Register newly created global object into the debugger,
    // in order to make it debuggable.
    if( dbg->addDebuggee( cx, *global ) != JSR_ERROR_NO_ERROR ) {
        XPLMDebugString("Cannot add debuggee.\n");
        return false;
    }



    bool ok = JS_EvaluateScript(cx, *global, scriptContent.c_str(), scriptContent.size(), this->scriptFileName.c_str(), lineno, rval.address());
    if (!ok)
        return false;
    return true;
}

void JSEngine::onError(const std::string& message, JSErrorReport *report) {
    std::stringstream ss;
    ss << "XPJS: [" << report->errorNumber << "] " << report->filename << ": "
                    << report->lineno << ":" << report->column << "\n\t" << message << "\n"
                    << "\t\t" << "at:\t" << report->tokenptr << "\n";
    XPLMDebugString(ss.str().c_str());
}

void JSEngine::dispatchError(JSContext* ctx, const char* message, JSErrorReport* report) {
    auto rt = JS_GetRuntime(ctx);
    auto rt_userdata = JS_GetRuntimePrivate(rt);
    if (rt_userdata) {
        auto req = static_cast<JSEngine*>(rt_userdata);
        req->onError(message, report);
    }
}

std::string& JSEngine::getScript() {
    return this->scriptContent;
}

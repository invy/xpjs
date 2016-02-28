#include <fstream>
#include <string>
#include <sstream>
#include <iterator>

#include <jsapi.h>
#include <jsdbgapi.h>


#include <XPLMUtilities.h>
#include <XPLMProcessing.h>

#include "importer.h"
#include "jsapi-util.h"
#include "jsfuncwrapper.h"
#include "jsengine.h"

#include "jsrdbg.h"


JSEngine::JSEngine() :
    loader(*this),
    callbacks(context)
{
    context.runtime = JS_NewRuntime(64L * 1024 * 1024, JS_USE_HELPER_THREADS);

    context.context = JS_NewContext(context.runtime, 8192);
    context.global = JS::RootedObject(context.context, JS_NewGlobalObject(context.context, &global_class, nullptr));

    ac = new JSAutoCompartment(context.context, context.global);

	JS_SetNativeStackQuota(context.runtime, 1024 * 1024);
    JS_SetGCParameter(context.runtime, JSGC_MAX_BYTES, 0xffffffff);

    JS_InitStandardClasses(context.context, context.global);

    for (int i = 0; i < GJS_STRING_LAST; i++)
        context.const_strings[i] = gjs_intern_string_to_id(context.context, gjs_const_strings[i]);
	
    JS_DefineFunctions(context.context, context.global, js_mappedFunctions);

    JS_SetRuntimePrivate(context.runtime, this);
    JS_SetErrorReporter(context.context, &JSEngine::dispatchError);

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

    if( dbg->install(context.context, "avionics-JS", dbgOptions) != JSR_ERROR_NO_ERROR ) {
        XPLMDebugString("Cannot install debugger.\n");
        return false;
    }

    if( dbg->start() != JSR_ERROR_NO_ERROR ) {
        dbg->uninstall( context.context );
        XPLMDebugString("Cannot start debugger.\n");
        return false;
    }

    return true;
}

JSEngine::~JSEngine() {
    dbg->stop();
    dbg->uninstall(context.context);

    delete ac;

    JS_DestroyContext(context.context);
    JS_DestroyRuntime(context.runtime);
    JS_ShutDown();
}

void JSEngine::callJsUpdate() {
    JSAutoRequest ar(context.context);
    JS::RootedValue rval(context.context);
    JS::AutoValueVector argv(context.context);
    argv.resize(0);
    JS_CallFunctionName(this->context.context, JS::Rooted<JSObject*>(context.context, context.global), "update", 0, argv.begin(), rval.address());
}

void JSEngine::callJsOnEnable() {
    JSAutoRequest ar(context.context);
    JS::RootedValue rval(context.context);
    JS::AutoValueVector argv(context.context);
    argv.resize(0);
    JS_CallFunctionName(this->context.context, JS::Rooted<JSObject*>(context.context, context.global), "onEnable", 0, argv.begin(), rval.address());
}

void JSEngine::callJsOnDisable() {
    JSAutoRequest ar(context.context);
    JS::RootedValue rval(context.context);
    JS::AutoValueVector argv(context.context);
    argv.resize(0);
    JS_CallFunctionName(this->context.context, JS::Rooted<JSObject*>(context.context, context.global), "onDisable", 0, argv.begin(), rval.address());
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
    JS::RootedValue rval(context.context);

    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);

    JSAutoRequest ar(context.context);
    JS_SetContextPrivate(context.context, &context);

	// Register newly created global object into the debugger,
    // in order to make it debuggable.
    if( dbg->addDebuggee( context.context, JS::Rooted<JSObject*>(context.context, context.global) ) != JSR_ERROR_NO_ERROR ) {
        XPLMDebugString("Cannot add debuggee.\n");
        return false;
    }

    std::vector<std::string> search_path;
	search_path.push_back(scriptPath);

    /* We create the global-to-runtime root importer with the
     * passed-in search path. If someone else already created
     * the root importer, this is a no-op.
     */
    if (!gjs_create_root_importer(context.context, search_path, true))
        XPLMDebugString("Failed to create root importer\n");

    /* Now copy the global root importer (which we just created,
     * if it didn't exist) to our global object
     */
    if (!gjs_define_root_importer(context.context, context.global))
        XPLMDebugString("Failed to point 'imports' property at root importer\n");

    bool ok = contextEval(&context, this->scriptFileName, scriptContent);
    
    if (!ok)
        return false;
    return true;
}

XPJSCallbacks& JSEngine::getCallbacks() {
	return this->callbacks;
}

void JSEngine::onError(const std::string& message, JSErrorReport *report) {
    std::stringstream ss;
    ss << "XPJS: [" << report->errorNumber << "] "
	                << ((report->filename != nullptr) ? 
	                    report->filename : "") << ": "
                    << report->lineno
                    << ":" << report->column << "\n\t" << message << "\n"
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

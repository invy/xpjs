/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2016  <copyright holder> <email>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>

#include <jsapi.h>

#include "jsapi-util.h"
#include "js_core_utils.h"


void gjs_debug(unsigned, const char *format, ...) {
    // do somehthing custom

    va_list args;
    va_start( args, format );

    vprintf( format, args );
	printf("\n");
	fflush(stdout);

    va_end( args );	
}


/**
 * gjs_get_string_id:
 * @context: a #JSContext
 * @id: a jsid that is an object hash key (could be an int or string)
 * @name_p place to store ASCII string version of key
 *
 * If the id is not a string ID, return false and set *name_p to %NULL.
 * Otherwise, return true and fill in *name_p with ASCII name of id.
 *
 * Returns: true if *name_p is non-%NULL
 **/
JSBool gjs_get_string_id (JSContext       *context,
                   jsid             id,
                   std::string      &name)
{
    jsval id_val;

    if (!JS_IdToValue(context, id, &id_val))
        return JS_FALSE;

    if (JSVAL_IS_STRING(id_val)) {
		name = JS_EncodeString(context, id_val.toString());
        return JS_TRUE;
    } else {
        return JS_FALSE;
    }
}


std::string vsout( std::string format, va_list args)
{
    va_list args_copy ;
    va_copy( args_copy, args ) ;

    const auto sz = std::vsnprintf( nullptr, 0, format.c_str(), args ) + 1 ;

    try
    {
        std::string result( sz, ' ' ) ;
        std::vsnprintf( &result.front(), sz, format.c_str(), args_copy ) ;

        va_end(args_copy) ;
        va_end(args) ;

        return result ;
    }

    catch( const std::bad_alloc& )
    {
        va_end(args_copy) ;
        va_end(args) ;
        throw ;
    }
}

JSBool gjs_string_from_utf8(JSContext  *context,
                     const std::string &utf8_string,
                     jsval      *value_p)
{   
    JSString *str;
    
    /* intentionally using n_bytes even though glib api suggests n_chars; with
    * n_chars (from g_utf8_strlen()) the result appears truncated
    */
    
    JS_BeginRequest(context);
    
    /* Avoid a copy - assumes that g_malloc == js_malloc == malloc */
    str = JS_NewStringCopyN(context, utf8_string.c_str(), utf8_string.size());
    
    if (str && value_p)
        *value_p = STRING_TO_JSVAL(str);

	JS_EndRequest(context);
    return str != NULL;
}

JSBool gjs_string_to_utf8 (JSContext  *context,
                    const jsval value,
                    std::string&      utf8_string_p)
{
    JSString *str;
    size_t len;
    char *bytes;

    JS_BeginRequest(context);

    if (!JSVAL_IS_STRING(value)) {
        gjs_throw(context, "Value is not a string, cannot convert to UTF-8");
        JS_EndRequest(context);
        return JS_FALSE;
    }

    str = value.toString();

    len = JS_GetStringEncodingLength(context, str);
    if (len == -1) {
        JS_EndRequest(context);
        return JS_FALSE;
    }

    bytes = JS_EncodeStringToUTF8(context, str);
    utf8_string_p = bytes;

    JS_EndRequest(context);

    return JS_TRUE;
}



static void gjs_throw_valist(JSContext       *context,
                 const std::string &error_class,
                 const std::string &format,
                 va_list           args)
{
    std::string s;
    JSBool result;
    jsval v_constructor, v_message;
    JSObject *err_obj;

    s = vsout(format, args);
	gjs_debug(0, format.c_str(), args);

    JSAutoCompartment compartment(context, JS_GetGlobalForScopeChain(context));

    JS_BeginRequest(context);

    if (JS_IsExceptionPending(context)) {
        /* Often it's unclear whether a given jsapi.h function
         * will throw an exception, so we will throw ourselves
         * "just in case"; in those cases, we don't want to
         * overwrite an exception that already exists.
         * (Do log in case our second exception adds more info,
         * but don't log as topic ERROR because if the exception is
         * caught we don't want an ERROR in the logs.)
         */
        std::cout << "Ignoring second exception: '" << s << "'\n";
        JS_EndRequest(context);
        return;
    }

    result = JS_FALSE;

    if (!gjs_string_from_utf8(context, s, &v_message)) {
        JS_ReportError(context, "Failed to copy exception string");
        goto out;
    }

    if (!JS_GetProperty(context, JS_GetGlobalForScopeChain(context), error_class.c_str(), &v_constructor) ||
        !v_constructor.isObjectOrNull()) {
        JS_ReportError(context, "??? Missing Error constructor in global object?");
        goto out;
    }

    /* throw new Error(message) */
    err_obj = JS_New(context, JSVAL_TO_OBJECT(v_constructor), 1, &v_message);
    JS_SetPendingException(context, OBJECT_TO_JSVAL(err_obj));

    result = JS_TRUE;

 out:

    if (!result) {
        /* try just reporting it to error handler? should not
         * happen though pretty much
         */
        JS_ReportError(context,
                       "Failed to throw exception '%s'",
                       s.c_str());
    }
    JS_EndRequest(context);
}

/* Throws an exception, like "throw new Error(message)"
 *
 * If an exception is already set in the context, this will
 * NOT overwrite it. That's an important semantic since
 * we want the "root cause" exception. To overwrite,
 * use JS_ClearPendingException() first.
 */
void gjs_throw(JSContext       *context,
          const std::string      &format,
          ...)
{
    va_list args;

    va_start(args, format.c_str());
    gjs_throw_valist(context, "Error", format, args);
    va_end(args);
}


/*
 * Like gjs_throw, but allows to customize the error
 * class. Mainly used for throwing TypeError instead of
 * error.
 */
void gjs_throw_custom(JSContext       *context,
                 const std::string     &error_class,
                 const std::string     &format,
                 ...)
{
    va_list args;

    va_start(args, format.c_str());
    gjs_throw_valist(context, error_class, format, args);
    va_end(args);
}

void gjs_throw_abstract_constructor_error(JSContext *context,
                                     jsval     *vp)
{
    jsval callee;
    jsval prototype;
    JSClass *proto_class;
    const char *name = "anonymous";

    callee = JS_CALLEE(context, vp);

    if (callee.isObject()) {
        if (gjs_object_get_property_const(context, JSVAL_TO_OBJECT(callee),
                                          GJS_STRING_PROTOTYPE, &prototype)) {
            proto_class = JS_GetClass(JSVAL_TO_OBJECT(prototype));
            name = proto_class->name;
        }
    }

    gjs_throw(context, "You cannot construct new instances of '%s'", name);
}
 

JSBool gjs_typecheck_instance(JSContext *context,
                       JSObject  *obj,
                       JSClass   *static_clasp,
                       JSBool     throw_error)
{
    if (!JS_InstanceOf(context, obj, static_clasp, NULL)) {
        if (throw_error) {
            JSClass *obj_class = JS_GetClass(obj);

            gjs_throw_custom(context, "TypeError",
                             "Object %p is not a subclass of %s, it's a %s",
                             obj, static_clasp->name, obj_class->name);
        }

        return JS_FALSE;
    }

    return JS_TRUE;
}

jsid gjs_context_get_const_string(JSContext      *context,
                             GjsConstString  name)
{
    GjsContext *gjs_context = (GjsContext *) JS_GetContextPrivate(context);
    return gjs_context->const_strings[name];
}


/* Returns whether the object had the property; if the object did
 * not have the property, always sets an exception. Treats
 * "the property's value is JSVAL_VOID" the same as "no such property,".
 * Guarantees that *value_p is set to something, if only JSVAL_VOID,
 * even if an exception is set and false is returned.
 *
 * Requires request.
 */
bool gjs_object_require_property(JSContext       *context,
                            JSObject        *obj,
                            const std::string      &obj_description,
                            jsid             property_name,
                            jsval           *value_p)
{   
    jsval value;
    std::string name;
    
    value = JSVAL_VOID;
    if (value_p) 
        *value_p = value;
    
    if ((!JS_GetPropertyById(context, obj, property_name, &value)))
        return false;
    
    if ((!JSVAL_IS_VOID(value))) {
        if (value_p) 
            *value_p = value;
        return true;
    }
     /* remember gjs_throw() is a no-op if JS_GetProperty()
     * already set an exception
     */

    gjs_get_string_id(context, property_name, name);

    if (obj_description.size() == 0)
        gjs_throw(context,
                  "No property '%s' in %s (or its value was undefined)",
                  name, obj_description);
    else
        gjs_throw(context,
                  "No property '%s' in object %p (or its value was undefined)",
                  name, obj);

    return false;
}

/**
 * gjs_get_import_global:
 * @context: a #JSContext
 *
 * Gets the "import global" for the context's runtime. The import
 * global object is the global object for the context. It is used
 * as the root object for the scope of modules loaded by GJS in this
 * runtime, and should also be used as the globals 'obj' argument passed
 * to JS_InitClass() and the parent argument passed to JS_ConstructObject()
 * when creating a native classes that are shared between all contexts using
 * the runtime. (The standard JS classes are not shared, but we share
 * classes such as GObject proxy classes since objects of these classes can
 * easily migrate between contexts and having different classes depending
 * on the context where they were first accessed would be confusing.)
 *
 * Return value: the "import global" for the context's
 *  runtime. Will never return %NULL while GJS has an active context
 *  for the runtime.
 */
JSObject* gjs_get_import_global(JSContext *context)
{
    GjsContext *gjs_context = (GjsContext *) JS_GetContextPrivate(context);
    return gjs_context->global;
}



JSObject *gjs_build_string_array(JSContext   *context,
                       std::vector<std::string> &array_values)
{
    JSObject *array;

	std::vector<jsval> elems;

    for (auto &av : array_values) {
        jsval element;
        element = STRING_TO_JSVAL(JS_NewStringCopyZ(context, av.c_str()));
        elems.push_back(element);
    }

    array = JS_NewArrayObject(context, elems.size(), (jsval*) &(elems[0]));

    return array;
}

JSObject* gjs_define_string_array(JSContext   *context,
                        JSObject    *in_object,
                        const std::string&  array_name,
                        std::vector<std::string> &array_values,
                        unsigned     attrs)
{
    JSObject *array;

    JSAutoRequest ar(context);

    array = gjs_build_string_array(context, array_values);

    if (array != NULL) {
        if (!JS_DefineProperty(context, in_object,
                               array_name.c_str(), OBJECT_TO_JSVAL(array),
                               NULL, NULL, attrs))
            array = NULL;
    }

    return array;
}

jsval gjs_get_global_slot (JSContext     *context,
                     GjsGlobalSlot  slot)
{
    JSObject *global;
    global = JS_GetGlobalForScopeChain(context);
    return JS_GetReservedSlot(global, JSCLASS_GLOBAL_SLOT_COUNT + slot);
}

void gjs_set_global_slot (JSContext     *context,
                     GjsGlobalSlot  slot,
                     jsval          value)
{
    JSObject *global;
    global = JS_GetGlobalForScopeChain(context);
    JS_SetReservedSlot(global, JSCLASS_GLOBAL_SLOT_COUNT + slot, value);
}

bool gjs_object_get_property_const(JSContext      *context,
                              JSObject       *obj,
                              GjsConstString  property_name,
                              jsval          *value_p)
{
    jsid pname;
    pname = gjs_context_get_const_string(context, property_name);
    return JS_GetPropertyById(context, obj, pname, value_p);
}

jsid gjs_intern_string_to_id (JSContext  *context,
                         const char *string)
{
    JSString *str;
    jsid id;
    JS_BeginRequest(context);
    str = JS_InternString(context, string);
    id = INTERNED_STRING_TO_JSID(context, str);
    JS_EndRequest(context);
    return id;
}

struct RuntimeData {
  JSBool in_gc_sweep;
};

JSRuntime *gjs_runtime_for_current_thread(void)
{
    JSRuntime *runtime = nullptr;//(JSRuntime *) g_private_get(&thread_runtime);
    RuntimeData *data;

    if (!runtime) {
        runtime = JS_NewRuntime(32*1024*1024 /* max bytes */, JS_USE_HELPER_THREADS);
        if (runtime == NULL)
            std::cout <<  "Failed to create javascript runtime\n";

        data = new RuntimeData;
        JS_SetRuntimePrivate(runtime, data);

        JS_SetNativeStackQuota(runtime, 1024*1024);
        JS_SetGCParameter(runtime, JSGC_MAX_BYTES, 0xffffffff);
//        JS_SetLocaleCallbacks(runtime, &gjs_locale_callbacks);
//        JS_SetFinalizeCallback(runtime, gjs_finalize_callback);

//        g_private_set(&thread_runtime, runtime);
    }

    return runtime;
}

void _gjs_context_schedule_gc_if_needed (GjsContext *js_context)
{
    if (js_context->auto_gc_id > 0)
        return;

/*    js_context->auto_gc_id = g_idle_add_full(G_PRIORITY_LOW,
                                             trigger_gc_if_needed,
                                             js_context, NULL);*/
}


void gjs_schedule_gc_if_needed (JSContext *context)
{
    GjsContext *gjs_context;

    /* We call JS_MaybeGC immediately, but defer a check for a full
     * GC cycle to an idle handler.
     */
    JS_MaybeGC(context);

    gjs_context = (GjsContext *) JS_GetContextPrivate(context);
    if (gjs_context)
        _gjs_context_schedule_gc_if_needed(gjs_context);
}


JSBool gjs_eval_with_scope(JSContext    *context,
                    JSObject     *object,
                    const char   *script,
                    long int        script_len,
                    const char   *filename,
                    jsval        *retval_p)
{
    int start_line_number = 1;
    jsval retval = JSVAL_VOID;
    JSAutoRequest ar(context);

    if (script_len < 0)
        script_len = strlen(script);

    /* log and clear exception if it's set (should not be, normally...) */
    if (JS_IsExceptionPending(context)) {
        std::cout << "gjs_eval_in_scope called with a pending exception\n";
        return JS_FALSE;
    }

    if (!object)
        object = JS_NewObject(context, NULL, NULL, NULL);

    JS::CompileOptions options(context);
    options.setUTF8(true)
           .setFileAndLine(filename, start_line_number)
           .setSourcePolicy(JS::CompileOptions::LAZY_SOURCE);

    js::RootedObject rootedObj(context, object);

    if (!JS::Evaluate(context, rootedObj, options, script, script_len, &retval))
        return JS_FALSE;

//    gjs_schedule_gc_if_needed(context);

    if (JS_IsExceptionPending(context)) {
        std::cout << ("EvaluateScript returned JS_TRUE but exception was pending; "
                  "did somebody call gjs_throw() without returning JS_FALSE?\n");
        return JS_FALSE;
    }

    gjs_debug(/*GJS_DEBUG_CONTEXT*/ 0,
              "Script evaluation succeeded");

    if (retval_p)
        *retval_p = retval;

    return JS_TRUE;
}

bool contextEval(GjsContext   *js_context, const std::string &filename, const std::string& script)
{
    jsval retval;

    JSAutoCompartment ac(js_context->context, js_context->global);
    JSAutoRequest ar(js_context->context);

    if (!gjs_eval_with_scope(js_context->context, NULL, script.c_str(), script.size(), filename.c_str(), &retval)) {
//        gjs_log_exception(js_context->context);
        gjs_debug(0, "JS_EvaluateScript() failed");
        return false;
    }
    return true;
}

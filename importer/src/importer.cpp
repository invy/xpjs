/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008-2010  litl, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include <string>
#include <vector>
#include <iostream>
#include <jsapi.h>

#include <string.h>

#include "utils.h"

#include "jsapi-util.h"

#include "compat.h"
#include "importer.h"
#include "native.h"


#define GJS_DEBUG_IMPORTER 0
#define g_assert assert
#define g_error printf



typedef struct {
    bool is_root;
} Importer;

typedef struct {
    std::vector<std::string> elements;
    unsigned int index;
} ImporterIterator;

const char MODULE_INIT_FILENAME[] =  "__init__.js";
const char MODULE_SUFFIX[] = ".so";
const char JS_SUFFIX[] = ".js";

std::vector<std::string> gjs_search_path;

extern struct JSClass gjs_importer_class;

GJS_DEFINE_PRIV_FROM_JS(Importer, gjs_importer_class)

static JSBool
define_meta_properties(JSContext  *context,
                       JSObject   *module_obj,
                       const char *full_path,
                       std::string module_name,
                       JSObject   *parent)
{
	bool parent_is_module;

    /* We define both __moduleName__ and __parentModule__ to null
     * on the root importer
     */
    parent_is_module = parent && JS_InstanceOf(context, parent, &gjs_importer_class, NULL);

    gjs_debug(GJS_DEBUG_IMPORTER, "Defining parent %p of %p '%s' is mod %d",
              parent, module_obj, module_name.size() > 0 ? module_name.c_str() : "<root>", parent_is_module);

    if (full_path != NULL) {
        if (!JS_DefineProperty(context, module_obj,
                               "__file__",
                               STRING_TO_JSVAL(JS_NewStringCopyZ(context, full_path)),
                               NULL, NULL,
                               /* don't set ENUMERATE since we wouldn't want to copy
                                * this symbol to any other object for example.
                                */
                               JSPROP_READONLY | JSPROP_PERMANENT))
            return JS_FALSE;
    }

    if (!JS_DefineProperty(context, module_obj,
                           "__moduleName__",
                           parent_is_module ?
                           STRING_TO_JSVAL(JS_NewStringCopyZ(context, module_name.c_str())) :
                           JSVAL_NULL,
                           NULL, NULL,
                           /* don't set ENUMERATE since we wouldn't want to copy
                            * this symbol to any other object for example.
                            */
                           JSPROP_READONLY | JSPROP_PERMANENT))
        return JS_FALSE;

    if (!JS_DefineProperty(context, module_obj,
                           "__parentModule__",
                           parent_is_module ? OBJECT_TO_JSVAL(parent) : JSVAL_NULL,
                           NULL, NULL,
                           /* don't set ENUMERATE since we wouldn't want to copy
                            * this symbol to any other object for example.
                            */
                           JSPROP_READONLY | JSPROP_PERMANENT))
        return JS_FALSE;
    return JS_TRUE;
}

static JSBool
import_directory(JSContext   *context,
                 JSObject    *obj,
                 const std::string &name,
                 std::vector<std::string> full_paths)
{
	JSObject *importer;

    gjs_debug(GJS_DEBUG_IMPORTER,
              "Importing directory '%s'",
              name);

    /* We define a sub-importer that has only the given directories on
     * its search path. gjs_define_importer() exits if it fails, so
     * this always succeeds.
     */
    importer = gjs_define_importer(context, obj, name, full_paths, false);
    if (importer == NULL)
        return JS_FALSE;

    return JS_TRUE;
}

static JSBool
define_import(JSContext  *context,
              JSObject   *obj,
              JSObject   *module_obj,
              std::string &name)
{
    if (!JS_DefineProperty(context, obj,
                           name.c_str(), OBJECT_TO_JSVAL(module_obj),
                           NULL, NULL,
                           GJS_MODULE_PROP_FLAGS & ~JSPROP_PERMANENT)) {
        gjs_debug(GJS_DEBUG_IMPORTER,
                  "Failed to define '%s' in importer",
                  name);
        return JS_FALSE;
    }

    return JS_TRUE;
}

/* Make the property we set in define_import permament;
 * we do this after the import succesfully completes.
 */
static JSBool
seal_import(JSContext  *context,
            JSObject   *obj,
            const std::string &name)
{
    JSBool found;
    unsigned attrs;

    if (!JS_GetPropertyAttributes(context, obj, name.c_str(),
                                  &attrs, &found) || !found) {
        gjs_debug(GJS_DEBUG_IMPORTER,
                  "Failed to get attributes to seal '%s' in importer",
                  name.c_str());
        return JS_FALSE;
    }
    attrs |= JSPROP_PERMANENT;

    if (!JS_SetPropertyAttributes(context, obj, name.c_str(),
                                  attrs, &found) || !found) {
        gjs_debug(GJS_DEBUG_IMPORTER,
                  "Failed to set attributes to seal '%s' in importer",
                  name.c_str());
        return JS_FALSE;
    }

    return JS_TRUE;
}

/* An import failed. Delete the property pointing to the import
 * from the parent namespace. In complicated situations this might
 * not be sufficient to get us fully back to a sane state. If:
 *
 *  - We import module A
 *  - module A imports module B
 *  - module B imports module A, storing a reference to the current
 *    module A module object
 *  - module A subsequently throws an exception
 *
 * Then module B is left imported, but the imported module B has
 * a reference to the failed module A module object. To handle this
 * we could could try to track the entire "import operation" and
 * roll back *all* modifications made to the namespace objects.
 * It's not clear that the complexity would be worth the small gain
 * in robustness. (You can still come up with ways of defeating
 * the attempt to clean up.)
 */
static void
cancel_import(JSContext  *context,
              JSObject   *obj,
              const std::string &name)
{
    gjs_debug(GJS_DEBUG_IMPORTER,
              "Cleaning up from failed import of '%s'",
              name.c_str());

    if (!JS_DeleteProperty(context, obj, name.c_str())) {
        gjs_debug(GJS_DEBUG_IMPORTER,
                  "Failed to delete '%s' in importer",
                  name);
    }
}

static JSBool
import_native_file(JSContext  *context,
                   JSObject   *obj,
                   std::string &name)
{
    JSObject *module_obj;
    JSBool retval = JS_FALSE;

    gjs_debug(GJS_DEBUG_IMPORTER, "Importing '%s'", name);

//    if (!gjs_import_native_module(context, name, &module_obj))
//        goto out;

    if (!define_meta_properties(context, module_obj, NULL, name, obj))
        goto out;

    if (JS_IsExceptionPending(context)) {
        /* I am not sure whether this can happen, but if it does we want to trap it.
         */
        gjs_debug(GJS_DEBUG_IMPORTER,
                  "Module '%s' reported an exception but gjs_import_native_module() returned true",
                  name.c_str());
        goto out;
    }

    if (!JS_DefineProperty(context, obj,
                           name.c_str(), OBJECT_TO_JSVAL(module_obj),
                           NULL, NULL, GJS_MODULE_PROP_FLAGS))
        goto out;

    retval = JS_TRUE;

 out:
    return retval;
}

static JSObject *
create_module_object(JSContext *context)
{
    return JS_NewObject(context, NULL, NULL, NULL);
}

static JSBool
import_file(JSContext  *context,
            const std::string &name,
            const std::string &file,
            JSObject   *module_obj)
{
    jsval script_retval;

    JS::CompileOptions options(context);

	std::string script;
	
	IOError ret = loadFileContent(file, script);
	
    if (ret == IO_ERROR_IS_DIRECTORY || ret == IO_ERROR_NOT_DIRECTORY || ret == IO_ERROR_NOT_FOUND) {
		std::cout << "io error?!\n";
        return JS_FALSE;
    }

    if (!gjs_eval_with_scope(context, module_obj, script.c_str(), script.size(),
                             file.c_str(), NULL)) {
		std::cout << "gjs_eval_with_scope failed\n";
        return JS_FALSE;
	}

	return JS_TRUE;
}

static JSObject *
load_module_init(JSContext  *context,
                 JSObject   *in_object,
                 std::string &full_path)
{
    JSObject *module_obj;
    JSBool found;
    jsid module_init_name;

    /* First we check if js module has already been loaded  */
    module_init_name = gjs_context_get_const_string(context, GJS_STRING_MODULE_INIT);
    if (JS_HasPropertyById(context, in_object, module_init_name, &found) && found) {
        jsval module_obj_val;

        if (JS_GetPropertyById(context,
                               in_object,
                               module_init_name,
                               &module_obj_val)) {
            return JSVAL_TO_OBJECT(module_obj_val);
        }
    }

    module_obj = create_module_object (context);
    if (!import_file (context, "__init__", full_path, module_obj))
		std::cout << "error\n";

    if (!JS_DefinePropertyById(context, in_object,
                               module_init_name, OBJECT_TO_JSVAL(module_obj),
                               NULL, NULL,
                               GJS_MODULE_PROP_FLAGS & ~JSPROP_PERMANENT))
		std::cout << "error\n";

    return module_obj;
}

static void
load_module_elements(JSContext *context,
                     JSObject *in_object,
                     ImporterIterator *iter,
                     std::string init_path) {
    JSObject *module_obj;
    JSObject *jsiter;

    module_obj = load_module_init(context, in_object, init_path);

    if (module_obj != NULL) {
        jsid idp;

        jsiter = JS_NewPropertyIterator(context, module_obj);

        if (jsiter == NULL) {
            return;
        }

        if (!JS_NextProperty(context, jsiter, &idp)) {
            return;
        }

        while (!JSID_IS_VOID(idp)) {
            std::string name;

            if (!gjs_get_string_id(context, idp, name)) {
                continue;
            }

            /* Pass ownership of name */
            iter->elements.push_back(name);

            if (!JS_NextProperty(context, jsiter, &idp)) {
                break;
            }
        }
    }
}

static JSBool
import_file_on_module(JSContext  *context,
                      JSObject   *obj,
                      std::string &name,
                      std::string &filePath)
{
    JSObject *module_obj;
    JSBool retval = JS_FALSE;

    module_obj = create_module_object (context);

    if (!define_import(context, obj, module_obj, name))
        goto out;

    if (!import_file(context, name, filePath, module_obj))
        goto out;

    if (!define_meta_properties(context, module_obj, filePath.c_str(), name, obj))
        goto out;

    if (!seal_import(context, obj, name))
        goto out;
    retval = JS_TRUE;

 out:
    if (!retval)
        cancel_import(context, obj, name);

    return retval;
}

static JSBool
do_import(JSContext  *context,
          JSObject   *obj,
          Importer   *priv,
          std::string &name)
{
    std::string filename;
    std::string full_path;
    std::string dirname;
    jsval search_path_val;
    JSObject *search_path;
    JSObject *module_obj = NULL;
    uint32_t search_path_len;
    uint32_t i;
    JSBool result;
    std::vector<std::string> directories;
    jsid search_path_name;
    bool exists;

    search_path_name = gjs_context_get_const_string(context, GJS_STRING_SEARCH_PATH);
    if (!gjs_object_require_property(context, obj, "importer", search_path_name, &search_path_val)) {
        return JS_FALSE;
    }

    if (!search_path_val.isObject()) {
        gjs_throw(context, "searchPath property on importer is not an object");
        return JS_FALSE;
    }

    search_path = JSVAL_TO_OBJECT(search_path_val);

    if (!JS_IsArrayObject(context, search_path)) {
        gjs_throw(context, "searchPath property on importer is not an array");
        return JS_FALSE;
    }

    if (!JS_GetArrayLength(context, search_path, &search_path_len)) {
        gjs_throw(context, "searchPath array has no length");
        return JS_FALSE;
    }

    result = JS_FALSE;

    filename = std::string(name) + ".js";

    /* First try importing an internal module like byteArray */
    if (priv->is_root &&
        gjs_is_registered_native_module(context, obj, name) &&
        import_native_file(context, obj, name)) {
        gjs_debug(GJS_DEBUG_IMPORTER,
                  "successfully imported module '%s'", name);
        result = JS_TRUE;
        goto out;
    }

    for (i = 0; i < search_path_len; ++i) {
        jsval elem;

        elem = JSVAL_VOID;
        if (!JS_GetElement(context, search_path, i, &elem)) {
            /* this means there was an exception, while elem == JSVAL_VOID
             * means no element found
             */
            goto out;
        }

        if (JSVAL_IS_VOID(elem))
            continue;

        if (!JSVAL_IS_STRING(elem)) {
            gjs_throw(context, "importer searchPath contains non-string");
            goto out;
        }

        if (!gjs_string_to_utf8(context, elem, dirname))
            goto out; /* Error message already set */

        /* Ignore empty path elements */
        if (dirname[0] == '\0')
            continue;

        /* Try importing __init__.js and loading the symbol from it */
        full_path = pathCombine(dirname, MODULE_INIT_FILENAME);

        module_obj = load_module_init(context, obj, full_path);
        if (module_obj != NULL) {
            jsval obj_val;

            if (JS_GetProperty(context,
                               module_obj,
                               name.c_str(),
                               &obj_val)) {
                if (!JSVAL_IS_VOID(obj_val) &&
                    JS_DefineProperty(context, obj,
                                      name.c_str(), obj_val,
                                      NULL, NULL,
                                      GJS_MODULE_PROP_FLAGS & ~JSPROP_PERMANENT)) {
                    result = JS_TRUE;
                    goto out;
                }
            }
        }

        /* Second try importing a directory (a sub-importer) */
        full_path = pathCombine(dirname, name);

        if (is_directory(full_path)) {
            std::cout << "Adding directory '" << full_path << "' to child importer '" << name << "'\n",

            directories.push_back(full_path);
        }

        /* If we just added to directories, we know we don't need to
         * check for a file.  If we added to directories on an earlier
         * iteration, we want to ignore any files later in the
         * path. So, always skip the rest of the loop block if we have
         * directories.
         */
        if (directories.size() > 0) {
            continue;
        }

        /* Third, if it's not a directory, try importing a file */
        full_path = pathCombine(dirname, filename);
		
		std::cout << "full path: " << full_path << "\n";
		
        exists = is_regular(full_path);

        if (!exists) {
            std::cout << "JS import '" << name << "' not found in " << dirname << "\n";
            continue;
        }

        if (import_file_on_module (context, obj, name, full_path)) {
			std::cout << "successfully imported module '" << name << "'\n";
            result = JS_TRUE;
        }

        /* Don't keep searching path if we fail to load the file for
         * reasons other than it doesn't exist... i.e. broken files
         * block searching for nonbroken ones
         */
        goto out;
    }

    if (directories.size() > 0) {
        /* NULL-terminate the char** */
        if (import_directory(context, obj, name, directories)) {
            std::cout << "successfully imported directory '" <<  name << "'\n";
            result = JS_TRUE;
        }
    }

 out:
    if (!result &&
        !JS_IsExceptionPending(context)) {
        /* If no exception occurred, the problem is just that we got to the
         * end of the path. Be sure an exception is set.
         */
        gjs_throw(context, "No JS module '%s' found in search path", name.c_str());
    }

    return result;
}

static ImporterIterator *
importer_iterator_new(void)
{
    ImporterIterator *iter = new ImporterIterator();

    iter->index = 0;

    return iter;
}

static void
importer_iterator_free(ImporterIterator *iter)
{
    delete iter;
}

/*
 * Like JSEnumerateOp, but enum provides contextual information as follows:
 *
 * JSENUMERATE_INIT: allocate private enum struct in state_p, return number
 * of elements in *id_p
 * JSENUMERATE_NEXT: return next property id in *id_p, and if no new property
 * free state_p and set to JSVAL_NULL
 * JSENUMERATE_DESTROY : destroy state_p
 *
 * Note that in a for ... in loop, this will be called first on the object,
 * then on its prototype.
 *
 */
static JSBool
importer_new_enumerate(JSContext  *context,
                       JS::HandleObject object,
                       JSIterateOp enum_op,
                       JS::MutableHandleValue statep,
                       JS::MutableHandleId idp)
{
    ImporterIterator *iter;

    switch (enum_op) {
    case JSENUMERATE_INIT_ALL:
    case JSENUMERATE_INIT: {
        Importer *priv;
        JSObject *search_path;
        jsval search_path_val;
        uint32_t search_path_len;
        uint32_t i;
        jsid search_path_name;

        statep.set(JSVAL_NULL);

        idp.set(INT_TO_JSID(0));

        priv = priv_from_js(context, object);

        if (!priv)
            /* we are enumerating the prototype properties */
            return JS_TRUE;

        search_path_name = gjs_context_get_const_string(context, GJS_STRING_SEARCH_PATH);
        if (!gjs_object_require_property(context, object, "importer", search_path_name, &search_path_val))
            return JS_FALSE;

        if (!search_path_val.isObject()) {
            gjs_throw(context, "searchPath property on importer is not an object");
            return JS_FALSE;
        }

        search_path = JSVAL_TO_OBJECT(search_path_val);

        if (!JS_IsArrayObject(context, search_path)) {
            gjs_throw(context, "searchPath property on importer is not an array");
            return JS_FALSE;
        }

        if (!JS_GetArrayLength(context, search_path, &search_path_len)) {
            gjs_throw(context, "searchPath array has no length");
            return JS_FALSE;
        }

        iter = importer_iterator_new();

        for (i = 0; i < search_path_len; ++i) {
            std::string dirname;
            std::string init_path;
            const char *filename;
            jsval elem;
            std::vector<std::string> dir;

            elem = JSVAL_VOID;
            if (!JS_GetElement(context, search_path, i, &elem)) {
                /* this means there was an exception, while elem == JSVAL_VOID
                 * means no element found
                 */
                importer_iterator_free(iter);
                return JS_FALSE;
            }

            if (JSVAL_IS_VOID(elem))
                continue;

            if (!JSVAL_IS_STRING(elem)) {
                gjs_throw(context, "importer searchPath contains non-string");
                importer_iterator_free(iter);
                return JS_FALSE;
            }

            if (!gjs_string_to_utf8(context, elem, dirname)) {
                importer_iterator_free(iter);
                return JS_FALSE; /* Error message already set */
            }

            init_path = pathCombine(dirname, MODULE_INIT_FILENAME);

            load_module_elements(context, object, iter, init_path);

            dir = enumerateFilesInDirectory(dirname);

            if (dir.size() == 0) {
                continue;
            }

            for(auto filename : dir) {
                std::string full_path;

                /* skip hidden files and directories (.svn, .git, ...) */
                if (filename[0] == '.')
                    continue;

                /* skip module init file */
                if (filename == MODULE_INIT_FILENAME)
                    continue;

                full_path = pathCombine(dirname, filename);

                if (is_directory(full_path)) {
                    iter->elements.push_back(filename);
                } else {
                    if (filename.rfind(MODULE_SUFFIX) != std::string::npos ||
                        filename.rfind(JS_SUFFIX) != std::string::npos) {
                        iter->elements.push_back(filename.substr(0, filename.size()-3));
                    }
                }
            }
        }

        statep.set(PRIVATE_TO_JSVAL(iter));

        idp.set(INT_TO_JSID(iter->elements.size()));

        break;
    }

    case JSENUMERATE_NEXT: {
        jsval element_val;

        if (JSVAL_IS_NULL(statep)) /* Iterating prototype */
            return JS_TRUE;

        iter = (ImporterIterator*) JSVAL_TO_PRIVATE(statep);

        if (iter->index < iter->elements.size()) {
            if (!gjs_string_from_utf8(context,
                                         iter->elements.at(iter->index++),
                                         &element_val))
                return JS_FALSE;

            jsid id;
            if (!JS_ValueToId(context, element_val, &id))
                return JS_FALSE;
            idp.set(id);

            break;
        }
        /* else fall through to destroying the iterator */
    }

    case JSENUMERATE_DESTROY: {
        if (!JSVAL_IS_NULL(statep)) {
            iter = (ImporterIterator*) JSVAL_TO_PRIVATE(statep);

            importer_iterator_free(iter);

            statep.set(JSVAL_NULL);
        }
    }
    }

    return JS_TRUE;
}

/*
 * Like JSResolveOp, but flags provide contextual information as follows:
 *
 *  JSRESOLVE_QUALIFIED   a qualified property id: obj.id or obj[id], not id
 *  JSRESOLVE_ASSIGNING   obj[id] is on the left-hand side of an assignment
 *  JSRESOLVE_DETECTING   'if (o.p)...' or similar detection opcode sequence
 *  JSRESOLVE_DECLARING   var, const, or function prolog declaration opcode
 *  JSRESOLVE_CLASSNAME   class name used when constructing
 *
 * The *objp out parameter, on success, should be null to indicate that id
 * was not resolved; and non-null, referring to obj or one of its prototypes,
 * if id was resolved.
 */
static JSBool
importer_new_resolve(JSContext *context,
                     JS::HandleObject obj,
                     JS::HandleId id,
                     unsigned flags,
                     JS::MutableHandleObject objp)
{
    Importer *priv;
    std::string name;
    JSBool ret = JS_TRUE;
    jsid module_init_name;

    module_init_name = gjs_context_get_const_string(context, GJS_STRING_MODULE_INIT);
    if (id == module_init_name)
        return JS_TRUE;

    if (!gjs_get_string_id(context, id, name))
        return JS_FALSE;

    /* let Object.prototype resolve these */
    if (name == "valueOf" ||
        name == "toString" ||
        name == "__iterator__")
        goto out;
    priv = priv_from_js(context, obj);

//    std::cout << "Resolve prop '" << name << "' hook obj " << (uint32_t)*obj << " priv " << (uint32_t)priv << "\n";
    if (priv == NULL) /* we are the prototype, or have the wrong class */
        goto out;
    JS_BeginRequest(context);
    if (do_import(context, obj, priv, name)) {
        objp.set(obj);
    } else {
        ret = JS_FALSE;
    }
    JS_EndRequest(context);

 out:
    return ret;
}

static JSBool
gjs_importer_constructor(JSContext  *context,
                         unsigned    argc,
                         jsval      *vp)
{
    gjs_throw_abstract_constructor_error(context, vp);
    return JS_FALSE;
}

static void
importer_finalize(JSFreeOp *fop,
                  JSObject *obj)
{
    Importer *priv;

    priv = (Importer*) JS_GetPrivate(obj);
//    std::cout <<  "finalize, obj %p priv %p", obj, priv);
    if (priv == NULL)
        return; /* we are the prototype, not a real instance */

//    GJS_DEC_COUNTER(importer);
    delete priv;
}

/* The bizarre thing about this vtable is that it applies to both
 * instances of the object, and to the prototype that instances of the
 * class have.
 */
struct JSClass gjs_importer_class = {
    "GjsFileImporter",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_NEW_RESOLVE |
    JSCLASS_NEW_ENUMERATE,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    (JSEnumerateOp) importer_new_enumerate, /* needs cast since it's the new enumerate signature */
    (JSResolveOp) importer_new_resolve, /* needs cast since it's the new resolve signature */
    JS_ConvertStub,
    importer_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSPropertySpec gjs_importer_proto_props[] = {
    { NULL }
};

JSFunctionSpec gjs_importer_proto_funcs[] = {
    { NULL }
};

static JSObject*
importer_new(JSContext *context,
             bool   is_root)
{
    JSObject *importer;
    Importer *priv;
    JSObject *global;
    JSBool found;

    global = gjs_get_import_global(context);

    if (!JS_HasProperty(context, global, gjs_importer_class.name, &found))
        g_error("HasProperty call failed creating importer class");

    if (!found) {
        JSObject *prototype;
        prototype = JS_InitClass(context, global,
                                 /* parent prototype JSObject* for
                                  * prototype; NULL for
                                  * Object.prototype
                                  */
                                 NULL,
                                 &gjs_importer_class,
                                 /* constructor for instances (NULL for
                                  * none - just name the prototype like
                                  * Math - rarely correct)
                                  */
                                 gjs_importer_constructor,
                                 /* number of constructor args */
                                 0,
                                 /* props of prototype */
                                 &gjs_importer_proto_props[0],
                                 /* funcs of prototype */
                                 &gjs_importer_proto_funcs[0],
                                 /* props of constructor, MyConstructor.myprop */
                                 NULL,
                                 /* funcs of constructor, MyConstructor.myfunc() */
                                 NULL);
        if (prototype == NULL)
            g_error("Can't init class %s", gjs_importer_class.name);

        gjs_debug(GJS_DEBUG_IMPORTER, "Initialized class %s prototype %p",
                  gjs_importer_class.name, prototype);
    }

    importer = JS_NewObject(context, &gjs_importer_class, NULL, global);
    if (importer == NULL)
        g_error("No memory to create importer importer");

    priv = new Importer;
    priv->is_root = is_root;

//    GJS_INC_COUNTER(importer);

//    g_assert(priv_from_js(context, importer) == NULL);
    JS_SetPrivate(importer, priv);

//    gjs_debug_lifecycle(GJS_DEBUG_IMPORTER,
//                        "importer constructor, obj %p priv %p", importer, priv);

    return importer;
}

std::vector<std::string>
gjs_get_search_path(void)
{
    std::vector<std::string> paths;

    if (gjs_search_path.size() == 0) {
		paths.push_back(".");
		paths.push_back("xpjs_modules");
		paths.push_back("js_avionics");
        /* in order of priority */

        gjs_search_path = paths;
    }
	return gjs_search_path;
}

static JSObject*
gjs_create_importer(JSContext    *context,
                    const std::string &importer_name,
                    std::vector<std::string>  &initial_search_path,
                    bool      add_standard_search_path,
                    bool      is_root,
                    JSObject     *in_object)
{
    JSObject *importer;
    std::vector<std::string> paths[2];
    std::vector<std::string> search_path;

    paths[0] = initial_search_path;
    if (add_standard_search_path) {
        /* Stick the "standard" shared search path after the provided one. */
        paths[1] = gjs_get_search_path();
    }

    for(auto p : paths) {
        search_path.insert(search_path.begin(), p.begin(), p.end());
	}
	for(auto &s : search_path)
        std::cout << "search_path: " << s << "\n";

	importer = importer_new(context, is_root);

    /* API users can replace this property from JS, is the idea */
    if (!gjs_define_string_array(context, importer,
                                 "searchPath", search_path,
                                 /* settable (no READONLY) but not deleteable (PERMANENT) */
                                 JSPROP_PERMANENT | JSPROP_ENUMERATE))
        g_error("no memory to define importer search path prop");

    if (!define_meta_properties(context, importer, NULL, importer_name, in_object))
        g_error("failed to define meta properties on importer");

    return importer;
}

JSObject*
gjs_define_importer(JSContext    *context,
                    JSObject     *in_object,
                    const std::string   &importer_name,
                    std::vector<std::string>  initial_search_path,
                    bool      add_standard_search_path)

{
    JSObject *importer;

    importer = gjs_create_importer(context, importer_name, initial_search_path, add_standard_search_path, false, in_object);

    if (!JS_DefineProperty(context, in_object,
                           importer_name.c_str(), OBJECT_TO_JSVAL(importer),
                           NULL, NULL,
                           GJS_MODULE_PROP_FLAGS))
        g_error("no memory to define importer property");

    gjs_debug(GJS_DEBUG_IMPORTER,
              "Defined importer '%s' %p in %p", importer_name, importer, in_object);

    return importer;
}

/* If this were called twice for the same runtime with different args it
 * would basically be a bug, but checking for that is a lot of code so
 * we just ignore all calls after the first and hope the args are the same.
 */
JSBool
gjs_create_root_importer(JSContext   *context,
                         std::vector<std::string> &initial_search_path,
                         bool     add_standard_search_path)
{
    jsval importer;

    JS_BeginRequest(context);

    importer = gjs_get_global_slot(context, GJS_GLOBAL_SLOT_IMPORTS);

    if ((!JSVAL_IS_VOID(importer))) {
        gjs_debug(GJS_DEBUG_IMPORTER,
                  "Someone else already created root importer, ignoring second request");

        JS_EndRequest(context);
        return JS_TRUE;
    }

    importer = OBJECT_TO_JSVAL(gjs_create_importer(context, "imports",
                                                   initial_search_path,
                                                   add_standard_search_path,
                                                   true, NULL));
    gjs_set_global_slot(context, GJS_GLOBAL_SLOT_IMPORTS, importer);

    JS_EndRequest(context);
    return JS_TRUE;
}

JSBool
gjs_define_root_importer_object(JSContext        *context,
                                JS::HandleObject  in_object,
                                JS::HandleObject  root_importer)
{
    JSBool success;
    jsid imports_name;

    success = JS_FALSE;
    JS_BeginRequest(context);

    JS::RootedValue importer (JS_GetRuntime(context),
                              OBJECT_TO_JSVAL(root_importer));
    imports_name = gjs_context_get_const_string(context, GJS_STRING_IMPORTS);
    if (!JS_DefinePropertyById(context, in_object,
                               imports_name, importer,
                               NULL, NULL,
                               GJS_MODULE_PROP_FLAGS)) {
        gjs_debug(GJS_DEBUG_IMPORTER, "DefineProperty imports on %p failed",
                  (JSObject *) in_object);
        goto fail;
    }

    success = JS_TRUE;
 fail:
    JS_EndRequest(context);
    return success;
}

JSBool
gjs_define_root_importer(JSContext   *context,
                         JSObject    *in_object)
{
    JS::RootedValue importer(JS_GetRuntime(context),
                             gjs_get_global_slot(context, GJS_GLOBAL_SLOT_IMPORTS));
    JS::RootedObject rooted_in_object(JS_GetRuntime(context),
                                      in_object);
    JS::RootedObject rooted_importer(JS_GetRuntime(context),
                                     JSVAL_TO_OBJECT(importer));
    return gjs_define_root_importer_object(context,
                                           rooted_in_object,
                                           rooted_importer);
}

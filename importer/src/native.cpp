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


#include <unordered_map>
#include <string>
#include <iostream>

#include <jsapi.h>

#include "native.h"
#include "jsapi-util.h"

static std::unordered_map<std::string, GjsDefineModuleFunc> modules;

void gjs_register_native_module (const std::string    &module_id,
                            GjsDefineModuleFunc  func)
{

	auto got = modules.find(module_id);
	
    if (got != modules.end()) {
        std::cout << "A second native module tried to register the same id '" << module_id << "'\n";
        return;
    }

    modules[module_id] = func;

    std::cout << "Registered native JS module '" << module_id << "'\n";
}

/**
 * gjs_is_registered_native_module:
 * @context:
 * @parent: the parent object defining the namespace
 * @name: name of the module
 *
 * Checks if a native module corresponding to @name has already
 * been registered. This is used to check to see if a name is a
 * builtin module without starting to try and load it.
 */
bool
gjs_is_registered_native_module(JSContext  *context,
                                JSObject   *parent,
                                const std::string &name)
{
    return modules.find(name) != modules.end();
}

/**
 * gjs_import_native_module:
 * @context:
 * @module_obj:
 *
 * Return a native module that's been preloaded.
 */
JSBool
gjs_import_native_module(JSContext   *context,
                         const std::string &name,
                         JSObject   **module_out)
{
    GjsDefineModuleFunc func;

    std::cout << "Defining native module '" << name << "\n";

	auto got = modules.find(name);
		
    if (got == modules.end()) {
        gjs_throw(context,
                  "No native module '%s' has registered itself",
                  name);
        return JS_FALSE;
    }
    func = got->second;

    return func (context, module_out);
}

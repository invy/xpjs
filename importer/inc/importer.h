/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
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

#ifndef __GJS_IMPORTER_H__
#define __GJS_IMPORTER_H__

#include <vector>
#include <string>

JSBool    gjs_create_root_importer (JSContext   *context,
                                    std::vector<std::string> &initial_search_path,
                                    bool     add_standard_search_path);
JSBool    gjs_define_root_importer (JSContext   *context,
                                    JSObject    *in_object);
JSBool    gjs_define_root_importer_object(JSContext        *context,
                                          JS::HandleObject  in_object,
                                          JS::HandleObject  root_importer);
JSObject* gjs_define_importer      (JSContext   *context,
                                    JSObject    *in_object,
                                    const std::string &importer_name,
                                    std::vector<std::string> initial_search_path,
                                    bool     add_standard_search_path);


#endif  /* __GJS_IMPORTER_H__ */

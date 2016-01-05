#ifndef JSFUNCWRAPPER_H
#define JSFUNCWRAPPER_H

#include <jsapi.h>


extern JSBool doit(JSContext *cx, unsigned argc, jsval *vp);
extern JSBool xplog(JSContext *cx, unsigned argc, jsval *vp);
extern JSBool requestDRef(JSContext *cx, unsigned argc, jsval *vp);
extern JSBool registerDRef(JSContext *cx, unsigned argc, jsval *vp);
extern JSBool get(JSContext *cx, unsigned argc, jsval *vp);
extern JSBool set(JSContext *cx, unsigned argc, jsval *vp);
extern JSBool getIdx(JSContext *cx, unsigned argc, jsval *vp);
extern JSBool setIdx(JSContext *, unsigned argc, jsval *vp);

extern JSFunctionSpec js_mappedFunctions[];


#endif // JSFUNCWRAPPER_H

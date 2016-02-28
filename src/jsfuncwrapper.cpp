
#include <XPLMUtilities.h>
#include <XPLMProcessing.h>
#include "jsfuncwrapper.h"

#include "jsengine.h"
#include "xpdataaccess.h"

extern XPDataAccess g_xpdata;
extern JSEngine* g_js;

/*
 * caching vectors, to avoid reallocations
 */
static std::vector<float> _floatCache;
static std::vector<int> _intCache;
static std::vector<char> _byteCache;

JSFunctionSpec js_mappedFunctions[] = {
    /* debugging */
    JS_FN("xplog", xplog, 1, 0),
    /* data access */
    JS_FN("requestDRef", requestDRef, 1, 0),
    JS_FN("registerDRef", registerDRef, 1, 0),
    JS_FN("get", get, 1, 0),
    JS_FN("set", set, 2, 0),
    JS_FN("getAt", getIdx, 2, 0),
    JS_FN("setAt", setIdx, 3, 0),
    /* callbacks */
	JS_FN("regFLCallback", registerFlightLoopCallback, 2, 0),
	JS_FN("unregFLCallback", unregisterFlightLoopCallback, 1, 0),
	JS_FN("schedFLCallback", scheduleFlightLoopCallback, 3, 0),
    /* etc... */
    JS_FS_END
};


JSBool xplog(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if(args.length() == 0 || args[0].isNull() || args[0].isUndefined()) {
        XPLMDebugString("XPJS: bad type\n");
    }
    XPLMDebugString("XPJS: ");
    XPLMDebugString(JS_EncodeString(cx, args[0].toString()));
    XPLMDebugString("\n");
    return true;
}

JSBool requestDRef(JSContext *cx, unsigned argc, jsval *vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(g_xpdata.requestDataRef(JS_EncodeString(cx, args[0].toString())));
    return true;
}

JSBool registerDRef(JSContext *, unsigned argc, jsval *vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(0);
    return true;
}

JSBool get(JSContext *cx, unsigned argc, jsval *vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    auto dataRef = g_xpdata.getDataRef(args[0].toInt32());

    if(dataRef.typeId == xplmType_Unknown) {
        args.rval().setInt32(0);
    }
    else if(dataRef.typeId & xplmType_Int) {
        int r = 0;
        args.rval().setInt32(dataRef.get(r));
    }
    else if(dataRef.typeId & xplmType_Float) {
        float r = 0;
        args.rval().setDouble(dataRef.get(r));
    }
    else if(dataRef.typeId & xplmType_Double) {
        double r = 0;
        args.rval().setDouble(dataRef.get(r));
    }
    else if(dataRef.typeId & xplmType_FloatArray) {
        auto size = dataRef.get(_floatCache);
        JSObject* arr = JS_NewArrayObject(cx, size, nullptr);
        for(size_t i = 0; i < size; ++i) {
            jsval v;
            v.setDouble(_floatCache[i]);
            JS_SetElement(cx, arr, i, &v);
        }
        args.rval().setObject(*arr);
    }
    else if(dataRef.typeId & xplmType_IntArray) {
        auto size = dataRef.get(_intCache);
        JSObject* arr = JS_NewArrayObject(cx, size, nullptr);
        for(size_t i = 0; i < size; ++i) {
            jsval v;
            v.setInt32(_intCache[i]);
            JS_SetElement(cx, arr, i, &v);
        }
        args.rval().setObject(*arr);
    }
    else if(dataRef.typeId & xplmType_Data) {
        auto size = dataRef.get(_byteCache);
        args.rval().setString(JS_NewStringCopyN(cx, &(_byteCache[0]), size));
    }

    return true;
}

JSBool set(JSContext *cx, unsigned argc, jsval *vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    auto dataRef = g_xpdata.getDataRef(args[0].toInt32());

    if(argc < 2)
        return false;

    if(dataRef.typeId == xplmType_Unknown) {
        args.rval().setInt32(0);
        return false;
    }
    else if(dataRef.typeId & xplmType_Int) {
        dataRef.set(args[1].toInt32());
    }
    else if(dataRef.typeId & xplmType_Float) {
        dataRef.set(static_cast<float>(args[1].toDouble()));
    }
    else if(dataRef.typeId & xplmType_Double) {
        dataRef.set(static_cast<double>(args[1].toDouble()));
    }
    else {
        JSObject *arr = &args[1].toObject();
        uint32_t length = 0;
        JS_GetArrayLength(cx, arr, &length);
        if(dataRef.typeId & xplmType_FloatArray) {
            std::vector<float> r(length);
            for(size_t i = 0; i < r.size(); ++i) {
                jsval v;
                JS_GetElement(cx, arr, i, &v);
                r[i] = v.toDouble();
            }
            dataRef.set(r);
        }
        else if(dataRef.typeId & xplmType_IntArray) {
            std::vector<int> r(length);
            for(size_t i = 0; i < r.size(); ++i) {
                jsval v;
                JS_GetElement(cx, arr, i, &v);
                r[i] = v.toInt32();
            }
            dataRef.set(r);
        }
        else if(dataRef.typeId & xplmType_Data) {
            return false;
    //        args.rval().setString(JS_NewStringCopyN(cx, &(dataRef.get(r)[0]), r.size()));
        }
    }

    return true;

}

JSBool getIdx(JSContext *, unsigned argc, jsval *vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if(argc < 2)
        return false;
    auto dataRef = g_xpdata.getDataRef(args[0].toInt32());
    size_t idx = args[1].toInt32();


    if(dataRef.typeId & xplmType_FloatArray) {
        args.rval().setDouble(dataRef.getf_At(idx));
    }
    else if(dataRef.typeId & xplmType_IntArray) {
        args.rval().setInt32(dataRef.geti_At(idx));
    }
    else
        return false;

    return true;
}

JSBool setIdx(JSContext *, unsigned argc, jsval *vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if(argc < 3)
        return false;
    auto dataRef = g_xpdata.getDataRef(args[0].toInt32());
    size_t idx = args[1].toInt32();


    if(dataRef.typeId & xplmType_FloatArray) {
        dataRef.setAt(static_cast<float>(args[2].toDouble()), idx);
    }
    else if(dataRef.typeId & xplmType_IntArray) {
        dataRef.setAt(static_cast<int>(args[2].toInt32()), idx);
    }
    else
        return false;

    return true;
}

JSBool registerFlightLoopCallback(JSContext*, unsigned int argc, jsval* vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if(argc < 2)
        return false;
	if(args[0].isObject()) {
		g_js->getCallbacks().registerFlightLoopCallback(reinterpret_cast<JSFunction*>(&args[0].toObject()), args[1].toDouble());
		return true;
	}
	return false;
}


JSBool scheduleFlightLoopCallback(JSContext*, unsigned int argc, jsval* vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if(argc < 2)
        return false;
	if(args[0].isObject()) {
		g_js->getCallbacks().registerFlightLoopCallback(reinterpret_cast<JSFunction*>(&args[0].toObject()), args[1].toDouble());
		return true;
	}
	return false;
}

JSBool unregisterFlightLoopCallback(JSContext *, unsigned argc, jsval *vp) {
	return false;
}

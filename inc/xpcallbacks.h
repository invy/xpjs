#ifndef XPCALLBACKS_H
#define XPCALLBACKS_H

#include <string>
#include <functional>

#include <XPLMUtilities.h>

#include <jsapi.h>

class FlightLoopCallbackObject {
    static size_t _id;
    std::string jsFuncName;
    JSContext *ctx;
    JS::RootedObject *global;
public:
    std::function<float(float, float, int, void*)> callback;
    size_t callbackId;
public:
    FlightLoopCallbackObject(const std::string &jsFuncName, JSContext *ctx, JS::RootedObject *global)
        : jsFuncName(jsFuncName),
          callbackId(FlightLoopCallbackObject::_id++),
          ctx(ctx),
          global(global),
          callback(std::bind(&FlightLoopCallbackObject::func, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4))
    {
    }
    float func(float elapsedSinceLastCall, float elapsedTimeSinceLastFlightLoop, int counter, void *refcon) {
        XPLMDebugString("callback called");
        JS::RootedValue rval(this->ctx);
        JS::AutoValueVector argv(this->ctx);
        argv.resize(3);
        argv[0].setDouble(elapsedSinceLastCall);
        argv[1].setDouble(elapsedTimeSinceLastFlightLoop);
        argv[2].setInt32(counter);
        JS_CallFunctionName(this->ctx, *(this->global), jsFuncName.c_str(), 0, argv.begin(), rval.address());
        return rval.toDouble();
    }
};

#endif // XPCALLBACKS_H

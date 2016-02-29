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

#ifndef XPJSCALLBACKS_H
#define XPJSCALLBACKS_H

#include <memory>
#include <deque>

#include "XPLMProcessing.h"

#include <jsapi.h>
#include "jsapi-util.h"

class XPJSCallbacks {
public:
	class XPJSCallback {
		JSFunction* jsCallback;
		const GjsContext& context;
		XPLMFlightLoopID callbackId;
	public:
		XPJSCallback(JSFunction *jsCallback, const GjsContext& context) :
			jsCallback(jsCallback), context(context), callbackId(nullptr) { }
		const GjsContext& getContext() { return context; }
		JSFunction* getJSCallback() { return jsCallback; }
		void setCallbackId(XPLMFlightLoopID callbackId) { this->callbackId = callbackId; }
		XPLMFlightLoopID getCallbackId() { return callbackId; }
	};
private:
	std::deque<std::shared_ptr<XPJSCallback>> callbacks;
	const GjsContext& context;
public:
	XPJSCallbacks(const GjsContext &context) : context(context) { }
	virtual ~XPJSCallbacks() {
		for(auto &c : callbacks) {
			XPLMDestroyFlightLoop(c->getCallbackId());
		}
	}
	uint64_t registerFlightLoopCallback(JSFunction *obj, float interval);
	void unregisterFlightLoopCallback(const uint64_t callbackId);

	static float jsFlightLoopCallback(
								float elapsedSinceLastCall,
								float elapsedTimeSinceLastFlightLoop,
								int counter,
								void *refcon)
	{
		auto jscallback = static_cast<XPJSCallback*>(refcon);
	    JSAutoRequest ar(jscallback->getContext().context);
		JS::RootedValue rval(jscallback->getContext().context);
		JS::AutoValueVector argv(jscallback->getContext().context);
		argv.resize(3);
		argv[0] = JS::DoubleValue(elapsedSinceLastCall);
		argv[1] = JS::DoubleValue(elapsedTimeSinceLastFlightLoop);
		argv[2] = JS::NumberValue<int>(counter);
		
		JS_CallFunction(jscallback->getContext().context,
						JS::Rooted<JSObject*>(jscallback->getContext().context,
						jscallback->getContext().global), 
						jscallback->getJSCallback(),
						argv.length(), argv.begin(),
						rval.address());
		return rval.toDouble();
	}
};

#endif // XPJSCALLBACKS_H

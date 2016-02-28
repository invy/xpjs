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

#include <memory>

#include <XPLMProcessing.h>
#include "xpjscallbacks.h"

uint64_t XPJSCallbacks::registerFlightLoopCallback(JSFunction* obj, float interval)
{
	auto xpjscallback = std::make_shared<XPJSCallback>(obj, this->context);
	
	XPLMCreateFlightLoop_t callback;
	callback.structSize = sizeof(XPLMCreateFlightLoop_t);
	callback.phase = xplm_FlightLoop_Phase_AfterFlightModel;
	callback.callbackFunc = jsFlightLoopCallback;
	callback.refcon = xpjscallback.get();
	
	auto callbackId = XPLMCreateFlightLoop(&callback);
	XPLMScheduleFlightLoop(callbackId, interval, 0);
	
	xpjscallback->setCallbackId(callbackId);
	
	this->callbacks.push_back(xpjscallback);
	return reinterpret_cast<uint64_t>(callbackId);
}

void XPJSCallbacks::unregisterFlightLoopCallback(const uint64_t callbackId) {
	XPLMDestroyFlightLoop(reinterpret_cast<void*>(callbackId));
}

/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "EventSendingController.h"

#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSEventSendingController.h"
#include <WebKit2/WKBundlePage.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundlePrivate.h>

namespace WTR {

static const float ZoomMultiplierRatio = 1.2f;

PassRefPtr<EventSendingController> EventSendingController::create()
{
    return adoptRef(new EventSendingController);
}

EventSendingController::EventSendingController()
{
}

EventSendingController::~EventSendingController()
{
}

JSClassRef EventSendingController::wrapperClass()
{
    return JSEventSendingController::eventSendingControllerClass();
}

static void setExceptionForString(JSContextRef context, JSValueRef* exception, const char* string)
{
    JSRetainPtr<JSStringRef> exceptionString(Adopt, JSStringCreateWithUTF8CString(string));
    *exception = JSValueMakeString(context, exceptionString.get());
}

void EventSendingController::mouseDown(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    setExceptionForString(context, exception, "EventSender.mouseDown is not yet supported.");
}

void EventSendingController::mouseUp(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    setExceptionForString(context, exception, "EventSender.mouseUp is not yet supported.");
}

void EventSendingController::mouseMoveTo(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    setExceptionForString(context, exception, "EventSender.mouseMoveTo is not yet supported.");
}

void EventSendingController::keyDown(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    setExceptionForString(context, exception, "EventSender.keyDown is not yet supported.");
}

void EventSendingController::contextClick(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    setExceptionForString(context, exception, "EventSender.contextClick is not yet supported.");
}

void EventSendingController::leapForward(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    setExceptionForString(context, exception, "EventSender.leapForward is not yet supported.");
}

void EventSendingController::textZoomIn()
{
    // Ensure page zoom is reset.
    WKBundlePageSetPageZoomFactor(InjectedBundle::shared().page()->page(), 1);

    double zoomFactor = WKBundlePageGetTextZoomFactor(InjectedBundle::shared().page()->page());
    WKBundlePageSetTextZoomFactor(InjectedBundle::shared().page()->page(), zoomFactor * ZoomMultiplierRatio);
}

void EventSendingController::textZoomOut()
{
    // Ensure page zoom is reset.
    WKBundlePageSetPageZoomFactor(InjectedBundle::shared().page()->page(), 1);

    double zoomFactor = WKBundlePageGetTextZoomFactor(InjectedBundle::shared().page()->page());
    WKBundlePageSetTextZoomFactor(InjectedBundle::shared().page()->page(), zoomFactor / ZoomMultiplierRatio);
}

void EventSendingController::zoomPageIn()
{
    // Ensure text zoom is reset.
    WKBundlePageSetTextZoomFactor(InjectedBundle::shared().page()->page(), 1);

    double zoomFactor = WKBundlePageGetPageZoomFactor(InjectedBundle::shared().page()->page());
    WKBundlePageSetPageZoomFactor(InjectedBundle::shared().page()->page(), zoomFactor / ZoomMultiplierRatio);
}

void EventSendingController::zoomPageOut()
{
    // Ensure text zoom is reset.
    WKBundlePageSetTextZoomFactor(InjectedBundle::shared().page()->page(), 1);

    double zoomFactor = WKBundlePageGetPageZoomFactor(InjectedBundle::shared().page()->page());
    WKBundlePageSetPageZoomFactor(InjectedBundle::shared().page()->page(), zoomFactor / ZoomMultiplierRatio);
}

// Object Creation

void EventSendingController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "eventSender", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

} // namespace WTR

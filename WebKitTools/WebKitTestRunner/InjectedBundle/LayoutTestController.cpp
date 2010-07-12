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

#include "LayoutTestController.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"

#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKStringCF.h>
#include <WebKit2/WebKit2.h>

namespace WTR {

PassRefPtr<LayoutTestController> LayoutTestController::create(const std::string& testPathOrURL)
{
    return adoptRef(new LayoutTestController(testPathOrURL));
}

LayoutTestController::LayoutTestController(const std::string& testPathOrURL)
    : m_dumpAsText(false)
    , m_waitToDump(false)
    , m_testPathOrURL(testPathOrURL)
{
}

LayoutTestController::~LayoutTestController()
{
}

static const CFTimeInterval waitToDumpWatchdogInterval = 30.0;


void LayoutTestController::invalidateWaitToDumpWatchdog()
{
    if (m_waitToDumpWatchdog) {
        CFRunLoopTimerInvalidate(m_waitToDumpWatchdog.get());
        m_waitToDumpWatchdog = 0;
    }
}

static void waitUntilDoneWatchdogFired(CFRunLoopTimerRef timer, void* info)
{
    InjectedBundle::shared().layoutTestController()->waitToDumpWatchdogTimerFired();
}

void LayoutTestController::setWaitToDump()
{
    m_waitToDump = true;
    if (!m_waitToDumpWatchdog) {
        m_waitToDumpWatchdog.adoptCF(CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + waitToDumpWatchdogInterval, 
                                                      0, 0, 0, waitUntilDoneWatchdogFired, NULL));
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), m_waitToDumpWatchdog.get(), kCFRunLoopCommonModes);
    }
}

void LayoutTestController::waitToDumpWatchdogTimerFired()
{
    invalidateWaitToDumpWatchdog();
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    InjectedBundle::shared().os() << message << "\n";
    InjectedBundle::shared().done();
}

void LayoutTestController::notifyDone()
{
    if (m_waitToDump && !InjectedBundle::shared().page()->isLoading())
        InjectedBundle::shared().page()->dump();
    m_waitToDump = false;
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleFrameGetNumberOfActiveAnimations(mainFrame);
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, elementId));
    WKRetainPtr<WKStringRef> idWK(AdoptWK, WKStringCreateWithCFString(idCF.get()));
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, animationName));
    WKRetainPtr<WKStringRef> nameWK(AdoptWK, WKStringCreateWithCFString(nameCF.get()));

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleFramePauseAnimationOnElementWithId(mainFrame, nameWK.get(), idWK.get(), time);
}

static JSValueRef dumpAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpAsText(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef waitUntilDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setWaitToDump();
    return JSValueMakeUndefined(context);
}

static JSValueRef notifyDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->notifyDone();
    return JSValueMakeUndefined(context);
}

static JSValueRef numberOfActiveAnimationsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, controller->numberOfActiveAnimations());
}

static JSValueRef pauseAnimationAtTimeOnElementWithIdCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 3)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> animationName(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    double time = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> elementId(Adopt, JSValueToStringCopy(context, arguments[2], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(context, controller->pauseAnimationAtTimeOnElementWithId(animationName.get(), time, elementId.get()));
}

// Object Finalization

static void layoutTestControllerObjectFinalize(JSObjectRef object)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(object));
    controller->deref();
}

// Object Creation

void LayoutTestController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> layoutTestContollerStr(Adopt, JSStringCreateWithUTF8CString("layoutTestController"));
    ref();

    JSClassRef classRef = getJSClass();
    JSValueRef layoutTestContollerObject = JSObjectMake(context, classRef, this);
    JSClassRelease(classRef);

    JSObjectSetProperty(context, windowObject, layoutTestContollerStr.get(), layoutTestContollerObject, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef LayoutTestController::getJSClass()
{
    static JSStaticFunction* staticFunctions = LayoutTestController::staticFunctions();
    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "LayoutTestController", 0, 0, staticFunctions,
        0, layoutTestControllerObjectFinalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    return JSClassCreate(&classDefinition);
}

JSStaticFunction* LayoutTestController::staticFunctions()
{
    static JSStaticFunction staticFunctions[] = {
        { "dumpAsText", dumpAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "notifyDone", notifyDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "numberOfActiveAnimations", numberOfActiveAnimationsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pauseAnimationAtTimeOnElementWithId", pauseAnimationAtTimeOnElementWithIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "waitUntilDone", waitUntilDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    return staticFunctions;
}

} // namespace WTR

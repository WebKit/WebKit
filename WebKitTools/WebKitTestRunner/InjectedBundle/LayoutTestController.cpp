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
#include "JSLayoutTestController.h"
#include "StringFunctions.h"
#include <WebKit2/WKBundleBackForwardList.h>
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundleFramePrivate.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundleScriptWorld.h>
#include <WebKit2/WKBundlePrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WebKit2.h>

namespace WTR {

// This is lower than DumpRenderTree's timeout, to make it easier to work through the failures
// Eventually it should be changed to match.
const double LayoutTestController::waitToDumpWatchdogTimerInterval = 6;

static JSValueRef propertyValue(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    if (!object)
        return 0;
    JSRetainPtr<JSStringRef> propertyNameString(Adopt, JSStringCreateWithUTF8CString(propertyName));
    JSValueRef exception;
    return JSObjectGetProperty(context, object, propertyNameString.get(), &exception);
}

static JSObjectRef propertyObject(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    JSValueRef value = propertyValue(context, object, propertyName);
    if (!value || !JSValueIsObject(context, value))
        return 0;
    return const_cast<JSObjectRef>(value);
}

static JSObjectRef getElementById(WKBundleFrameRef frame, JSStringRef elementId)
{
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    JSObjectRef document = propertyObject(context, JSContextGetGlobalObject(context), "document");
    if (!document)
        return 0;
    JSValueRef getElementById = propertyObject(context, document, "getElementById");
    if (!getElementById || !JSValueIsObject(context, getElementById))
        return 0;
    JSValueRef elementIdValue = JSValueMakeString(context, elementId);
    JSValueRef exception;
    JSValueRef element = JSObjectCallAsFunction(context, const_cast<JSObjectRef>(getElementById), document, 1, &elementIdValue, &exception);
    if (!element || !JSValueIsObject(context, element))
        return 0;
    return const_cast<JSObjectRef>(element);
}

PassRefPtr<LayoutTestController> LayoutTestController::create()
{
    return adoptRef(new LayoutTestController);
}

LayoutTestController::LayoutTestController()
    : m_whatToDump(RenderTree)
    , m_shouldDumpAllFrameScrollPositions(false)
    , m_shouldDumpBackForwardListsForAllWindows(false)
    , m_shouldAllowEditing(true)
    , m_shouldCloseExtraWindows(false)
    , m_dumpEditingCallbacks(false)
    , m_dumpStatusCallbacks(false)
    , m_dumpTitleChanges(false)
    , m_waitToDump(false)
    , m_testRepaint(false)
    , m_testRepaintSweepHorizontally(false)
{
    platformInitialize();
}

LayoutTestController::~LayoutTestController()
{
}

JSClassRef LayoutTestController::wrapperClass()
{
    return JSLayoutTestController::layoutTestControllerClass();
}

void LayoutTestController::display()
{
    // FIXME: actually implement, once we want pixel tests
}

void LayoutTestController::waitUntilDone()
{
    m_waitToDump = true;
    initializeWaitToDumpWatchdogTimerIfNeeded();
}

void LayoutTestController::waitToDumpWatchdogTimerFired()
{
    invalidateWaitToDumpWatchdogTimer();
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
    // FIXME: Is it OK this works only for the main frame?
    // FIXME: If this is needed only for the main frame, then why is the function on WKBundleFrame instead of WKBundlePage?
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleFrameGetNumberOfActiveAnimations(mainFrame);
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    // FIXME: Is it OK this works only for the main frame?
    // FIXME: If this is needed only for the main frame, then why is the function on WKBundleFrame instead of WKBundlePage?
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleFramePauseAnimationOnElementWithId(mainFrame, toWK(animationName).get(), toWK(elementId).get(), time);
}

void LayoutTestController::suspendAnimations()
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    WKBundleFrameSuspendAnimations(mainFrame);
}

void LayoutTestController::resumeAnimations()
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    WKBundleFrameResumeAnimations(mainFrame);
}

JSRetainPtr<JSStringRef> LayoutTestController::layerTreeAsText() const
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    WKRetainPtr<WKStringRef> text(AdoptWK, WKBundleFrameCopyLayerTreeAsText(mainFrame));
    return toJS(text);
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    WKRetainPtr<WKStringRef> sourceWK = toWK(source);
    WKRetainPtr<WKBundleScriptWorldRef> scriptWorld(AdoptWK, WKBundleScriptWorldCreateWorld());

    WKBundleAddUserScript(InjectedBundle::shared().bundle(), scriptWorld.get(), sourceWK.get(), 0, 0, 0,
        (runAtStart ? kWKInjectAtDocumentStart : kWKInjectAtDocumentEnd),
        (allFrames ? kWKInjectInAllFrames : kWKInjectInTopFrameOnly));
}

void LayoutTestController::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    WKRetainPtr<WKStringRef> sourceWK = toWK(source);
    WKRetainPtr<WKBundleScriptWorldRef> scriptWorld(AdoptWK, WKBundleScriptWorldCreateWorld());

    WKBundleAddUserStyleSheet(InjectedBundle::shared().bundle(), scriptWorld.get(), sourceWK.get(), 0, 0, 0,
        (allFrames ? kWKInjectInAllFrames : kWKInjectInTopFrameOnly));
}

void LayoutTestController::keepWebHistory()
{
    WKBundleSetShouldTrackVisitedLinks(InjectedBundle::shared().bundle(), true);
}

JSValueRef LayoutTestController::computedStyleIncludingVisitedInfo(JSValueRef element)
{
    // FIXME: Is it OK this works only for the main frame?
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    if (!JSValueIsObject(context, element))
        return JSValueMakeUndefined(context);
    JSValueRef value = WKBundleFrameGetComputedStyleIncludingVisitedInfo(mainFrame, const_cast<JSObjectRef>(element));
    if (!value)
        return JSValueMakeUndefined(context);
    return value;
}

JSRetainPtr<JSStringRef> LayoutTestController::counterValueForElementById(JSStringRef elementId)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSObjectRef element = getElementById(mainFrame, elementId);
    if (!element)
        return 0;
    WKRetainPtr<WKStringRef> value(AdoptWK, WKBundleFrameCopyCounterValue(mainFrame, const_cast<JSObjectRef>(element)));
    return toJS(value);
}

JSRetainPtr<JSStringRef> LayoutTestController::markerTextForListItem(JSValueRef element)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    if (!element || !JSValueIsObject(context, element))
        return 0;
    WKRetainPtr<WKStringRef> text(AdoptWK, WKBundleFrameCopyMarkerText(mainFrame, const_cast<JSObjectRef>(element)));
    if (WKStringIsEmpty(text.get()))
        return 0;
    return toJS(text);
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef argument)
{
    WKBundlePageExecuteEditingCommand(InjectedBundle::shared().page()->page(), toWK(name).get(), toWK(argument).get());
}

bool LayoutTestController::isCommandEnabled(JSStringRef name)
{
    return WKBundlePageIsEditingCommandEnabled(InjectedBundle::shared().page()->page(), toWK(name).get());
}

void LayoutTestController::setCanOpenWindows(bool)
{
    // It's not clear if or why any tests require opening windows be forbidden.
    // For now, just ignore this setting, and if we find later it's needed we can add it.
}

void LayoutTestController::setXSSAuditorEnabled(bool enabled)
{
    WKBundleOverrideXSSAuditorEnabledForTestRunner(InjectedBundle::shared().bundle(), true);
}

unsigned LayoutTestController::windowCount()
{
    return InjectedBundle::shared().pageCount();
}

void LayoutTestController::clearBackForwardList()
{
    WKBundleBackForwardListClear(WKBundlePageGetBackForwardList(InjectedBundle::shared().page()->page()));
}

// Object Creation

void LayoutTestController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "layoutTestController", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

} // namespace WTR

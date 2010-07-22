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
    : m_whatToDump(RenderTree)
    , m_shouldDumpAllFrameScrollPositions(false)
    , m_dumpStatusCallbacks(false)
    , m_waitToDump(false)
    , m_testRepaint(false)
    , m_testRepaintSweepHorizontally(false)
    , m_testPathOrURL(testPathOrURL)
{
}

LayoutTestController::~LayoutTestController()
{
}

JSClassRef LayoutTestController::wrapperClass()
{
    return JSLayoutTestController::layoutTestControllerClass();
}

// This is lower than DumpRenderTree's timeout, to make it easier to work through the failures
// Eventually it should be changed to match.
static const CFTimeInterval waitToDumpWatchdogInterval = 6.0;

void LayoutTestController::display()
{
    // FIXME: actually implement, once we want pixel tests
}

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

void LayoutTestController::waitUntilDone()
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

// Object Creation

void LayoutTestController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> layoutTestContollerStr(Adopt, JSStringCreateWithUTF8CString("layoutTestController"));
    JSObjectSetProperty(context, windowObject, layoutTestContollerStr.get(), JSWrapper::wrap(context, this), kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

} // namespace WTR

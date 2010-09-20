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

#ifndef LayoutTestController_h
#define LayoutTestController_h

#include "JSWrappable.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>
#include <string>

namespace WTR {

class LayoutTestController : public JSWrappable {
public:
    static PassRefPtr<LayoutTestController> create();
    virtual ~LayoutTestController();

    // JSWrappable
    virtual JSClassRef wrapperClass();

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);

    // The basics.
    void dumpAsText() { m_whatToDump = MainFrameText; }
    void dumpChildFramesAsText() { m_whatToDump = AllFramesText; }
    void waitUntilDone();
    void notifyDone();

    // Other dumping.
    void dumpChildFrameScrollPositions() { m_shouldDumpAllFrameScrollPositions = true; }
    void dumpEditingCallbacks() { m_dumpEditingCallbacks = true; }
    void dumpSelectionRect() { } // Will need to do something when we support pixel tests.
    void dumpStatusCallbacks() { m_dumpStatusCallbacks = true; }
    void dumpTitleChanges() { m_dumpTitleChanges = true; }

    // Special options.
    void keepWebHistory();
    void setAcceptsEditing(bool value) { m_shouldAllowEditing = value; }
    void setCanOpenWindows(bool);
    void setCloseRemainingWindowsWhenComplete(bool value) { m_shouldCloseExtraWindows = value; }
    void setXSSAuditorEnabled(bool);
    unsigned windowCount();

    // Special DOM functions.
    JSValueRef computedStyleIncludingVisitedInfo(JSValueRef element);
    JSRetainPtr<JSStringRef> counterValueForElementById(JSStringRef elementId);
    JSRetainPtr<JSStringRef> markerTextForListItem(JSValueRef element);
    void execCommand(JSStringRef name, JSStringRef argument);
    bool isCommandEnabled(JSStringRef name);

    // Repaint testing.
    void testRepaint() { m_testRepaint = true; }
    void repaintSweepHorizontally() { m_testRepaintSweepHorizontally = true; }
    void display();

    // Animation testing.
    unsigned numberOfActiveAnimations() const;
    bool pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId);

    // UserContent testing.
    void addUserScript(JSStringRef source, bool runAtStart, bool allFrames);
    void addUserStyleSheet(JSStringRef source, bool allFrames);

    enum WhatToDump { RenderTree, MainFrameText, AllFramesText };
    WhatToDump whatToDump() const { return m_whatToDump; }

    bool shouldDumpAllFrameScrollPositions() const { return m_shouldDumpAllFrameScrollPositions; }
    bool shouldDumpEditingCallbacks() const { return m_dumpEditingCallbacks; }
    bool shouldDumpMainFrameScrollPosition() const { return m_whatToDump == RenderTree; }

    bool shouldDumpStatusCallbacks() const { return m_dumpStatusCallbacks; }
    bool shouldDumpTitleChanges() const { return m_dumpTitleChanges; }

    bool waitToDump() const { return m_waitToDump; }
    void waitToDumpWatchdogTimerFired();
    void invalidateWaitToDumpWatchdogTimer();

    bool shouldAllowEditing() const { return m_shouldAllowEditing; }

    bool shouldCloseExtraWindowsAfterRunningTest() const { return m_shouldCloseExtraWindows; }

private:
    static const double waitToDumpWatchdogTimerInterval;

    LayoutTestController();

    void platformInitialize();
    void initializeWaitToDumpWatchdogTimerIfNeeded();

    WhatToDump m_whatToDump;
    bool m_shouldDumpAllFrameScrollPositions;

    bool m_shouldAllowEditing;
    bool m_shouldCloseExtraWindows;

    bool m_dumpEditingCallbacks;
    bool m_dumpStatusCallbacks;
    bool m_dumpTitleChanges;
    bool m_waitToDump; // True if waitUntilDone() has been called, but notifyDone() has not yet been called.
    bool m_testRepaint;
    bool m_testRepaintSweepHorizontally;

#if PLATFORM(MAC)
    RetainPtr<CFRunLoopTimerRef> m_waitToDumpWatchdogTimer;
#elif PLATFORM(WIN)
    UINT_PTR m_waitToDumpWatchdogTimer;
#endif
};

} // namespace WTR

#endif // LayoutTestController_h

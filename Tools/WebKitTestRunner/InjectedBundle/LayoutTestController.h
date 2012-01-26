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
#include <WebKit2/WKBundleScriptWorld.h>
#include <string>
#include <wtf/PassRefPtr.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#include <CoreFoundation/CFRunLoop.h>
typedef RetainPtr<CFRunLoopTimerRef> PlatformTimerRef;
#elif PLATFORM(WIN)
typedef UINT_PTR PlatformTimerRef;
#elif PLATFORM(QT)
#include <QTimer>
typedef QTimer PlatformTimerRef;
#elif PLATFORM(GTK)
typedef unsigned int PlatformTimerRef;
#endif

namespace WTR {

class LayoutTestController : public JSWrappable {
public:
    static PassRefPtr<LayoutTestController> create();
    virtual ~LayoutTestController();

    // JSWrappable
    virtual JSClassRef wrapperClass();

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);

    // The basics.
    void dumpAsText(bool dumpPixels);
    void waitForPolicyDelegate();
    void dumpChildFramesAsText() { m_whatToDump = AllFramesText; }
    void waitUntilDone();
    void notifyDone();

    // Other dumping.
    void dumpBackForwardList() { m_shouldDumpBackForwardListsForAllWindows = true; }
    void dumpChildFrameScrollPositions() { m_shouldDumpAllFrameScrollPositions = true; }
    void dumpEditingCallbacks() { m_dumpEditingCallbacks = true; }
    void dumpSelectionRect() { } // Will need to do something when we support pixel tests.
    void dumpStatusCallbacks() { m_dumpStatusCallbacks = true; }
    void dumpTitleChanges() { m_dumpTitleChanges = true; }
    void dumpFullScreenCallbacks() { m_dumpFullScreenCallbacks = true; }
    void dumpConfigurationForViewport(int deviceDPI, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight);

    // Special options.
    void keepWebHistory();
    void setAcceptsEditing(bool value) { m_shouldAllowEditing = value; }
    void setCanOpenWindows(bool);
    void setCloseRemainingWindowsWhenComplete(bool value) { m_shouldCloseExtraWindows = value; }
    void setXSSAuditorEnabled(bool);
    void setAllowUniversalAccessFromFileURLs(bool);
    void setAllowFileAccessFromFileURLs(bool);
    void setFrameFlatteningEnabled(bool);
    void setGeolocationPermission(bool);
    void setJavaScriptCanAccessClipboard(bool);
    void setPrivateBrowsingEnabled(bool);
    void setPopupBlockingEnabled(bool);
    void setAuthorAndUserStylesEnabled(bool);
    void setCustomPolicyDelegate(bool enabled, bool permissive = false);
    void addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);
    void removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);

    // Special DOM functions.
    JSValueRef computedStyleIncludingVisitedInfo(JSValueRef element);
    JSRetainPtr<JSStringRef> counterValueForElementById(JSStringRef elementId);
    void clearBackForwardList();
    void execCommand(JSStringRef name, JSStringRef argument);
    bool isCommandEnabled(JSStringRef name);
    JSRetainPtr<JSStringRef> markerTextForListItem(JSValueRef element);
    unsigned windowCount();

    // Repaint testing.
    void testRepaint() { m_testRepaint = true; }
    void repaintSweepHorizontally() { m_testRepaintSweepHorizontally = true; }
    void display();

    // Animation testing.
    unsigned numberOfActiveAnimations() const;
    bool pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId);
    bool pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId);
    void suspendAnimations();
    void resumeAnimations();
    
    // Compositing testing.
    JSRetainPtr<JSStringRef> layerTreeAsText() const;
    
    // UserContent testing.
    void addUserScript(JSStringRef source, bool runAtStart, bool allFrames);
    void addUserStyleSheet(JSStringRef source, bool allFrames);

    // Text search testing.
    bool findString(JSStringRef, JSValueRef optionsArray);

    // Local storage
    void clearAllDatabases();
    void setDatabaseQuota(uint64_t);
    JSRetainPtr<JSStringRef> pathToLocalResource(JSStringRef);

    // Application Cache
    void clearAllApplicationCaches();
    void setAppCacheMaximumSize(uint64_t);

    // Printing
    int numberOfPages(double pageWidthInPixels, double pageHeightInPixels);
    int pageNumberForElementById(JSStringRef, double pageWidthInPixels, double pageHeightInPixels);
    JSRetainPtr<JSStringRef> pageSizeAndMarginsInPixels(int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft);
    bool isPageBoxVisible(int pageIndex);

    enum WhatToDump { RenderTree, MainFrameText, AllFramesText };
    WhatToDump whatToDump() const { return m_whatToDump; }

    bool shouldDumpAllFrameScrollPositions() const { return m_shouldDumpAllFrameScrollPositions; }
    bool shouldDumpBackForwardListsForAllWindows() const { return m_shouldDumpBackForwardListsForAllWindows; }
    bool shouldDumpEditingCallbacks() const { return m_dumpEditingCallbacks; }
    bool shouldDumpMainFrameScrollPosition() const { return m_whatToDump == RenderTree; }
    bool shouldDumpStatusCallbacks() const { return m_dumpStatusCallbacks; }
    bool shouldDumpTitleChanges() const { return m_dumpTitleChanges; }
    bool shouldDumpPixels() const { return m_dumpPixels; }
    bool shouldDumpFullScreenCallbacks() const { return m_dumpFullScreenCallbacks; }
    bool isPolicyDelegateEnabled() const { return m_policyDelegateEnabled; }
    bool isPolicyDelegatePermissive() const { return m_policyDelegatePermissive; }

    bool waitToDump() const { return m_waitToDump; }
    void waitToDumpWatchdogTimerFired();
    void invalidateWaitToDumpWatchdogTimer();

    bool shouldAllowEditing() const { return m_shouldAllowEditing; }

    bool shouldCloseExtraWindowsAfterRunningTest() const { return m_shouldCloseExtraWindows; }

    void evaluateScriptInIsolatedWorld(JSContextRef, unsigned worldID, JSStringRef script);
    static unsigned worldIDForWorld(WKBundleScriptWorldRef);

    void showWebInspector();
    void closeWebInspector();
    void evaluateInWebInspector(long callId, JSStringRef script);
    void setJavaScriptProfilingEnabled(bool);

    void setPOSIXLocale(JSStringRef);

    bool willSendRequestReturnsNull() { return m_willSendRequestReturnsNull; }
    void setWillSendRequestReturnsNull(bool f) { m_willSendRequestReturnsNull = f; }

    void setTextDirection(JSStringRef);

    void setShouldStayOnPageAfterHandlingBeforeUnload(bool);
    
    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool value) { m_globalFlag = value; }
    
    void addChromeInputField(JSValueRef);
    void removeChromeInputField(JSValueRef);
    void focusWebView(JSValueRef);
    void setBackingScaleFactor(double, JSValueRef);

    void setWindowIsKey(bool);

    void callAddChromeInputFieldCallback();
    void callRemoveChromeInputFieldCallback();
    void callFocusWebViewCallback();
    void callSetBackingScaleFactorCallback();

    JSRetainPtr<JSStringRef> platformName();
    
private:
    static const double waitToDumpWatchdogTimerInterval;

    LayoutTestController();

    void platformInitialize();
    void initializeWaitToDumpWatchdogTimerIfNeeded();

    WhatToDump m_whatToDump;
    bool m_shouldDumpAllFrameScrollPositions;
    bool m_shouldDumpBackForwardListsForAllWindows;

    bool m_shouldAllowEditing;
    bool m_shouldCloseExtraWindows;

    bool m_dumpEditingCallbacks;
    bool m_dumpStatusCallbacks;
    bool m_dumpTitleChanges;
    bool m_dumpPixels;
    bool m_dumpFullScreenCallbacks;
    bool m_waitToDump; // True if waitUntilDone() has been called, but notifyDone() has not yet been called.
    bool m_testRepaint;
    bool m_testRepaintSweepHorizontally;

    bool m_willSendRequestReturnsNull;

    bool m_policyDelegateEnabled;
    bool m_policyDelegatePermissive;
    
    bool m_globalFlag;

    PlatformTimerRef m_waitToDumpWatchdogTimer;
};

} // namespace WTR

#endif // LayoutTestController_h

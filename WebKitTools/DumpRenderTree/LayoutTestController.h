/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#ifndef LayoutTestController_h
#define LayoutTestController_h

#include <JavaScriptCore/JSObjectRef.h>
#include <wtf/RefCounted.h>
#include <string>

class LayoutTestController : public RefCounted<LayoutTestController> {
public:
    LayoutTestController(const std::string& testPathOrURL, const std::string& expectedPixelHash);
    ~LayoutTestController();

    void makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception);

    void addDisallowedURL(JSStringRef url);
    void clearAllDatabases();
    void clearBackForwardList();
    void clearPersistentUserStyleSheet();
    JSStringRef copyDecodedHostName(JSStringRef name);
    JSStringRef copyEncodedHostName(JSStringRef name);
    void dispatchPendingLoadRequests();
    void display();
    void execCommand(JSStringRef name, JSStringRef value);
    bool isCommandEnabled(JSStringRef name);
    void keepWebHistory();
    void notifyDone();
    JSStringRef pathToLocalResource(JSContextRef, JSStringRef url);
    void queueBackNavigation(int howFarBackward);
    void queueForwardNavigation(int howFarForward);
    void queueLoad(JSStringRef url, JSStringRef target);
    void queueLoadingScript(JSStringRef script);
    void queueNonLoadingScript(JSStringRef script);
    void queueReload();
    void setAcceptsEditing(bool acceptsEditing);
    void setAuthorAndUserStylesEnabled(bool);
    void setCacheModel(int);
    void setCustomPolicyDelegate(bool setDelegate, bool permissive);
    void setDatabaseQuota(unsigned long long quota);
    void setIconDatabaseEnabled(bool iconDatabaseEnabled);
    void setJavaScriptProfilingEnabled(bool profilingEnabled);
    void setMainFrameIsFirstResponder(bool flag);
    void setPersistentUserStyleSheetLocation(JSStringRef path);
    void setPopupBlockingEnabled(bool flag);
    void setPrivateBrowsingEnabled(bool flag);
    void setXSSAuditorEnabled(bool flag);
    void setSelectTrailingWhitespaceEnabled(bool flag);
    void setSmartInsertDeleteEnabled(bool flag);
    void setTabKeyCyclesThroughElements(bool cycles);
    void setUseDashboardCompatibilityMode(bool flag);
    void setUserStyleSheetEnabled(bool flag);
    void setUserStyleSheetLocation(JSStringRef path);
    void waitForPolicyDelegate();
    size_t webHistoryItemCount();
    int windowCount();
    
    bool elementDoesAutoCompleteForElementWithId(JSStringRef id);

    bool dumpAsText() const { return m_dumpAsText; }
    void setDumpAsText(bool dumpAsText) { m_dumpAsText = dumpAsText; }

    bool dumpAsPDF() const { return m_dumpAsPDF; }
    void setDumpAsPDF(bool dumpAsPDF) { m_dumpAsPDF = dumpAsPDF; }
    
    bool dumpBackForwardList() const { return m_dumpBackForwardList; }
    void setDumpBackForwardList(bool dumpBackForwardList) { m_dumpBackForwardList = dumpBackForwardList; }

    bool dumpChildFrameScrollPositions() const { return m_dumpChildFrameScrollPositions; }
    void setDumpChildFrameScrollPositions(bool dumpChildFrameScrollPositions) { m_dumpChildFrameScrollPositions = dumpChildFrameScrollPositions; }

    bool dumpChildFramesAsText() const { return m_dumpChildFramesAsText; }
    void setDumpChildFramesAsText(bool dumpChildFramesAsText) { m_dumpChildFramesAsText = dumpChildFramesAsText; }

    bool dumpDatabaseCallbacks() const { return m_dumpDatabaseCallbacks; }
    void setDumpDatabaseCallbacks(bool dumpDatabaseCallbacks) { m_dumpDatabaseCallbacks = dumpDatabaseCallbacks; }

    bool dumpStatusCallbacks() const { return m_dumpStatusCallbacks; }
    void setDumpStatusCallbacks(bool dumpStatusCallbacks) { m_dumpStatusCallbacks = dumpStatusCallbacks; }

    bool dumpDOMAsWebArchive() const { return m_dumpDOMAsWebArchive; }
    void setDumpDOMAsWebArchive(bool dumpDOMAsWebArchive) { m_dumpDOMAsWebArchive = dumpDOMAsWebArchive; }

    bool dumpSelectionRect() const { return m_dumpSelectionRect; }
    void setDumpSelectionRect(bool dumpSelectionRect) { m_dumpSelectionRect = dumpSelectionRect; }

    bool dumpSourceAsWebArchive() const { return m_dumpSourceAsWebArchive; }
    void setDumpSourceAsWebArchive(bool dumpSourceAsWebArchive) { m_dumpSourceAsWebArchive = dumpSourceAsWebArchive; }

    bool dumpTitleChanges() const { return m_dumpTitleChanges; }
    void setDumpTitleChanges(bool dumpTitleChanges) { m_dumpTitleChanges = dumpTitleChanges; }

    bool dumpEditingCallbacks() const { return m_dumpEditingCallbacks; }
    void setDumpEditingCallbacks(bool dumpEditingCallbacks) { m_dumpEditingCallbacks = dumpEditingCallbacks; }

    bool dumpResourceLoadCallbacks() const { return m_dumpResourceLoadCallbacks; }
    void setDumpResourceLoadCallbacks(bool dumpResourceLoadCallbacks) { m_dumpResourceLoadCallbacks = dumpResourceLoadCallbacks; }

    bool dumpFrameLoadCallbacks() const { return m_dumpFrameLoadCallbacks; }
    void setDumpFrameLoadCallbacks(bool dumpFrameLoadCallbacks) { m_dumpFrameLoadCallbacks = dumpFrameLoadCallbacks; }

    bool callCloseOnWebViews() const { return m_callCloseOnWebViews; }
    void setCallCloseOnWebViews(bool callCloseOnWebViews) { m_callCloseOnWebViews = callCloseOnWebViews; }

    bool canOpenWindows() const { return m_canOpenWindows; }
    void setCanOpenWindows(bool canOpenWindows) { m_canOpenWindows = canOpenWindows; }

    bool closeRemainingWindowsWhenComplete() const { return m_closeRemainingWindowsWhenComplete; }
    void setCloseRemainingWindowsWhenComplete(bool closeRemainingWindowsWhenComplete) { m_closeRemainingWindowsWhenComplete = closeRemainingWindowsWhenComplete; }
    
    bool stopProvisionalFrameLoads() const { return m_stopProvisionalFrameLoads; }
    void setStopProvisionalFrameLoads(bool stopProvisionalFrameLoads) { m_stopProvisionalFrameLoads = stopProvisionalFrameLoads; }

    bool testOnscreen() const { return m_testOnscreen; }
    void setTestOnscreen(bool testOnscreen) { m_testOnscreen = testOnscreen; }

    bool testRepaint() const { return m_testRepaint; }
    void setTestRepaint(bool testRepaint) { m_testRepaint = testRepaint; }

    bool testRepaintSweepHorizontally() const { return m_testRepaintSweepHorizontally; }
    void setTestRepaintSweepHorizontally(bool testRepaintSweepHorizontally) { m_testRepaintSweepHorizontally = testRepaintSweepHorizontally; }

    bool waitToDump() const { return m_waitToDump; }
    void setWaitToDump(bool waitToDump);

    bool windowIsKey() const { return m_windowIsKey; }
    void setWindowIsKey(bool windowIsKey);

    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool globalFlag) { m_globalFlag = globalFlag; }
    
    const std::string& testPathOrURL() const { return m_testPathOrURL; }
    const std::string& expectedPixelHash() const { return m_expectedPixelHash; }
    
    bool pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId);
    bool pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId);
    unsigned numberOfActiveAnimations() const;
    
private:
    bool m_dumpAsText;
    bool m_dumpAsPDF;
    bool m_dumpBackForwardList;
    bool m_dumpChildFrameScrollPositions;
    bool m_dumpChildFramesAsText;
    bool m_dumpDatabaseCallbacks;
    bool m_dumpDOMAsWebArchive;
    bool m_dumpSelectionRect;
    bool m_dumpSourceAsWebArchive;
    bool m_dumpStatusCallbacks;
    bool m_dumpTitleChanges;
    bool m_dumpEditingCallbacks;
    bool m_dumpResourceLoadCallbacks;
    bool m_dumpFrameLoadCallbacks;
    bool m_callCloseOnWebViews;
    bool m_canOpenWindows;
    bool m_closeRemainingWindowsWhenComplete;
    bool m_stopProvisionalFrameLoads;
    bool m_testOnscreen;
    bool m_testRepaint;
    bool m_testRepaintSweepHorizontally;
    bool m_waitToDump; // True if waitUntilDone() has been called, but notifyDone() has not yet been called.
    bool m_windowIsKey;

    bool m_globalFlag;

    std::string m_testPathOrURL;
    std::string m_expectedPixelHash;    // empty string if no hash
    
    static JSClassRef getJSClass();
    static JSStaticValue* staticValues();
    static JSStaticFunction* staticFunctions();
};

#endif // LayoutTestController_h

/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

class LayoutTestController {
public:
    LayoutTestController();
    ~LayoutTestController();

    void makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception);

    // Controller Methods - platfrom independant implementations
    void dumpAsText();
    void dumpBackForwardList();
    void dumpChildFramesAsText();
    void dumpChildFrameScrollPositions();
    void dumpDOMAsWebArchive();
    void dumpEditingCallbacks();
    void dumpFrameLoadCallbacks();
    void dumpResourceLoadCallbacks();
    void dumpSelectionRect();
    void dumpSourceAsWebArchive();
    void dumpTitleChanges();
    void repaintSweepHorizontally();
    void setCallCloseOnWebViews();
    void setCanOpenWindows();
    void setCloseRemainingWindowsWhenComplete();
    void testRepaint();
    void addFileToPasteboardOnDrag();
    void addDisallowedURL(JSStringRef url);
    void clearBackForwardList();
    JSStringRef decodeHostName(JSStringRef name);
    JSStringRef encodeHostName(JSStringRef name);
    void display();
    void keepWebHistory();
    void notifyDone();
    void queueBackNavigation(int howFarBackward);
    void queueForwardNavigation(int howFarForward);
    void queueLoad(JSStringRef url, JSStringRef target);
    void queueReload();
    void queueScript(JSStringRef url);
    void setAcceptsEditing(bool acceptsEditing);
    void setCustomPolicyDelegate(bool setDelegate);
    void setMainFrameIsFirstResponder(bool flag);
    void setTabKeyCyclesThroughElements(bool cycles);
    void setUseDashboardCompatibilityMode(bool flag);
    void setUserStyleSheetEnabled(bool flag);
    void setUserStyleSheetLocation(JSStringRef path);
    void setWindowIsKey(bool flag);
    void waitUntilDone();
    int windowCount();


private:
    static JSClassRef getLayoutTestControllerJSClass();
    static JSStaticFunction* staticFunctions();
};

#endif // LayoutTestController_h

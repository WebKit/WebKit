/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(COCOA)
#include <wtf/Deque.h>
#include <wtf/RetainPtr.h>
#endif

extern bool receivedScriptMessage;
extern bool isDone;
extern bool done;
extern bool didAttachLocalInspectorCalled;
extern bool didShowExtensionTabWasCalled;
extern bool pendingCallbackWasCalled;
extern bool didHideExtensionTabWasCalled;
extern bool inspectedPageDidNavigateWasCalled;
extern bool didEnterPiP;
extern bool didExitPiP;
extern bool readyToContinue;
extern bool didFinishLoad;
extern bool didBecomeUnresponsive;
extern bool wasPrompted;
extern bool didFinishNavigationBoolean;
extern bool didCreateWebView;
extern bool receivedLoadedMessage;
extern bool receivedFullscreenChangeMessage;

void resetGlobalState();

#if PLATFORM(COCOA)
OBJC_CLASS NSString;
OBJC_CLASS NSMutableArray;
OBJC_CLASS TestInspectorURLSchemeHandler;
OBJC_CLASS WKScriptMessage;
OBJC_CLASS _WKInspectorExtension;
extern RetainPtr<WKScriptMessage> lastScriptMessage;
extern RetainPtr<TestInspectorURLSchemeHandler> sharedURLSchemeHandler;
extern RetainPtr<_WKInspectorExtension> sharedInspectorExtension;
extern RetainPtr<NSString> sharedExtensionTabIdentifier;
extern RetainPtr<NSMutableArray> receivedMessages;
extern Deque<RetainPtr<WKScriptMessage>> scriptMessages;
WKScriptMessage *getNextMessage();
#endif

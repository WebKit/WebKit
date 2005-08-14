/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

// This header contains WebView declarations that can be used anywhere in the Web Kit, but are neither SPI nor API.

#import <WebKit/WebViewPrivate.h>

@class DOMCSSStyleDeclaration;
@class WebBackForwardList;

@protocol WebDocumentDragging;

@interface WebViewPrivate : NSObject
{
@public
    WebFrame *mainFrame;
    
    id UIDelegate;
    id UIDelegateForwarder;
    id resourceProgressDelegate;
    id resourceProgressDelegateForwarder;
    id downloadDelegate;
    id policyDelegate;
    id policyDelegateForwarder;
    id frameLoadDelegate;
    id frameLoadDelegateForwarder;
    id <WebFormDelegate> formDelegate;
    id editingDelegate;
    id editingDelegateForwarder;
    id scriptDebugDelegate;
    id scriptDebugDelegateForwarder;
    
    WebBackForwardList *backForwardList;
    BOOL useBackForwardList;
    
    float textSizeMultiplier;

    NSString *applicationNameForUserAgent;
    NSString *userAgent;
    BOOL userAgentOverridden;
    
    BOOL defersCallbacks;

    NSString *setName;

    WebPreferences *preferences;
    WebCoreSettings *settings;
        
    BOOL lastElementWasNonNil;

    NSWindow *hostWindow;

    int programmaticFocusCount;
    
    WebResourceDelegateImplementationCache resourceLoadDelegateImplementations;

    long long totalPageAndResourceBytesToLoad;
    long long totalBytesReceived;
    double progressValue;
    double lastNotifiedProgressValue;
    double lastNotifiedProgressTime;
    double progressNotificationInterval;
    double progressNotificationTimeInterval;
    BOOL finalProgressChangedSent;
    WebFrame *orginatingProgressFrame;
    
    int numProgressTrackedFrames;
    NSMutableDictionary *progressItems;
    
    void *observationInfo;
    
    BOOL drawsBackground;
    BOOL editable;
    BOOL initiatedDrag;
        
    NSString *mediaStyle;
    
    NSView <WebDocumentDragging> *draggingDocumentView;
    unsigned int dragDestinationActionMask;
    WebBridge *dragCaretBridge;
    
    BOOL hasSpellCheckerDocumentTag;
    int spellCheckerDocumentTag;

    BOOL continuousSpellCheckingEnabled;
    BOOL smartInsertDeleteEnabled;
    
    BOOL dashboardBehaviorAlwaysSendMouseEventsToAllWindows;
    BOOL dashboardBehaviorAlwaysSendActiveNullEventsToPlugIns;
    BOOL dashboardBehaviorAlwaysAcceptsFirstMouse;
    BOOL dashboardBehaviorAllowWheelScrolling;
    
    BOOL shouldUseFontSmoothing;
    BOOL selectWordBeforeMenuEvent;
}
@end

@interface WebView (WebViewEditingExtras)
- (BOOL)_interceptEditingKeyEvent:(NSEvent *)event;
- (BOOL)_shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag;
- (BOOL)_shouldBeginEditingInDOMRange:(DOMRange *)range;
- (BOOL)_shouldEndEditingInDOMRange:(DOMRange *)range;
- (BOOL)_canPaste;
@end

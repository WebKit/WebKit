/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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


@class DumpRenderTreeDraggingInfo;
@class EditingDelegate;
@class FrameLoadDelegate;
@class NavigationController;
@class PolicyDelegate;
@class ResourceLoadDelegate;
@class UIDelegate;
@class WebFrame;
@class WebScriptObject;
@class WebView;


extern BOOL dumpAsText;
extern BOOL dumpDOMAsWebArchive;
extern BOOL dumpSourceAsWebArchive;
extern BOOL dumpSelectionRect;
extern BOOL dumpTitleChanges;
extern BOOL dumpBackForwardList;
extern BOOL dumpChildFrameScrollPositions;
extern BOOL dumpChildFramesAsText;
extern BOOL testRepaint;
extern BOOL repaintSweepHorizontally;
extern BOOL windowIsKey;
extern BOOL shouldDumpEditingCallbacks;
extern BOOL shouldDumpResourceLoadCallbacks;
extern BOOL shouldDumpFrameLoadCallbacks;
extern BOOL shouldDumpResourceLoadCallbacks;
extern BOOL shouldDumpFrameLoadCallbacks;
extern BOOL waitToDump;
extern BOOL canOpenWindows;
extern BOOL closeWebViews;
extern BOOL closeRemainingWindowsWhenComplete;
extern BOOL addFileToPasteboardOnDrag;

extern volatile BOOL done;

extern WebFrame *mainFrame;
extern DumpRenderTreeDraggingInfo *draggingInfo;
extern WebFrame *topLoadingFrame;
extern NavigationController *navigationController;
extern NSTimer *waitToDumpWatchdog;
extern NSTimeInterval waitToDumpWatchdogInterval;
extern CFMutableArrayRef allWindowsRef;
extern NSMutableSet *disallowedURLs;

// Delegates
extern FrameLoadDelegate *frameLoadDelegate;
extern UIDelegate *uiDelegate;
extern EditingDelegate *editingDelegate;
extern ResourceLoadDelegate *resourceLoadDelegate;
extern PolicyDelegate *policyDelegate;

WebView *createWebView();
void displayWebView();
void dump(void);

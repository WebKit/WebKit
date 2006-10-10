/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

// This header contains WebFrame declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import "WebFramePrivate.h"

#import <WebCore/WebFrameLoaderClient.h>
#import <WebCore/WebFrameLoader.h>
#import "WebPolicyDelegatePrivate.h"


@class WebDocumentLoader;
@class WebInspector;
@class WebFrameView;
@class WebFrameBridge;
@class WebFormState;

@interface WebFrame (WebInternal)

- (void)_updateBackground;
- (void)_setInternalLoadDelegate:(id)internalLoadDelegate;
- (id)_internalLoadDelegate;
- (void)_unmarkAllMisspellings;
// Note that callers should not perform any ops on these views that could change the set of frames
- (NSArray *)_documentViews;

- (BOOL)_hasSelection;
- (void)_clearSelection;
- (WebFrame *)_findFrameWithSelection;
- (void)_clearSelectionInOtherFrames;
- (id)_initWithWebFrameView:(WebFrameView *)fv webView:(WebView *)v bridge:(WebFrameBridge *)bridge;

- (void)_addPlugInView:(NSView *)plugInView;
- (void)_removeAllPlugInViews;

// This should be called when leaving a page or closing the WebView
- (void)_willCloseURL;

- (BOOL)_isMainFrame;

- (void)_addInspector:(WebInspector *)inspector;
- (void)_removeInspector:(WebInspector *)inspector;

- (WebFrameLoader *)_frameLoader;
- (WebDataSource *)_dataSourceForDocumentLoader:(WebDocumentLoader *)loader;

- (NSURLRequest *)_webDataRequestForData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;

- (WebFrameBridge *)_bridge;

- (void)_goToItem:(WebHistoryItem *)item withLoadType:(FrameLoadType)type;
- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer intoChild:(WebFrame *)childFrame;

- (void)_viewWillMoveToHostWindow:(NSWindow *)hostWindow;
- (void)_viewDidMoveToHostWindow;

- (void)_addChild:(WebFrame *)child;

- (WebHistoryItem *)_itemForSavingDocState;
- (WebHistoryItem *)_itemForRestoringDocState;

- (void)_saveDocumentAndScrollState;

+ (CFAbsoluteTime)_timeOfLastCompletedLoad;
- (BOOL)_canCachePage;
- (void)_purgePageCache;


- (WebFrame *)_nextFrameWithWrap:(BOOL)wrapFlag;
- (WebFrame *)_previousFrameWithWrap:(BOOL)wrapFlag;

- (int)_numPendingOrLoadingRequests:(BOOL)recurse;

- (void)_reloadForPluginChanges;

- (void)_attachScriptDebugger;
- (void)_detachScriptDebugger;

- (void)_recursive_pauseNullEventsForAllNetscapePlugins;
- (void)_recursive_resumeNullEventsForAllNetscapePlugins;

- (void)_addDocumentLoader:(WebDocumentLoader *)loader toUnarchiveState:(WebArchive *)archive;

@end

@interface NSObject (WebInternalFrameLoadDelegate)
- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error;
@end

@interface WebFrame (FrameTraversal)
- (WebFrame *)_firstChildFrame;
- (WebFrame *)_lastChildFrame;
- (unsigned)_childFrameCount;
- (WebFrame *)_previousSiblingFrame;
- (WebFrame *)_nextSiblingFrame;
- (WebFrame *)_traverseNextFrameStayWithin:(WebFrame *)stayWithin;
@end

@interface WebFrame (WebFrameLoaderClient) <WebFrameLoaderClient>
@end

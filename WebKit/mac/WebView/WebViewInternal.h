/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

// This header contains WebView declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import "WebPreferences.h"
#import "WebViewPrivate.h"
#import "WebTypesInternal.h"

#ifdef __cplusplus
#import <WebCore/WebCoreKeyboardUIMode.h>
#endif

#ifdef __cplusplus
namespace WebCore {
    class KeyboardEvent;
    class KURL;
    class Page;
    class String;
}
typedef WebCore::KeyboardEvent WebCoreKeyboardEvent;
typedef WebCore::Page WebCorePage;
#else
@class WebCoreKeyboardEvent;
@class WebCorePage;
#endif

@class WebBasePluginPackage;
@class WebDownload;
@class WebNodeHighlight;

@interface WebView (WebViewEditingExtras)
- (BOOL)_interceptEditingKeyEvent:(WebCoreKeyboardEvent *)event shouldSaveCommand:(BOOL)shouldSave;
- (BOOL)_shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag;
@end

@interface WebView (AllWebViews)
+ (void)_makeAllWebViewsPerformSelector:(SEL)selector;
- (void)_removeFromAllWebViewsSet;
- (void)_addToAllWebViewsSet;
@end

@interface WebView (WebViewInternal)
#ifdef __cplusplus
- (WebCore::String)_userAgentForURL:(const WebCore::KURL&)url;
- (WebCore::KeyboardUIMode)_keyboardUIMode;
#endif
@end

@interface WebView (WebViewMiscInternal)

+ (void)_setCacheModel:(WebCacheModel)cacheModel;
+ (WebCacheModel)_cacheModel;
- (WebCorePage*)page;
- (NSMenu *)_menuForElement:(NSDictionary *)element defaultItems:(NSArray *)items;
- (id)_UIDelegateForwarder;
- (id)_editingDelegateForwarder;
- (id)_policyDelegateForwarder;
- (id)_scriptDebugDelegateForwarder;
- (void)_pushPerformingProgrammaticFocus;
- (void)_popPerformingProgrammaticFocus;
- (void)_incrementProgressForIdentifier:(id)identifier response:(NSURLResponse *)response;
- (void)_incrementProgressForIdentifier:(id)identifier length:(int)length;
- (void)_completeProgressForIdentifier:(id)identifer;
- (void)_progressStarted:(WebFrame *)frame;
- (void)_didStartProvisionalLoadForFrame:(WebFrame *)frame;
+ (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType;
- (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType;
+ (NSString *)_MIMETypeForFile:(NSString *)path;
- (WebDownload *)_downloadURL:(NSURL *)URL;
+ (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme;
+ (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme;
- (BOOL)_isPerformingProgrammaticFocus;
- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(NSUInteger)modifierFlags;
- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request;
- (void)_writeImageForElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard;
- (void)_writeLinkElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard;
- (void)_openFrameInNewWindowFromMenu:(NSMenuItem *)sender;
- (void)_searchWithGoogleFromMenu:(id)sender;
- (void)_searchWithSpotlightFromMenu:(id)sender;
- (void)_progressCompleted:(WebFrame *)frame;
- (void)_didCommitLoadForFrame:(WebFrame *)frame;
- (void)_didFinishLoadForFrame:(WebFrame *)frame;
- (void)_didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
- (void)_didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
- (void)_willChangeValueForKey:(NSString *)key;
- (void)_didChangeValueForKey:(NSString *)key;
- (WebBasePluginPackage *)_pluginForMIMEType:(NSString *)MIMEType;
- (WebBasePluginPackage *)_pluginForExtension:(NSString *)extension;
- (BOOL)_isMIMETypeRegisteredAsPlugin:(NSString *)MIMEType;

- (void)setCurrentNodeHighlight:(WebNodeHighlight *)nodeHighlight;
- (WebNodeHighlight *)currentNodeHighlight;

- (void)addPluginInstanceView:(NSView *)view;
- (void)removePluginInstanceView:(NSView *)view;
- (void)removePluginInstanceViewsFor:(WebFrame*)webFrame;

- (void)_addObject:(id)object forIdentifier:(unsigned long)identifier;
- (id)_objectForIdentifier:(unsigned long)identifier;
- (void)_removeObjectForIdentifier:(unsigned long)identifier;
- (BOOL)_becomingFirstResponderFromOutside;

- (void)_registerForIconNotification:(BOOL)listen;
- (void)_dispatchDidReceiveIconFromWebFrame:(WebFrame *)webFrame;

- (void)_setZoomMultiplier:(float)m isTextOnly:(BOOL)isTextOnly;
- (float)_zoomMultiplier:(BOOL)isTextOnly;
- (float)_realZoomMultiplier;
- (BOOL)_realZoomMultiplierIsTextOnly;
- (BOOL)_canZoomOut:(BOOL)isTextOnly;
- (BOOL)_canZoomIn:(BOOL)isTextOnly;
- (IBAction)_zoomOut:(id)sender isTextOnly:(BOOL)isTextOnly;
- (IBAction)_zoomIn:(id)sender isTextOnly:(BOOL)isTextOnly;
- (BOOL)_canResetZoom:(BOOL)isTextOnly;
- (IBAction)_resetZoom:(id)sender isTextOnly:(BOOL)isTextOnly;

- (BOOL)_mustDrawUnionedRect:(NSRect)rect singleRects:(const NSRect *)rects count:(NSInteger)count;
- (void)_updateFocusedAndActiveStateForFrame:(WebFrame *)webFrame;

+ (BOOL)_canHandleRequest:(NSURLRequest *)request forMainFrame:(BOOL)forMainFrame;

#if USE(ACCELERATED_COMPOSITING)
- (BOOL)_needsOneShotDrawingSynchronization;
- (void)_setNeedsOneShotDrawingSynchronization:(BOOL)needsSynchronization;
#endif    

@end

typedef struct _WebResourceDelegateImplementationCache {
    IMP didCancelAuthenticationChallengeFunc;
    IMP didReceiveAuthenticationChallengeFunc;
    IMP identifierForRequestFunc;
    IMP willSendRequestFunc;
    IMP didReceiveResponseFunc;
    IMP didReceiveContentLengthFunc;
    IMP didFinishLoadingFromDataSourceFunc;
    IMP didFailLoadingWithErrorFromDataSourceFunc;
    IMP didLoadResourceFromMemoryCacheFunc;
    IMP willCacheResponseFunc;
    IMP plugInFailedWithErrorFunc;
    IMP shouldUseCredentialStorageFunc;
} WebResourceDelegateImplementationCache;

typedef struct _WebFrameLoadDelegateImplementationCache {
    IMP didClearWindowObjectForFrameFunc;
    IMP windowScriptObjectAvailableFunc;
    IMP didHandleOnloadEventsForFrameFunc;
    IMP didReceiveServerRedirectForProvisionalLoadForFrameFunc;
    IMP didCancelClientRedirectForFrameFunc;
    IMP willPerformClientRedirectToURLDelayFireDateForFrameFunc;
    IMP didChangeLocationWithinPageForFrameFunc;
    IMP willCloseFrameFunc;
    IMP didStartProvisionalLoadForFrameFunc;
    IMP didReceiveTitleForFrameFunc;
    IMP didCommitLoadForFrameFunc;
    IMP didFailProvisionalLoadWithErrorForFrameFunc;
    IMP didFailLoadWithErrorForFrameFunc;
    IMP didFinishLoadForFrameFunc;
    IMP didFirstLayoutInFrameFunc;
    IMP didFirstVisuallyNonEmptyLayoutInFrameFunc;
    IMP didReceiveIconForFrameFunc;
    IMP didFinishDocumentLoadForFrameFunc;
} WebFrameLoadDelegateImplementationCache;

WebResourceDelegateImplementationCache* WebViewGetResourceLoadDelegateImplementations(WebView *webView);
WebFrameLoadDelegateImplementationCache* WebViewGetFrameLoadDelegateImplementations(WebView *webView);

#ifdef __cplusplus

id CallFormDelegate(WebView *, SEL, id, id);
id CallFormDelegate(WebView *self, SEL selector, id object1, id object2, id object3, id object4, id object5);
BOOL CallFormDelegateReturningBoolean(BOOL, WebView *, SEL, id, SEL, id);

id CallUIDelegate(WebView *, SEL);
id CallUIDelegate(WebView *, SEL, id);
id CallUIDelegate(WebView *, SEL, NSRect);
id CallUIDelegate(WebView *, SEL, id, id);
id CallUIDelegate(WebView *, SEL, id, BOOL);
id CallUIDelegate(WebView *, SEL, id, id, id);
id CallUIDelegate(WebView *, SEL, id, NSUInteger);
float CallUIDelegateReturningFloat(WebView *, SEL);
BOOL CallUIDelegateReturningBoolean(BOOL, WebView *, SEL);
BOOL CallUIDelegateReturningBoolean(BOOL, WebView *, SEL, id);
BOOL CallUIDelegateReturningBoolean(BOOL, WebView *, SEL, id, id);
BOOL CallUIDelegateReturningBoolean(BOOL, WebView *, SEL, id, BOOL);

id CallFrameLoadDelegate(IMP, WebView *, SEL);
id CallFrameLoadDelegate(IMP, WebView *, SEL, id);
id CallFrameLoadDelegate(IMP, WebView *, SEL, id, id);
id CallFrameLoadDelegate(IMP, WebView *, SEL, id, id, id);
id CallFrameLoadDelegate(IMP, WebView *, SEL, id, id, id, id);
id CallFrameLoadDelegate(IMP, WebView *, SEL, id, NSTimeInterval, id, id);

id CallResourceLoadDelegate(IMP, WebView *, SEL, id, id);
id CallResourceLoadDelegate(IMP, WebView *, SEL, id, id, id);
id CallResourceLoadDelegate(IMP, WebView *, SEL, id, id, id, id);
id CallResourceLoadDelegate(IMP, WebView *, SEL, id, NSInteger, id);
id CallResourceLoadDelegate(IMP, WebView *, SEL, id, id, NSInteger, id);

BOOL CallResourceLoadDelegateReturningBoolean(BOOL, IMP, WebView *, SEL, id, id);

#endif

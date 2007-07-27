/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#import "WebViewPrivate.h"
#import "WebTypesInternal.h"

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
#endif
@end

id WebViewGetResourceLoadDelegate(WebView *webView);
WebResourceDelegateImplementationCache WebViewGetResourceLoadDelegateImplementations(WebView *webView);

id WebViewGetFrameLoadDelegate(WebView *webView);
WebFrameLoadDelegateImplementationCache WebViewGetFrameLoadDelegateImplementations(WebView *webView);

@interface WebView (WebViewMiscInternal)
+ (void)_initializeCacheSizesIfNecessary;
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

- (void)_addObject:(id)object forIdentifier:(unsigned long)identifier;
- (id)_objectForIdentifier:(unsigned long)identifier;
- (void)_removeObjectForIdentifier:(unsigned long)identifier;
- (BOOL)_becomingFirstResponderFromOutside;
@end

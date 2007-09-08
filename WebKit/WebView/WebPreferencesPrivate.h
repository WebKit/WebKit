/*
 * Copyright (C) 2005, 2007 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebPreferences.h>
#import <Quartz/Quartz.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
#define WebNSUInteger unsigned int
#else
#define WebNSUInteger NSUInteger
#endif

// WebKitEditableLinkBehavior needs to match the EditableLinkBehavior enum in WebCore
typedef enum {
    WebKitEditableLinkDefaultBehavior = 0,
    WebKitEditableLinkAlwaysLive,
    WebKitEditableLinkOnlyLiveWithShiftKey,
    WebKitEditableLinkLiveWhenNotFocused,
    WebKitEditableLinkNeverLive
} WebKitEditableLinkBehavior;

/*!
@enum WebCacheModel

@abstract Specifies a usage model for a WebView, which WebKit will use to 
determine its caching behavior.

@constant WebCacheModelDocumentViewer Appropriate for an application displaying 
fixed documents -- like splash screens, chat documents, or word processing 
documents -- with no UI for navigation. The WebView will behave like any other 
view, releasing resources when they are no longer referenced. Remote resources, 
if any, will be cached to disk. This is the most memory-efficient setting.

Examples: iChat, Mail, TextMate, Growl.

@constant WebCacheModelDocumentBrowser Appropriate for an application displaying 
a browsable series of documents with a UI for navigating between them -- for 
example, a reference materials browser or a website design application. The 
WebView will cache a reasonable number of resources and previously viewed 
documents in memory and/or on disk.

Examples: Dictionary, Help Viewer, Coda.

@constant WebCacheModelPrimaryWebBrowser Appropriate for the application that 
acts as the user's primary web browser. The WebView will cache a very large 
number of resources and previously viewed documents in memory and/or on disk.

Examples: Safari, OmniWeb, Shiira.
*/
enum {
    WebCacheModelDocumentViewer = 0,
    WebCacheModelDocumentBrowser = 1,
    WebCacheModelPrimaryWebBrowser = 2
};
typedef WebNSUInteger WebCacheModel;

extern NSString *WebPreferencesChangedNotification;
extern NSString *WebPreferencesRemovedNotification;

@interface WebPreferences (WebPrivate)

// Preferences that might be public in a future release
- (BOOL)respectStandardStyleKeyEquivalents;
- (void)setRespectStandardStyleKeyEquivalents:(BOOL)flag;

- (BOOL)showsURLsInToolTips;
- (void)setShowsURLsInToolTips:(BOOL)flag;

- (BOOL)textAreasAreResizable;
- (void)setTextAreasAreResizable:(BOOL)flag;

- (PDFDisplayMode)PDFDisplayMode;
- (void)setPDFDisplayMode:(PDFDisplayMode)mode;

- (BOOL)shrinksStandaloneImagesToFit;
- (void)setShrinksStandaloneImagesToFit:(BOOL)flag;

/*!
@method setCacheModel:

@abstract Specifies a usage model for a WebView, which WebKit will use to 
determine its caching behavior.

@param cacheModel The application's usage model for WebKit. If necessary, 
WebKit will prune its caches to match cacheModel.

@discussion Research indicates that users tend to browse within clusters of 
documents that hold resources in common, and to revisit previously visited 
documents. WebKit and the frameworks below it include built-in caches that take 
advantage of these patterns, substantially improving document load speed in 
browsing situations. The WebKit cache model controls the behaviors of all of 
these caches, including NSURLCache and the various WebCore caches.

Applications with a browsing interface can improve document load speed 
substantially by specifying WebCacheModelDocumentBrowser. Applications without 
a browsing interface can reduce memory usage substantially by specifying 
WebCacheModelDocumentViewer.

If setCacheModel: is not called, WebKit will select a cache model automatically.
*/
- (void)setCacheModel:(WebCacheModel)cacheModel;

/*!
@method cacheModel:

@abstract Returns the usage model according to which WebKit determines its 
caching behavior.

@result The usage model.
*/
- (WebCacheModel)cacheModel;

- (BOOL)automaticallyDetectsCacheModel;
- (void)setAutomaticallyDetectsCacheModel:(BOOL)automaticallyDetectsCacheModel;

// zero means do AutoScale
- (float)PDFScaleFactor;
- (void)setPDFScaleFactor:(float)scale;

- (WebKitEditableLinkBehavior)editableLinkBehavior;
- (void)setEditableLinkBehavior:(WebKitEditableLinkBehavior)behavior;

// If site-specific spoofing is enabled, some pages that do inappropriate user-agent string checks will be
// passed a nonstandard user-agent string to get them to work correctly. This method might be removed in
// the future when there's no more need for it.
- (BOOL)_useSiteSpecificSpoofing;
- (void)_setUseSiteSpecificSpoofing:(BOOL)newValue;

// For debugging purposes, can be removed when no longer needed
- (BOOL)_usePDFPreviewView;
- (void)_setUsePDFPreviewView:(BOOL)newValue;

// WARNING: Allowing paste through the DOM API opens a security hole. We only use it for testing purposes.
- (BOOL)isDOMPasteAllowed;
- (void)setDOMPasteAllowed:(BOOL)DOMPasteAllowed;

- (NSString *)_ftpDirectoryTemplatePath;
- (void)_setFTPDirectoryTemplatePath:(NSString *)path;
- (void)_setForceFTPDirectoryListings:(BOOL)force;
- (BOOL)_forceFTPDirectoryListings;

// Other private methods
- (void)_postPreferencesChangesNotification;
+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)identifier;
+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)identifier;
+ (void)_removeReferenceForIdentifier:(NSString *)identifier;
- (NSTimeInterval)_backForwardCacheExpirationInterval;
+ (CFStringEncoding)_systemCFStringEncoding;
+ (void)_setInitialDefaultTextEncodingToSystemEncoding;
+ (void)_setIBCreatorID:(NSString *)string;

// For WebView's use only.
- (void)willAddToWebView;
- (void)didRemoveFromWebView;

@end

#undef WebNSUInteger

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

// This header contains the WebFrame SPI.

#import <WebKit/WebFrame.h>
#import <JavaScriptCore/JSBase.h>

#if !defined(ENABLE_NETSCAPE_PLUGIN_API)
#define ENABLE_NETSCAPE_PLUGIN_API 1
#endif

@class DOMDocumentFragment;
@class DOMNode;
@class WebIconFetcher;
@class WebScriptObject;

// Keys for accessing the values in the page cache dictionary.
extern NSString *WebPageCacheEntryDateKey;
extern NSString *WebPageCacheDataSourceKey;
extern NSString *WebPageCacheDocumentViewKey;

extern NSString *WebFrameMainDocumentError;
extern NSString *WebFrameHasPlugins;
extern NSString *WebFrameHasUnloadListener;
extern NSString *WebFrameUsesDatabases;
extern NSString *WebFrameUsesGeolocation;
extern NSString *WebFrameUsesApplicationCache;
extern NSString *WebFrameCanSuspendActiveDOMObjects;

typedef enum {
    WebFrameLoadTypeStandard,
    WebFrameLoadTypeBack,
    WebFrameLoadTypeForward,
    WebFrameLoadTypeIndexedBackForward, // a multi-item hop in the backforward list
    WebFrameLoadTypeReload,
    WebFrameLoadTypeReloadAllowingStaleData,
    WebFrameLoadTypeSame,               // user loads same URL again (but not reload button)
    WebFrameLoadTypeInternal,           // maps to WebCore::FrameLoadTypeRedirectWithLockedBackForwardList
    WebFrameLoadTypeReplace,
    WebFrameLoadTypeReloadFromOrigin,
    WebFrameLoadTypeBackWMLDeckNotAccessible
} WebFrameLoadType;

@interface WebFrame (WebPrivate)
- (BOOL)_isDescendantOfFrame:(WebFrame *)frame;
- (void)_setShouldCreateRenderers:(BOOL)f;
- (NSColor *)_bodyBackgroundColor;
- (BOOL)_isFrameSet;
- (BOOL)_firstLayoutDone;
- (WebFrameLoadType)_loadType;

// These methods take and return NSRanges based on the root editable element as the positional base.
// This fits with AppKit's idea of an input context. These methods are slow compared to their DOMRange equivalents.
// You should use WebView's selectedDOMRange and setSelectedDOMRange whenever possible.
- (NSRange)_selectedNSRange;
- (void)_selectNSRange:(NSRange)range;

- (BOOL)_isDisplayingStandaloneImage;

- (unsigned) _pendingFrameUnloadEventCount;

- (WebIconFetcher *)fetchApplicationIcon:(id)target
                                selector:(SEL)selector;

- (void)_setIsDisconnected:(bool)isDisconnected;
- (void)_setExcludeFromTextSearch:(bool)exclude;

#if ENABLE_NETSCAPE_PLUGIN_API
- (void)_recursive_resumeNullEventsForAllNetscapePlugins;
- (void)_recursive_pauseNullEventsForAllNetscapePlugins;
#endif

- (NSString *)_stringByEvaluatingJavaScriptInIsolatedWorld:(unsigned)worldID WithGlobalObject:(JSObjectRef)globalObject FromString:(NSString *)string;

// Pause a given CSS animation or transition on the target node at a specific time.
// If the animation or transition is already paused, it will update its pause time.
// This method is only intended to be used for testing the CSS animation and transition system.
- (BOOL)_pauseAnimation:(NSString*)name onNode:(DOMNode *)node atTime:(NSTimeInterval)time;
- (BOOL)_pauseTransitionOfProperty:(NSString*)name onNode:(DOMNode*)node atTime:(NSTimeInterval)time;

// Returns the total number of currently running animations (includes both CSS transitions and CSS animations).
- (unsigned) _numberOfActiveAnimations;

- (void)_replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)_replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;
- (void)_replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;

- (NSMutableDictionary *)_cacheabilityDictionary;

- (BOOL)_allowsFollowingLink:(NSURL *)URL;
@end

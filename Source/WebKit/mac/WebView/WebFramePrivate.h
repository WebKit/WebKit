/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#if !TARGET_OS_IPHONE
#if !defined(ENABLE_NETSCAPE_PLUGIN_API)
#define ENABLE_NETSCAPE_PLUGIN_API 1
#endif
#endif

#if TARGET_OS_IPHONE
#include <CoreText/CoreText.h>
#include <WebKit/WAKAppKitStubs.h>
#endif

@class DOMDocumentFragment;
@class DOMNode;
@class DOMRange;
@class WebScriptObject;
@class WebScriptWorld;

#if TARGET_OS_IPHONE
@class DOMElement;
@class DOMRange;
@class WebEvent;
#endif

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
} WebFrameLoadType;

@interface WebFrame (WebPrivate)

- (BOOL)_isDescendantOfFrame:(WebFrame *)frame;
- (void)_setShouldCreateRenderers:(BOOL)shouldCreateRenderers;
#if !TARGET_OS_IPHONE
- (NSColor *)_bodyBackgroundColor;
#else
- (CGColorRef)_bodyBackgroundColor;
#endif
- (BOOL)_isFrameSet;
- (BOOL)_firstLayoutDone;
- (BOOL)_isVisuallyNonEmpty;
- (WebFrameLoadType)_loadType;
#if TARGET_OS_IPHONE
- (BOOL)needsLayout; // Needed for Mail <rdar://problem/6228038>
- (void)_setLoadsSynchronously:(BOOL)flag;
- (BOOL)_loadsSynchronously;
- (unsigned)formElementsCharacterCount;
- (void)setTimeoutsPaused:(BOOL)flag;

/*!
    @method setPluginsPaused
    @abstract Stop/start all plugins based on the flag passed if we have a WebHTMLView
    @param flag YES to stop plugins on the html view, NO to start them
 */
- (void)setPluginsPaused:(BOOL)flag;
- (void)prepareForPause;
- (void)resumeFromPause;
- (void)updateLayout;
- (void)selectNSRange:(NSRange)range;
- (void)selectWithoutClosingTypingNSRange:(NSRange)range;
- (NSRange)selectedNSRange;
- (void)forceLayoutAdjustingViewSize:(BOOL)adjust;
- (void)_handleKeyEvent:(WebEvent *)event;
- (void)_selectAll;
- (void)_setSelectionFromNone;
- (void)_saveViewState;
- (void)_restoreViewState;
- (void)sendOrientationChangeEvent:(int)newOrientation;
- (void)setNeedsLayout;
- (CGSize)renderedSizeOfNode:(DOMNode *)node constrainedToWidth:(float)width;
- (DOMNode *)deepestNodeAtViewportLocation:(CGPoint)aViewportLocation;
- (DOMNode *)scrollableNodeAtViewportLocation:(CGPoint)aViewportLocation;
- (DOMNode *)approximateNodeAtViewportLocation:(CGPoint *)aViewportLocation;
- (CGRect)renderRectForPoint:(CGPoint)point isReplaced:(BOOL *)isReplaced fontSize:(float *)fontSize;

- (void)_setProhibitsScrolling:(BOOL)flag;

- (void)revealSelectionAtExtent:(BOOL)revealExtent;
- (void)resetSelection;
- (BOOL)hasEditableSelection;

- (int)preferredHeight;
// Returns the line height of the inner node of a text control.
// For other nodes, the value is the same as lineHeight.
- (int)innerLineHeight:(DOMNode *)node;
- (void)setIsActive:(BOOL)flag;
- (void)setSelectionChangeCallbacksDisabled:(BOOL)flag;
- (NSRect)caretRect;
- (NSRect)rectForScrollToVisible; // return caretRect if selection is caret, selectionRect otherwise
- (void)setCaretColor:(CGColorRef)color;
- (NSView *)documentView;
- (int)layoutCount;
- (BOOL)isTelephoneNumberParsingAllowed;
- (BOOL)isTelephoneNumberParsingEnabled;
- (BOOL)mediaDataLoadsAutomatically;
- (void)setMediaDataLoadsAutomatically:(BOOL)flag;

- (DOMRange *)selectedDOMRange;
- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)affinity closeTyping:(BOOL)closeTyping;
- (NSSelectionAffinity)selectionAffinity;
- (void)expandSelectionToElementContainingCaretSelection;
- (DOMRange *)elementRangeContainingCaretSelection;
- (void)expandSelectionToWordContainingCaretSelection;
- (void)expandSelectionToStartOfWordContainingCaretSelection;
- (unichar)characterInRelationToCaretSelection:(int)amount;
- (unichar)characterBeforeCaretSelection;
- (unichar)characterAfterCaretSelection;
- (DOMRange *)wordRangeContainingCaretSelection;
- (NSString *)wordInRange:(DOMRange *)range;
- (int)wordOffsetInRange:(DOMRange *)range;
- (BOOL)spaceFollowsWordInRange:(DOMRange *)range;
- (NSArray *)wordsInCurrentParagraph;
- (BOOL)selectionAtDocumentStart;
- (BOOL)selectionAtSentenceStart;
- (BOOL)selectionAtWordStart;
- (DOMRange *)rangeByMovingCurrentSelection:(int)amount;
- (DOMRange *)rangeByExtendingCurrentSelection:(int)amount;
- (void)selectNSRange:(NSRange)range onElement:(DOMElement *)element;
- (DOMRange *)markedTextDOMRange;
- (void)setMarkedText:(NSString *)text selectedRange:(NSRange)newSelRange;
- (void)setMarkedText:(NSString *)text forCandidates:(BOOL)forCandidates;
- (void)confirmMarkedText:(NSString *)text;
- (void)setText:(NSString *)text asChildOfElement:(DOMElement *)element;
- (void)setDictationPhrases:(NSArray *)dictationPhrases metadata:(id)metadata asChildOfElement:(DOMElement *)element;
- (NSArray *)interpretationsForCurrentRoot;
- (void)getDictationResultRanges:(NSArray **)ranges andMetadatas:(NSArray **)metadatas;
- (id)dictationResultMetadataForRange:(DOMRange *)range;
- (void)recursiveSetUpdateAppearanceEnabled:(BOOL)enabled;

// WebCoreFrameBridge methods used by iOS applications and frameworks
+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName;

- (NSRect)caretRectAtNode:(DOMNode *)node offset:(int)offset affinity:(NSSelectionAffinity)affinity;
- (DOMRange *)characterRangeAtPoint:(NSPoint)point;
- (NSRange)convertDOMRangeToNSRange:(DOMRange *)range;
- (DOMRange *)convertNSRangeToDOMRange:(NSRange)nsrange;
- (NSRect)firstRectForDOMRange:(DOMRange *)range;
- (CTFontRef)fontForSelection:(BOOL *)hasMultipleFonts;
- (void)sendScrollEvent;
- (void)_userScrolled;
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture;
- (NSString *)stringForRange:(DOMRange *)range;
- (void)_replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
#endif // TARGET_OS_IPHONE

// These methods take and return NSRanges based on the root editable element as the positional base.
// This fits with AppKit's idea of an input context. These methods are slow compared to their DOMRange equivalents.
// You should use WebView's selectedDOMRange and setSelectedDOMRange whenever possible.
- (NSRange)_selectedNSRange;
- (void)_selectNSRange:(NSRange)range;

#if TARGET_OS_IPHONE
// FIXME: selection
- (NSArray *)_rectsForRange:(DOMRange *)domRange;
- (DOMRange *)_selectionRangeForPoint:(CGPoint)point;
- (DOMRange *)_selectionRangeForFirstPoint:(CGPoint)first secondPoint:(CGPoint)second;
#endif

- (BOOL)_isDisplayingStandaloneImage;

- (unsigned)_pendingFrameUnloadEventCount;

#if !TARGET_OS_IPHONE
#if ENABLE_NETSCAPE_PLUGIN_API
- (void)_recursive_resumeNullEventsForAllNetscapePlugins;
- (void)_recursive_pauseNullEventsForAllNetscapePlugins;
#endif
#endif

- (NSString *)_stringByEvaluatingJavaScriptFromString:(NSString *)string withGlobalObject:(JSObjectRef)globalObject inScriptWorld:(WebScriptWorld *)world;
- (JSGlobalContextRef)_globalContextForScriptWorld:(WebScriptWorld *)world;

#if JSC_OBJC_API_ENABLED
- (JSContext *)_javaScriptContextForScriptWorld:(WebScriptWorld *)world;
#endif

- (void)resetTextAutosizingBeforeLayout;
- (void)_setVisibleSize:(CGSize)size;
- (void)_setTextAutosizingWidth:(CGFloat)width;

#if TARGET_OS_IPHONE
- (void)_replaceSelectionWithWebArchive:(WebArchive *)webArchive selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;
#endif

- (void)_replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)_replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;
- (void)_replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;

#if !TARGET_OS_IPHONE
- (void)_smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)rangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString;
#endif

- (NSMutableDictionary *)_cacheabilityDictionary;

- (BOOL)_allowsFollowingLink:(NSURL *)URL;

#if !TARGET_OS_IPHONE
// Sets whether the scrollbars, if any, should be shown inside the document's border 
// (thus overlapping some content) or outside the webView's border (default behavior). 
// Changing this flag changes the size of the contentView and maintains the size of the frameView.
- (void)setAllowsScrollersToOverlapContent:(BOOL)flag;

// Sets if the scrollbar is always hidden, regardless of other scrollbar visibility settings. 
// This does not affect the scrollability of the document.
- (void)setAlwaysHideHorizontalScroller:(BOOL)flag;
- (void)setAlwaysHideVerticalScroller:(BOOL)flag;
#endif

// Sets the name presented to accessibility clients for the web area object.
- (void)setAccessibleName:(NSString *)name;

- (NSString*)_layerTreeAsText;

// The top of the accessibility tree.
- (id)accessibilityRoot;

// Clears frame opener. This is executed between layout tests runs
- (void)_clearOpener;

// Printing.
- (NSArray *)_computePageRectsWithPrintScaleFactor:(float)printWidthScaleFactor pageSize:(NSSize)pageSize;

#if TARGET_OS_IPHONE
- (DOMDocumentFragment *)_documentFragmentForText:(NSString *)text;
// These have the side effect of adding subresources to our WebDataSource where appropriate.
- (DOMDocumentFragment *)_documentFragmentForWebArchive:(WebArchive *)webArchive;
- (DOMDocumentFragment *)_documentFragmentForImageData:(NSData *)data withRelativeURLPart:(NSString *)relativeURLPart andMIMEType:(NSString *)mimeType;

- (BOOL)focusedNodeHasContent;

- (void)_dispatchDidReceiveTitle:(NSString *)title;
- (void)removeUnchangeableStyles;
- (BOOL)hasRichlyEditableSelection;
#endif

- (JSValueRef)jsWrapperForNode:(DOMNode *)node inScriptWorld:(WebScriptWorld *)world;

- (NSDictionary *)elementAtPoint:(NSPoint)point;

- (NSURL *)_unreachableURL;
@end

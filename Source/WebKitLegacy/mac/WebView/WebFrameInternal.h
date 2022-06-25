/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

// This header contains WebFrame declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import "WebFramePrivate.h"
#import "WebPreferencesPrivate.h"
#import <WebCore/EditAction.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/FrameSelection.h>
#import <WebCore/Position.h>
#import <WebCore/Settings.h>
#import <wtf/NakedPtr.h>

@class DOMCSSStyleDeclaration;
@class DOMDocumentFragment;
@class DOMElement;
@class DOMNode;
@class DOMRange;
@class WebFrameView;
@class WebHistoryItem;

class WebScriptDebugger;

namespace WebCore {
    class CSSStyleDeclaration;
    class Document;
    class DocumentLoader;
    class Element;
    class Frame;
    class Frame;
    class HistoryItem;
    class HTMLElement;
    class HTMLFrameOwnerElement;
    class Node;
    class Page;
    class Range;
}

typedef WebCore::HistoryItem WebCoreHistoryItem;

enum class WebRangeIsRelativeTo : uint8_t {
    EditableRoot,
    Paragraph,
};

WebCore::Frame* core(WebFrame *);
WebFrame *kit(WebCore::Frame *);

WebCore::Page* core(WebView *);
WebView *kit(WebCore::Page*);

WebCore::EditableLinkBehavior core(WebKitEditableLinkBehavior);
WebCore::TextDirectionSubmenuInclusionBehavior core(WebTextDirectionSubmenuInclusionBehavior);

#if defined(__cplusplus) && PLATFORM(IOS_FAMILY)
Vector<Vector<String>> vectorForDictationPhrasesArray(NSArray *);
#endif

WebView *getWebView(WebFrame *webFrame);

@interface WebFramePrivate : NSObject {
@public
    NakedPtr<WebCore::Frame> coreFrame;
    RetainPtr<WebFrameView> webFrameView;
    std::unique_ptr<WebScriptDebugger> scriptDebugger;
    id internalLoadDelegate;
    BOOL shouldCreateRenderers;
    BOOL includedInWebKitStatistics;
    RetainPtr<NSString> url;
    RetainPtr<NSString> provisionalURL;
#if PLATFORM(IOS_FAMILY)
    BOOL isCommitting;
#endif
}
@end

@protocol WebCoreRenderTreeCopier <NSObject>
- (NSObject *)nodeWithName:(NSString *)name position:(NSPoint)position rect:(NSRect)rect view:(NSView *)view children:(NSArray *)children;
@end

@interface WebFrame (WebInternal)

+ (void)_createMainFrameWithPage:(WebCore::Page*)page frameName:(const WTF::AtomString&)name frameView:(WebFrameView *)frameView;
+ (Ref<WebCore::Frame>)_createSubframeWithOwnerElement:(WebCore::HTMLFrameOwnerElement*)ownerElement frameName:(const WTF::AtomString&)name frameView:(WebFrameView *)frameView;
- (id)_initWithWebFrameView:(WebFrameView *)webFrameView webView:(WebView *)webView;

- (void)_clearCoreFrame;

- (BOOL)_isIncludedInWebKitStatistics;

- (void)_updateBackgroundAndUpdatesWhileOffscreen;
- (void)_setInternalLoadDelegate:(id)internalLoadDelegate;
- (id)_internalLoadDelegate;
- (void)_unmarkAllBadGrammar;
- (void)_unmarkAllMisspellings;

- (BOOL)_hasSelection;
- (void)_clearSelection;
- (WebFrame *)_findFrameWithSelection;
- (void)_clearSelectionInOtherFrames;

- (void)_attachScriptDebugger;
- (void)_detachScriptDebugger;

// dataSource reports null for the initial empty document's data source; this is needed
// to preserve compatibility with Mail and Safari among others. But internal to WebKit,
// we need to be able to get the initial data source as well, so the _dataSource method
// should be used instead.
- (WebDataSource *)_dataSource;

#if PLATFORM(IOS_FAMILY)
+ (void)_createMainFrameWithSimpleHTMLDocumentWithPage:(WebCore::Page*)page frameView:(WebFrameView *)frameView style:(NSString *)style;

- (BOOL)_isCommitting;
- (void)_setIsCommitting:(BOOL)value;
#endif

- (BOOL)_needsLayout;
- (void)_drawRect:(NSRect)rect contentsOnly:(BOOL)contentsOnly;
- (BOOL)_getVisibleRect:(NSRect*)rect;

- (NSString *)_stringByEvaluatingJavaScriptFromString:(NSString *)string;
- (NSString *)_stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture;

- (NSString *)_selectedString;
- (NSString *)_stringForRange:(DOMRange *)range;

- (DOMRange *)_convertNSRangeToDOMRange:(NSRange)range;
- (NSRange)_convertDOMRangeToNSRange:(DOMRange *)range;

- (NSRect)_caretRectAtPosition:(const WebCore::Position&)pos affinity:(NSSelectionAffinity)affinity;
- (NSRect)_firstRectForDOMRange:(DOMRange *)range;
- (void)_scrollDOMRangeToVisible:(DOMRange *)range;
#if PLATFORM(IOS_FAMILY)
- (void)_scrollDOMRangeToVisible:(DOMRange *)range withInset:(CGFloat)inset;
#endif

#if !PLATFORM(IOS_FAMILY)
- (DOMRange *)_rangeByAlteringCurrentSelection:(WebCore::FrameSelection::EAlteration)alteration direction:(WebCore::SelectionDirection)direction granularity:(WebCore::TextGranularity)granularity;
#endif

- (NSRange)_convertToNSRange:(const WebCore::SimpleRange&)range;
- (std::optional<WebCore::SimpleRange>)_convertToDOMRange:(NSRange)range;
- (std::optional<WebCore::SimpleRange>)_convertToDOMRange:(NSRange)range rangeIsRelativeTo:(WebRangeIsRelativeTo)rangeIsRelativeTo;

- (DOMDocumentFragment *)_documentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString;
- (DOMDocumentFragment *)_documentFragmentWithNodesAsParagraphs:(NSArray *)nodes;

- (void)_replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;

- (void)_insertParagraphSeparatorInQuotedContent;

- (DOMRange *)_characterRangeAtPoint:(NSPoint)point;

- (DOMCSSStyleDeclaration *)_typingStyle;
- (void)_setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebCore::EditAction)undoAction;

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
- (void)_dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation;
#endif

- (BOOL)_canProvideDocumentSource;
- (BOOL)_canSaveAsWebArchive;

- (void)_commitData:(NSData *)data;

@end

@interface NSObject (WebInternalFrameLoadDelegate)
- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error;
@end

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

// This header contains WebInspector declarations that can be used anywhere in the Web Kit, but are neither SPI nor API.

#import <Foundation/NSObject.h>
#import <WebKit/WebInspector.h>
#import <WebKit/DOMCore.h>

@class WebFrame;
@class NSView;
@class NSPopUpButton;
@class NSSegmentedControl;
@class NSSearchField;
@class NSTabView;
@class NSTextField;
@class NSTableView;
@class NSOutlineView;
@class NSTextView;
@class NSWindow;
@class NSNotification;
@class WebNodeHighlight;
@class DOMCSSRuleList;

@interface WebInspectorPrivate : NSObject
{
@public
    WebView *webView;
    DOMHTMLDocument *domDocument;
    WebFrame *webFrame;
    DOMNode *rootNode;
    DOMNode *focusedNode;
    NSString *searchQuery;
    NSMutableSet *nodeCache;
    NSMutableArray *searchResults;
    NSScrollView *treeScrollView;
    NSOutlineView *treeOutlineView;
    DOMCSSRuleList *matchedRules;
    WebNodeHighlight *currentHighlight;
    NSImage *rightArrowImage;
    NSImage *downArrowImage;
    BOOL ignoreWhitespace;
    BOOL windowLoaded;
    BOOL webViewLoaded;
    BOOL searchResultsVisible;
    BOOL preventHighlight;
    BOOL preventRevealOnFocus;
    BOOL preventSelectionRefocus;
    BOOL isSharedInspector;
}
@end

@interface WebInspector (WebInspectorPrivate) <DOMEventListener>
- (void)_highlightNode:(DOMNode *)node;
- (void)_revealAndSelectNodeInTree:(DOMNode *)node;
- (void)_update;
- (void)_updateRoot;
- (void)_updateTreeScrollbar;
- (void)_showSearchResults:(BOOL)show;
- (void)_refreshSearch;
@end

@interface DOMHTMLElement (DOMHTMLElementInspectorAdditions)
- (void)_addClassName:(NSString *)name;
- (void)_removeClassName:(NSString *)name;
@end

@interface DOMNode (DOMNodeInspectorAdditions)
- (NSString *)_contentPreview;

- (BOOL)_isAncestorOfNode:(DOMNode *)node;
- (BOOL)_isDescendantOfNode:(DOMNode *)node;

- (BOOL)_isWhitespace;

- (unsigned long)_lengthOfChildNodesIgnoringWhitespace;
- (DOMNode *)_childNodeAtIndexIgnoringWhitespace:(unsigned long)nodeIndex;

- (DOMNode *)_nextSiblingSkippingWhitespace;
- (DOMNode *)_previousSiblingSkippingWhitespace;

- (DOMNode *)_firstChildSkippingWhitespace;
- (DOMNode *)_lastChildSkippingWhitespace;

- (DOMNode *)_firstAncestorCommonWithNode:(DOMNode *)node;

- (DOMNode *)_traverseNextNodeStayingWithin:(DOMNode *)stayWithin;
- (DOMNode *)_traverseNextNodeSkippingWhitespaceStayingWithin:(DOMNode *)stayWithin;
- (DOMNode *)_traversePreviousNode;
- (DOMNode *)_traversePreviousNodeSkippingWhitespace;

- (NSString *)_nodeTypeName;
- (NSString *)_shortDisplayName;
- (NSString *)_displayName;
@end

@interface NSWindow (NSWindowContentShadow)
- (void)_setContentHasShadow:(BOOL)hasShadow;
@end

/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <WebCore/EditAction.h>
#import <WebCore/SelectionController.h>
#import <WebCore/TextGranularity.h>

@class DOMCSSStyleDeclaration;
@class DOMDocument;
@class DOMDocumentFragment;
@class DOMElement;
@class DOMNode;
@class DOMRange;

@protocol WebCoreRenderTreeCopier <NSObject>
- (NSObject *)nodeWithName:(NSString *)name position:(NSPoint)position rect:(NSRect)rect view:(NSView *)view children:(NSArray *)children;
@end

// WebCoreFrameBridge objects are used by WebCore to abstract away operations that need
// to be implemented by library clients, for example WebKit. The objects are also
// used in the opposite direction, for simple access to WebCore functions without dealing
// directly with the KHTML C++ classes.

// A WebCoreFrameBridge creates and holds a reference to a Frame.

// The WebCoreFrameBridge interface contains methods for use by the non-WebCore side of the bridge.

@interface WebCoreFrameBridge : NSObject {
@public
    WebCore::Frame* m_frame;
    BOOL _shouldCreateRenderers;
    BOOL _closed;
}

- (void)setWebCoreFrame:(WebCore::Frame*)webCoreFrame;
- (WebCore::Frame*)_frame; // underscore to prevent conflict with -[NSView frame]

- (id)init;
- (void)close;

- (NSURL *)baseURL;

- (void)forceLayoutAdjustingViewSize:(BOOL)adjustSizeFlag;
- (void)forceLayoutWithMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)adjustSizeFlag;
- (void)sendScrollEvent;
- (BOOL)needsLayout;
- (void)drawRect:(NSRect)rect;
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit;
- (NSArray*)computePageRectsWithPrintWidthScaleFactor:(float)printWidthScaleFactor printHeight:(float)printHeight;

- (NSObject *)copyRenderTree:(id <WebCoreRenderTreeCopier>)copier;
- (NSString *)renderTreeAsExternalRepresentation;

- (NSURL *)URLWithAttributeString:(NSString *)string;

- (DOMElement *)elementWithName:(NSString *)name inForm:(DOMElement *)form;
- (BOOL)elementDoesAutoComplete:(DOMElement *)element;
- (BOOL)elementIsPassword:(DOMElement *)element;
- (DOMElement *)formForElement:(DOMElement *)element;
- (DOMElement *)currentForm;
- (NSArray *)controlsInForm:(DOMElement *)form;
- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(DOMElement *)element;
- (NSString *)matchLabels:(NSArray *)labels againstElement:(DOMElement *)element;

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection;
- (unsigned)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag limit:(unsigned)limit;
- (BOOL)markedTextMatchesAreHighlighted;
- (void)setMarkedTextMatchesAreHighlighted:(BOOL)doHighlight;
- (void)unmarkAllTextMatches;
- (NSArray *)rectsForTextMatches;

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string;
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture;
- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)string;

- (NSString *)selectedString;

- (NSString *)stringForRange:(DOMRange *)range;

- (NSString *)markupStringFromNode:(DOMNode *)node nodes:(NSArray **)nodes;
- (NSString *)markupStringFromRange:(DOMRange *)range nodes:(NSArray **)nodes;

- (NSRect)caretRectAtNode:(DOMNode *)node offset:(int)offset affinity:(NSSelectionAffinity)affinity;
- (NSRect)firstRectForDOMRange:(DOMRange *)range;
- (void)scrollDOMRangeToVisible:(DOMRange *)range;

- (NSFont *)fontForSelection:(BOOL *)hasMultipleFonts;

- (NSString *)stringWithData:(NSData *)data; // using the encoding of the frame's main resource
+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName; // nil for textEncodingName means Latin-1

- (void)setShouldCreateRenderers:(BOOL)shouldCreateRenderers;

- (void)setBaseBackgroundColor:(NSColor *)backgroundColor;
- (void)setDrawsBackground:(BOOL)drawsBackround;

- (id)accessibilityTree;

- (DOMRange *)rangeByAlteringCurrentSelection:(WebCore::SelectionController::EAlteration)alteration direction:(WebCore::SelectionController::EDirection)direction granularity:(WebCore::TextGranularity)granularity;
- (WebCore::TextGranularity)selectionGranularity;
- (void)smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)charRangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString;
- (void)selectNSRange:(NSRange)range;
- (NSRange)selectedNSRange;
- (NSRange)markedTextNSRange;
- (DOMRange *)convertNSRangeToDOMRange:(NSRange)range;
- (NSRange)convertDOMRangeToNSRange:(DOMRange *)range;

- (DOMDocumentFragment *)documentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString;
- (DOMDocumentFragment *)documentFragmentWithText:(NSString *)text inContext:(DOMRange *)context;
- (DOMDocumentFragment *)documentFragmentWithNodesAsParagraphs:(NSArray *)nodes;

- (void)replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;
- (void)replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;

- (void)insertParagraphSeparatorInQuotedContent;

- (DOMRange *)characterRangeAtPoint:(NSPoint)point;

- (DOMCSSStyleDeclaration *)typingStyle;
- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebCore::EditAction)undoAction;

- (void)dragSourceMovedTo:(NSPoint)windowLoc;
- (void)dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation;

- (BOOL)getData:(NSData **)data andResponse:(NSURLResponse **)response forURL:(NSString *)URL;
- (void)getAllResourceDatas:(NSArray **)datas andResponses:(NSArray **)responses;

- (BOOL)canProvideDocumentSource;
- (BOOL)canSaveAsWebArchive;

- (void)receivedData:(NSData *)data textEncodingName:(NSString *)textEncodingName;

@end

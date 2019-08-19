/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKViewInternal.h"

#if PLATFORM(MAC)

#import "APIHitTestResult.h"
#import "APIIconLoadingClient.h"
#import "APIPageConfiguration.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKDragDestinationAction.h"
#import "WKNSData.h"
#import "WKProcessGroupPrivate.h"
#import "WebBackForwardListItem.h"
#import "WebKit2Initialize.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebPreferencesKeys.h"
#import "WebProcessPool.h"
#import "WebViewImpl.h"
#import "_WKLinkIconParametersInternal.h"
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/BlockPtr.h>

@interface WKViewData : NSObject {
@public
    std::unique_ptr<WebKit::WebViewImpl> _impl;
}

@end

@implementation WKViewData
@end

@interface WKView () <WebViewImplDelegate>
@end

#if HAVE(TOUCH_BAR)
@interface WKView () <NSTouchBarProvider>
@end
#endif

#if ENABLE(DRAG_SUPPORT)

@interface WKView () <NSFilePromiseProviderDelegate, NSDraggingSource>
@end

#endif

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKView
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (id)initWithFrame:(NSRect)frame processGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup
{
    return [self initWithFrame:frame contextRef:processGroup._contextRef pageGroupRef:browsingContextGroup._pageGroupRef relatedToPage:nil];
}

- (id)initWithFrame:(NSRect)frame processGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup relatedToView:(WKView *)relatedView
{
    return [self initWithFrame:frame contextRef:processGroup._contextRef pageGroupRef:browsingContextGroup._pageGroupRef relatedToPage:relatedView ? relatedView.pageRef : nil];
}

- (void)dealloc
{
    _data->_impl->page().setIconLoadingClient(nullptr);
    _data->_impl = nullptr;

    [_data release];
    _data = nil;

    [super dealloc];
}

- (WKBrowsingContextController *)browsingContextController
{
    return _data->_impl->browsingContextController();
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    _data->_impl->setDrawsBackground(drawsBackground);
}

- (BOOL)drawsBackground
{
    return _data->_impl->drawsBackground();
}

- (NSColor *)_backgroundColor
{
    return _data->_impl->backgroundColor();
}

- (void)_setBackgroundColor:(NSColor *)backgroundColor
{
    _data->_impl->setBackgroundColor(backgroundColor);
}

- (void)setDrawsTransparentBackground:(BOOL)drawsTransparentBackground
{
    _data->_impl->setDrawsBackground(!drawsTransparentBackground);
}

- (BOOL)drawsTransparentBackground
{
    return !_data->_impl->drawsBackground();
}

- (BOOL)acceptsFirstResponder
{
    return _data->_impl->acceptsFirstResponder();
}

- (BOOL)becomeFirstResponder
{
    return _data->_impl->becomeFirstResponder();
}

- (BOOL)resignFirstResponder
{
    return _data->_impl->resignFirstResponder();
}

- (void)viewWillStartLiveResize
{
    _data->_impl->viewWillStartLiveResize();
}

- (void)viewDidEndLiveResize
{
    _data->_impl->viewDidEndLiveResize();
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSSize)intrinsicContentSize
{
    return NSSizeFromCGSize(_data->_impl->intrinsicContentSize());
}

- (void)prepareContentInRect:(NSRect)rect
{
    _data->_impl->prepareContentInRect(NSRectToCGRect(rect));
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    _data->_impl->setFrameSize(NSSizeToCGSize(size));
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)renewGState
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    _data->_impl->renewGState();
    [super renewGState];
}

#define WEBCORE_COMMAND(command) - (void)command:(id)sender { _data->_impl->executeEditCommandForSelector(_cmd); }

WEBCORE_COMMAND(alignCenter)
WEBCORE_COMMAND(alignJustified)
WEBCORE_COMMAND(alignLeft)
WEBCORE_COMMAND(alignRight)
WEBCORE_COMMAND(copy)
WEBCORE_COMMAND(cut)
WEBCORE_COMMAND(delete)
WEBCORE_COMMAND(deleteBackward)
WEBCORE_COMMAND(deleteBackwardByDecomposingPreviousCharacter)
WEBCORE_COMMAND(deleteForward)
WEBCORE_COMMAND(deleteToBeginningOfLine)
WEBCORE_COMMAND(deleteToBeginningOfParagraph)
WEBCORE_COMMAND(deleteToEndOfLine)
WEBCORE_COMMAND(deleteToEndOfParagraph)
WEBCORE_COMMAND(deleteToMark)
WEBCORE_COMMAND(deleteWordBackward)
WEBCORE_COMMAND(deleteWordForward)
WEBCORE_COMMAND(ignoreSpelling)
WEBCORE_COMMAND(indent)
WEBCORE_COMMAND(insertBacktab)
WEBCORE_COMMAND(insertLineBreak)
WEBCORE_COMMAND(insertNewline)
WEBCORE_COMMAND(insertNewlineIgnoringFieldEditor)
WEBCORE_COMMAND(insertParagraphSeparator)
WEBCORE_COMMAND(insertTab)
WEBCORE_COMMAND(insertTabIgnoringFieldEditor)
WEBCORE_COMMAND(makeTextWritingDirectionLeftToRight)
WEBCORE_COMMAND(makeTextWritingDirectionNatural)
WEBCORE_COMMAND(makeTextWritingDirectionRightToLeft)
WEBCORE_COMMAND(moveBackward)
WEBCORE_COMMAND(moveBackwardAndModifySelection)
WEBCORE_COMMAND(moveDown)
WEBCORE_COMMAND(moveDownAndModifySelection)
WEBCORE_COMMAND(moveForward)
WEBCORE_COMMAND(moveForwardAndModifySelection)
WEBCORE_COMMAND(moveLeft)
WEBCORE_COMMAND(moveLeftAndModifySelection)
WEBCORE_COMMAND(moveParagraphBackwardAndModifySelection)
WEBCORE_COMMAND(moveParagraphForwardAndModifySelection)
WEBCORE_COMMAND(moveRight)
WEBCORE_COMMAND(moveRightAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfDocument)
WEBCORE_COMMAND(moveToBeginningOfDocumentAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfLine)
WEBCORE_COMMAND(moveToBeginningOfLineAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfParagraph)
WEBCORE_COMMAND(moveToBeginningOfParagraphAndModifySelection)
WEBCORE_COMMAND(moveToBeginningOfSentence)
WEBCORE_COMMAND(moveToBeginningOfSentenceAndModifySelection)
WEBCORE_COMMAND(moveToEndOfDocument)
WEBCORE_COMMAND(moveToEndOfDocumentAndModifySelection)
WEBCORE_COMMAND(moveToEndOfLine)
WEBCORE_COMMAND(moveToEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveToEndOfParagraph)
WEBCORE_COMMAND(moveToEndOfParagraphAndModifySelection)
WEBCORE_COMMAND(moveToEndOfSentence)
WEBCORE_COMMAND(moveToEndOfSentenceAndModifySelection)
WEBCORE_COMMAND(moveToLeftEndOfLine)
WEBCORE_COMMAND(moveToLeftEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveToRightEndOfLine)
WEBCORE_COMMAND(moveToRightEndOfLineAndModifySelection)
WEBCORE_COMMAND(moveUp)
WEBCORE_COMMAND(moveUpAndModifySelection)
WEBCORE_COMMAND(moveWordBackward)
WEBCORE_COMMAND(moveWordBackwardAndModifySelection)
WEBCORE_COMMAND(moveWordForward)
WEBCORE_COMMAND(moveWordForwardAndModifySelection)
WEBCORE_COMMAND(moveWordLeft)
WEBCORE_COMMAND(moveWordLeftAndModifySelection)
WEBCORE_COMMAND(moveWordRight)
WEBCORE_COMMAND(moveWordRightAndModifySelection)
WEBCORE_COMMAND(outdent)
WEBCORE_COMMAND(pageDown)
WEBCORE_COMMAND(pageDownAndModifySelection)
WEBCORE_COMMAND(pageUp)
WEBCORE_COMMAND(pageUpAndModifySelection)
WEBCORE_COMMAND(paste)
WEBCORE_COMMAND(pasteAsPlainText)
WEBCORE_COMMAND(scrollPageDown)
WEBCORE_COMMAND(scrollPageUp)
WEBCORE_COMMAND(scrollLineDown)
WEBCORE_COMMAND(scrollLineUp)
WEBCORE_COMMAND(scrollToBeginningOfDocument)
WEBCORE_COMMAND(scrollToEndOfDocument)
WEBCORE_COMMAND(selectAll)
WEBCORE_COMMAND(selectLine)
WEBCORE_COMMAND(selectParagraph)
WEBCORE_COMMAND(selectSentence)
WEBCORE_COMMAND(selectToMark)
WEBCORE_COMMAND(selectWord)
WEBCORE_COMMAND(setMark)
WEBCORE_COMMAND(subscript)
WEBCORE_COMMAND(superscript)
WEBCORE_COMMAND(swapWithMark)
WEBCORE_COMMAND(takeFindStringFromSelection)
WEBCORE_COMMAND(transpose)
WEBCORE_COMMAND(underline)
WEBCORE_COMMAND(unscript)
WEBCORE_COMMAND(yank)
WEBCORE_COMMAND(yankAndSelect)

#undef WEBCORE_COMMAND

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    return _data->_impl->writeSelectionToPasteboard(pasteboard, types);
}

- (void)centerSelectionInVisibleArea:(id)sender 
{ 
    _data->_impl->centerSelectionInVisibleArea();
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    return _data->_impl->validRequestorForSendAndReturnTypes(sendType, returnType);
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pasteboard 
{
    return _data->_impl->readSelectionFromPasteboard(pasteboard);
}

- (void)changeFont:(id)sender
{
    _data->_impl->changeFontFromFontManager();
}

/*

When possible, editing-related methods should be implemented in WebCore with the
EditorCommand mechanism and invoked via WEBCORE_COMMAND, rather than implementing
individual methods here with Mac-specific code.

Editing-related methods still unimplemented that are implemented in WebKit1:

- (void)complete:(id)sender;
- (void)copyFont:(id)sender;
- (void)makeBaseWritingDirectionLeftToRight:(id)sender;
- (void)makeBaseWritingDirectionNatural:(id)sender;
- (void)makeBaseWritingDirectionRightToLeft:(id)sender;
- (void)pasteFont:(id)sender;
- (void)scrollLineDown:(id)sender;
- (void)scrollLineUp:(id)sender;
- (void)showGuessPanel:(id)sender;

Some other editing-related methods still unimplemented:

- (void)changeCaseOfLetter:(id)sender;
- (void)copyRuler:(id)sender;
- (void)insertContainerBreak:(id)sender;
- (void)insertDoubleQuoteIgnoringSubstitution:(id)sender;
- (void)insertSingleQuoteIgnoringSubstitution:(id)sender;
- (void)pasteRuler:(id)sender;
- (void)toggleRuler:(id)sender;
- (void)transposeWords:(id)sender;

*/

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    return _data->_impl->validateUserInterfaceItem(item);
}

- (IBAction)startSpeaking:(id)sender
{
    _data->_impl->startSpeaking();
}

- (IBAction)stopSpeaking:(id)sender
{
    _data->_impl->stopSpeaking(sender);
}

- (IBAction)showGuessPanel:(id)sender
{
    _data->_impl->showGuessPanel(sender);
}

- (IBAction)checkSpelling:(id)sender
{
    _data->_impl->checkSpelling();
}

- (void)changeSpelling:(id)sender
{
    _data->_impl->changeSpelling(sender);
}

- (IBAction)toggleContinuousSpellChecking:(id)sender
{
    _data->_impl->toggleContinuousSpellChecking();
}

- (BOOL)isGrammarCheckingEnabled
{
    return _data->_impl->isGrammarCheckingEnabled();
}

- (void)setGrammarCheckingEnabled:(BOOL)flag
{
    _data->_impl->setGrammarCheckingEnabled(flag);
}

- (IBAction)toggleGrammarChecking:(id)sender
{
    _data->_impl->toggleGrammarChecking();
}

- (IBAction)toggleAutomaticSpellingCorrection:(id)sender
{
    _data->_impl->toggleAutomaticSpellingCorrection();
}

- (void)orderFrontSubstitutionsPanel:(id)sender
{
    _data->_impl->orderFrontSubstitutionsPanel(sender);
}

- (IBAction)toggleSmartInsertDelete:(id)sender
{
    _data->_impl->toggleSmartInsertDelete();
}

- (BOOL)isAutomaticQuoteSubstitutionEnabled
{
    return _data->_impl->isAutomaticQuoteSubstitutionEnabled();
}

- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag
{
    _data->_impl->setAutomaticQuoteSubstitutionEnabled(flag);
}

- (void)toggleAutomaticQuoteSubstitution:(id)sender
{
    _data->_impl->toggleAutomaticQuoteSubstitution();
}

- (BOOL)isAutomaticDashSubstitutionEnabled
{
    return _data->_impl->isAutomaticDashSubstitutionEnabled();
}

- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag
{
    _data->_impl->setAutomaticDashSubstitutionEnabled(flag);
}

- (void)toggleAutomaticDashSubstitution:(id)sender
{
    _data->_impl->toggleAutomaticDashSubstitution();
}

- (BOOL)isAutomaticLinkDetectionEnabled
{
    return _data->_impl->isAutomaticLinkDetectionEnabled();
}

- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag
{
    _data->_impl->setAutomaticLinkDetectionEnabled(flag);
}

- (void)toggleAutomaticLinkDetection:(id)sender
{
    _data->_impl->toggleAutomaticLinkDetection();
}

- (BOOL)isAutomaticTextReplacementEnabled
{
    return _data->_impl->isAutomaticTextReplacementEnabled();
}

- (void)setAutomaticTextReplacementEnabled:(BOOL)flag
{
    _data->_impl->setAutomaticTextReplacementEnabled(flag);
}

- (void)toggleAutomaticTextReplacement:(id)sender
{
    _data->_impl->toggleAutomaticTextReplacement();
}

- (void)uppercaseWord:(id)sender
{
    _data->_impl->uppercaseWord();
}

- (void)lowercaseWord:(id)sender
{
    _data->_impl->lowercaseWord();
}

- (void)capitalizeWord:(id)sender
{
    _data->_impl->capitalizeWord();
}

- (BOOL)_wantsKeyDownForEvent:(NSEvent *)event
{
    return _data->_impl->wantsKeyDownForEvent(event);
}

- (void)scrollWheel:(NSEvent *)event
{
    _data->_impl->scrollWheel(event);
}

- (void)swipeWithEvent:(NSEvent *)event
{
    _data->_impl->swipeWithEvent(event);
}

- (void)mouseMoved:(NSEvent *)event
{
    _data->_impl->mouseMoved(event);
}

- (void)mouseDown:(NSEvent *)event
{
    _data->_impl->mouseDown(event);
}

- (void)mouseUp:(NSEvent *)event
{
    _data->_impl->mouseUp(event);
}

- (void)mouseDragged:(NSEvent *)event
{
    _data->_impl->mouseDragged(event);
}

- (void)mouseEntered:(NSEvent *)event
{
    _data->_impl->mouseEntered(event);
}

- (void)mouseExited:(NSEvent *)event
{
    _data->_impl->mouseExited(event);
}

- (void)otherMouseDown:(NSEvent *)event
{
    _data->_impl->otherMouseDown(event);
}

- (void)otherMouseDragged:(NSEvent *)event
{
    _data->_impl->otherMouseDragged(event);
}

- (void)otherMouseUp:(NSEvent *)event
{
    _data->_impl->otherMouseUp(event);
}

- (void)rightMouseDown:(NSEvent *)event
{
    _data->_impl->rightMouseDown(event);
}

- (void)rightMouseDragged:(NSEvent *)event
{
    _data->_impl->rightMouseDragged(event);
}

- (void)rightMouseUp:(NSEvent *)event
{
    _data->_impl->rightMouseUp(event);
}

- (void)pressureChangeWithEvent:(NSEvent *)event
{
    _data->_impl->pressureChangeWithEvent(event);
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return _data->_impl->acceptsFirstMouse(event);
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    return _data->_impl->shouldDelayWindowOrderingForEvent(event);
}

- (void)doCommandBySelector:(SEL)selector
{
    _data->_impl->doCommandBySelector(selector);
}

- (void)insertText:(id)string
{
    _data->_impl->insertText(string);
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    _data->_impl->insertText(string, replacementRange);
}

- (NSTextInputContext *)inputContext
{
    return _data->_impl->inputContext();
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    return _data->_impl->performKeyEquivalent(event);
}

- (void)keyUp:(NSEvent *)theEvent
{
    _data->_impl->keyUp(theEvent);
}

- (void)keyDown:(NSEvent *)theEvent
{
    _data->_impl->keyDown(theEvent);
}

- (void)flagsChanged:(NSEvent *)theEvent
{
    _data->_impl->flagsChanged(theEvent);
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelectedRange replacementRange:(NSRange)replacementRange
{
    _data->_impl->setMarkedText(string, newSelectedRange, replacementRange);
}

- (void)unmarkText
{
    _data->_impl->unmarkText();
}

- (NSRange)selectedRange
{
    return _data->_impl->selectedRange();
}

- (BOOL)hasMarkedText
{
    return _data->_impl->hasMarkedText();
}

- (NSRange)markedRange
{
    return _data->_impl->markedRange();
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)nsRange actualRange:(NSRangePointer)actualRange
{
    return _data->_impl->attributedSubstringForProposedRange(nsRange, actualRange);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
    return _data->_impl->characterIndexForPoint(thePoint);
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange actualRange:(NSRangePointer)actualRange
{
    return _data->_impl->firstRectForCharacterRange(theRange, actualRange);
}

- (void)selectedRangeWithCompletionHandler:(void(^)(NSRange selectedRange))completionHandlerPtr
{
    _data->_impl->selectedRangeWithCompletionHandler(completionHandlerPtr);
}

- (void)markedRangeWithCompletionHandler:(void(^)(NSRange markedRange))completionHandlerPtr
{
    _data->_impl->markedRangeWithCompletionHandler(completionHandlerPtr);
}

- (void)hasMarkedTextWithCompletionHandler:(void(^)(BOOL hasMarkedText))completionHandlerPtr
{
    _data->_impl->hasMarkedTextWithCompletionHandler(completionHandlerPtr);
}

- (void)attributedSubstringForProposedRange:(NSRange)nsRange completionHandler:(void(^)(NSAttributedString *attrString, NSRange actualRange))completionHandlerPtr
{
    _data->_impl->attributedSubstringForProposedRange(nsRange, completionHandlerPtr);
}

- (void)firstRectForCharacterRange:(NSRange)theRange completionHandler:(void(^)(NSRect firstRect, NSRange actualRange))completionHandlerPtr
{
    _data->_impl->firstRectForCharacterRange(theRange, completionHandlerPtr);
}

- (void)characterIndexForPoint:(NSPoint)thePoint completionHandler:(void(^)(NSUInteger))completionHandlerPtr
{
    _data->_impl->characterIndexForPoint(thePoint, completionHandlerPtr);
}

- (NSArray *)validAttributesForMarkedText
{
    return _data->_impl->validAttributesForMarkedText();
}

#if ENABLE(DRAG_SUPPORT)
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)draggedImage:(NSImage *)image endedAt:(NSPoint)endPoint operation:(NSDragOperation)operation
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    _data->_impl->draggedImage(image, NSPointToCGPoint(endPoint), operation);
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)draggingInfo
{
    return _data->_impl->draggingEntered(draggingInfo);
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)draggingInfo
{
    return _data->_impl->draggingUpdated(draggingInfo);
}

- (void)draggingExited:(id <NSDraggingInfo>)draggingInfo
{
    _data->_impl->draggingExited(draggingInfo);
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    return _data->_impl->prepareForDragOperation(draggingInfo);
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    return _data->_impl->performDragOperation(draggingInfo);
}

- (NSView *)_hitTest:(NSPoint *)point dragTypes:(NSSet *)types
{
    return _data->_impl->hitTestForDragTypes(NSPointToCGPoint(*point), types);
}
#endif // ENABLE(DRAG_SUPPORT)

- (BOOL)_windowResizeMouseLocationIsInVisibleScrollerThumb:(NSPoint)point
{
    return _data->_impl->windowResizeMouseLocationIsInVisibleScrollerThumb(NSPointToCGPoint(point));
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    _data->_impl->viewWillMoveToWindow(window);
}

- (void)viewDidMoveToWindow
{
    _data->_impl->viewDidMoveToWindow();
}

- (void)drawRect:(NSRect)rect
{
    _data->_impl->drawRect(NSRectToCGRect(rect));
}

- (BOOL)isOpaque
{
    return _data->_impl->isOpaque();
}

- (BOOL)mouseDownCanMoveWindow
{
    return WebKit::WebViewImpl::mouseDownCanMoveWindow();
}

- (void)viewDidHide
{
    _data->_impl->viewDidHide();
}

- (void)viewDidUnhide
{
    _data->_impl->viewDidUnhide();
}

- (void)viewDidChangeBackingProperties
{
    _data->_impl->viewDidChangeBackingProperties();
}

- (void)_activeSpaceDidChange:(NSNotification *)notification
{
    _data->_impl->activeSpaceDidChange();
}

- (id)accessibilityFocusedUIElement
{
    return _data->_impl->accessibilityFocusedUIElement();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsIgnored
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return _data->_impl->accessibilityIsIgnored();
}

- (id)accessibilityHitTest:(NSPoint)point
{
    return _data->_impl->accessibilityHitTest(NSPointToCGPoint(point));
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return _data->_impl->accessibilityAttributeValue(attribute);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return _data->_impl->accessibilityAttributeValue(attribute, parameter);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray<NSString *> *)accessibilityParameterizedAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    NSArray<NSString *> *names = [super accessibilityParameterizedAttributeNames];
    return [names arrayByAddingObject:@"AXConvertRelativeFrame"];
}

- (NSView *)hitTest:(NSPoint)point
{
    if (!_data)
        return [super hitTest:point];
    return _data->_impl->hitTest(NSPointToCGPoint(point));
}

- (NSInteger)conversationIdentifier
{
    return (NSInteger)self;
}

- (void)quickLookWithEvent:(NSEvent *)event
{
    _data->_impl->quickLookWithEvent(event);
}

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    return _data->_impl->addTrackingRect(NSRectToCGRect(rect), owner, data, assumeInside);
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    return _data->_impl->addTrackingRectWithTrackingNum(NSRectToCGRect(rect), owner, data, assumeInside, tag);
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
    CGRect *cgRects = (CGRect *)calloc(1, sizeof(CGRect));
    for (int i = 0; i < count; i++)
        cgRects[i] = NSRectToCGRect(rects[i]);
    _data->_impl->addTrackingRectsWithTrackingNums(cgRects, owner, userDataList, assumeInsideList, trackingNums, count);
    free(cgRects);
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    if (!_data)
        return;
    _data->_impl->removeTrackingRect(tag);
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
    if (!_data)
        return;
    _data->_impl->removeTrackingRects(tags, count);
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return _data->_impl->stringForToolTip(tag);
}

- (void)pasteboardChangedOwner:(NSPasteboard *)pasteboard
{
    _data->_impl->pasteboardChangedOwner(pasteboard);
}

- (void)pasteboard:(NSPasteboard *)pasteboard provideDataForType:(NSString *)type
{
    _data->_impl->provideDataForPasteboard(pasteboard, type);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return _data->_impl->namesOfPromisedFilesDroppedAtDestination(dropDestination);
}

- (void)maybeInstallIconLoadingClient
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    class IconLoadingClient : public API::IconLoadingClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit IconLoadingClient(WKView *wkView)
            : m_wkView(wkView)
        {
        }

        static SEL delegateSelector()
        {
            return @selector(_shouldLoadIconWithParameters:completionHandler:);
        }

    private:
        typedef void (^IconLoadCompletionHandler)(NSData*);

        void getLoadDecisionForIcon(const WebCore::LinkIcon& linkIcon, WTF::CompletionHandler<void(WTF::Function<void(API::Data*, WebKit::CallbackBase::Error)>&&)>&& completionHandler) override
        {
            RetainPtr<_WKLinkIconParameters> parameters = adoptNS([[_WKLinkIconParameters alloc] _initWithLinkIcon:linkIcon]);

            [m_wkView _shouldLoadIconWithParameters:parameters.get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](IconLoadCompletionHandler loadCompletionHandler) mutable {
                ASSERT(RunLoop::isMain());
                if (loadCompletionHandler) {
                    completionHandler([loadCompletionHandler = BlockPtr<void (NSData *)>(loadCompletionHandler)](API::Data* data, WebKit::CallbackBase::Error error) {
                        if (error != WebKit::CallbackBase::Error::None || !data)
                            loadCompletionHandler(nil);
                        else
                            loadCompletionHandler(wrapper(*data));
                    });
                } else
                    completionHandler(nullptr);
            }).get()];
        }

        WKView *m_wkView;
    };
    ALLOW_DEPRECATED_DECLARATIONS_END

    if ([self respondsToSelector:IconLoadingClient::delegateSelector()])
        _data->_impl->page().setIconLoadingClient(makeUnique<IconLoadingClient>(self));
}

- (instancetype)initWithFrame:(NSRect)frame processPool:(WebKit::WebProcessPool&)processPool configuration:(Ref<API::PageConfiguration>&&)configuration
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    WebKit::InitializeWebKit2();

    _data = [[WKViewData alloc] init];
    _data->_impl = makeUnique<WebKit::WebViewImpl>(self, nullptr, processPool, WTFMove(configuration));

    [self maybeInstallIconLoadingClient];

    return self;
}

- (void)_setThumbnailView:(_WKThumbnailView *)thumbnailView
{
    _data->_impl->setThumbnailView(thumbnailView);
}

- (_WKThumbnailView *)_thumbnailView
{
    if (!_data->_impl)
        return nil;
    return _data->_impl->thumbnailView();
}

- (NSTextInputContext *)_web_superInputContext
{
    return [super inputContext];
}

- (void)_web_superQuickLookWithEvent:(NSEvent *)event
{
    [super quickLookWithEvent:event];
}

- (void)_web_superSwipeWithEvent:(NSEvent *)event
{
    [super swipeWithEvent:event];
}

- (void)_web_superMagnifyWithEvent:(NSEvent *)event
{
    [super magnifyWithEvent:event];
}

- (void)_web_superSmartMagnifyWithEvent:(NSEvent *)event
{
    [super smartMagnifyWithEvent:event];
}

- (void)_web_superRemoveTrackingRect:(NSTrackingRectTag)tag
{
    [super removeTrackingRect:tag];
}

- (id)_web_superAccessibilityAttributeValue:(NSString *)attribute
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [super accessibilityAttributeValue:attribute];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)_web_superDoCommandBySelector:(SEL)selector
{
    [super doCommandBySelector:selector];
}

- (BOOL)_web_superPerformKeyEquivalent:(NSEvent *)event
{
    return [super performKeyEquivalent:event];
}

- (void)_web_superKeyDown:(NSEvent *)event
{
    [super keyDown:event];
}

- (NSView *)_web_superHitTest:(NSPoint)point
{
    return [super hitTest:point];
}

- (id)_web_immediateActionAnimationControllerForHitTestResultInternal:(API::HitTestResult*)hitTestResult withType:(uint32_t)type userData:(API::Object*)userData
{
    return [self _immediateActionAnimationControllerForHitTestResult:WebKit::toAPI(hitTestResult) withType:type userData:WebKit::toAPI(userData)];
}

- (void)_web_prepareForImmediateActionAnimation
{
    [self _prepareForImmediateActionAnimation];
}

- (void)_web_cancelImmediateActionAnimation
{
    [self _cancelImmediateActionAnimation];
}

- (void)_web_completeImmediateActionAnimation
{
    [self _completeImmediateActionAnimation];
}

- (void)_web_didChangeContentSize:(NSSize)newSize
{
    [self _didChangeContentSize:newSize];
}

#if ENABLE(DRAG_SUPPORT)

- (void)_web_didPerformDragOperation:(BOOL)handled
{
    UNUSED_PARAM(handled);
}

- (WKDragDestinationAction)_web_dragDestinationActionForDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    return WKDragDestinationActionAny;
}

#endif

- (void)_web_dismissContentRelativeChildWindows
{
    [self _dismissContentRelativeChildWindows];
}

- (void)_web_dismissContentRelativeChildWindowsWithAnimation:(BOOL)withAnimation
{
    [self _dismissContentRelativeChildWindowsWithAnimation:withAnimation];
}

- (void)_web_editorStateDidChange
{
}

- (void)_web_gestureEventWasNotHandledByWebCore:(NSEvent *)event
{
    [self _gestureEventWasNotHandledByWebCore:event];
}

- (void)_didHandleAcceptedCandidate
{
}

- (void)_didUpdateCandidateListVisibility:(BOOL)visible
{
}

#if HAVE(TOUCH_BAR)

@dynamic touchBar;

- (NSTouchBar *)makeTouchBar
{
    return _data->_impl->makeTouchBar();
}

- (NSCandidateListTouchBarItem *)candidateListTouchBarItem
{
    return _data->_impl->candidateListTouchBarItem();
}

- (void)_web_didAddMediaControlsManager:(id)controlsManager
{
    [self _addMediaPlaybackControlsView:controlsManager];
}

- (void)_web_didRemoveMediaControlsManager
{
    [self _removeMediaPlaybackControlsView];
}

#endif // HAVE(TOUCH_BAR)

#if ENABLE(DRAG_SUPPORT)

- (NSString *)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider fileNameForType:(NSString *)fileType
{
    return _data->_impl->fileNameForFilePromiseProvider(filePromiseProvider, fileType);
}

- (void)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider writePromiseToURL:(NSURL *)url completionHandler:(void (^)(NSError *error))completionHandler
{
    _data->_impl->writeToURLForFilePromiseProvider(filePromiseProvider, url, completionHandler);
}

- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
    return _data->_impl->dragSourceOperationMask(session, context);
}

- (void)draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation
{
    _data->_impl->draggingSessionEnded(session, screenPoint, operation);
}

#endif // ENABLE(DRAG_SUPPORT)

@end

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKView (Private)
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (void)saveBackForwardSnapshotForCurrentItem
{
    _data->_impl->saveBackForwardSnapshotForCurrentItem();
}

- (void)saveBackForwardSnapshotForItem:(WKBackForwardListItemRef)item
{
    _data->_impl->saveBackForwardSnapshotForItem(*WebKit::toImpl(item));
}

- (id)initWithFrame:(NSRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef
{
    return [self initWithFrame:frame contextRef:contextRef pageGroupRef:pageGroupRef relatedToPage:nil];
}

#if PLATFORM(MAC)
static WebCore::UserInterfaceLayoutDirection toUserInterfaceLayoutDirection(NSUserInterfaceLayoutDirection direction)
{
    switch (direction) {
    case NSUserInterfaceLayoutDirectionLeftToRight:
        return WebCore::UserInterfaceLayoutDirection::LTR;
    case NSUserInterfaceLayoutDirectionRightToLeft:
        return WebCore::UserInterfaceLayoutDirection::RTL;
    }
    return WebCore::UserInterfaceLayoutDirection::LTR;
}
#endif

- (id)initWithFrame:(NSRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage
{
    auto configuration = API::PageConfiguration::create();
    configuration->setProcessPool(WebKit::toImpl(contextRef));
    configuration->setPageGroup(WebKit::toImpl(pageGroupRef));
    configuration->setRelatedPage(WebKit::toImpl(relatedPage));
#if PLATFORM(MAC)
    configuration->preferenceValues().set(WebKit::WebPreferencesKey::systemLayoutDirectionKey(), WebKit::WebPreferencesStore::Value(static_cast<uint32_t>(toUserInterfaceLayoutDirection(self.userInterfaceLayoutDirection))));
#endif

    return [self initWithFrame:frame processPool:*WebKit::toImpl(contextRef) configuration:WTFMove(configuration)];
}

- (id)initWithFrame:(NSRect)frame configurationRef:(WKPageConfigurationRef)configurationRef
{
    Ref<API::PageConfiguration> configuration = WebKit::toImpl(configurationRef)->copy();
    auto& processPool = *configuration->processPool();

    return [self initWithFrame:frame processPool:processPool configuration:WTFMove(configuration)];
}

- (BOOL)wantsUpdateLayer
{
    return WebKit::WebViewImpl::wantsUpdateLayer();
}

- (void)updateLayer
{
    _data->_impl->updateLayer();
}

- (WKPageRef)pageRef
{
    return WebKit::toAPI(&_data->_impl->page());
}

- (BOOL)canChangeFrameLayout:(WKFrameRef)frameRef
{
    return _data->_impl->canChangeFrameLayout(*WebKit::toImpl(frameRef));
}

- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo forFrame:(WKFrameRef)frameRef
{
    return _data->_impl->printOperationWithPrintInfo(printInfo, *WebKit::toImpl(frameRef));
}

- (void)setFrame:(NSRect)rect andScrollBy:(NSSize)offset
{
    _data->_impl->setFrameAndScrollBy(NSRectToCGRect(rect), NSSizeToCGSize(offset));
}

- (void)disableFrameSizeUpdates
{
    _data->_impl->disableFrameSizeUpdates();
}

- (void)enableFrameSizeUpdates
{
    _data->_impl->enableFrameSizeUpdates();
}

- (BOOL)frameSizeUpdatesDisabled
{
    return _data->_impl->frameSizeUpdatesDisabled();
}

+ (void)hideWordDefinitionWindow
{
    WebKit::WebViewImpl::hideWordDefinitionWindow();
}

- (NSSize)minimumSizeForAutoLayout
{
    return NSSizeFromCGSize(_data->_impl->minimumSizeForAutoLayout());
}

- (void)setMinimumSizeForAutoLayout:(NSSize)minimumSizeForAutoLayout
{
    _data->_impl->setMinimumSizeForAutoLayout(NSSizeToCGSize(minimumSizeForAutoLayout));
}

- (BOOL)shouldExpandToViewHeightForAutoLayout
{
    return _data->_impl->shouldExpandToViewHeightForAutoLayout();
}

- (void)setShouldExpandToViewHeightForAutoLayout:(BOOL)shouldExpand
{
    return _data->_impl->setShouldExpandToViewHeightForAutoLayout(shouldExpand);
}

- (BOOL)shouldClipToVisibleRect
{
    return _data->_impl->clipsToVisibleRect();
}

- (void)setShouldClipToVisibleRect:(BOOL)clipsToVisibleRect
{
    _data->_impl->setClipsToVisibleRect(clipsToVisibleRect);
}

- (NSColor *)underlayColor
{
    return _data->_impl->underlayColor();
}

- (void)setUnderlayColor:(NSColor *)underlayColor
{
    _data->_impl->setUnderlayColor(underlayColor);
}

- (NSView *)_inspectorAttachmentView
{
    return _data->_impl->inspectorAttachmentView();
}

- (void)_setInspectorAttachmentView:(NSView *)newView
{
    _data->_impl->setInspectorAttachmentView(newView);
}

- (BOOL)_requiresUserActionForEditingControlsManager
{
    return _data->_impl->requiresUserActionForEditingControlsManager();
}

- (void)_setRequiresUserActionForEditingControlsManager:(BOOL)requiresUserAction
{
    _data->_impl->setRequiresUserActionForEditingControlsManager(requiresUserAction);
}

- (NSView *)fullScreenPlaceholderView
{
    return _data->_impl->fullScreenPlaceholderView();
}

// FIXME: This returns an autoreleased object. Should it really be prefixed 'create'?
- (NSWindow *)createFullScreenWindow
{
    return _data->_impl->fullScreenWindow();
}

- (void)beginDeferringViewInWindowChanges
{
    _data->_impl->beginDeferringViewInWindowChanges();
}

- (void)endDeferringViewInWindowChanges
{
    _data->_impl->endDeferringViewInWindowChanges();
}

- (void)endDeferringViewInWindowChangesSync
{
    _data->_impl->endDeferringViewInWindowChangesSync();
}

- (void)_prepareForMoveToWindow:(NSWindow *)targetWindow withCompletionHandler:(void(^)(void))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    _data->_impl->prepareForMoveToWindow(targetWindow, [completionHandlerCopy] {
        completionHandlerCopy();
    });
}

- (BOOL)isDeferringViewInWindowChanges
{
    return _data->_impl->isDeferringViewInWindowChanges();
}

- (BOOL)windowOcclusionDetectionEnabled
{
    return _data->_impl->windowOcclusionDetectionEnabled();
}

- (void)setWindowOcclusionDetectionEnabled:(BOOL)enabled
{
    _data->_impl->setWindowOcclusionDetectionEnabled(enabled);
}

- (void)setAllowsBackForwardNavigationGestures:(BOOL)allowsBackForwardNavigationGestures
{
    _data->_impl->setAllowsBackForwardNavigationGestures(allowsBackForwardNavigationGestures);
}

- (BOOL)allowsBackForwardNavigationGestures
{
    return _data->_impl->allowsBackForwardNavigationGestures();
}

- (BOOL)allowsLinkPreview
{
    return _data->_impl->allowsLinkPreview();
}

- (void)setAllowsLinkPreview:(BOOL)allowsLinkPreview
{
    _data->_impl->setAllowsLinkPreview(allowsLinkPreview);
}

- (void)_setIgnoresAllEvents:(BOOL)ignoresAllEvents
{
    _data->_impl->setIgnoresAllEvents(ignoresAllEvents);
}

// Forward _setIgnoresNonWheelMouseEvents to _setIgnoresNonWheelEvents to avoid breaking existing clients.
- (void)_setIgnoresNonWheelMouseEvents:(BOOL)ignoresNonWheelMouseEvents
{
    _data->_impl->setIgnoresNonWheelEvents(ignoresNonWheelMouseEvents);
}

- (void)_setIgnoresNonWheelEvents:(BOOL)ignoresNonWheelEvents
{
    _data->_impl->setIgnoresNonWheelEvents(ignoresNonWheelEvents);
}

- (BOOL)_ignoresNonWheelEvents
{
    return _data->_impl->ignoresNonWheelEvents();
}

- (BOOL)_ignoresAllEvents
{
    return _data->_impl->ignoresAllEvents();
}

- (void)_setOverrideDeviceScaleFactor:(CGFloat)deviceScaleFactor
{
    _data->_impl->setOverrideDeviceScaleFactor(deviceScaleFactor);
}

- (CGFloat)_overrideDeviceScaleFactor
{
    return _data->_impl->overrideDeviceScaleFactor();
}

- (WKLayoutMode)_layoutMode
{
    return _data->_impl->layoutMode();
}

- (void)_setLayoutMode:(WKLayoutMode)layoutMode
{
    _data->_impl->setLayoutMode(layoutMode);
}

- (CGSize)_fixedLayoutSize
{
    return _data->_impl->fixedLayoutSize();
}

- (void)_setFixedLayoutSize:(CGSize)fixedLayoutSize
{
    _data->_impl->setFixedLayoutSize(fixedLayoutSize);
}

- (CGFloat)_viewScale
{
    return _data->_impl->viewScale();
}

- (void)_setViewScale:(CGFloat)viewScale
{
    if (viewScale <= 0 || isnan(viewScale) || isinf(viewScale))
        [NSException raise:NSInvalidArgumentException format:@"View scale should be a positive number"];

    _data->_impl->setViewScale(viewScale);
}

- (void)_setTopContentInset:(CGFloat)contentInset
{
    return _data->_impl->setTopContentInset(contentInset);
}

- (CGFloat)_topContentInset
{
    return _data->_impl->topContentInset();
}

- (void)_setTotalHeightOfBanners:(CGFloat)totalHeightOfBanners
{
    _data->_impl->setTotalHeightOfBanners(totalHeightOfBanners);
}

- (CGFloat)_totalHeightOfBanners
{
    return _data->_impl->totalHeightOfBanners();
}

static Optional<WebCore::ScrollbarOverlayStyle> toCoreScrollbarStyle(_WKOverlayScrollbarStyle scrollbarStyle)
{
    switch (scrollbarStyle) {
    case _WKOverlayScrollbarStyleDark:
        return WebCore::ScrollbarOverlayStyleDark;
    case _WKOverlayScrollbarStyleLight:
        return WebCore::ScrollbarOverlayStyleLight;
    case _WKOverlayScrollbarStyleDefault:
        return WebCore::ScrollbarOverlayStyleDefault;
    case _WKOverlayScrollbarStyleAutomatic:
    default:
        break;
    }

    return WTF::nullopt;
}

static _WKOverlayScrollbarStyle toAPIScrollbarStyle(Optional<WebCore::ScrollbarOverlayStyle> coreScrollbarStyle)
{
    if (!coreScrollbarStyle)
        return _WKOverlayScrollbarStyleAutomatic;

    switch (coreScrollbarStyle.value()) {
    case WebCore::ScrollbarOverlayStyleDark:
        return _WKOverlayScrollbarStyleDark;
    case WebCore::ScrollbarOverlayStyleLight:
        return _WKOverlayScrollbarStyleLight;
    case WebCore::ScrollbarOverlayStyleDefault:
        return _WKOverlayScrollbarStyleDefault;
    default:
        return _WKOverlayScrollbarStyleAutomatic;
    }
}

- (void)_setOverlayScrollbarStyle:(_WKOverlayScrollbarStyle)scrollbarStyle
{
    _data->_impl->setOverlayScrollbarStyle(toCoreScrollbarStyle(scrollbarStyle));
}

- (_WKOverlayScrollbarStyle)_overlayScrollbarStyle
{
    return toAPIScrollbarStyle(_data->_impl->overlayScrollbarStyle());
}

- (NSColor *)_pageExtendedBackgroundColor
{
    return _data->_impl->pageExtendedBackgroundColor();
}

- (BOOL)isUsingUISideCompositing
{
    return _data->_impl->isUsingUISideCompositing();
}

- (void)setAllowsMagnification:(BOOL)allowsMagnification
{
    _data->_impl->setAllowsMagnification(allowsMagnification);
}

- (BOOL)allowsMagnification
{
    return _data->_impl->allowsMagnification();
}

- (void)magnifyWithEvent:(NSEvent *)event
{
    _data->_impl->magnifyWithEvent(event);
}

#if ENABLE(MAC_GESTURE_EVENTS)
- (void)rotateWithEvent:(NSEvent *)event
{
    _data->_impl->rotateWithEvent(event);
}
#endif

- (void)_gestureEventWasNotHandledByWebCore:(NSEvent *)event
{
    _data->_impl->gestureEventWasNotHandledByWebCoreFromViewOnly(event);
}

- (void)smartMagnifyWithEvent:(NSEvent *)event
{
    _data->_impl->smartMagnifyWithEvent(event);
}

- (void)setMagnification:(double)magnification centeredAtPoint:(NSPoint)point
{
    _data->_impl->setMagnification(magnification, NSPointToCGPoint(point));
}

- (void)setMagnification:(double)magnification
{
    _data->_impl->setMagnification(magnification);
}

- (double)magnification
{
    return _data->_impl->magnification();
}

- (void)_setCustomSwipeViews:(NSArray *)customSwipeViews
{
    _data->_impl->setCustomSwipeViews(customSwipeViews);
}

- (void)_setCustomSwipeViewsTopContentInset:(float)topContentInset
{
    _data->_impl->setCustomSwipeViewsTopContentInset(topContentInset);
}

- (BOOL)_tryToSwipeWithEvent:(NSEvent *)event ignoringPinnedState:(BOOL)ignoringPinnedState
{
    return _data->_impl->tryToSwipeWithEvent(event, ignoringPinnedState);
}

- (void)_setDidMoveSwipeSnapshotCallback:(void(^)(CGRect))callback
{
    _data->_impl->setDidMoveSwipeSnapshotCallback(callback);
}

- (id)_immediateActionAnimationControllerForHitTestResult:(WKHitTestResultRef)hitTestResult withType:(uint32_t)type userData:(WKTypeRef)userData
{
    return nil;
}

- (void)_prepareForImmediateActionAnimation
{
}

- (void)_cancelImmediateActionAnimation
{
}

- (void)_completeImmediateActionAnimation
{
}

- (void)_didChangeContentSize:(NSSize)newSize
{
}

- (void)_dismissContentRelativeChildWindows
{
    _data->_impl->dismissContentRelativeChildWindowsFromViewOnly();
}

- (void)_dismissContentRelativeChildWindowsWithAnimation:(BOOL)withAnimation
{
    _data->_impl->dismissContentRelativeChildWindowsWithAnimationFromViewOnly(withAnimation);
}

- (void)_setAutomaticallyAdjustsContentInsets:(BOOL)automaticallyAdjustsContentInsets
{
    _data->_impl->setAutomaticallyAdjustsContentInsets(automaticallyAdjustsContentInsets);
}

- (BOOL)_automaticallyAdjustsContentInsets
{
    return _data->_impl->automaticallyAdjustsContentInsets();
}

- (void)setUserInterfaceLayoutDirection:(NSUserInterfaceLayoutDirection)userInterfaceLayoutDirection
{
    [super setUserInterfaceLayoutDirection:userInterfaceLayoutDirection];

    _data->_impl->setUserInterfaceLayoutDirection(userInterfaceLayoutDirection);
}

- (BOOL)_wantsMediaPlaybackControlsView
{
#if HAVE(TOUCH_BAR)
    return _data->_impl->clientWantsMediaPlaybackControlsView();
#else
    return NO;
#endif
}

- (void)_setWantsMediaPlaybackControlsView:(BOOL)wantsMediaPlaybackControlsView
{
#if HAVE(TOUCH_BAR)
    _data->_impl->setClientWantsMediaPlaybackControlsView(wantsMediaPlaybackControlsView);
#endif
}

- (id)_mediaPlaybackControlsView
{
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    return _data->_impl->clientWantsMediaPlaybackControlsView() ? _data->_impl->mediaPlaybackControlsView() : nil;
#else
    return nil;
#endif
}

// This method is for subclasses to override.
- (void)_addMediaPlaybackControlsView:(id)mediaPlaybackControlsView
{
}

// This method is for subclasses to override.
- (void)_removeMediaPlaybackControlsView
{
}

- (void)_doAfterNextPresentationUpdate:(void (^)(void))updateBlock
{
    auto updateBlockCopy = makeBlockPtr(updateBlock);
    _data->_impl->page().callAfterNextPresentationUpdate([updateBlockCopy](WebKit::CallbackBase::Error error) {
        if (updateBlockCopy)
            updateBlockCopy();
    });
}

- (void)_setShouldSuppressFirstResponderChanges:(BOOL)shouldSuppress
{
    _data->_impl->setShouldSuppressFirstResponderChanges(shouldSuppress);
}

- (void)viewDidChangeEffectiveAppearance
{
    // This can be called during [super initWithCoder:] and [super initWithFrame:].
    // That is before _data or _impl is ready to be used, so check. <rdar://problem/39611236>
    if (!_data || !_data->_impl)
        return;

    _data->_impl->effectiveAppearanceDidChange();
}

- (void)_setUseSystemAppearance:(BOOL)useSystemAppearance
{
    _data->_impl->setUseSystemAppearance(useSystemAppearance);
}

- (BOOL)_useSystemAppearance
{
    return _data->_impl->useSystemAppearance();
}

@end
ALLOW_DEPRECATED_DECLARATIONS_END

#endif // PLATFORM(MAC)

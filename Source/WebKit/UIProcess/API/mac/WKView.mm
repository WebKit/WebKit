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
#import "AppKitSPI.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKNSData.h"
#import "WKWebViewMac.h"
#import "WebBackForwardListItem.h"
#import "WebKit2Initialize.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessPool.h"
#import "WebViewImpl.h"
#import "_WKLinkIconParametersInternal.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebViewVisualIdentificationOverlay.h>
#import <WebKit/WKDragDestinationAction.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/mac/NSViewSPI.h>
#import <wtf/BlockPtr.h>

@interface WKViewData : NSObject
@end

@implementation WKViewData
@end

@interface WKView () <WebViewImplDelegate>
@end

#if HAVE(TOUCH_BAR)
@interface WKView () <NSTouchBarProvider>
@end
#endif

@interface WKView () <NSScrollViewSeparatorTrackingAdapter>
@end

#if ENABLE(DRAG_SUPPORT)

@interface WKView () <NSFilePromiseProviderDelegate, NSDraggingSource>
@end

#endif

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKView
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (void)dealloc
{
    [super dealloc];
}

- (WKBrowsingContextController *)browsingContextController
{
    return nil;
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
}

- (BOOL)drawsBackground
{
    return NO;
}

- (NSColor *)_backgroundColor
{
    return nil;
}

- (void)_setBackgroundColor:(NSColor *)backgroundColor
{
}

- (void)setDrawsTransparentBackground:(BOOL)drawsTransparentBackground
{
}

- (BOOL)drawsTransparentBackground
{
    return NO;
}

- (BOOL)acceptsFirstResponder
{
    return NO;
}

- (BOOL)becomeFirstResponder
{
    return NO;
}

- (BOOL)resignFirstResponder
{
    return NO;
}

- (void)viewWillStartLiveResize
{
}

- (void)viewDidEndLiveResize
{
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSSize)intrinsicContentSize
{
    return { };
}

- (void)prepareContentInRect:(NSRect)rect
{
}

- (void)setFrameSize:(NSSize)size
{
}

- (void)_setSemanticContext:(NSViewSemanticContext)semanticContext
{
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)renewGState
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
}

#define WEBCORE_COMMAND(command) - (void)command:(id)sender { }

WEBCORE_COMMAND(alignCenter)
WEBCORE_COMMAND(alignJustified)
WEBCORE_COMMAND(alignLeft)
WEBCORE_COMMAND(alignRight)
WEBCORE_COMMAND(copy)
WEBCORE_COMMAND(copyFont)
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
WEBCORE_COMMAND(pasteFont)
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
    return NO;
}

- (void)centerSelectionInVisibleArea:(id)sender 
{ 
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    return nil;
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pasteboard 
{
    return NO;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)changeFont:(id)sender
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
}

/*

When possible, editing-related methods should be implemented in WebCore with the
EditorCommand mechanism and invoked via WEBCORE_COMMAND, rather than implementing
individual methods here with Mac-specific code.

Editing-related methods still unimplemented that are implemented in WebKit1:

- (void)complete:(id)sender;
- (void)makeBaseWritingDirectionLeftToRight:(id)sender;
- (void)makeBaseWritingDirectionNatural:(id)sender;
- (void)makeBaseWritingDirectionRightToLeft:(id)sender;
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
    return NO;
}

- (IBAction)startSpeaking:(id)sender
{
}

- (IBAction)stopSpeaking:(id)sender
{
}

- (IBAction)showGuessPanel:(id)sender
{
}

- (IBAction)checkSpelling:(id)sender
{
}

- (void)changeSpelling:(id)sender
{
}

- (IBAction)toggleContinuousSpellChecking:(id)sender
{
}

- (BOOL)isGrammarCheckingEnabled
{
    return NO;
}

- (void)setGrammarCheckingEnabled:(BOOL)flag
{
}

- (IBAction)toggleGrammarChecking:(id)sender
{
}

- (IBAction)toggleAutomaticSpellingCorrection:(id)sender
{
}

- (void)orderFrontSubstitutionsPanel:(id)sender
{
}

- (IBAction)toggleSmartInsertDelete:(id)sender
{
}

- (BOOL)isAutomaticQuoteSubstitutionEnabled
{
    return NO;
}

- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag
{
}

- (void)toggleAutomaticQuoteSubstitution:(id)sender
{
}

- (BOOL)isAutomaticDashSubstitutionEnabled
{
    return NO;
}

- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag
{
}

- (void)toggleAutomaticDashSubstitution:(id)sender
{
}

- (BOOL)isAutomaticLinkDetectionEnabled
{
    return NO;
}

- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag
{
}

- (void)toggleAutomaticLinkDetection:(id)sender
{
}

- (BOOL)isAutomaticTextReplacementEnabled
{
    return NO;
}

- (void)setAutomaticTextReplacementEnabled:(BOOL)flag
{
}

- (void)toggleAutomaticTextReplacement:(id)sender
{
}

- (void)uppercaseWord:(id)sender
{
}

- (void)lowercaseWord:(id)sender
{
}

- (void)capitalizeWord:(id)sender
{
}

- (BOOL)_wantsKeyDownForEvent:(NSEvent *)event
{
    return NO;
}

- (void)scrollWheel:(NSEvent *)event
{
}

- (void)swipeWithEvent:(NSEvent *)event
{
}

- (void)mouseDown:(NSEvent *)event
{
}

- (void)mouseUp:(NSEvent *)event
{
}

- (void)mouseDragged:(NSEvent *)event
{
}

- (void)otherMouseDown:(NSEvent *)event
{
}

- (void)otherMouseDragged:(NSEvent *)event
{
}

- (void)otherMouseUp:(NSEvent *)event
{
}

- (void)rightMouseDown:(NSEvent *)event
{
}

- (void)rightMouseDragged:(NSEvent *)event
{
}

- (void)rightMouseUp:(NSEvent *)event
{
}

- (void)pressureChangeWithEvent:(NSEvent *)event
{
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return NO;
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    return NO;
}

- (void)doCommandBySelector:(SEL)selector
{
}

- (void)insertText:(id)string
{
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
}

- (NSTextInputContext *)inputContext
{
    return nil;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    return NO;
}

- (void)keyUp:(NSEvent *)theEvent
{
}

- (void)keyDown:(NSEvent *)theEvent
{
}

- (void)flagsChanged:(NSEvent *)theEvent
{
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelectedRange replacementRange:(NSRange)replacementRange
{
}

- (void)unmarkText
{
}

- (NSRange)selectedRange
{
    return { };
}

- (BOOL)hasMarkedText
{
    return NO;
}

- (NSRange)markedRange
{
    return { };
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)nsRange actualRange:(NSRangePointer)actualRange
{
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange actualRange:(NSRangePointer)actualRange
{
    return { };
}

- (void)selectedRangeWithCompletionHandler:(void(^)(NSRange selectedRange))completionHandlerPtr
{
}

- (void)markedRangeWithCompletionHandler:(void(^)(NSRange markedRange))completionHandlerPtr
{
}

- (void)hasMarkedTextWithCompletionHandler:(void(^)(BOOL hasMarkedText))completionHandlerPtr
{
}

- (void)attributedSubstringForProposedRange:(NSRange)nsRange completionHandler:(void(^)(NSAttributedString *attrString, NSRange actualRange))completionHandlerPtr
{
}

- (void)firstRectForCharacterRange:(NSRange)theRange completionHandler:(void(^)(NSRect firstRect, NSRange actualRange))completionHandlerPtr
{
}

- (void)characterIndexForPoint:(NSPoint)thePoint completionHandler:(void(^)(NSUInteger))completionHandlerPtr
{
}

- (NSArray *)validAttributesForMarkedText
{
    return nil;
}

#if ENABLE(DRAG_SUPPORT)
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)draggedImage:(NSImage *)image endedAt:(NSPoint)endPoint operation:(NSDragOperation)operation
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)draggingInfo
{
    return 0;
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)draggingInfo
{
    return 0;
}

- (void)draggingExited:(id <NSDraggingInfo>)draggingInfo
{
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    return NO;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    return NO;
}

- (NSView *)_hitTest:(NSPoint *)point dragTypes:(NSSet *)types
{
    return nil;
}
#endif // ENABLE(DRAG_SUPPORT)

- (BOOL)_windowResizeMouseLocationIsInVisibleScrollerThumb:(NSPoint)point
{
    return NO;
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
}

- (void)viewDidMoveToWindow
{
}

- (void)drawRect:(NSRect)rect
{
}

- (BOOL)isOpaque
{
    return NO;
}

- (BOOL)mouseDownCanMoveWindow
{
    return NO;
}

- (void)viewDidHide
{
}

- (void)viewDidUnhide
{
}

- (void)viewDidChangeBackingProperties
{
}

- (void)_activeSpaceDidChange:(NSNotification *)notification
{
}

- (id)accessibilityFocusedUIElement
{
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsIgnored
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return NO;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray<NSString *> *)accessibilityParameterizedAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return nil;
}

- (NSView *)hitTest:(NSPoint)point
{
    return nil;
}

- (NSInteger)conversationIdentifier
{
    return 0;
}

- (void)quickLookWithEvent:(NSEvent *)event
{
}

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    return { };
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    return { };
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)pasteboardChangedOwner:(NSPasteboard *)pasteboard
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)pasteboard:(NSPasteboard *)pasteboard provideDataForType:(NSString *)type
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return nil;
}

- (void)maybeInstallIconLoadingClient
{
}

- (instancetype)initWithFrame:(NSRect)frame processPool:(std::reference_wrapper<WebKit::WebProcessPool>)processPool configuration:(Ref<API::PageConfiguration>&&)configuration
{
    return nil;
}

- (void)_setThumbnailView:(_WKThumbnailView *)thumbnailView
{
}

- (_WKThumbnailView *)_thumbnailView
{
    return nil;
}

- (NSTextInputContext *)_web_superInputContext
{
    return nil;
}

- (void)_web_superQuickLookWithEvent:(NSEvent *)event
{
}

- (void)_web_superSwipeWithEvent:(NSEvent *)event
{
}

- (void)_web_superMagnifyWithEvent:(NSEvent *)event
{
}

- (void)_web_superSmartMagnifyWithEvent:(NSEvent *)event
{
}

- (void)_web_superRemoveTrackingRect:(NSTrackingRectTag)tag
{
}

- (id)_web_superAccessibilityAttributeValue:(NSString *)attribute
{
    return nil;
}

- (void)_web_superDoCommandBySelector:(SEL)selector
{
}

- (BOOL)_web_superPerformKeyEquivalent:(NSEvent *)event
{
    return NO;
}

- (void)_web_superKeyDown:(NSEvent *)event
{
}

- (NSView *)_web_superHitTest:(NSPoint)point
{
    return nil;
}

- (id)_web_immediateActionAnimationControllerForHitTestResultInternal:(API::HitTestResult*)hitTestResult withType:(uint32_t)type userData:(API::Object*)userData
{
    return nil;
}

- (void)_web_prepareForImmediateActionAnimation
{
}

- (void)_web_cancelImmediateActionAnimation
{
}

- (void)_web_completeImmediateActionAnimation
{
}

- (void)_web_didChangeContentSize:(NSSize)newSize
{
}

#if ENABLE(DRAG_SUPPORT)

- (void)_web_didPerformDragOperation:(BOOL)handled
{
}

- (WKDragDestinationAction)_web_dragDestinationActionForDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    return 0;
}

#endif

- (void)_web_dismissContentRelativeChildWindows
{
}

- (void)_web_dismissContentRelativeChildWindowsWithAnimation:(BOOL)withAnimation
{
}

- (void)_web_editorStateDidChange
{
}

- (void)_web_gestureEventWasNotHandledByWebCore:(NSEvent *)event
{
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
    return nil;
}

- (NSCandidateListTouchBarItem *)candidateListTouchBarItem
{
    return nil;
}

- (void)_web_didAddMediaControlsManager:(id)controlsManager
{
}

- (void)_web_didRemoveMediaControlsManager
{
}

#endif // HAVE(TOUCH_BAR)

- (NSRect)scrollViewFrame
{
    return { };
}

- (BOOL)hasScrolledContentsUnderTitlebar
{
    return NO;
}

#if ENABLE(DRAG_SUPPORT)

- (NSString *)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider fileNameForType:(NSString *)fileType
{
    return nil;
}

- (void)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider writePromiseToURL:(NSURL *)url completionHandler:(void (^)(NSError *error))completionHandler
{
}

- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
    return 0;
}

- (void)draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation
{
}

#endif // ENABLE(DRAG_SUPPORT)

@end

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKView (Private)
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (void)saveBackForwardSnapshotForCurrentItem
{
}

- (void)saveBackForwardSnapshotForItem:(WKBackForwardListItemRef)item
{
}

- (id)initWithFrame:(NSRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef
{
    return nil;
}

- (id)initWithFrame:(NSRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage
{
    return nil;
}

- (id)initWithFrame:(NSRect)frame configurationRef:(WKPageConfigurationRef)configurationRef
{
    return nil;
}

- (BOOL)wantsUpdateLayer
{
    return NO;
}

- (void)updateLayer
{
}

- (WKPageRef)pageRef
{
    return nullptr;
}

- (BOOL)canChangeFrameLayout:(WKFrameRef)frameRef
{
    return NO;
}

- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo forFrame:(WKFrameRef)frameRef
{
    return nil;
}

- (void)setFrame:(NSRect)rect andScrollBy:(NSSize)offset
{
}

- (void)disableFrameSizeUpdates
{
}

- (void)enableFrameSizeUpdates
{
}

- (BOOL)frameSizeUpdatesDisabled
{
    return NO;
}

+ (void)hideWordDefinitionWindow
{
}

- (NSSize)minimumSizeForAutoLayout
{
    return { };
}

- (void)setMinimumSizeForAutoLayout:(NSSize)minimumSizeForAutoLayout
{
}

- (NSSize)sizeToContentAutoSizeMaximumSize
{
    return { };
}

- (void)setSizeToContentAutoSizeMaximumSize:(NSSize)sizeToContentAutoSizeMaximumSize
{
}

- (BOOL)shouldExpandToViewHeightForAutoLayout
{
    return NO;
}

- (void)setShouldExpandToViewHeightForAutoLayout:(BOOL)shouldExpand
{
}

- (BOOL)shouldClipToVisibleRect
{
    return NO;
}

- (void)setShouldClipToVisibleRect:(BOOL)clipsToVisibleRect
{
}

- (NSColor *)underlayColor
{
    return nil;
}

- (void)setUnderlayColor:(NSColor *)underlayColor
{
}

- (NSView *)_inspectorAttachmentView
{
    return nil;
}

- (void)_setInspectorAttachmentView:(NSView *)newView
{
}

- (NSView *)fullScreenPlaceholderView
{
    return nil;
}

// FIXME: This returns an autoreleased object. Should it really be prefixed 'create'?
- (NSWindow *)createFullScreenWindow
{
    return nil;
}

- (void)beginDeferringViewInWindowChanges
{
}

- (void)endDeferringViewInWindowChanges
{
}

- (void)endDeferringViewInWindowChangesSync
{
}

- (void)_prepareForMoveToWindow:(NSWindow *)targetWindow withCompletionHandler:(void(^)(void))completionHandler
{
}

- (BOOL)isDeferringViewInWindowChanges
{
    return NO;
}

- (BOOL)windowOcclusionDetectionEnabled
{
    return NO;
}

- (void)setWindowOcclusionDetectionEnabled:(BOOL)enabled
{
}

- (void)setAllowsBackForwardNavigationGestures:(BOOL)allowsBackForwardNavigationGestures
{
}

- (BOOL)allowsBackForwardNavigationGestures
{
    return NO;
}

- (BOOL)allowsLinkPreview
{
    return NO;
}

- (void)setAllowsLinkPreview:(BOOL)allowsLinkPreview
{
}

- (void)_setIgnoresAllEvents:(BOOL)ignoresAllEvents
{
}

// Forward _setIgnoresNonWheelMouseEvents to _setIgnoresNonWheelEvents to avoid breaking existing clients.
- (void)_setIgnoresNonWheelMouseEvents:(BOOL)ignoresNonWheelMouseEvents
{
}

- (void)_setIgnoresNonWheelEvents:(BOOL)ignoresNonWheelEvents
{
}

- (BOOL)_ignoresNonWheelEvents
{
    return NO;
}

- (BOOL)_ignoresAllEvents
{
    return NO;
}

- (void)_setOverrideDeviceScaleFactor:(CGFloat)deviceScaleFactor
{
}

- (CGFloat)_overrideDeviceScaleFactor
{
    return { };
}

- (WKLayoutMode)_layoutMode
{
    return 0;
}

- (void)_setLayoutMode:(WKLayoutMode)layoutMode
{
}

- (CGSize)_fixedLayoutSize
{
    return { };
}

- (void)_setFixedLayoutSize:(CGSize)fixedLayoutSize
{
}

- (CGFloat)_viewScale
{
    return 0;
}

- (void)_setViewScale:(CGFloat)viewScale
{
}

- (void)_setTopContentInset:(CGFloat)contentInset
{
}

- (CGFloat)_topContentInset
{
    return 0;
}

- (void)_setTotalHeightOfBanners:(CGFloat)totalHeightOfBanners
{
}

- (CGFloat)_totalHeightOfBanners
{
    return 0;
}

- (void)_setOverlayScrollbarStyle:(_WKOverlayScrollbarStyle)scrollbarStyle
{
}

- (_WKOverlayScrollbarStyle)_overlayScrollbarStyle
{
    return _WKOverlayScrollbarStyleDefault;
}

- (BOOL)isUsingUISideCompositing
{
    return NO;
}

- (void)setAllowsMagnification:(BOOL)allowsMagnification
{
}

- (BOOL)allowsMagnification
{
    return NO;
}

- (void)magnifyWithEvent:(NSEvent *)event
{
}

#if ENABLE(MAC_GESTURE_EVENTS)
- (void)rotateWithEvent:(NSEvent *)event
{
}
#endif

- (void)_gestureEventWasNotHandledByWebCore:(NSEvent *)event
{
}

- (void)_simulateMouseMove:(NSEvent *)event
{
}

- (void)smartMagnifyWithEvent:(NSEvent *)event
{
}

- (void)setMagnification:(double)magnification centeredAtPoint:(NSPoint)point
{
}

- (void)setMagnification:(double)magnification
{
}

- (double)magnification
{
    return 0;
}

- (void)_setCustomSwipeViews:(NSArray *)customSwipeViews
{
}

- (void)_setCustomSwipeViewsTopContentInset:(float)topContentInset
{
}

- (BOOL)_tryToSwipeWithEvent:(NSEvent *)event ignoringPinnedState:(BOOL)ignoringPinnedState
{
    return NO;
}

- (void)_setDidMoveSwipeSnapshotCallback:(void(^)(CGRect))callback
{
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
}

- (void)_dismissContentRelativeChildWindowsWithAnimation:(BOOL)withAnimation
{
}

- (void)_setAutomaticallyAdjustsContentInsets:(BOOL)automaticallyAdjustsContentInsets
{
}

- (BOOL)_automaticallyAdjustsContentInsets
{
    return NO;
}

- (void)setUserInterfaceLayoutDirection:(NSUserInterfaceLayoutDirection)userInterfaceLayoutDirection
{
}

- (BOOL)_wantsMediaPlaybackControlsView
{
    return NO;
}

- (void)_setWantsMediaPlaybackControlsView:(BOOL)wantsMediaPlaybackControlsView
{
}

- (id)_mediaPlaybackControlsView
{
    return nil;
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
}

- (void)_setShouldSuppressFirstResponderChanges:(BOOL)shouldSuppress
{
}

- (void)viewDidChangeEffectiveAppearance
{
}

- (void)_setUseSystemAppearance:(BOOL)useSystemAppearance
{
}

- (BOOL)_useSystemAppearance
{
    return NO;
}

- (BOOL)acceptsPreviewPanelControl:(QLPreviewPanel *)panel
{
    return NO;
}

- (void)beginPreviewPanelControl:(QLPreviewPanel *)panel
{
}

- (void)endPreviewPanelControl:(QLPreviewPanel *)panel
{
}

@end
ALLOW_DEPRECATED_DECLARATIONS_END

#endif // PLATFORM(MAC)

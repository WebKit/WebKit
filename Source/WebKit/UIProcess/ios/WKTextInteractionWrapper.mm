/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WKTextInteractionWrapper.h"

#if PLATFORM(IOS_FAMILY)

#import "WKContentViewInteraction.h"

@implementation WKTextInteractionWrapper {
    __weak WKContentView *_view;
    RetainPtr<UIWKTextInteractionAssistant> _textInteractionAssistant;
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    RetainPtr<WKSETextInteraction> _asyncTextInteraction;
    RetainPtr<NSTimer> _showEditMenuTimer;
    BOOL _showEditMenuAfterNextSelectionChange;
#endif
    BOOL _shouldRestoreEditMenuAfterOverflowScrolling;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    if (view.shouldUseAsyncInteractions) {
        _asyncTextInteraction = adoptNS([[WKSETextInteraction alloc] init]);
        [_asyncTextInteraction setDelegate:view];
        [view addInteraction:_asyncTextInteraction.get()];
    } else
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
    {
        _textInteractionAssistant = adoptNS([[UIWKTextInteractionAssistant alloc] initWithView:view]);
    }

    _view = view;
    return self;
}

- (void)dealloc
{
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [self stopShowEditMenuTimer];
    if (_asyncTextInteraction)
        [_view removeInteraction:_asyncTextInteraction.get()];
#endif

    [super dealloc];
}

- (UIWKTextInteractionAssistant *)textInteractionAssistant
{
    return _textInteractionAssistant.get();
}

- (void)activateSelection
{
    [_textInteractionAssistant activateSelection];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = YES;
#endif
}

- (void)deactivateSelection
{
    [_textInteractionAssistant deactivateSelection];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = NO;
    _showEditMenuAfterNextSelectionChange = NO;
    [self stopShowEditMenuTimer];
#endif
}

- (void)selectionChanged
{
    [_textInteractionAssistant selectionChanged];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction refreshKeyboardUI];
#else
    [_asyncTextInteraction selectionChanged];
#endif

    [self stopShowEditMenuTimer];
    if (std::exchange(_showEditMenuAfterNextSelectionChange, NO))
        _showEditMenuTimer = [NSTimer scheduledTimerWithTimeInterval:0.2 target:self selector:@selector(showEditMenuTimerFired) userInfo:nil repeats:NO];
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)setGestureRecognizers
{
    [_textInteractionAssistant setGestureRecognizers];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction editabilityChanged];
#endif
}

- (void)willStartScrollingOverflow
{
    [_textInteractionAssistant willStartScrollingOverflow];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    _shouldRestoreEditMenuAfterOverflowScrolling = _view.isPresentingEditMenu;
    [_asyncTextInteraction dismissEditMenuForSelection];
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = NO;
#endif
}

- (void)didEndScrollingOverflow
{
    [_textInteractionAssistant didEndScrollingOverflow];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    if (std::exchange(_shouldRestoreEditMenuAfterOverflowScrolling, NO))
        [_asyncTextInteraction presentEditMenuForSelection];
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = YES;
#endif
}

- (void)willStartScrollingOrZooming
{
    [_textInteractionAssistant willStartScrollingOrZooming];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    _shouldRestoreEditMenuAfterOverflowScrolling = _view.isPresentingEditMenu;
    [_asyncTextInteraction dismissEditMenuForSelection];
#endif
}

- (void)didEndScrollingOrZooming
{
    [_textInteractionAssistant didEndScrollingOrZooming];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    if (std::exchange(_shouldRestoreEditMenuAfterOverflowScrolling, NO))
        [_asyncTextInteraction presentEditMenuForSelection];
#endif
}

- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(WKSEGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(WKSESelectionFlags)flags
{
    [_textInteractionAssistant selectionChangedWithGestureAt:point withGesture:static_cast<UIWKGestureType>(gestureType) withState:gestureState withFlags:static_cast<UIWKSelectionFlags>(flags)];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction selectionChangedWithGestureAtPoint:point gesture:gestureType state:gestureState flags:flags];
#else
    [_asyncTextInteraction selectionChangedWithGestureAt:point withGesture:gestureType withState:gestureState withFlags:flags];
#endif
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(WKSESelectionTouchPhase)touch withFlags:(WKSESelectionFlags)flags
{
    [_textInteractionAssistant selectionChangedWithTouchAt:point withSelectionTouch:static_cast<UIWKSelectionTouch>(touch) withFlags:static_cast<UIWKSelectionFlags>(flags)];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction selectionBoundaryAdjustedToPoint:point touchPhase:touch flags:flags];
#else
    [_asyncTextInteraction selectionChangedWithTouchAt:point withSelectionTouch:touch withFlags:flags];
#endif
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)lookup:(NSString *)textWithContext withRange:(NSRange)range fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant lookup:textWithContext withRange:range fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction showDictionaryForTextInContext:textWithContext definingTextInRange:range fromRect:presentationRect];
#else
    [_asyncTextInteraction lookup:textWithContext withRange:range fromRect:presentationRect];
#endif
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)showShareSheetFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant showShareSheetFor:selectedTerm fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction shareText:selectedTerm fromRect:presentationRect];
#else
    [_asyncTextInteraction showShareSheetFor:selectedTerm fromRect:presentationRect];
#endif
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant showTextServiceFor:selectedTerm fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction addShortcutForText:selectedTerm fromRect:presentationRect];
#else
    [_asyncTextInteraction showTextServiceFor:selectedTerm fromRect:presentationRect];
#endif
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)scheduleReplacementsForText:(NSString *)text
{
    [_textInteractionAssistant scheduleReplacementsForText:text];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction showReplacementsForText:text];
#else
    [_asyncTextInteraction scheduleReplacementsForText:text];
#endif
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)scheduleChineseTransliterationForText:(NSString *)text
{
    [_textInteractionAssistant scheduleChineseTransliterationForText:text];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction transliterateChineseForText:text];
#else
    [_asyncTextInteraction scheduleChineseTransliterationForText:text];
#endif
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)translate:(NSString *)text fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant translate:text fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
#if SERVICE_EXTENSIONS_TEXT_INPUT_IS_AVAILABLE
    [_asyncTextInteraction translateText:text fromRect:presentationRect];
#else
    [_asyncTextInteraction translate:text fromRect:presentationRect];
#endif
#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)
}

- (void)selectWord
{
    [_textInteractionAssistant selectWord];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    _showEditMenuAfterNextSelectionChange = YES;
#endif
}

- (void)selectAll:(id)sender
{
    [_textInteractionAssistant selectAll:sender];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    _showEditMenuAfterNextSelectionChange = YES;
#endif
}

#if HAVE(UI_ASYNC_TEXT_INTERACTION)

- (void)showEditMenuTimerFired
{
    [self stopShowEditMenuTimer];
    [_asyncTextInteraction presentEditMenuForSelection];
}

- (void)stopShowEditMenuTimer
{
    [std::exchange(_showEditMenuTimer, nil) invalidate];
}

#endif // HAVE(UI_ASYNC_TEXT_INTERACTION)

#if USE(UICONTEXTMENU)

- (UIContextMenuInteraction *)contextMenuInteraction
{
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    if (_asyncTextInteraction)
        return [_asyncTextInteraction contextMenuInteraction];
#endif
    return [_textInteractionAssistant contextMenuInteraction];
}

- (void)setExternalContextMenuInteractionDelegate:(id<UIContextMenuInteractionDelegate>)delegate
{
    [_textInteractionAssistant setExternalContextMenuInteractionDelegate:delegate];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction setContextMenuInteractionDelegate:delegate];
#endif
}

#endif // USE(UICONTEXTMENU)

@end

#endif // PLATFORM(IOS_FAMILY)

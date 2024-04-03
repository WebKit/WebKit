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
#if USE(BROWSERENGINEKIT)
    RetainPtr<BETextInteraction> _asyncTextInteraction;
    RetainPtr<NSTimer> _showEditMenuTimer;
    BOOL _showEditMenuAfterNextSelectionChange;
#endif
    BOOL _shouldRestoreEditMenuAfterOverflowScrolling;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;

#if USE(BROWSERENGINEKIT)
    if (view.shouldUseAsyncInteractions) {
        _asyncTextInteraction = adoptNS([[BETextInteraction alloc] init]);
        [_asyncTextInteraction setDelegate:view];
        [view addInteraction:_asyncTextInteraction.get()];
        return self;
    }
#endif

    _textInteractionAssistant = adoptNS([[UIWKTextInteractionAssistant alloc] initWithView:view]);
    return self;
}

- (void)dealloc
{
#if USE(BROWSERENGINEKIT)
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
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = YES;
#endif
}

- (void)deactivateSelection
{
    [_textInteractionAssistant deactivateSelection];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = NO;
    _showEditMenuAfterNextSelectionChange = NO;
    [self stopShowEditMenuTimer];
#endif
}

- (void)selectionChanged
{
    [_textInteractionAssistant selectionChanged];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction refreshKeyboardUI];

    [self stopShowEditMenuTimer];
    if (std::exchange(_showEditMenuAfterNextSelectionChange, NO))
        _showEditMenuTimer = [NSTimer scheduledTimerWithTimeInterval:0.2 target:self selector:@selector(showEditMenuTimerFired) userInfo:nil repeats:NO];
#endif
}

- (void)setGestureRecognizers
{
    [_textInteractionAssistant setGestureRecognizers];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction editabilityChanged];
#endif
}

- (void)willStartScrollingOverflow
{
    [_textInteractionAssistant willStartScrollingOverflow];
#if USE(BROWSERENGINEKIT)
    _shouldRestoreEditMenuAfterOverflowScrolling = _view.isPresentingEditMenu;
    [_asyncTextInteraction dismissEditMenuForSelection];
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = NO;
#endif
}

- (void)didEndScrollingOverflow
{
    [_textInteractionAssistant didEndScrollingOverflow];
#if USE(BROWSERENGINEKIT)
    if (std::exchange(_shouldRestoreEditMenuAfterOverflowScrolling, NO))
        [_asyncTextInteraction presentEditMenuForSelection];
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = YES;
#endif
}

- (void)willStartScrollingOrZooming
{
    [_textInteractionAssistant willStartScrollingOrZooming];
#if USE(BROWSERENGINEKIT)
    _shouldRestoreEditMenuAfterOverflowScrolling = _view.isPresentingEditMenu;
    [_asyncTextInteraction dismissEditMenuForSelection];
#endif
}

- (void)didEndScrollingOrZooming
{
    [_textInteractionAssistant didEndScrollingOrZooming];
#if USE(BROWSERENGINEKIT)
    if (std::exchange(_shouldRestoreEditMenuAfterOverflowScrolling, NO))
        [_asyncTextInteraction presentEditMenuForSelection];
#endif
}

- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(WKBEGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(WKBESelectionFlags)flags
{
    [_textInteractionAssistant selectionChangedWithGestureAt:point withGesture:static_cast<UIWKGestureType>(gestureType) withState:gestureState withFlags:static_cast<UIWKSelectionFlags>(flags)];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction selectionChangedWithGestureAtPoint:point gesture:gestureType state:gestureState flags:flags];
#endif
}

- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(WKBESelectionTouchPhase)touch withFlags:(WKBESelectionFlags)flags
{
    [_textInteractionAssistant selectionChangedWithTouchAt:point withSelectionTouch:static_cast<UIWKSelectionTouch>(touch) withFlags:static_cast<UIWKSelectionFlags>(flags)];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction selectionBoundaryAdjustedToPoint:point touchPhase:touch flags:flags];
#endif
}

- (void)lookup:(NSString *)textWithContext withRange:(NSRange)range fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant lookup:textWithContext withRange:range fromRect:presentationRect];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction showDictionaryForTextInContext:textWithContext definingTextInRange:range fromRect:presentationRect];
#endif
}

- (void)showShareSheetFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant showShareSheetFor:selectedTerm fromRect:presentationRect];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction shareText:selectedTerm fromRect:presentationRect];
#endif
}

- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant showTextServiceFor:selectedTerm fromRect:presentationRect];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction addShortcutForText:selectedTerm fromRect:presentationRect];
#endif
}

- (void)scheduleReplacementsForText:(NSString *)text
{
    [_textInteractionAssistant scheduleReplacementsForText:text];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction showReplacementsForText:text];
#endif
}

- (void)scheduleChineseTransliterationForText:(NSString *)text
{
    [_textInteractionAssistant scheduleChineseTransliterationForText:text];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction transliterateChineseForText:text];
#endif
}

- (void)translate:(NSString *)text fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant translate:text fromRect:presentationRect];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction translateText:text fromRect:presentationRect];
#endif
}

- (void)selectWord
{
    [_textInteractionAssistant selectWord];
#if USE(BROWSERENGINEKIT)
    _showEditMenuAfterNextSelectionChange = YES;
#endif
}

- (void)selectAll:(id)sender
{
    [_textInteractionAssistant selectAll:sender];
#if USE(BROWSERENGINEKIT)
    _showEditMenuAfterNextSelectionChange = YES;
#endif
}

#if USE(BROWSERENGINEKIT)

- (void)showEditMenuTimerFired
{
    [self stopShowEditMenuTimer];
    [_asyncTextInteraction presentEditMenuForSelection];
}

- (void)stopShowEditMenuTimer
{
    [std::exchange(_showEditMenuTimer, nil) invalidate];
}

#endif // USE(BROWSERENGINEKIT)

#if USE(UICONTEXTMENU)

- (UIContextMenuInteraction *)contextMenuInteraction
{
#if USE(BROWSERENGINEKIT)
    if (_asyncTextInteraction)
        return [_asyncTextInteraction contextMenuInteraction];
#endif
    return [_textInteractionAssistant contextMenuInteraction];
}

- (void)setExternalContextMenuInteractionDelegate:(id<UIContextMenuInteractionDelegate>)delegate
{
    [_textInteractionAssistant setExternalContextMenuInteractionDelegate:delegate];
#if USE(BROWSERENGINEKIT)
    [_asyncTextInteraction setContextMenuInteractionDelegate:delegate];
#endif
}

#endif // USE(UICONTEXTMENU)

@end

#endif // PLATFORM(IOS_FAMILY)

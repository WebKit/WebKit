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
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(UIKit) // NOLINT
SOFT_LINK_CLASS_OPTIONAL(UIKit, UIAsyncTextInteraction)

@implementation WKTextInteractionWrapper {
    RetainPtr<UIWKTextInteractionAssistant> _textInteractionAssistant;
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    RetainPtr<UIAsyncTextInteraction> _asyncTextInteraction;
#endif
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    static bool hasAsyncTextInteraction = !!getUIAsyncTextInteractionClass();
    if (hasAsyncTextInteraction && view.shouldUseAsyncInteractions) {
        _asyncTextInteraction = adoptNS([allocUIAsyncTextInteractionInstance() init]);
        [view addInteraction:_asyncTextInteraction.get()];
    } else
#endif
        _textInteractionAssistant = adoptNS([[UIWKTextInteractionAssistant alloc] initWithView:view]);

    return self;
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
#endif
}

- (void)selectionChanged
{
    [_textInteractionAssistant selectionChanged];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction selectionChanged];
#endif
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
    [_asyncTextInteraction dismissEditMenuForSelection];
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = NO;
#endif
}

- (void)didEndScrollingOverflow
{
    [_textInteractionAssistant didEndScrollingOverflow];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction presentEditMenuForSelection];
    [_asyncTextInteraction textSelectionDisplayInteraction].activated = YES;
#endif
}

- (void)willStartScrollingOrZooming
{
    [_textInteractionAssistant willStartScrollingOrZooming];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction dismissEditMenuForSelection];
#endif
}

- (void)didEndScrollingOrZooming
{
    [_textInteractionAssistant didEndScrollingOrZooming];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction presentEditMenuForSelection];
#endif
}

- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(UIWKSelectionFlags)flags
{
    [_textInteractionAssistant selectionChangedWithGestureAt:point withGesture:gestureType withState:gestureState withFlags:flags];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction selectionChangedWithGestureAt:point withGesture:gestureType withState:gestureState withFlags:flags];
#endif
}

- (void)showDictionaryFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant showDictionaryFor:selectedTerm fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction showDictionaryFor:selectedTerm fromRect:presentationRect];
#endif
}

- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch withFlags:(UIWKSelectionFlags)flags
{
    [_textInteractionAssistant selectionChangedWithTouchAt:point withSelectionTouch:touch withFlags:flags];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction selectionChangedWithTouchAt:point withSelectionTouch:touch withFlags:flags];
#endif
}

- (void)lookup:(NSString *)textWithContext withRange:(NSRange)range fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant lookup:textWithContext withRange:range fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction lookup:textWithContext withRange:range fromRect:presentationRect];
#endif
}

- (void)showShareSheetFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant showShareSheetFor:selectedTerm fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction showShareSheetFor:selectedTerm fromRect:presentationRect];
#endif
}

- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant showTextServiceFor:selectedTerm fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction showTextServiceFor:selectedTerm fromRect:presentationRect];
#endif
}

- (void)scheduleReplacementsForText:(NSString *)text
{
    [_textInteractionAssistant scheduleReplacementsForText:text];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction scheduleReplacementsForText:text];
#endif
}

- (void)scheduleChineseTransliterationForText:(NSString *)text
{
    [_textInteractionAssistant scheduleChineseTransliterationForText:text];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction scheduleChineseTransliterationForText:text];
#endif
}

- (void)translate:(NSString *)text fromRect:(CGRect)presentationRect
{
    [_textInteractionAssistant translate:text fromRect:presentationRect];
#if HAVE(UI_ASYNC_TEXT_INTERACTION)
    [_asyncTextInteraction translate:text fromRect:presentationRect];
#endif
}

- (void)selectWord
{
    [_textInteractionAssistant selectWord];
}

- (void)selectAll:(id)sender
{
    [_textInteractionAssistant selectAll:sender];
}

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

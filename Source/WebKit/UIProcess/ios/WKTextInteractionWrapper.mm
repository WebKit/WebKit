/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#import "EditorState.h"
#import "RemoteLayerTreeNode.h"
#import "RemoteLayerTreeViews.h"
#import "UIKitUtilities.h"
#import "WKContentViewInteraction.h"
#import "WebPageProxy.h"
#import <WebCore/TileController.h>
#import <wtf/TZoneMallocInlines.h>

@interface WKTextInteractionWrapper ()

@property (nonatomic, readonly, weak) WKContentView *view;
@property (nonatomic) BOOL shouldRestoreEditMenuAfterOverflowScrolling;

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
@property (nonatomic, readonly) UITextSelectionDisplayInteraction *textSelectionDisplayInteraction;
#endif

#if USE(BROWSERENGINEKIT)
@property (nonatomic, readonly) BETextInteraction *asyncTextInteraction;
#endif

@end

namespace WebKit {

enum class DeactivateSelection : bool { No, Yes };

class HideEditMenuScope {
    WTF_MAKE_TZONE_ALLOCATED(HideEditMenuScope);
    WTF_MAKE_NONCOPYABLE(HideEditMenuScope);
public:
    HideEditMenuScope(WKTextInteractionWrapper *wrapper, DeactivateSelection deactivateSelection)
        : m_wrapper { wrapper }
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
        , m_reactivateSelection { deactivateSelection == DeactivateSelection::Yes }
#endif
    {
        [wrapper.textInteractionAssistant willStartScrollingOverflow];
#if USE(BROWSERENGINEKIT)
        wrapper.shouldRestoreEditMenuAfterOverflowScrolling = [wrapper view].isPresentingEditMenu;
        [wrapper.asyncTextInteraction dismissEditMenuForSelection];
#endif

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
        if (deactivateSelection == DeactivateSelection::Yes)
            wrapper.textSelectionDisplayInteraction.activated = NO;
#endif
    }

    ~HideEditMenuScope()
    {
        RetainPtr wrapper = m_wrapper;
        [[wrapper textInteractionAssistant] didEndScrollingOverflow];
#if USE(BROWSERENGINEKIT)
        if ([wrapper shouldRestoreEditMenuAfterOverflowScrolling]) {
            [wrapper setShouldRestoreEditMenuAfterOverflowScrolling:NO];
            [[wrapper asyncTextInteraction] presentEditMenuForSelection];
        }
#endif
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
        if (m_reactivateSelection)
            [wrapper textSelectionDisplayInteraction].activated = YES;
#endif
    }

private:
    __weak WKTextInteractionWrapper *m_wrapper { nil };
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    bool m_reactivateSelection { false };
#endif
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(HideEditMenuScope);

} // namespace WebKit

@implementation WKTextInteractionWrapper {
    __weak WKContentView *_view;
    RetainPtr<UIWKTextInteractionAssistant> _textInteractionAssistant;
    std::unique_ptr<WebKit::HideEditMenuScope> _hideEditMenuScope;
#if USE(BROWSERENGINEKIT)
    RetainPtr<BETextInteraction> _asyncTextInteraction;
    RetainPtr<NSTimer> _showEditMenuTimer;
    BOOL _showEditMenuAfterNextSelectionChange;
#endif
    Vector<WeakObjCPtr<UIView>> _managedTextSelectionViews;
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

#if PLATFORM(APPLETV) && HAVE(UI_TEXT_CONTEXT_MENU_INTERACTION)
    if (RetainPtr contextMenuInteraction = [self contextMenuInteraction]) {
        [view removeInteraction:contextMenuInteraction.get()];
        [self setExternalContextMenuInteractionDelegate:nil];
    }
#endif

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

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)

- (UITextSelectionDisplayInteraction *)textSelectionDisplayInteraction
{
#if USE(BROWSERENGINEKIT)
    if (_asyncTextInteraction)
        return [_asyncTextInteraction textSelectionDisplayInteraction];
#endif

    for (id<UIInteraction> interaction in _view.interactions) {
        if (auto* selectionInteraction = dynamic_objc_cast<UITextSelectionDisplayInteraction>(interaction))
            return selectionInteraction;
    }

    return nil;
}

#endif // HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)

- (NSArray<UIView *> *)managedTextSelectionViews
{
    RetainPtr views = adoptNS([[NSMutableArray alloc] initWithCapacity:_managedTextSelectionViews.size()]);
    _managedTextSelectionViews.removeAllMatching([views](auto& weakView) {
        if (RetainPtr view = weakView.get()) {
            [views addObject:view.get()];
            return false;
        }
        return true;
    });
    return views.autorelease();
}

- (UIView<UITextSelectionHighlightView> *)selectionHighlightView
{
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    return [[self textSelectionDisplayInteraction] highlightView];
#else
    return nil;
#endif
}

- (void)prepareToMoveSelectionContainer:(UIView *)newContainer
{
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    RetainPtr displayInteraction = [self textSelectionDisplayInteraction];
    RetainPtr highlightView = [displayInteraction highlightView];
    if ([highlightView superview] != newContainer) {
        NSMutableSet<UIView *> *viewsBeforeInstallingInteraction = [NSMutableSet set];
        for (UIView *subview in newContainer.subviews)
            [viewsBeforeInstallingInteraction addObject:subview];

        // When the display interaction is in the activated state, calling these delegate methods tells it
        // to remove and reparent all internally managed views (e.g. selection highlight views, selection
        // handles) in the new selection container.
        [self activateSelection];
        [displayInteraction willMoveToView:_view];
        [displayInteraction didMoveToView:_view];

        _managedTextSelectionViews = { };
        for (UIView *subview in newContainer.subviews) {
            if (![viewsBeforeInstallingInteraction containsObject:subview])
                _managedTextSelectionViews.append(subview);
        }
    }

    if (newContainer == _view)
        return;

    auto findParentViewBelowNewContainer = [&](UIView *view) -> RetainPtr<UIView> {
        if (view == newContainer)
            return nil;

        for (RetainPtr foundView = view; foundView; foundView = [foundView superview]) {
            if (foundView == _view)
                return nil;

            if ([foundView superview] == newContainer)
                return foundView;
        }

        return nil;
    };

    RetainPtr viewsIntersectingSelection = [_view allViewsIntersectingSelectionRange];
    RetainPtr subviewsBeforeSelection = adoptNS([NSMutableSet new]);
    for (UIView *view in viewsIntersectingSelection.get()) {
        if (RetainPtr parentBelowNewContainer = findParentViewBelowNewContainer(view))
            [subviewsBeforeSelection addObject:parentBelowNewContainer.get()];
    }

    RefPtr page = [_view page];
    bool insertSelectionAfterCompositingViews = page && page->editorState().visualData->enclosingLayerUsesContentsLayer;

    // Ensure that the selection highlight is inserted after all `subviewsBeforeSelection`.
    __block std::optional<NSUInteger> indexOfHighlightView;
    __block std::optional<NSUInteger> indexToInsertManagedSelectionViews = 0;
    RetainPtr tileGridContainerName = WebCore::TileController::tileGridContainerLayerName().createNSString();
    [[newContainer subviews] enumerateObjectsUsingBlock:^(UIView *view, NSUInteger index, BOOL*) {
        if (view == highlightView) {
            indexOfHighlightView = index;
            return;
        }

        bool insertSelectionAfterView = [view.layer.name isEqualToString:tileGridContainerName.get()]
            || [subviewsBeforeSelection containsObject:view]
            || (insertSelectionAfterCompositingViews && [view conformsToProtocol:@protocol(WKContentControlled)]);

        if (insertSelectionAfterView)
            indexToInsertManagedSelectionViews = index + 1;
    }];

    if (indexToInsertManagedSelectionViews == indexOfHighlightView)
        return;

    // The first managed selection view in the array of subviews should be the highlight view.
    // If there are any other views between the tile grid container and the highlight view,
    // reposition the managed selection views above the tile grid container.
    for (UIView *view in self.managedTextSelectionViews.reverseObjectEnumerator) {
        if (newContainer == view.superview)
            [newContainer insertSubview:view atIndex:*indexToInsertManagedSelectionViews];
    }
#endif // HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
}

- (void)setNeedsSelectionUpdate
{
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    [self.textSelectionDisplayInteraction setNeedsSelectionUpdate];
#endif
}

- (void)activateSelection
{
#if USE(BROWSERENGINEKIT)
    self.textSelectionDisplayInteraction.activated = YES;
#else
    [_textInteractionAssistant activateSelection];
#endif
}

- (void)deactivateSelection
{
#if USE(BROWSERENGINEKIT)
    self.textSelectionDisplayInteraction.activated = NO;
    _showEditMenuAfterNextSelectionChange = NO;
    [self stopShowEditMenuTimer];
#else
    [_textInteractionAssistant deactivateSelection];
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

- (WKContentView *)view
{
    return _view;
}

- (void)willBeginDragLift
{
    if (_hideEditMenuScope)
        return;

#if USE(UICONTEXTMENU)
    if (self.contextMenuInteraction._wk_isMenuVisible)
        return;
#endif

    _hideEditMenuScope = WTF::makeUnique<WebKit::HideEditMenuScope>(self, WebKit::DeactivateSelection::Yes);
}

- (void)didConcludeDrop
{
    _hideEditMenuScope = nullptr;
}

- (void)willStartScrollingOverflow:(UIScrollView *)scrollView
{
    if (_hideEditMenuScope)
        return;

    auto deactivateSelection = [_view _shouldHideSelectionDuringOverflowScroll:scrollView] ? WebKit::DeactivateSelection::Yes : WebKit::DeactivateSelection::No;
    _hideEditMenuScope = WTF::makeUnique<WebKit::HideEditMenuScope>(self, deactivateSelection);
}

- (void)didEndScrollingOverflow
{
    _hideEditMenuScope = nullptr;
}

- (void)reset
{
    _shouldRestoreEditMenuAfterOverflowScrolling = NO;
    _hideEditMenuScope = nullptr;
}

- (void)willStartScrollingOrZooming
{
    // FIXME: Adopt `HideEditMenuScope` here once `BETextInput` is used on all iOS-family platforms.
    [_textInteractionAssistant willStartScrollingOrZooming];
#if USE(BROWSERENGINEKIT)
    _shouldRestoreEditMenuAfterOverflowScrolling = _view.isPresentingEditMenu;
    [_asyncTextInteraction dismissEditMenuForSelection];
#endif
}

- (void)didEndScrollingOrZooming
{
    // FIXME: Adopt `HideEditMenuScope` here once `BETextInput` is used on all iOS-family platforms.
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

- (BETextInteraction *)asyncTextInteraction
{
    return _asyncTextInteraction.get();
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

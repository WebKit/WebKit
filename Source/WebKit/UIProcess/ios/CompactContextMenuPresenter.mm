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
#import "CompactContextMenuPresenter.h"

#if USE(UICONTEXTMENU)

#import <wtf/TZoneMallocInlines.h>

@interface UIContextMenuInteraction (SPI)
- (void)_presentMenuAtLocation:(CGPoint)location;
@end

#if HAVE(UI_BUTTON_PERFORM_PRIMARY_ACTION)
@interface UIButton (Staging_112292156)
- (void)performPrimaryAction;
@end
#endif

@interface WKCompactContextMenuPresenterButton : UIButton
@property (nonatomic, weak) id<UIContextMenuInteractionDelegate> externalDelegate;
@end

@implementation WKCompactContextMenuPresenterButton

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location
{
    RetainPtr externalDelegate = _externalDelegate;
    if ([externalDelegate respondsToSelector:@selector(contextMenuInteraction:configurationForMenuAtLocation:)])
        return [externalDelegate contextMenuInteraction:interaction configurationForMenuAtLocation:location];

    return [super contextMenuInteraction:interaction configurationForMenuAtLocation:location];
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configuration:(UIContextMenuConfiguration *)configuration highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier
{
    RetainPtr externalDelegate = _externalDelegate;
    if ([externalDelegate respondsToSelector:@selector(contextMenuInteraction:configuration:highlightPreviewForItemWithIdentifier:)])
        return [externalDelegate contextMenuInteraction:interaction configuration:configuration highlightPreviewForItemWithIdentifier:identifier];

    return [super contextMenuInteraction:interaction configuration:configuration highlightPreviewForItemWithIdentifier:identifier];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionAnimating>)animator
{
    [super contextMenuInteraction:interaction willDisplayMenuForConfiguration:configuration animator:animator];

    RetainPtr externalDelegate = _externalDelegate;
    if ([externalDelegate respondsToSelector:@selector(contextMenuInteraction:willDisplayMenuForConfiguration:animator:)])
        [externalDelegate contextMenuInteraction:interaction willDisplayMenuForConfiguration:configuration animator:animator];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionAnimating>)animator
{
    [super contextMenuInteraction:interaction willEndForConfiguration:configuration animator:animator];

    RetainPtr externalDelegate = _externalDelegate;
    if ([externalDelegate respondsToSelector:@selector(contextMenuInteraction:willEndForConfiguration:animator:)])
        [externalDelegate contextMenuInteraction:interaction willEndForConfiguration:configuration animator:animator];
}

@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CompactContextMenuPresenter);

CompactContextMenuPresenter::CompactContextMenuPresenter(UIView *rootView, id<UIContextMenuInteractionDelegate> delegate)
    : m_rootView(rootView)
    , m_button([WKCompactContextMenuPresenterButton buttonWithType:UIButtonTypeSystem])
{
    [m_button setExternalDelegate:delegate];
    [m_button layer].zPosition = CGFLOAT_MIN;
    [m_button setHidden:YES];
    [m_button setUserInteractionEnabled:NO];
    [m_button setContextMenuInteractionEnabled:YES];
    [m_button setShowsMenuAsPrimaryAction:YES];
}

CompactContextMenuPresenter::~CompactContextMenuPresenter()
{
    [UIView performWithoutAnimation:^{
        dismiss();
    }];
    [m_button removeFromSuperview];
}

void CompactContextMenuPresenter::present(CGPoint locationInRootView)
{
    present(CGRect { locationInRootView, CGSizeZero });
}

UIContextMenuInteraction *CompactContextMenuPresenter::interaction() const
{
    return [m_button contextMenuInteraction];
}

void CompactContextMenuPresenter::present(CGRect rectInRootView)
{
    RetainPtr rootView = m_rootView;
    if (!rootView.get().window)
        return;

    [m_button setFrame:rectInRootView];
    if (![m_button superview])
        [rootView addSubview:m_button.get()];

#if HAVE(UI_BUTTON_PERFORM_PRIMARY_ACTION)
    static BOOL canPerformPrimaryAction = [UIButton instancesRespondToSelector:@selector(performPrimaryAction)];
    if (canPerformPrimaryAction) {
        [m_button performPrimaryAction];
        return;
    }
#endif

    [protect(interaction()) _presentMenuAtLocation:CGPointMake(CGRectGetMidX(rectInRootView), CGRectGetMidY(rectInRootView))];
}

void CompactContextMenuPresenter::dismiss()
{
    [[m_button contextMenuInteraction] dismissMenu];
}

void CompactContextMenuPresenter::updateVisibleMenu(UIMenu *(^updateBlock)(UIMenu *))
{
    [protect(interaction()) updateVisibleMenuWithBlock:updateBlock];
}

} // namespace WebKit

#endif // USE(UICONTEXTMENU)

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

@interface WKCompactContextMenuPresenterControl : UIControl <WKCompactContextMenuPresenter>
@end

@interface WKCompactContextMenuPresenterButton : UIButton <WKCompactContextMenuPresenter>
@end

@implementation WKCompactContextMenuPresenterControl
@synthesize externalDelegate = _externalDelegate;

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

@implementation WKCompactContextMenuPresenterButton
@synthesize externalDelegate = _externalDelegate;

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

CompactContextMenuPresenter::CompactContextMenuPresenter(UIView *rootView, id<UIContextMenuInteractionDelegate> delegate, ShowsMenuFromSource showsMenuFromSource)
    : m_rootView(rootView)
{
    if (showsMenuFromSource == ShowsMenuFromSource::Yes)
        m_control = [WKCompactContextMenuPresenterButton buttonWithType:UIButtonTypeSystem];
    else
        m_control = adoptNS([[WKCompactContextMenuPresenterControl alloc] init]);

    [m_control setExternalDelegate:delegate];
    [m_control layer].zPosition = CGFLOAT_MIN;
    [m_control setHidden:YES];
    [m_control setUserInteractionEnabled:NO];
    [m_control setContextMenuInteractionEnabled:YES];
    [m_control setShowsMenuAsPrimaryAction:YES];
}

CompactContextMenuPresenter::~CompactContextMenuPresenter()
{
    [UIView performWithoutAnimation:^{
        dismiss();
    }];
    [m_control removeFromSuperview];
}

void CompactContextMenuPresenter::present(CGPoint locationInRootView)
{
    present(CGRect { locationInRootView, CGSizeZero });
}

UIContextMenuInteraction *CompactContextMenuPresenter::interaction() const
{
    return [m_control contextMenuInteraction];
}

void CompactContextMenuPresenter::present(CGRect rectInRootView)
{
    RetainPtr rootView = m_rootView;
    if (!rootView.get().window)
        return;

    [m_control setFrame:rectInRootView];
    if (![m_control superview])
        [rootView addSubview:m_control.get()];

    [m_control performPrimaryAction];
}

void CompactContextMenuPresenter::dismiss()
{
    [[m_control contextMenuInteraction] dismissMenu];
}

void CompactContextMenuPresenter::updateVisibleMenu(UIMenu *(^updateBlock)(UIMenu *))
{
    [protect(interaction()) updateVisibleMenuWithBlock:updateBlock];
}

} // namespace WebKit

#endif // USE(UICONTEXTMENU)

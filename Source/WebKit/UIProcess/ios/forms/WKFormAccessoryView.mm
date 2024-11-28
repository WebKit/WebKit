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
#import "WKFormAccessoryView.h"

#if PLATFORM(IOS_FAMILY)

#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {

static constexpr auto fixedSpaceBetweenButtonItems = 6;

inline static UIFont *doneButtonFont()
{
    return [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
}

inline static UIFont *singleLineAutoFillButtonFont()
{
    return [UIFont fontWithDescriptor:[UIFontDescriptor preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline] size:0];
}

inline static UIFont *multipleLineAutoFillButtonFont()
{
    return [UIFont fontWithDescriptor:[UIFontDescriptor preferredFontDescriptorWithTextStyle:UIFontTextStyleFootnote] size:0];
}

inline static UIImage *upArrow()
{
    return [UIImage systemImageNamed:@"chevron.up"];
}

inline static UIImage *downArrow()
{
    return [UIImage systemImageNamed:@"chevron.down"];
}

inline static RetainPtr<UIToolbar> createToolbarWithItems(NSArray<UIBarButtonItem *> *items)
{
    auto bar = adoptNS([[UIToolbar alloc] init]);
    [bar setBarStyle:UIBarStyleDefault];
    [bar setTranslucent:YES];
    [bar setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [bar setItems:items];
    return bar;
}

} // namespace WebKit

@implementation WKFormAccessoryView {
    __weak id<WKFormAccessoryViewDelegate> _delegate;
    RetainPtr<UIToolbar> _leftToolbar;
    RetainPtr<UIToolbar> _rightToolbar;
    RetainPtr<UIBarButtonItem> _doneButton;
    RetainPtr<UIBarButtonItem> _flexibleSpaceItem;
    RetainPtr<UIBarButtonItem> _previousItem;
    RetainPtr<UIBarButtonItem> _nextItem;
    RetainPtr<UIBarButtonItem> _nextPreviousSpacer;
    RetainPtr<UIBarButtonItem> _autoFillButtonItem;
    RetainPtr<UIBarButtonItem> _autoFillButtonItemSpacer;
    RetainPtr<UIBarButtonItemGroup> _buttonGroupAutoFill;
    RetainPtr<UIBarButtonItemGroup> _buttonGroupNavigation;
    RetainPtr<UIView> _leftContainerView;
    RetainPtr<UIView> _rightContainerView;
    BOOL _usesUniversalControlBar;
}

- (instancetype)_initForUniversalControlBar:(UITextInputAssistantItem *)inputAssistant
{
    _usesUniversalControlBar = YES;

    auto button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button addTarget:self action:@selector(_autoFill) forControlEvents:UIControlEventTouchUpInside];
    auto buttonFrame = CGRectMake(0, 0, 0, self.bounds.size.height);
    button.frame = buttonFrame;
    button.tintColor = UIColor.labelColor;

    auto label = button.titleLabel;
    label.frame = buttonFrame;
    label.numberOfLines = 2;
    _autoFillButtonItem = adoptNS([[UIBarButtonItem alloc] initWithCustomView:button]);

    _previousItem = adoptNS([[UIBarButtonItem alloc] initWithImage:WebKit::upArrow() style:UIBarButtonItemStylePlain target:self action:@selector(_previousTapped)]);
    [_previousItem setTintColor:UIColor.blackColor];
    [_previousItem setEnabled:NO];

    _nextItem = adoptNS([[UIBarButtonItem alloc] initWithImage:WebKit::downArrow() style:UIBarButtonItemStylePlain target:self action:@selector(_nextTapped)]);
    [_nextItem setTintColor:UIColor.blackColor];
    [_nextItem setEnabled:NO];

    _buttonGroupAutoFill = adoptNS([[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[ _autoFillButtonItem.get() ] representativeItem:nil]);
    [_buttonGroupAutoFill setHidden:YES];
    _buttonGroupNavigation = adoptNS([[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[ _previousItem.get(), _nextItem.get() ] representativeItem:nil]);

    auto assistantItem = inputAssistant;
    auto leadingGroup = adoptNS([assistantItem.leadingBarButtonGroups mutableCopy]);
    [leadingGroup addObject:_buttonGroupAutoFill.get()];
    assistantItem.leadingBarButtonGroups = leadingGroup.get();

    auto trailingGroup = adoptNS([assistantItem.trailingBarButtonGroups mutableCopy]);
    [trailingGroup addObject:_buttonGroupNavigation.get()];
    assistantItem.trailingBarButtonGroups = trailingGroup.get();

    return self;
}

- (instancetype)initWithInputAssistantItem:(UITextInputAssistantItem *)inputAssistant delegate:(id<WKFormAccessoryViewDelegate>)delegate
{
    self = [self init];
    if (!self)
        return nil;

    _delegate = delegate;

    auto subviews = self.subviews;
    _leftContainerView = subviews.firstObject;
    _rightContainerView = subviews.lastObject;

    if (!PAL::currentUserInterfaceIdiomIsSmallScreen())
        return [self _initForUniversalControlBar:inputAssistant];

    // Add all items to left side toolbar. If the keyboard is split, the right-side toolbar will hold the AutoFill button.
    auto items = [NSMutableArray array];

    _previousItem = adoptNS([[UIBarButtonItem alloc] initWithImage:WebKit::upArrow() style:UIBarButtonItemStylePlain target:self action:@selector(_previousTapped)]);
    [_previousItem setEnabled:NO];
    [items addObject:_previousItem.get()];

    _nextPreviousSpacer = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace target:nil action:nil]);
    [_nextPreviousSpacer setWidth:WebKit::fixedSpaceBetweenButtonItems];
    [items addObject:_nextPreviousSpacer.get()];

    _nextItem = adoptNS([[UIBarButtonItem alloc] initWithImage:WebKit::downArrow() style:UIBarButtonItemStylePlain target:self action:@selector(_nextTapped)]);
    [_nextItem setEnabled:NO];
    [items addObject:_nextItem.get()];

    _flexibleSpaceItem = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil]);
    _autoFillButtonItemSpacer = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace target:nil action:nil]);
    [_autoFillButtonItemSpacer setWidth:WebKit::fixedSpaceBetweenButtonItems];

    // iPad doesn't show the "Done" button since the keyboard has its own dismiss key.
    _doneButton = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(_done)]);
    [items addObject:_flexibleSpaceItem.get()];
    [items addObject:_doneButton.get()];

    _leftToolbar = WebKit::createToolbarWithItems(items);
    [_leftContainerView addSubview:_leftToolbar.get()];

    _rightToolbar = WebKit::createToolbarWithItems(@[ ]);
    [_rightContainerView addSubview:_rightToolbar.get()];

    [self _updateFrame];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME (262139): Adopt -registerForTraitChanges: once iOS 17 is the minimum supported iOS version.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_updateFrame) name:UIApplicationDidChangeStatusBarOrientationNotification object:nil];
    ALLOW_DEPRECATED_DECLARATIONS_END

    return self;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
// FIXME (262139): Adopt -registerForTraitChanges: once iOS 17 is the minimum supported iOS version.
- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection
{
    [super traitCollectionDidChange:previousTraitCollection];

    auto newTraitCollection = self.traitCollection;
    if (![newTraitCollection hasDifferentColorAppearanceComparedToTraitCollection:previousTraitCollection])
        return;

    auto doneButtonAttributes = [NSMutableDictionary dictionaryWithObject:WebKit::doneButtonFont() forKey:NSFontAttributeName];

    UIColor *tintColor = nil;

    if (newTraitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
        tintColor = UIColor.whiteColor;
        doneButtonAttributes[NSForegroundColorAttributeName] = tintColor;
    }

    self.tintColor = tintColor;

    [_doneButton setTitleTextAttributes:doneButtonAttributes forState:UIControlStateNormal];
}
ALLOW_DEPRECATED_DECLARATIONS_END
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (void)_updateFrame
{
    auto frame = self.frame;
    frame.size.height = [_leftToolbar sizeThatFits:frame.size].height;
    self.frame = frame;
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    if ((_usesUniversalControlBar && [_buttonGroupAutoFill isHidden]) || !_autoFillButtonItem)
        return;

    [self _refreshAutofillPresentation];
    [self _applyTreatmentToAutoFillLabel];

    [_autoFillButtonItem setWidth:self._autoFillButton.frame.size.width];
}

- (void)_done
{
    [_delegate accessoryViewDone:self];
}

- (void)_previousTapped
{
    [_delegate accessoryView:self tabInDirection:WebKit::TabDirection::Previous];
}

- (void)_nextTapped
{
    [_delegate accessoryView:self tabInDirection:WebKit::TabDirection::Next];
}

- (void)_autoFill
{
    [_delegate accessoryViewAutoFill:self];
}

- (UIButton *)_autoFillButton
{
    return dynamic_objc_cast<UIButton>([_autoFillButtonItem customView]);
}

- (void)_refreshAutofillPresentation
{
    if (!_autoFillButtonItem)
        return;

    // For the AutoFill button there are three cases:
    // 1) iPhone. There will be a Done button that dictates AutoFill button width.
    // 2) iPad, unified keyboard. No Done button, AutoFill goes in left toolbar.
    // 3) iPad, split keyboard. No Done button, AutoFill goes in right toolbar.
    BOOL isSplit = CGRectGetMaxX([_leftContainerView frame]) != CGRectGetMinX([_rightContainerView frame]);

    auto leftItems = adoptNS([[_leftToolbar items] mutableCopy]);
    auto rightItems = adoptNS([[_rightToolbar items] mutableCopy]);

    [leftItems removeObject:_autoFillButtonItemSpacer.get()];
    [leftItems removeObject:_autoFillButtonItem.get()];
    [rightItems removeObject:_flexibleSpaceItem.get()];
    [rightItems removeObject:_autoFillButtonItem.get()];

    if (isSplit) {
        [rightItems insertObject:_flexibleSpaceItem.get() atIndex:0];
        [rightItems addObject:_autoFillButtonItem.get()];
    } else {
        NSUInteger indexAfterNextItem = 0;
        if ([leftItems containsObject:_nextItem.get()])
            indexAfterNextItem = [leftItems indexOfObject:_nextItem.get()] + 1;
        [leftItems insertObject:_autoFillButtonItemSpacer.get() atIndex:indexAfterNextItem];
        [leftItems insertObject:_autoFillButtonItem.get() atIndex:indexAfterNextItem + 1];
    }

    [_leftToolbar setItems:leftItems.get()];
    [_rightToolbar setItems:rightItems.get()];
}

- (void)_applyTreatmentToAutoFillLabel
{
    auto button = self._autoFillButton;
    __block UILabel *label = nil;
    // This -performWithoutAnimation: works around <rdar://14476163>.
    [UIView performWithoutAnimation:^{
        label = button.titleLabel;
    }];
    auto labelFrame = label.frame;
    label.font = WebKit::singleLineAutoFillButtonFont();

    BOOL compactScreen = self.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact;
    auto autoFillMaximumButtonWidth = compactScreen ? 180 : 200;

    if ([label sizeThatFits:CGSizeMake(CGFLOAT_MAX, labelFrame.size.height)].width > autoFillMaximumButtonWidth)
        label.font = WebKit::multipleLineAutoFillButtonFont();

    labelFrame.size.width = [label sizeThatFits:CGSizeMake(autoFillMaximumButtonWidth, CGFLOAT_MAX)].width;
    label.frame = labelFrame;

    auto buttonFrame = button.frame;
    buttonFrame.size.width = labelFrame.size.width;
    button.frame = buttonFrame;
}

- (void)hideAutoFillButton
{
    if (!_autoFillButtonItem)
        return;

    auto leftItems = adoptNS([[_leftToolbar items] mutableCopy]);
    [leftItems removeObject:_autoFillButtonItem.get()];
    [_leftToolbar setItems:leftItems.get()];

    if (_usesUniversalControlBar)
        [_buttonGroupAutoFill setHidden:YES];
    else
        _autoFillButtonItem = nil;
}

- (void)showAutoFillButtonWithTitle:(NSString *)title
{
    auto button = self._autoFillButton;
    if (!button) {
        ASSERT(!_autoFillButtonItem);
        button = [UIButton buttonWithType:UIButtonTypeSystem];
        [button addTarget:self action:@selector(_autoFill) forControlEvents:UIControlEventTouchUpInside];
        auto buttonFrame = CGRectMake(0, 0, 0, self.bounds.size.height);
        button.frame = buttonFrame;

        auto label = button.titleLabel;
        label.frame = buttonFrame;
        label.numberOfLines = 2;
        _autoFillButtonItem = adoptNS([[UIBarButtonItem alloc] initWithCustomView:button]);
    }

    if (![title isEqualToString:[button titleForState:UIControlStateNormal]])
        [button setTitle:title forState:UIControlStateNormal];

    if (_usesUniversalControlBar)
        [_buttonGroupAutoFill setHidden:NO];

    [self setNeedsLayout];
}

- (UIBarButtonItem *)autoFillButtonItem
{
    return _autoFillButtonItem.get();
}

- (void)setNextPreviousItemsVisible:(BOOL)visible
{
    if (_usesUniversalControlBar) {
        [_buttonGroupNavigation setHidden:!visible];
        return;
    }

    auto leftToolbarItems = [_leftToolbar items];
    BOOL toolbarContainsPreviousItem = [leftToolbarItems containsObject:_previousItem.get()];
    BOOL toolbarContainsNextPreviousSpacer = [leftToolbarItems containsObject:_nextPreviousSpacer.get()];
    BOOL toolbarContainsNextItem = [leftToolbarItems containsObject:_nextItem.get()];

    if (visible && toolbarContainsPreviousItem && toolbarContainsNextPreviousSpacer && toolbarContainsNextItem)
        return;

    if (!visible && !toolbarContainsPreviousItem && !toolbarContainsNextPreviousSpacer && !toolbarContainsNextItem)
        return;

    auto newLeftToolbarItems = adoptNS(leftToolbarItems.mutableCopy);
    if (visible) {
        if (!toolbarContainsNextItem)
            [newLeftToolbarItems insertObject:_nextItem.get() atIndex:0];
        if (!toolbarContainsNextPreviousSpacer)
            [newLeftToolbarItems insertObject:_nextPreviousSpacer.get() atIndex:0];
        if (!toolbarContainsPreviousItem)
            [newLeftToolbarItems insertObject:_previousItem.get() atIndex:0];
    } else {
        if (toolbarContainsPreviousItem)
            [newLeftToolbarItems removeObject:_previousItem.get()];
        if (toolbarContainsNextPreviousSpacer)
            [newLeftToolbarItems removeObject:_nextPreviousSpacer.get()];
        if (toolbarContainsNextItem)
            [newLeftToolbarItems removeObject:_nextItem.get()];
    }

    [_leftToolbar setItems:newLeftToolbarItems.get()];

    [self setNeedsLayout];
}

- (void)setNextEnabled:(BOOL)enabled
{
    [_nextItem setEnabled:enabled];
}

- (BOOL)isNextEnabled
{
    return [_nextItem isEnabled];
}

- (void)setPreviousEnabled:(BOOL)enabled
{
    [_previousItem setEnabled:enabled];
}

- (BOOL)isPreviousEnabled
{
    return [_previousItem isEnabled];
}

- (CGFloat)contentRatio
{
    // FIXME: Adopt future API to make the left container fill the content width instead
    // of overriding this method.
    return 1;
}

@end

#endif // PLATFORM(IOS_FAMILY)

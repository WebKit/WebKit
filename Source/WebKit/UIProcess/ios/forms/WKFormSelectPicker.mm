/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#import "WKFormSelectPicker.h"

#if PLATFORM(IOS_FAMILY)

#import "CompactContextMenuPresenter.h"
#import "UIKitUtilities.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPopover.h"
#import "WKFormSelectControl.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebPageProxy.h"
#import "WebPreferencesDefaultValues.h"
#import <UIKit/UIKit.h>
#import <WebCore/LocalizedStrings.h>
#import <numbers>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/WeakObjCPtr.h>

using namespace WebKit;

static const float DisabledOptionAlpha = 0.3;
static const float GroupOptionTextColorAlpha = 0.5;

@interface UIPickerView (UIPickerViewInternal)
- (BOOL)allowsMultipleSelection;
- (void)setAllowsMultipleSelection:(BOOL)aFlag;
- (UITableView*)tableViewForColumn:(NSInteger)column;
@end

@interface WKOptionPickerCell : UIPickerContentView {
    BOOL _disabled;
}

@property(nonatomic) BOOL disabled;

- (instancetype)initWithOptionItem:(const OptionItem&)item;

@end

@implementation WKOptionPickerCell

- (BOOL)_isSelectable
{
    return !self.disabled;
}

- (instancetype)init
{
    if (!(self = [super initWithFrame:CGRectZero]))
        return nil;
    [[self titleLabel] setLineBreakMode:NSLineBreakByTruncatingMiddle];
    return self;
}

- (instancetype)initWithOptionItem:(const OptionItem&)item
{
    if (!(self = [self init]))
        return nil;

    RetainPtr trimmedText = adoptNS([item.text.createNSString() mutableCopy]);
    CFStringTrimWhitespace((CFMutableStringRef)trimmedText.get());

    [[self titleLabel] setText:trimmedText.get()];
    [self setChecked:item.isSelected];
    [self setDisabled:item.disabled];
    if (_disabled)
        [[self titleLabel] setTextColor:[UIColor colorWithWhite:0.0 alpha:DisabledOptionAlpha]];

    return self;
}

@end


@interface WKOptionGroupPickerCell : WKOptionPickerCell
- (instancetype)initWithOptionItem:(const OptionItem&)item;
@end

@implementation WKOptionGroupPickerCell

- (instancetype)initWithOptionItem:(const OptionItem&)item
{
    if (!(self = [self init]))
        return nil;

    RetainPtr trimmedText = adoptNS([item.text.createNSString() mutableCopy]);
    CFStringTrimWhitespace((CFMutableStringRef)trimmedText.get());

    [[self titleLabel] setText:trimmedText.get()];
    [self setChecked:NO];
    [[self titleLabel] setTextColor:[UIColor colorWithWhite:0.0 alpha:GroupOptionTextColorAlpha]];
    [self setDisabled:YES];

    return self;
}

- (CGFloat)labelWidthForBounds:(CGRect)bounds
{
    return CGRectGetWidth(bounds) - [UIPickerContentView _checkmarkOffset];
}

- (void)layoutSubviews
{
    if (!self.titleLabel)
        return;

    CGRect bounds = self.bounds;
    self.titleLabel.frame = CGRectMake([UIPickerContentView _checkmarkOffset], 0, CGRectGetMaxX(bounds) - [UIPickerContentView _checkmarkOffset], CGRectGetHeight(bounds));
}

@end


@implementation WKMultipleSelectPicker {
    WeakObjCPtr<WKContentView> _view;
    NSTextAlignment _textAlignment;
    NSUInteger _singleSelectionIndex;
    bool _allowsMultipleSelection;
    CGFloat _layoutWidth;
    CGFloat _fontSize;
    CGFloat _maximumTextWidth;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super initWithFrame:CGRectZero]))
        return nil;

    _view = view;
    _allowsMultipleSelection = [view focusedElementInformation].isMultiSelect;
    _singleSelectionIndex = NSNotFound;
    [self setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [self setDataSource:self];
    [self setDelegate:self];
    [self _setUsesCheckedSelection:YES];

    [self _setMagnifierEnabled:NO];
    NSWritingDirection writingDirection = NSWritingDirectionLeftToRight;
    // FIXME: retrieve from WebProcess writing direction.
    _textAlignment = (writingDirection == NSWritingDirectionLeftToRight) ? NSTextAlignmentLeft : NSTextAlignmentRight;

    [self setAllowsMultipleSelection:_allowsMultipleSelection];

    CGRect frame = self.frame;
    frame.size = view.sizeForLegacyFormControlPickerViews;
    [self setFrame:frame];

    [self reloadAllComponents];

    if (!_allowsMultipleSelection) {
        const Vector<OptionItem>& selectOptions = [_view.get() focusedSelectElementOptions];
        for (size_t i = 0; i < selectOptions.size(); ++i) {
            const OptionItem& item = selectOptions[i];
            if (item.isGroup)
                continue;

            if (item.isSelected) {
                _singleSelectionIndex = i;
                [self selectRow:_singleSelectionIndex inComponent:0 animated:NO];
                break;
            }
        }
    }

    return self;
}

- (void)dealloc
{
    [self setDataSource:nil];
    [self setDelegate:nil];

    [super dealloc];
}

- (UIView *)controlView
{
    return self;
}

- (void)controlBeginEditing
{
}

- (void)controlUpdateEditing
{
    [self reloadAllComponents];
}

- (void)controlEndEditing
{
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    if (_singleSelectionIndex != NSNotFound) {
        [self selectRow:_singleSelectionIndex inComponent:0 animated:NO];
    }

    // Make sure all rows are sized properly after a rotation.
    if (_layoutWidth != self.frame.size.width) {
        [self reloadAllComponents];
        _layoutWidth = self.frame.size.width;
    }
}

- (UIView *)pickerView:(UIPickerView *)pickerView viewForRow:(NSInteger)rowIndex forComponent:(NSInteger)columnIndex reusingView:(UIView *)view
{
    auto& item = [_view.get() focusedSelectElementOptions][rowIndex];
    RetainPtr<WKOptionPickerCell> pickerItem;
    if (item.isGroup)
        pickerItem = adoptNS([[WKOptionGroupPickerCell alloc] initWithOptionItem:item]);
    else
        pickerItem = adoptNS([[WKOptionPickerCell alloc] initWithOptionItem:item]);

    // The cell starts out with a null frame. We need to set its frame now so we can find the right font size.
    UITableView *table = [pickerView tableViewForColumn:0];
    CGRect frame = [table rectForRowAtIndexPath:[NSIndexPath indexPathForRow:rowIndex inSection:0]];
    [pickerItem setFrame:frame];

    UILabel *titleTextLabel = [pickerItem titleLabel];
    float width = [pickerItem labelWidthForBounds:CGRectMake(0, 0, CGRectGetWidth(frame), CGRectGetHeight(frame))];
    ASSERT(width > 0);

    // Assume all cells have the same available text width.
    UIFont *font = titleTextLabel.font;
    if (width != _maximumTextWidth || _fontSize == 0) {
        _maximumTextWidth = width;
        _fontSize = adjustedFontSize(_maximumTextWidth, font, titleTextLabel.font.pointSize, [_view.get() focusedSelectElementOptions]);
    }

    [titleTextLabel setFont:[font fontWithSize:_fontSize]];
    [titleTextLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [titleTextLabel setNumberOfLines:2];
    [titleTextLabel setTextAlignment:_textAlignment];

    return pickerItem.autorelease();
}

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)aPickerView
{
    return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)columnIndex
{
    return [_view.get() focusedSelectElementOptions].size();
}

- (NSInteger)findItemIndexAt:(int)rowIndex
{
    RetainPtr view = _view.get();
    ASSERT(rowIndex >= 0 && (size_t)rowIndex < [view focusedSelectElementOptions].size());
    NSInteger itemIndex = 0;
    for (int i = 0; i < rowIndex; ++i) {
        if ([view focusedSelectElementOptions][i].isGroup)
            continue;
        itemIndex++;
    }

    ASSERT(itemIndex >= 0);
    return itemIndex;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)pickerView:(UIPickerView *)pickerView row:(int)rowIndex column:(int)columnIndex checked:(BOOL)isChecked
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    RetainPtr view = _view.get();
    auto numberOfOptions = static_cast<NSUInteger>([view focusedSelectElementOptions].size());
    if (numberOfOptions <= static_cast<NSUInteger>(rowIndex))
        return;

    auto& item = [view focusedSelectElementOptions][rowIndex];

    // FIXME: Remove this workaround once <rdar://problem/18745253> is fixed.
    // Group rows and disabled rows should not be checkable, but we are getting
    // this delegate for those rows. As a workaround, if we get this delegate
    // for a group or disabled row, reset the styles for the content view so it
    // still appears unselected.
    if (item.isGroup || item.disabled) {
        RetainPtr viewForRow = (UIPickerContentView *)[self viewForRow:rowIndex forComponent:columnIndex];
        [viewForRow setChecked:NO];
        [[viewForRow titleLabel] setTextColor:[UIColor colorWithWhite:0.0 alpha:item.isGroup ? GroupOptionTextColorAlpha : DisabledOptionAlpha]];
        return;
    }

    if ([self allowsMultipleSelection]) {
        [view updateFocusedElementSelectedIndex:[self findItemIndexAt:rowIndex] allowsMultipleSelection:true];
        item.isSelected = isChecked;
    } else if (isChecked) {
        // Single selection.
        if (_singleSelectionIndex < numberOfOptions)
            [view focusedSelectElementOptions][_singleSelectionIndex].isSelected = false;

        _singleSelectionIndex = rowIndex;

        // This private delegate often gets called for multiple rows in the picker,
        // so we only activate and set as selected the checked item in single selection.
        [view updateFocusedElementSelectedIndex:[self findItemIndexAt:rowIndex] allowsMultipleSelection:false];
        item.isSelected = true;
    } else
        item.isSelected = false;
}

// WKSelectTesting
- (void)selectRow:(NSInteger)rowIndex inComponent:(NSInteger)componentIndex extendingSelection:(BOOL)extendingSelection
{
    // FIXME: handle extendingSelection.
    [self selectRow:rowIndex inComponent:0 animated:NO];
    // Progammatic selection changes don't call the delegate, so do that manually.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [self pickerView:self row:rowIndex column:0 checked:YES];
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (BOOL)selectFormAccessoryHasCheckedItemAtRow:(long)rowIndex
{
    auto numberOfRows = [self numberOfRowsInComponent:0];
    if (rowIndex >= numberOfRows)
        return NO;

    return [(UIPickerContentView *)[self viewForRow:rowIndex forComponent:0] isChecked];
}

@end

@implementation WKSelectSinglePicker {
    WeakObjCPtr<WKContentView> _view;
    NSInteger _selectedIndex;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super initWithFrame:CGRectZero]))
        return nil;

    _view = view;
    [self setDelegate:self];
    [self setDataSource:self];
    [self setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

    _selectedIndex = NSNotFound;

    for (size_t i = 0; i < [view focusedSelectElementOptions].size(); ++i) {
        if ([view focusedSelectElementOptions][i].isSelected) {
            _selectedIndex = i;
            break;
        }
    }

    [self reloadAllComponents];

    if (_selectedIndex != NSNotFound)
        [self selectRow:_selectedIndex inComponent:0 animated:NO];

    return self;
}

- (void)dealloc
{
    [self setDelegate:nil];
    [self setDataSource:nil];

    [super dealloc];
}

- (UIView *)controlView
{
    return self;
}

- (void)controlBeginEditing
{
}

- (void)controlUpdateEditing
{
    [self reloadAllComponents];
}

- (void)controlEndEditing
{
    if (_selectedIndex == NSNotFound)
        return;

    RetainPtr view = _view.get();
    if (_selectedIndex < (NSInteger)[view focusedSelectElementOptions].size()) {
        [view focusedSelectElementOptions][_selectedIndex].isSelected = true;
        [view updateFocusedElementSelectedIndex:_selectedIndex allowsMultipleSelection:false];
    }
}

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)columnIndex
{
    return [_view.get() focusedElementInformation].selectOptions.size();
}

- (NSAttributedString *)pickerView:(UIPickerView *)pickerView attributedTitleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    RetainPtr view = _view.get();
    if (row < 0 || row >= (NSInteger)[view focusedSelectElementOptions].size())
        return nil;

    const OptionItem& option = [view focusedSelectElementOptions][row];
    RetainPtr trimmedText = adoptNS([option.text.createNSString() mutableCopy]);
    CFStringTrimWhitespace((CFMutableStringRef)trimmedText.get());

    auto attributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:trimmedText.get()]);
    if (option.disabled)
        [attributedString addAttribute:NSForegroundColorAttributeName value:[UIColor colorWithWhite:0.0 alpha:DisabledOptionAlpha] range:NSMakeRange(0, [trimmedText length])];

    return attributedString.autorelease();
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    RetainPtr view = _view.get();
    if (row < 0 || row >= (NSInteger)[view focusedSelectElementOptions].size())
        return;

    const OptionItem& newSelectedOption = [view focusedSelectElementOptions][row];
    if (newSelectedOption.disabled) {
        NSInteger rowToSelect = NSNotFound;

        // Search backwards for the previous enabled option.
        for (NSInteger i = row - 1; i >= 0; --i) {
            const OptionItem& earlierOption = [view focusedSelectElementOptions][i];
            if (!earlierOption.disabled) {
                rowToSelect = i;
                break;
            }
        }

        // If nothing previous, search forwards for the next enabled option.
        if (rowToSelect == NSNotFound) {
            for (size_t i = row + 1; i < [view focusedSelectElementOptions].size(); ++i) {
                const OptionItem& laterOption = [view focusedSelectElementOptions][i];
                if (!laterOption.disabled) {
                    rowToSelect = i;
                    break;
                }
            }
        }

        if (rowToSelect == NSNotFound)
            return;

        [self selectRow:rowToSelect inComponent:0 animated:YES];
        row = rowToSelect;
    }

    _selectedIndex = row;
}

// WKSelectTesting
- (void)selectRow:(NSInteger)rowIndex inComponent:(NSInteger)componentIndex extendingSelection:(BOOL)extendingSelection
{
    // FIXME: handle extendingSelection.
    [self selectRow:rowIndex inComponent:0 animated:NO];
    // Progammatic selection changes don't call the delegate, so do that manually.
    [self.delegate pickerView:self didSelectRow:rowIndex inComponent:0];
}

@end

#pragma mark - Form Control Refresh

@implementation WKSelectPicker {
    WeakObjCPtr<WKContentView> _view;
    CGPoint _interactionPoint;

#if USE(UICONTEXTMENU)
    RetainPtr<UIMenu> _selectMenu;
    std::unique_ptr<WebKit::CompactContextMenuPresenter> _selectContextMenuPresenter;
    BOOL _isAnimatingContextMenuDismissal;
#endif
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _interactionPoint = [view lastInteractionLocation];

    return self;
}

- (UIView *)controlView
{
    return nil;
}

- (void)controlBeginEditing
{
    RetainPtr view = _view.get();
    // Don't show the menu if the element is entirely offscreen.
    if (!CGRectIntersectsRect([view focusedElementInformation].interactionRect, [view bounds]))
        return;

    [view startRelinquishingFirstResponderToFocusedElement];

#if USE(UICONTEXTMENU)
    _selectMenu = [self createMenu];
    [self showSelectPicker];
#endif
}

- (void)controlUpdateEditing
{
#if USE(UICONTEXTMENU)
    if (!_selectContextMenuPresenter)
        return;

    _selectMenu = [self createMenu];
    _selectContextMenuPresenter->updateVisibleMenu(^UIMenu *(UIMenu *) {
        return _selectMenu.get();
    });
#endif
}

- (void)controlEndEditing
{
    [_view.get() stopRelinquishingFirstResponderToFocusedElement];

#if USE(UICONTEXTMENU)
    [self resetContextMenuPresenter];
#endif
}

- (void)dealloc
{
#if USE(UICONTEXTMENU)
    [self resetContextMenuPresenter];
#endif
    [super dealloc];
}

- (void)didSelectOptionIndex:(NSInteger)index
{
    NSInteger optionIndex = 0;
    RetainPtr view = _view.get();
    for (auto& option : [view focusedSelectElementOptions]) {
        if (option.isGroup)
            continue;

        option.isSelected = optionIndex == index;
        optionIndex++;
    }

    [view updateFocusedElementSelectedIndex:index allowsMultipleSelection:false];
}

#if USE(UICONTEXTMENU)

static constexpr auto removeLineLimitForChildrenMenuOption = static_cast<UIMenuOptions>(1 << 6);

- (UIMenu *)createMenu
{
    RetainPtr view = _view.get();
    if (![view focusedSelectElementOptions].size()) {
        RetainPtr emptyAction = [UIAction actionWithTitle:WEB_UI_STRING_KEY("No Options", "No Options Select Popover", "Empty select list").createNSString().get() image:nil identifier:nil handler:^(__kindof UIAction *action) { }];
        emptyAction.get().attributes = UIMenuElementAttributesDisabled;
        return [UIMenu menuWithTitle:@"" children:@[emptyAction.get()]];
    }

    NSMutableArray *items = [NSMutableArray array];
    NSInteger optionIndex = 0;

    size_t currentIndex = 0;
    while (currentIndex < [view focusedSelectElementOptions].size()) {
        auto& optionItem = [view focusedSelectElementOptions][currentIndex];
        if (optionItem.isGroup) {
            auto groupID = optionItem.parentGroupID;
            RetainPtr groupText = optionItem.text.createNSString();
            NSMutableArray *groupedItems = [NSMutableArray array];

            currentIndex++;
            while (currentIndex < [view focusedSelectElementOptions].size()) {
                auto& childOptionItem = [view focusedSelectElementOptions][currentIndex];
                if (childOptionItem.isGroup || childOptionItem.parentGroupID != groupID)
                    break;

                UIAction *action = [self actionForOptionItem:childOptionItem withIndex:optionIndex];
                [groupedItems addObject:action];
                optionIndex++;
                currentIndex++;
            }

            UIMenu *groupMenu = [UIMenu menuWithTitle:groupText.get() image:nil identifier:nil options:UIMenuOptionsDisplayInline | removeLineLimitForChildrenMenuOption children:groupedItems];
            [items addObject:groupMenu];
            continue;
        }

        UIAction *action = [self actionForOptionItem:optionItem withIndex:optionIndex];
        [items addObject:action];
        optionIndex++;
        currentIndex++;
    }

    return [UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsSingleSelection | removeLineLimitForChildrenMenuOption children:items];
}

- (UIAction *)actionForOptionItem:(const OptionItem&)option withIndex:(NSInteger)optionIndex
{
    RetainPtr optionAction = [UIAction actionWithTitle:option.text.createNSString().get() image:nil identifier:nil handler:^(__kindof UIAction *action) {
        [self didSelectOptionIndex:optionIndex];
    }];

    if (option.disabled)
        optionAction.get().attributes = UIMenuElementAttributesDisabled;

    if (option.isSelected)
        optionAction.get().state = UIMenuElementStateOn;

    return optionAction.autorelease();
}

- (UIAction *)actionForOptionIndex:(NSInteger)optionIndex
{
    NSInteger currentIndex = 0;

    NSArray<UIMenuElement *> *menuElements = [_selectMenu children];
    for (UIMenuElement *menuElement in menuElements) {
        if ([menuElement isKindOfClass:UIAction.class]) {
            if (currentIndex == optionIndex)
                return checked_objc_cast<UIAction>(menuElement);

            ++currentIndex;
            continue;
        }

        RetainPtr groupedMenu = checked_objc_cast<UIMenu>(menuElement);
        NSUInteger numGroupedOptions = [groupedMenu children].count;

        if (currentIndex + numGroupedOptions <= (NSUInteger)optionIndex)
            currentIndex += numGroupedOptions;
        else
            return checked_objc_cast<UIAction>([[groupedMenu children] objectAtIndex:([groupedMenu children].count - numGroupedOptions) + (optionIndex - currentIndex)]);
    }

    return nil;
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configuration:(UIContextMenuConfiguration *)configuration highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier
{
    return [_view.get() _createTargetedContextMenuHintPreviewForFocusedElement:WebKit::TargetedPreviewPositioning::Default];
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location
{
    UIContextMenuActionProvider actionMenuProvider = [weakSelf = WeakObjCPtr<WKSelectPicker>(self)] (NSArray<UIMenuElement *> *) -> UIMenu * {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return nil;

        return strongSelf->_selectMenu.get();
    };

    return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:actionMenuProvider];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id <UIContextMenuInteractionAnimating>)animator
{
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKSelectPicker>(self)] {
        auto strongSelf = weakSelf.get();
        if (strongSelf)
            [[strongSelf->_view.get() webView] _didShowContextMenu];
    }];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id <UIContextMenuInteractionAnimating>)animator
{
    _isAnimatingContextMenuDismissal = YES;
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKSelectPicker>(self), elementContext = [_view.get() focusedElementInformation].elementContext] {
        if (RetainPtr strongSelf = weakSelf.get()) {
            RetainPtr view = strongSelf->_view.get();
            if ([view _isSameAsFocusedElement:elementContext])
                [view accessoryDone];
            [[view webView] _didDismissContextMenu];
            strongSelf->_isAnimatingContextMenuDismissal = NO;
        }
    }];
}

- (void)resetContextMenuPresenter
{
    if (!_selectContextMenuPresenter)
        return;

    _selectContextMenuPresenter = nullptr;
    RetainPtr view = _view.get();
    [view _removeContextMenuHintContainerIfPossible];

    if (!_isAnimatingContextMenuDismissal)
        [[view webView] _didDismissContextMenu];
}

- (void)showSelectPicker
{
    if (!_selectContextMenuPresenter)
        _selectContextMenuPresenter = makeUnique<WebKit::CompactContextMenuPresenter>(_view.get().get(), self);
    _selectContextMenuPresenter->present(_interactionPoint);
}

#endif // USE(UICONTEXTMENU)

// WKSelectTesting

- (void)selectRow:(NSInteger)rowIndex inComponent:(NSInteger)componentIndex extendingSelection:(BOOL)extendingSelection
{
#if USE(UICONTEXTMENU)
    UIAction *optionAction = [self actionForOptionIndex:rowIndex];
    if (optionAction) {
        [optionAction performWithSender:nil target:nil];
        [_view.get() accessoryDone];
    }
#endif
}

- (BOOL)selectFormAccessoryHasCheckedItemAtRow:(long)rowIndex
{
#if USE(UICONTEXTMENU)
    UIAction *optionAction = [self actionForOptionIndex:rowIndex];
    if (optionAction)
        return optionAction.state == UIMenuElementStateOn;
#endif

    return NO;
}

- (NSArray<NSString *> *)menuItemTitles
{
#if USE(UICONTEXTMENU)
    NSMutableArray<NSString *> *itemTitles = [NSMutableArray array];
    for (UIMenuElement *menuElement in [_selectMenu children]) {
        if (RetainPtr action = dynamic_objc_cast<UIAction>(menuElement)) {
            [itemTitles addObject:action.get().title];
            continue;
        }

        if (RetainPtr menu = dynamic_objc_cast<UIMenu>(menuElement)) {
            for (UIMenuElement *groupedMenuElement in [menu children])
                [itemTitles addObject:groupedMenuElement.title];
        }
    }
    return itemTitles;
#else
    return nil;
#endif
}

@end

@interface WKSelectPickerGroupHeaderView : UIView
@property (nonatomic, readonly) NSInteger section;
@property (nonatomic, readonly) BOOL isCollapsible;
@end

@interface WKSelectPickerTableViewController : UITableViewController
- (void)didTapSelectPickerGroupHeaderView:(WKSelectPickerGroupHeaderView *)headerView;
@end

static const CGFloat groupHeaderMargin = 16.0f;
static const CGFloat groupHeaderLabelImageMargin = 4.0f;
static const CGFloat groupHeaderCollapseButtonTransitionDuration = 0.3f;

@implementation WKSelectPickerGroupHeaderView {
    RetainPtr<UILabel> _label;
    RetainPtr<UIImageView> _collapseIndicatorView;

    WeakObjCPtr<WKSelectPickerTableViewController> _tableViewController;

    BOOL _collapsed;
}

- (instancetype)initWithGroupName:(NSString *)groupName section:(NSInteger)section isCollapsible:(BOOL)isCollapsible
{
    if (!(self = [super init]))
        return nil;

    _section = section;
    _isCollapsible = isCollapsible;

    auto tapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(didTapHeader:)]);
    [self addGestureRecognizer:tapGestureRecognizer.get()];

    _label = adoptNS([[UILabel alloc] init]);
    [_label setText:groupName];
    [_label setFont:WKSelectPickerGroupHeaderView.preferredFont];
    [_label setAdjustsFontForContentSizeCategory:YES];
    [_label setAdjustsFontSizeToFitWidth:NO];
    [_label setLineBreakMode:NSLineBreakByTruncatingTail];
    [_label setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:_label.get()];

    if (_isCollapsible) {
        _collapseIndicatorView = adoptNS([[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"chevron.down"]]);
        [_label setTranslatesAutoresizingMaskIntoConstraints:NO];
        [_collapseIndicatorView setPreferredSymbolConfiguration:[UIImageSymbolConfiguration configurationWithFont:WKSelectPickerGroupHeaderView.preferredFont scale:UIImageSymbolScaleSmall]];
        [_collapseIndicatorView setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        [_collapseIndicatorView setTranslatesAutoresizingMaskIntoConstraints:NO];
        [self addSubview:_collapseIndicatorView.get()];

        [NSLayoutConstraint activateConstraints:@[
            [[_label leadingAnchor] constraintEqualToAnchor:[self leadingAnchor] constant:WKSelectPickerGroupHeaderView.preferredMargin],
            [[_label trailingAnchor] constraintEqualToAnchor:[_collapseIndicatorView leadingAnchor] constant:-groupHeaderLabelImageMargin],
            [[_label topAnchor] constraintEqualToAnchor:[self topAnchor] constant:0],
        ]];

        [NSLayoutConstraint activateConstraints:@[
            [[_collapseIndicatorView trailingAnchor] constraintEqualToAnchor:[self trailingAnchor] constant:-WKSelectPickerGroupHeaderView.preferredMargin],
            [[_collapseIndicatorView topAnchor] constraintEqualToAnchor:[_label topAnchor] constant:0],
            [[_collapseIndicatorView bottomAnchor] constraintEqualToAnchor:[_label bottomAnchor] constant:0],
        ]];
    } else {
        [NSLayoutConstraint activateConstraints:@[
            [[_label leadingAnchor] constraintEqualToAnchor:[self leadingAnchor] constant:WKSelectPickerGroupHeaderView.preferredMargin],
            [[_label trailingAnchor] constraintEqualToAnchor:[self trailingAnchor] constant:-WKSelectPickerGroupHeaderView.preferredMargin],
            [[_label topAnchor] constraintEqualToAnchor:[self topAnchor]],
            [[_label bottomAnchor] constraintEqualToAnchor:[self bottomAnchor]],
        ]];
    }

    return self;
}

- (void)setCollapsed:(BOOL)collapsed animated:(BOOL)animated
{
    if (!_isCollapsible || _collapsed == collapsed)
        return;

    _collapsed = collapsed;

    auto animations = [protectedSelf = retainPtr(self)] {
        auto layoutDirectionMultipler = ([UIView userInterfaceLayoutDirectionForSemanticContentAttribute:[protectedSelf semanticContentAttribute]] == UIUserInterfaceLayoutDirectionLeftToRight) ? -1.0f : 1.0f;
        auto transform = protectedSelf->_collapsed ? CGAffineTransformMakeRotation(layoutDirectionMultipler * std::numbers::pi / 2) : CGAffineTransformIdentity;
        [protectedSelf->_collapseIndicatorView setTransform:transform];
    };

    if (animated)
        [UIView animateWithDuration:groupHeaderCollapseButtonTransitionDuration animations:animations];
    else
        animations();
}

- (void)setTableViewController:(WKSelectPickerTableViewController *)tableViewController
{
    _tableViewController = tableViewController;
}

- (void)didTapHeader:(id)sender
{
    [_tableViewController didTapSelectPickerGroupHeaderView:self];
    [self setCollapsed:!_collapsed animated:YES];
}

+ (UIFont *)preferredFont
{
    UIFontDescriptor *descriptor = [UIFontDescriptor preferredFontDescriptorWithTextStyle:UIFontTextStyleTitle3];
    descriptor = [descriptor fontDescriptorByAddingAttributes:@{
        UIFontDescriptorTraitsAttribute: @{
            UIFontWeightTrait: @(UIFontWeightSemibold)
        }
    }];

    return [UIFont fontWithDescriptor:descriptor size:0];
}

+ (CGFloat)preferredMargin
{
    return groupHeaderMargin;
}

+ (CGFloat)preferredHeight
{
    return WKSelectPickerGroupHeaderView.preferredFont.lineHeight + WKSelectPickerGroupHeaderView.preferredMargin;
}

@end

static const CGFloat nextPreviousSpacerWidth = 6.0f;
static const CGFloat selectPopoverLength = 320.0f;
static NSString *optionCellReuseIdentifier = @"WKSelectPickerTableViewCell";

@implementation WKSelectPickerTableViewController {
    __weak WKContentView *_contentView;

    NSInteger _numberOfSections;
    RetainPtr<NSMutableSet<NSNumber *>> _collapsedSections;

    RetainPtr<UIBarButtonItem> _previousButton;
    RetainPtr<UIBarButtonItem> _nextButton;
}

- (id)initWithView:(WKContentView *)view
{
    if (!(self = [super initWithStyle:UITableViewStyleGrouped]))
        return nil;

    // Ideally, we would use UITableViewStyleInsetGrouped as the style, but it's unavailable
    // on tvOS. To avoid a separate codepath for tvOS, we use UITableViewStyleGrouped with
    // sectionContentInsetFollowsLayoutMargins set to YES.
    [self.tableView _setSectionContentInsetFollowsLayoutMargins:YES];

    _contentView = view;

#if !PLATFORM(APPLETV)
    _previousButton = adoptNS([[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"chevron.up"] style:UIBarButtonItemStylePlain target:self action:@selector(previous:)]);
    _nextButton = adoptNS([[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"chevron.down"] style:UIBarButtonItemStylePlain target:self action:@selector(next:)]);

    bool useGlassAppearance = false;
#if HAVE(LIQUID_GLASS)
    useGlassAppearance = isLiquidGlassEnabled();
#endif

    if (useGlassAppearance) {
        self.tableView.backgroundColor = [UIColor clearColor];
        self.navigationItem.leftBarButtonItems = @[ _previousButton.get(), _nextButton.get() ];
    } else {
        auto nextPreviousSpacer = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace target:nil action:NULL]);
        [nextPreviousSpacer setWidth:nextPreviousSpacerWidth];
        self.navigationItem.leftBarButtonItems = @[ _previousButton.get(), nextPreviousSpacer.get(), _nextButton.get() ];
    }
#endif

    auto closeButton = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemClose target:self action:@selector(close:)]);
    self.navigationItem.rightBarButtonItem = closeButton.get();

    _collapsedSections = adoptNS([[NSMutableSet alloc] init]);

    _numberOfSections = 1;
    for (auto& option : _contentView.focusedSelectElementOptions) {
        if (option.isGroup)
            _numberOfSections++;
    }

    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

#if PLATFORM(APPLETV)
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    self.tableView.tintColor = UIColor.systemBlueColor;
#endif
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

    [_previousButton setEnabled:_contentView.focusedElementInformation.hasPreviousNode];
    [_nextButton setEnabled:_contentView.focusedElementInformation.hasNextNode];
}

- (NSInteger)numberOfRowsInGroup:(NSInteger)groupID
{
    NSInteger rowCount = 0;
    for (auto& option : _contentView.focusedSelectElementOptions) {
        if (option.isGroup)
            continue;

        if (option.parentGroupID == groupID)
            rowCount++;

        if (option.parentGroupID > groupID)
            break;
    }

    return rowCount;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return _numberOfSections;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    if (_contentView.focusedSelectElementOptions.isEmpty())
        return 1;

    if ([_collapsedSections containsObject:@(section)])
        return 0;

    return [self numberOfRowsInGroup:section];
}

- (CGFloat)tableView:(UITableView *)tableView heightForHeaderInSection:(NSInteger)section
{
    if (!section || ![[self tableView:tableView titleForHeaderInSection:section] length])
        return tableView.layoutMargins.left;

    return WKSelectPickerGroupHeaderView.preferredHeight;
}

- (CGFloat)tableView:(UITableView *)tableView heightForFooterInSection:(NSInteger)section
{
    if (!section && ![self numberOfRowsInGroup:0] && _numberOfSections > 1)
        return CGFLOAT_MIN;

    return tableView.layoutMargins.left;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    if (!section)
        return nil;

    NSInteger groupCount = 0;
    for (auto& option : _contentView.focusedSelectElementOptions) {
        if (!option.isGroup)
            continue;

        groupCount++;
        if (option.isGroup && groupCount == section)
            return option.text.createNSString().autorelease();
    }

    return nil;
}

- (UIView *)tableView:(UITableView *)tableView viewForFooterInSection:(NSInteger)section
{
    return nil;
}

- (UIView *)tableView:(UITableView *)tableView viewForHeaderInSection:(NSInteger)section
{
    if (!section)
        return nil;

    auto title = [self tableView:tableView titleForHeaderInSection:section];
    if (!title.length)
        return nil;

    BOOL isCollapsible = [self numberOfRowsInGroup:section] > 0;

    auto headerView = adoptNS([[WKSelectPickerGroupHeaderView alloc] initWithGroupName:title section:section isCollapsible:isCollapsible]);
    [headerView setCollapsed:[_collapsedSections containsObject:@(section)] animated:NO];
    [headerView setTableViewController:self];

    return headerView.autorelease();
}

- (void)didTapSelectPickerGroupHeaderView:(WKSelectPickerGroupHeaderView *)headerView
{
    if (!headerView.isCollapsible)
        return;

    NSInteger section = headerView.section;
    NSInteger rowCount = [self numberOfRowsInGroup:section];

    NSMutableArray *indexPaths = [NSMutableArray arrayWithCapacity:rowCount];
    for (NSInteger i = 0; i < rowCount; i++)
        [indexPaths addObject:[NSIndexPath indexPathForRow:i inSection:section]];

    RetainPtr object = @(section);
    if ([_collapsedSections containsObject:object.get()]) {
        [_collapsedSections removeObject:object.get()];
        [self.tableView insertRowsAtIndexPaths:indexPaths withRowAnimation:UITableViewRowAnimationFade];
    } else {
        [_collapsedSections addObject:object.get()];
        [self.tableView deleteRowsAtIndexPaths:indexPaths withRowAnimation:UITableViewRowAnimationFade];
    }
}

- (NSInteger)findItemIndexAt:(NSIndexPath *)indexPath
{
    int optionIndex = 0;
    int rowIndex = 0;

    for (auto& option : _contentView.focusedSelectElementOptions) {
        if (option.isGroup) {
            rowIndex = 0;
            continue;
        }

        if (option.parentGroupID == indexPath.section && rowIndex == indexPath.row)
            return optionIndex;

        optionIndex++;
        rowIndex++;
    }

    return NSNotFound;
}

- (OptionItem *)optionItemAtIndexPath:(NSIndexPath *)indexPath
{
    NSInteger index = 0;
    for (auto& option : _contentView.focusedSelectElementOptions) {
        if (option.isGroup || option.parentGroupID != indexPath.section)
            continue;

        if (index == indexPath.row)
            return &option;

        index++;
    }

    return nil;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    auto cell = retainPtr([tableView dequeueReusableCellWithIdentifier:optionCellReuseIdentifier]);
    if (!cell)
        cell = adoptNS([[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:optionCellReuseIdentifier]);

    if (_contentView.focusedSelectElementOptions.isEmpty()) {
        [cell textLabel].enabled = NO;
        [cell textLabel].text = WEB_UI_STRING_KEY("No Options", "No Options Select Popover", "Empty select list").createNSString().get();
        [cell setUserInteractionEnabled:NO];
        [cell imageView].image = nil;
        return cell.autorelease();
    }

    auto option = [self optionItemAtIndexPath:indexPath];
    if (!option)
        return cell.autorelease();

    [cell textLabel].text = option->text.createNSString().get();
    [cell textLabel].enabled = !option->disabled;
    [cell setUserInteractionEnabled:!option->disabled];
    [cell imageView].preferredSymbolConfiguration = [UIImageSymbolConfiguration configurationWithTextStyle:UIFontTextStyleBody scale:UIImageSymbolScaleLarge];

    if (option->isSelected)
        [cell imageView].image = [UIImage systemImageNamed:@"checkmark.circle.fill"];
    else if (option->disabled)
        [cell imageView].image = [[UIImage systemImageNamed:@"circle"] imageWithTintColor:UIColor.quaternaryLabelColor renderingMode:UIImageRenderingModeAlwaysOriginal];
    else
        [cell imageView].image = [[UIImage systemImageNamed:@"circle"] imageWithTintColor:UIColor.tertiaryLabelColor renderingMode:UIImageRenderingModeAlwaysOriginal];

    return cell.autorelease();
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:[tableView indexPathForSelectedRow] animated:NO];

    UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <rdar://131638865> UITableViewCell.textLabel is deprecated.
    if (!cell.textLabel.enabled)
        return;

    auto option = [self optionItemAtIndexPath:indexPath];
    if (!option)
        return;

    if (!option->isSelected)
        cell.imageView.image = [UIImage systemImageNamed:@"checkmark.circle.fill"];
    else
        cell.imageView.image = [[UIImage systemImageNamed:@"circle"] imageWithTintColor:UIColor.tertiaryLabelColor renderingMode:UIImageRenderingModeAlwaysOriginal];
ALLOW_DEPRECATED_DECLARATIONS_END

    [_contentView updateFocusedElementSelectedIndex:[self findItemIndexAt:indexPath] allowsMultipleSelection:true];
    option->isSelected = !option->isSelected;
}

- (UIFont *)groupHeaderFont
{
    UIFontDescriptor *descriptor = [UIFontDescriptor preferredFontDescriptorWithTextStyle:UIFontTextStyleTitle3];
    descriptor = [descriptor fontDescriptorByAddingAttributes:@{
        UIFontDescriptorTraitsAttribute: @{
            UIFontWeightTrait: @(UIFontWeightSemibold)
        }
    }];

    return [UIFont fontWithDescriptor:descriptor size:0];
}

- (void)next:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:[weakContentView = WeakObjCPtr<WKContentView>(_contentView)] {
        auto strongContentView = weakContentView.get();
        if (strongContentView)
            [strongContentView accessoryTab:YES];
    }];
}

- (void)previous:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:[weakContentView = WeakObjCPtr<WKContentView>(_contentView)] {
        auto strongContentView = weakContentView.get();
        if (strongContentView)
            [strongContentView accessoryTab:NO];
    }];
}

- (void)close:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:[weakContentView = WeakObjCPtr<WKContentView>(_contentView)] {
        auto strongContentView = weakContentView.get();
        if (strongContentView)
            [strongContentView accessoryDone];
    }];
}

@end

@interface WKSelectMultiplePicker () <UIPopoverPresentationControllerDelegate>
@end

@implementation WKSelectMultiplePicker {
    WeakObjCPtr<WKContentView> _view;

    RetainPtr<UINavigationController> _navigationController;
    RetainPtr<WKSelectPickerTableViewController> _tableViewController;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _tableViewController = adoptNS([[WKSelectPickerTableViewController alloc] initWithView:view]);
    _navigationController = adoptNS([[UINavigationController alloc] initWithRootViewController:_tableViewController.get()]);

    return self;
}

- (void)configurePresentation
{
    if (PAL::currentUserInterfaceIdiomIsSmallScreen()) {
        [[_navigationController navigationBar] setBarTintColor:UIColor.systemGroupedBackgroundColor];

#if PLATFORM(APPLETV)
        [_navigationController setModalPresentationStyle:UIModalPresentationPageSheet];
#endif

        UIPresentationController *presentationController = [_navigationController presentationController];
        presentationController.delegate = self;

        if (RetainPtr sheetPresentationController = dynamic_objc_cast<UISheetPresentationController>(presentationController)) {
            sheetPresentationController.get().detents = @[UISheetPresentationControllerDetent.mediumDetent, UISheetPresentationControllerDetent.largeDetent];
            sheetPresentationController.get().widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
            sheetPresentationController.get().prefersEdgeAttachedInCompactHeight = YES;
        }
    } else {
        [_navigationController setModalPresentationStyle:UIModalPresentationPopover];
        [_navigationController setNavigationBarHidden:YES];
        [_tableViewController setPreferredContentSize:CGSizeMake(selectPopoverLength, selectPopoverLength)];

        UIPopoverPresentationController *presentationController = [_navigationController popoverPresentationController];
        presentationController.delegate = self;
        RetainPtr view = _view.get();
        presentationController.sourceView = view.get();
        presentationController.sourceRect = CGRectIntegral([view focusedElementInformation].interactionRect);
    }
}

#pragma mark WKFormControl

- (UIView *)controlView
{
    return nil;
}

- (void)controlBeginEditing
{
    RetainPtr view = _view.get();
    [view startRelinquishingFirstResponderToFocusedElement];

    [self configurePresentation];
    RetainPtr<UIViewController> presentingViewController = [view _wk_viewControllerForFullScreenPresentation];
#if PLATFORM(VISION)
    [view page]->dispatchWillPresentModalUI();
#endif
    [presentingViewController.get() presentViewController:_navigationController.get() animated:YES completion:nil];
}

- (void)controlUpdateEditing
{
    [[_tableViewController tableView] reloadData];
}

- (void)controlEndEditing
{
    [_view.get() stopRelinquishingFirstResponderToFocusedElement];
    [_tableViewController dismissViewControllerAnimated:NO completion:nil];
}

#pragma mark UIPopoverPresentationControllerDelegate

- (void)presentationControllerDidDismiss:(UIPresentationController *)presentationController
{
    [_view.get() accessoryDone];
}

#pragma mark WKTesting

- (NSIndexPath *)_indexPathForRow:(NSInteger)rowIndex
{
    NSInteger currentSection = 0;
    NSInteger currentRow = 0;
    NSInteger totalRows = 0;

    for (auto& option : [_view.get() focusedSelectElementOptions]) {
        if (option.isGroup) {
            currentSection++;
            currentRow = 0;
            continue;
        }

        if (totalRows == rowIndex)
            return [NSIndexPath indexPathForRow:currentRow inSection:currentSection];

        currentRow++;
        totalRows++;
    }

    return nil;
}

- (void)selectRow:(NSInteger)rowIndex inComponent:(NSInteger)componentIndex extendingSelection:(BOOL)extendingSelection
{
    NSIndexPath *indexPath = [self _indexPathForRow:rowIndex];
    if (!indexPath)
        return;

    [[_tableViewController tableView] selectRowAtIndexPath:indexPath animated:NO scrollPosition:UITableViewScrollPositionMiddle];
    [_tableViewController tableView:[_tableViewController tableView] didSelectRowAtIndexPath:indexPath];
}

@end

#endif // PLATFORM(IOS_FAMILY)

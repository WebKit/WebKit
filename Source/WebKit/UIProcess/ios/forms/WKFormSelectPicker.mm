/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "UIKitSPI.h"
#import "UserInterfaceIdiom.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPopover.h"
#import "WKFormSelectControl.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>

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

    auto trimmedText = adoptNS([item.text mutableCopy]);
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

    auto trimmedText = adoptNS([item.text mutableCopy]);
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
    WKContentView *_view;
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
    _allowsMultipleSelection = _view.focusedElementInformation.isMultiSelect;
    _singleSelectionIndex = NSNotFound;
    [self setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [self setDataSource:self];
    [self setDelegate:self];
    [self _setUsesCheckedSelection:YES];

    [self _setMagnifierEnabled:NO];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    UITextWritingDirection writingDirection = UITextWritingDirectionLeftToRight;
    // FIXME: retrieve from WebProcess writing direction.
    _textAlignment = (writingDirection == UITextWritingDirectionLeftToRight) ? NSTextAlignmentLeft : NSTextAlignmentRight;
    ALLOW_DEPRECATED_DECLARATIONS_END

    [self setAllowsMultipleSelection:_allowsMultipleSelection];
    [self setSize:[UIKeyboard defaultSizeForInterfaceOrientation:view.interfaceOrientation]];
    [self reloadAllComponents];

    if (!_allowsMultipleSelection) {
        const Vector<OptionItem>& selectOptions = [_view focusedSelectElementOptions];
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
    auto& item = [_view focusedSelectElementOptions][rowIndex];
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
        _fontSize = adjustedFontSize(_maximumTextWidth, font, titleTextLabel.font.pointSize, [_view focusedSelectElementOptions]);
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
    return [_view focusedSelectElementOptions].size();
}

- (NSInteger)findItemIndexAt:(int)rowIndex
{
    ASSERT(rowIndex >= 0 && (size_t)rowIndex < [_view focusedSelectElementOptions].size());
    NSInteger itemIndex = 0;
    for (int i = 0; i < rowIndex; ++i) {
        if ([_view focusedSelectElementOptions][i].isGroup)
            continue;
        itemIndex++;
    }

    ASSERT(itemIndex >= 0);
    return itemIndex;
}

- (void)pickerView:(UIPickerView *)pickerView row:(int)rowIndex column:(int)columnIndex checked:(BOOL)isChecked
{
    auto numberOfOptions = static_cast<NSUInteger>([_view focusedSelectElementOptions].size());
    if (numberOfOptions <= static_cast<NSUInteger>(rowIndex))
        return;

    auto& item = [_view focusedSelectElementOptions][rowIndex];

    // FIXME: Remove this workaround once <rdar://problem/18745253> is fixed.
    // Group rows and disabled rows should not be checkable, but we are getting
    // this delegate for those rows. As a workaround, if we get this delegate
    // for a group or disabled row, reset the styles for the content view so it
    // still appears unselected.
    if (item.isGroup || item.disabled) {
        UIPickerContentView *view = (UIPickerContentView *)[self viewForRow:rowIndex forComponent:columnIndex];
        [view setChecked:NO];
        [[view titleLabel] setTextColor:[UIColor colorWithWhite:0.0 alpha:item.isGroup ? GroupOptionTextColorAlpha : DisabledOptionAlpha]];
        return;
    }

    if ([self allowsMultipleSelection]) {
        [_view page]->setFocusedElementSelectedIndex([self findItemIndexAt:rowIndex], true);
        item.isSelected = isChecked;
    } else if (isChecked) {
        // Single selection.
        if (_singleSelectionIndex < numberOfOptions)
            [_view focusedSelectElementOptions][_singleSelectionIndex].isSelected = false;

        _singleSelectionIndex = rowIndex;

        // This private delegate often gets called for multiple rows in the picker,
        // so we only activate and set as selected the checked item in single selection.
        [_view page]->setFocusedElementSelectedIndex([self findItemIndexAt:rowIndex]);
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
    [self pickerView:self row:rowIndex column:0 checked:YES];
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
    WKContentView *_view;
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
        if ([_view focusedSelectElementOptions][i].isSelected) {
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

- (void)controlEndEditing
{
    if (_selectedIndex == NSNotFound)
        return;

    if (_selectedIndex < (NSInteger)[_view focusedSelectElementOptions].size()) {
        [_view focusedSelectElementOptions][_selectedIndex].isSelected = true;
        [_view page]->setFocusedElementSelectedIndex(_selectedIndex);
    }
}

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)columnIndex
{
    return _view.focusedElementInformation.selectOptions.size();
}

- (NSAttributedString *)pickerView:(UIPickerView *)pickerView attributedTitleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    if (row < 0 || row >= (NSInteger)[_view focusedSelectElementOptions].size())
        return nil;

    const OptionItem& option = [_view focusedSelectElementOptions][row];
    auto trimmedText = adoptNS([option.text mutableCopy]);
    CFStringTrimWhitespace((CFMutableStringRef)trimmedText.get());

    auto attributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:trimmedText.get()]);
    if (option.disabled)
        [attributedString addAttribute:NSForegroundColorAttributeName value:[UIColor colorWithWhite:0.0 alpha:DisabledOptionAlpha] range:NSMakeRange(0, [trimmedText length])];

    return attributedString.autorelease();
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    if (row < 0 || row >= (NSInteger)[_view focusedSelectElementOptions].size())
        return;

    const OptionItem& newSelectedOption = [_view focusedSelectElementOptions][row];
    if (newSelectedOption.disabled) {
        NSInteger rowToSelect = NSNotFound;

        // Search backwards for the previous enabled option.
        for (NSInteger i = row - 1; i >= 0; --i) {
            const OptionItem& earlierOption = [_view focusedSelectElementOptions][i];
            if (!earlierOption.disabled) {
                rowToSelect = i;
                break;
            }
        }

        // If nothing previous, search forwards for the next enabled option.
        if (rowToSelect == NSNotFound) {
            for (size_t i = row + 1; i < [_view focusedSelectElementOptions].size(); ++i) {
                const OptionItem& laterOption = [_view focusedSelectElementOptions][i];
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
    __weak WKContentView *_view;
    CGPoint _interactionPoint;

#if USE(UICONTEXTMENU)
    RetainPtr<UIMenu> _selectMenu;
    RetainPtr<UIContextMenuInteraction> _selectContextMenuInteraction;
#endif
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _interactionPoint = [_view lastInteractionLocation];
#if USE(UICONTEXTMENU)
    _selectMenu = [self createMenu];
#endif

    return self;
}

- (UIView *)controlView
{
    return nil;
}

- (void)controlBeginEditing
{
    [_view startRelinquishingFirstResponderToFocusedElement];

#if USE(UICONTEXTMENU)
    WebKit::InteractionInformationRequest positionInformationRequest { WebCore::IntPoint(_view.focusedElementInformation.lastInteractionLocation) };
    [_view doAfterPositionInformationUpdate:^(WebKit::InteractionInformationAtPosition interactionInformation) {
        [self showSelectPicker];
    } forRequest:positionInformationRequest];
#endif
}

- (void)controlEndEditing
{
    [_view stopRelinquishingFirstResponderToFocusedElement];

#if USE(UICONTEXTMENU)
    [self removeContextMenuInteraction];
#endif
}

- (void)dealloc
{
#if USE(UICONTEXTMENU)
    [self removeContextMenuInteraction];
#endif
    [super dealloc];
}

- (void)didSelectOptionIndex:(NSInteger)index
{
    [_view page]->setFocusedElementSelectedIndex(index);
}

#if USE(UICONTEXTMENU)

- (UIMenu *)createMenu
{
    if (!_view.focusedSelectElementOptions.size()) {
        UIAction *emptyAction = [UIAction actionWithTitle:WEB_UI_STRING_KEY("No Options", "No Options Select Popover", "Empty select list") image:nil identifier:nil handler:^(__kindof UIAction *action) { }];
        emptyAction.attributes = UIMenuElementAttributesDisabled;
        return [UIMenu menuWithTitle:@"" children:@[emptyAction]];
    }

    NSMutableArray *items = [NSMutableArray array];
    NSInteger optionIndex = 0;

    size_t currentIndex = 0;
    while (currentIndex < _view.focusedSelectElementOptions.size()) {
        auto& optionItem = _view.focusedSelectElementOptions[currentIndex];
        if (optionItem.isGroup) {
            NSString *groupText = optionItem.text;
            NSMutableArray *groupedItems = [NSMutableArray array];

            currentIndex++;
            while (currentIndex < _view.focusedSelectElementOptions.size()) {
                optionItem = _view.focusedSelectElementOptions[currentIndex];
                if (optionItem.isGroup)
                    break;

                UIAction *action = [self actionForOptionItem:optionItem withIndex:optionIndex];
                [groupedItems addObject:action];
                optionIndex++;
                currentIndex++;
            }

            UIMenu *groupMenu = [UIMenu menuWithTitle:groupText children:groupedItems];
            [items addObject:groupMenu];
            continue;
        }

        UIAction *action = [self actionForOptionItem:optionItem withIndex:optionIndex];
        [items addObject:action];
        optionIndex++;
        currentIndex++;
    }

    return [UIMenu menuWithTitle:@"" children:items];
}

- (UIAction *)actionForOptionItem:(const OptionItem&)option withIndex:(NSInteger)optionIndex
{
    UIAction *optionAction = [UIAction actionWithTitle:option.text image:nil identifier:nil handler:^(__kindof UIAction *action) {
        [self didSelectOptionIndex:optionIndex];
    }];

    if (option.disabled)
        optionAction.attributes = UIMenuElementAttributesDisabled;

    if (option.isSelected)
        optionAction.state = UIMenuElementStateOn;

    return optionAction;
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    return [_view _createTargetedContextMenuHintPreviewIfPossible];
}

- (_UIContextMenuStyle *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction styleForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    _UIContextMenuStyle *style = [_UIContextMenuStyle defaultStyle];
    style.preferredLayout = _UIContextMenuLayoutCompactMenu;
    return style;
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
            [strongSelf->_view.webView _didShowContextMenu];
    }];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id <UIContextMenuInteractionAnimating>)animator
{
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKSelectPicker>(self)] {
        auto strongSelf = weakSelf.get();
        if (strongSelf) {
            [strongSelf->_view accessoryDone];
            [strongSelf->_view.webView _didDismissContextMenu];
        }
    }];
}

- (void)removeContextMenuInteraction
{
    if (!_selectContextMenuInteraction)
        return;

    [_view removeInteraction:_selectContextMenuInteraction.get()];
    _selectContextMenuInteraction = nil;
    [_view _removeContextMenuViewIfPossible];
    [_view.webView _didDismissContextMenu];
}

- (void)ensureContextMenuInteraction
{
    if (_selectContextMenuInteraction)
        return;

    _selectContextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
    [_view addInteraction:_selectContextMenuInteraction.get()];
}

- (void)showSelectPicker
{
    [self ensureContextMenuInteraction];
    [_selectContextMenuInteraction _presentMenuAtLocation:_interactionPoint];
}

#endif // USE(UICONTEXTMENU)

// WKSelectTesting
- (void)selectRow:(NSInteger)rowIndex inComponent:(NSInteger)componentIndex extendingSelection:(BOOL)extendingSelection
{
#if USE(UICONTEXTMENU)
    NSInteger currentRow = 0;

    NSArray<UIMenuElement *> *menuElements = [_selectMenu children];
    for (UIMenuElement *menuElement in menuElements) {
        if ([menuElement isKindOfClass:UIAction.class]) {
            if (currentRow == rowIndex) {
                [(UIAction *)menuElement _performActionWithSender:nil];
                break;
            }

            currentRow++;
            continue;
        }

        UIMenu *groupedMenu = (UIMenu *)menuElement;
        if (currentRow + groupedMenu.children.count <= (NSUInteger)rowIndex)
            currentRow += groupedMenu.children.count;
        else {
            UIAction *action = (UIAction *)[groupedMenu.children objectAtIndex:rowIndex - currentRow];
            [action _performActionWithSender:nil];
            break;
        }
    }

    [self removeContextMenuInteraction];
#endif
}

@end

static const CGFloat nextPreviousSpacerWidth = 6.0f;
static const CGFloat sectionHeaderCollapseButtonSize = 14.0f;
static const CGFloat sectionHeaderCollapseButtonTransitionDuration = 0.2f;
static const CGFloat sectionHeaderFontSize = 22.0f;
static const CGFloat sectionHeaderHeight = 36.0f;
static const CGFloat sectionHeaderMargin = 16.0f;
static const CGFloat selectPopoverLength = 320.0f;
static NSString *optionCellReuseIdentifier = @"WKSelectPickerTableViewCell";

@interface WKSelectPickerTableViewController : UITableViewController
@end

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

    _previousButton = adoptNS([[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"chevron.up"] style:UIBarButtonItemStylePlain target:self action:@selector(previous:)]);
    auto nextPreviousSpacer = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace target:nil action:NULL]);
    [nextPreviousSpacer setWidth:nextPreviousSpacerWidth];
    _nextButton = adoptNS([[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"chevron.down"] style:UIBarButtonItemStylePlain target:self action:@selector(next:)]);
    auto closeButton = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemClose target:self action:@selector(close:)]);

    self.navigationItem.leftBarButtonItems = @[ _previousButton.get(), nextPreviousSpacer.get(), _nextButton.get() ];
    self.navigationItem.rightBarButtonItem = closeButton.get();

    _collapsedSections = adoptNS([[NSMutableSet alloc] init]);

    _numberOfSections = 1;
    for (auto& option : _contentView.focusedSelectElementOptions) {
        if (option.isGroup)
            _numberOfSections++;
    }

    return self;
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
    if (!section)
        return tableView.layoutMargins.left;

    return sectionHeaderHeight;
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
            return option.text;
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

    auto sectionView = adoptNS([[UIView alloc] init]);

    auto sectionLabel = adoptNS([[UILabel alloc] init]);
    [sectionLabel setText:[self tableView:tableView titleForHeaderInSection:section]];
    [sectionLabel setTextColor:UIColor.blackColor];
    [sectionLabel setFont:[UIFont boldSystemFontOfSize:sectionHeaderFontSize]];
    [sectionLabel setAdjustsFontSizeToFitWidth:NO];
    [sectionLabel setLineBreakMode:NSLineBreakByTruncatingTail];
    [sectionView addSubview:sectionLabel.get()];

    [sectionLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [NSLayoutConstraint activateConstraints:@[
        [[sectionLabel leadingAnchor] constraintEqualToAnchor:[sectionView leadingAnchor] constant:sectionHeaderMargin],
        [[sectionLabel topAnchor] constraintEqualToAnchor:[sectionView topAnchor] constant:0],
    ]];

    auto collapseButton = adoptNS([[UIButton alloc] init]);
    [collapseButton setTag:section];
    [collapseButton setImage:[UIImage systemImageNamed:@"chevron.down" withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:sectionHeaderCollapseButtonSize weight:UIImageSymbolWeightSemibold]] forState:UIControlStateNormal];
    [collapseButton addTarget:self action:@selector(collapseSection:) forControlEvents:UIControlEventTouchUpInside];
    [sectionView addSubview:collapseButton.get()];

    if ([_collapsedSections containsObject:@(section)])
        [collapseButton setTransform:CGAffineTransformMakeRotation(-M_PI / 2)];

    [collapseButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [NSLayoutConstraint activateConstraints:@[
        [[collapseButton trailingAnchor] constraintEqualToAnchor:[sectionView trailingAnchor] constant:-sectionHeaderMargin],
        [[collapseButton topAnchor] constraintEqualToAnchor:[sectionLabel topAnchor] constant:0],
        [[collapseButton bottomAnchor] constraintEqualToAnchor:[sectionLabel bottomAnchor] constant:0],
    ]];

    return sectionView.autorelease();
}

- (void)collapseSection:(UIButton *)button
{
    NSInteger section = button.tag;
    NSInteger rowCount = [self numberOfRowsInGroup:section];

    NSMutableArray *indexPaths = [NSMutableArray arrayWithCapacity:rowCount];
    for (NSInteger i = 0; i < rowCount; i++)
        [indexPaths addObject:[NSIndexPath indexPathForRow:i inSection:section]];

    NSNumber *object = @(section);
    if ([_collapsedSections containsObject:object]) {
        [_collapsedSections removeObject:object];
        [self.tableView insertRowsAtIndexPaths:indexPaths withRowAnimation:UITableViewRowAnimationFade];
    } else {
        [_collapsedSections addObject:object];
        [self.tableView deleteRowsAtIndexPaths:indexPaths withRowAnimation:UITableViewRowAnimationFade];
    }

    [UIView animateWithDuration:sectionHeaderCollapseButtonTransitionDuration animations:^{
        if (CGAffineTransformEqualToTransform(button.transform, CGAffineTransformIdentity))
            button.transform = CGAffineTransformMakeRotation(-M_PI / 2);
        else
            button.transform = CGAffineTransformIdentity;
    }];
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
        [cell textLabel].text = WEB_UI_STRING_KEY("No Options", "No Options Select Popover", "Empty select list");
        [cell setUserInteractionEnabled:NO];
        [cell imageView].image = nil;
        return cell.autorelease();
    }

    auto option = [self optionItemAtIndexPath:indexPath];
    if (!option)
        return cell.autorelease();

    [cell textLabel].text = option->text;
    [cell textLabel].enabled = !option->disabled;
    [cell setUserInteractionEnabled:!option->disabled];

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
    if (!cell.textLabel.enabled)
        return;

    auto option = [self optionItemAtIndexPath:indexPath];
    if (!option)
        return;

    if (!option->isSelected)
        cell.imageView.image = [UIImage systemImageNamed:@"checkmark.circle.fill"];
    else
        cell.imageView.image = [[UIImage systemImageNamed:@"circle"] imageWithTintColor:UIColor.tertiaryLabelColor renderingMode:UIImageRenderingModeAlwaysOriginal];

    [_contentView page]->setFocusedElementSelectedIndex([self findItemIndexAt:indexPath], true);
    option->isSelected = !option->isSelected;
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
    __weak WKContentView *_view;

    RetainPtr<UINavigationController> _navigationController;
    RetainPtr<WKSelectPickerTableViewController> _tableViewController;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _tableViewController = adoptNS([[WKSelectPickerTableViewController alloc] initWithView:_view]);
    _navigationController = adoptNS([[UINavigationController alloc] initWithRootViewController:_tableViewController.get()]);

    return self;
}

- (void)configurePresentation
{
    if (WebKit::currentUserInterfaceIdiomIsPadOrMac()) {
        [_navigationController setModalPresentationStyle:UIModalPresentationPopover];
        [_navigationController setNavigationBarHidden:YES];
        [_tableViewController setPreferredContentSize:CGSizeMake(selectPopoverLength, selectPopoverLength)];

        UIPopoverPresentationController *presentationController = [_navigationController popoverPresentationController];
        presentationController.delegate = self;
        presentationController.sourceView = _view;
        presentationController.sourceRect = CGRectIntegral(_view.focusedElementInformation.interactionRect);
    } else {
        [[_navigationController navigationBar] setBarTintColor:UIColor.systemGroupedBackgroundColor];

        UIPresentationController *presentationController = [_navigationController presentationController];
        presentationController.delegate = self;
        if ([presentationController isKindOfClass:[_UISheetPresentationController class]]) {
            _UISheetPresentationController *sheetPresentationController = (_UISheetPresentationController *)presentationController;
            sheetPresentationController._detents = @[_UISheetDetent._mediumDetent, _UISheetDetent._largeDetent];
            sheetPresentationController._widthFollowsPreferredContentSizeWhenBottomAttached = YES;
            sheetPresentationController._wantsBottomAttachedInCompactHeight = YES;
        }
    }
}

#pragma mark WKFormControl

- (UIView *)controlView
{
    return nil;
}

- (void)controlBeginEditing
{
    [_view startRelinquishingFirstResponderToFocusedElement];

    [self configurePresentation];
    UIViewController *presentingViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:_view];
    [presentingViewController presentViewController:_navigationController.get() animated:YES completion:nil];
}

- (void)controlEndEditing
{
    [_view stopRelinquishingFirstResponderToFocusedElement];
    [_tableViewController dismissViewControllerAnimated:NO completion:nil];
}

#pragma mark UIPopoverPresentationControllerDelegate

- (void)presentationControllerDidDismiss:(UIPresentationController *)presentationController
{
    [_view accessoryDone];
}

#pragma mark WKTesting

- (NSIndexPath *)_indexPathForRow:(NSInteger)rowIndex
{
    NSInteger currentSection = 0;
    NSInteger currentRow = 0;
    NSInteger totalRows = 0;

    for (auto& option : _view.focusedSelectElementOptions) {
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

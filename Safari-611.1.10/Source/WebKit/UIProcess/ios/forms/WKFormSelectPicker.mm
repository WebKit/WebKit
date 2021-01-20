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
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPopover.h"
#import "WKFormSelectControl.h"
#import "WebPageProxy.h"

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

    NSMutableString *trimmedText = [[item.text mutableCopy] autorelease];
    CFStringTrimWhitespace((CFMutableStringRef)trimmedText);

    [[self titleLabel] setText:trimmedText];
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

    NSMutableString *trimmedText = [[item.text mutableCopy] autorelease];
    CFStringTrimWhitespace((CFMutableStringRef)trimmedText);

    [[self titleLabel] setText:trimmedText];
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
    UIPickerContentView *pickerItem = item.isGroup ? [[[WKOptionGroupPickerCell alloc] initWithOptionItem:item] autorelease] : [[[WKOptionPickerCell alloc] initWithOptionItem:item] autorelease];

    // The cell starts out with a null frame. We need to set its frame now so we can find the right font size.
    UITableView *table = [pickerView tableViewForColumn:0];
    CGRect frame = [table rectForRowAtIndexPath:[NSIndexPath indexPathForRow:rowIndex inSection:0]];
    pickerItem.frame = frame;

    UILabel *titleTextLabel = pickerItem.titleLabel;
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

    return pickerItem;
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
    NSMutableString *trimmedText = [[option.text mutableCopy] autorelease];
    CFStringTrimWhitespace((CFMutableStringRef)trimmedText);

    NSMutableAttributedString *attributedString = [[NSMutableAttributedString alloc] initWithString:trimmedText];
    if (option.disabled)
        [attributedString addAttribute:NSForegroundColorAttributeName value:[UIColor colorWithWhite:0.0 alpha:DisabledOptionAlpha] range:NSMakeRange(0, [trimmedText length])];

    return [attributedString autorelease];
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

#endif  // PLATFORM(IOS_FAMILY)

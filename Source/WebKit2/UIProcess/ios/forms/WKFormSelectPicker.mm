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
#import "WKFormSelectControl.h"

#if PLATFORM(IOS)

#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPopover.h"
#import "WebPageProxy.h"
#import <CoreFoundation/CFUniChar.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIDevice_Private.h>
#import <UIKit/UIKeyboard_Private.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

static const float DisabledOptionAlpha = 0.3;

@implementation WKSelectSinglePicker {
    WKContentView *_view;
    NSInteger _selectedIndex;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super initWithFrame:CGRectZero]))
        return nil;

    _view = view;
    self.delegate = self;
    self.dataSource = self;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    _selectedIndex = NSNotFound;

    for (size_t i = 0; i < [view assistedNodeSelectOptions].size(); ++i) {
        const WKOptionItem item = [_view assistedNodeSelectOptions][i];
        if (item.isSelected) {
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
    self.delegate = nil;
    self.dataSource = nil;

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

    if (_selectedIndex < (NSInteger)[_view assistedNodeSelectOptions].size()) {
        [_view assistedNodeSelectOptions][_selectedIndex].isSelected = false;
        [_view page]->setAssistedNodeSelectedIndex(_selectedIndex);
    }
}

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 1;
}

static NSString *stringByTrimmingWhitespaceAndNewlines(NSString *string)
{
    NSRange r = NSMakeRange(0, [string length]);

    if (!r.length)
        return string;

    NSUInteger originalLength = r.length;

    // First strip all white space off the end of the range
    while (r.length > 0 && CFUniCharIsMemberOf([string characterAtIndex: r.length - 1], kCFUniCharWhitespaceAndNewlineCharacterSet))
        r.length -= 1;

    // Then, trim any whitespace from the start of the range.
    while (r.length > 0 && CFUniCharIsMemberOf([string characterAtIndex: r.location], kCFUniCharWhitespaceAndNewlineCharacterSet)) {
        r.location += 1;
        r.length -= 1;
    }

    // If we changed the length of the string from what it originally was, return a new string with just those
    // characters.  Otherwise just return.
    if (originalLength != r.length)
        return [string substringWithRange:r];

    return string;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)columnIndex
{
    return _view.assistedNodeInformation.selectOptions.size();
}

- (NSAttributedString *)pickerView:(UIPickerView *)pickerView attributedTitleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    if (row < 0 || row >= (NSInteger)[_view assistedNodeSelectOptions].size())
        return nil;

    const WKOptionItem& option = [_view assistedNodeSelectOptions][row];
    NSString *text = stringByTrimmingWhitespaceAndNewlines(option.text);

    NSMutableAttributedString *attributedString = [[NSMutableAttributedString alloc] initWithString:text];
    if (option.disabled)
        [attributedString addAttribute:NSForegroundColorAttributeName value:[UIColor colorWithWhite:0.0 alpha:DisabledOptionAlpha] range:NSMakeRange(0, [text length])];

    return [attributedString autorelease];
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    if (row < 0 || row >= (NSInteger)[_view assistedNodeSelectOptions].size())
        return;

    const WKOptionItem& newSelectedOption = [_view assistedNodeSelectOptions][row];
    if (newSelectedOption.disabled) {
        NSInteger rowToSelect = NSNotFound;

        // Search backwards for the previous enabled option.
        for (NSInteger i = row - 1; i >= 0; --i) {
            const WKOptionItem& earlierOption = [_view assistedNodeSelectOptions][i];
            if (!earlierOption.disabled) {
                rowToSelect = i;
                break;
            }
        }

        // If nothing previous, search forwards for the next enabled option.
        if (rowToSelect == NSNotFound) {
            for (size_t i = row + 1; i < [_view assistedNodeSelectOptions].size(); ++i) {
                const WKOptionItem& laterOption = [_view assistedNodeSelectOptions][i];
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

@end

#endif  // PLATFORM(IOS)

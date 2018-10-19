/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebDataListSuggestionsDropdownIOS.h"

#if ENABLE(DATALIST_ELEMENT) && PLATFORM(IOS_FAMILY)

#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPeripheral.h"
#import "WKFormPopover.h"
#import "WebPageProxy.h"

static const CGFloat maxVisibleSuggestions = 5;
static const CGFloat suggestionsPopoverCellHeight = 44;
static const CGFloat suggestionsPopoverWidth = 320;
static NSString * const suggestionCellReuseIdentifier = @"WKDataListSuggestionCell";

@interface WKDataListSuggestionsControl : NSObject {
    WebKit::WebDataListSuggestionsDropdownIOS* _dropdown;
    Vector<String> _suggestions;
}

@property (nonatomic, weak) WKContentView *view;

- (instancetype)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(WKContentView *)view;
- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information;
- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS *)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType;
- (void)didSelectOptionAtIndex:(NSInteger)index;
- (void)invalidate;

- (NSArray<WKDataListTextSuggestion *> *)textSuggestions;
- (NSInteger)suggestionsCount;
- (String)suggestionAtIndex:(NSInteger)index;
- (NSTextAlignment)textAlignment;
@end

@interface WKDataListSuggestionsPicker : WKDataListSuggestionsControl <UIPickerViewDataSource, UIPickerViewDelegate>
@end

@interface WKDataListSuggestionsPickerView : UIPickerView <WKFormControl>
@property (nonatomic, weak) WKDataListSuggestionsControl *control;
@end

@interface WKDataListSuggestionsPopover : WKDataListSuggestionsControl
@end

@interface WKDataListSuggestionsViewController : UITableViewController
@property (nonatomic, weak) WKDataListSuggestionsControl *control;

- (void)reloadData;
@end

@implementation WKDataListTextSuggestion
@end

#pragma mark - WebDataListSuggestionsDropdownIOS

namespace WebKit {

Ref<WebDataListSuggestionsDropdownIOS> WebDataListSuggestionsDropdownIOS::create(WebPageProxy& page, WKContentView *view)
{
    return adoptRef(*new WebDataListSuggestionsDropdownIOS(page, view));
}

WebDataListSuggestionsDropdownIOS::WebDataListSuggestionsDropdownIOS(WebPageProxy& page, WKContentView *view)
    : WebDataListSuggestionsDropdown(page)
    , m_contentView(view)
{
}

void WebDataListSuggestionsDropdownIOS::show(WebCore::DataListSuggestionInformation&& information)
{
    if (m_suggestionsControl) {
        [m_suggestionsControl updateWithInformation:WTFMove(information)];
        return;
    }

    WebCore::DataListSuggestionActivationType type = information.activationType;

    if (currentUserInterfaceIdiomIsPad())
        m_suggestionsControl = adoptNS([[WKDataListSuggestionsPopover alloc] initWithInformation:WTFMove(information) inView:m_contentView]);
    else
        m_suggestionsControl = adoptNS([[WKDataListSuggestionsPicker alloc] initWithInformation:WTFMove(information) inView:m_contentView]);

    [m_suggestionsControl showSuggestionsDropdown:this activationType:type];
}

void WebDataListSuggestionsDropdownIOS::handleKeydownWithIdentifier(const String&)
{
}

void WebDataListSuggestionsDropdownIOS::close()
{
    [m_suggestionsControl invalidate];
    m_suggestionsControl = nil;
    m_page->didCloseSuggestions();
}

void WebDataListSuggestionsDropdownIOS::didSelectOption(const String& selectedOption)
{
    if (!m_page)
        return;

    m_page->didSelectOption(selectedOption);
    close();
}

} // namespace WebKit

#pragma mark - WKDataListSuggestionsControl

@implementation WKDataListSuggestionsControl

- (instancetype)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _suggestions = WTFMove(information.suggestions);

    return self;
}

- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information
{
    _suggestions = WTFMove(information.suggestions);
}

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS *)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType
{
    _dropdown = dropdown;
}

- (void)didSelectOptionAtIndex:(NSInteger)index
{
    _dropdown->didSelectOption(_suggestions[index]);
}

- (void)invalidate
{
}

- (NSArray<WKDataListTextSuggestion *> *)textSuggestions
{
    NSMutableArray *suggestions = [NSMutableArray array];

    for (auto suggestion : _suggestions) {
        [suggestions addObject:[WKDataListTextSuggestion textSuggestionWithInputText:suggestion]];
        if (suggestions.count == 3)
            break;
    }

    return suggestions;
}

- (NSInteger)suggestionsCount
{
    return _suggestions.size();
}

- (String)suggestionAtIndex:(NSInteger)index
{
    return _suggestions[index];
}

- (NSTextAlignment)textAlignment
{
    return _view.assistedNodeInformation.isRTL ? NSTextAlignmentRight : NSTextAlignmentLeft;
}

@end

#pragma mark - WKDataListSuggestionsPicker

@implementation WKDataListSuggestionsPicker  {
    RetainPtr<WKDataListSuggestionsPickerView> _pickerView;
}

- (instancetype)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(WKContentView *)view
{
    if (!(self = [super initWithInformation:WTFMove(information) inView:view]))
        return nil;

    _pickerView = adoptNS([[WKDataListSuggestionsPickerView alloc] initWithFrame:CGRectZero]);
    [_pickerView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [_pickerView setDataSource:self];
    [_pickerView setDelegate:self];
    [_pickerView setControl:self];
    [_pickerView setSize:[UIKeyboard defaultSizeForInterfaceOrientation:UIApp.interfaceOrientation]];

    return self;
}

- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information
{
    [super updateWithInformation:WTFMove(information)];
    if (information.activationType != WebCore::DataListSuggestionActivationType::IndicatorClicked) {
        self.view.dataListTextSuggestionsInputView = nil;
        self.view.dataListTextSuggestions = self.textSuggestions;
        return;
    }

    self.view.dataListTextSuggestionsInputView = _pickerView.get();

    [_pickerView reloadAllComponents];
    [_pickerView selectRow:0 inComponent:0 animated:NO];
}

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS *)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType
{
    [super showSuggestionsDropdown:dropdown activationType:activationType];
    if (activationType == WebCore::DataListSuggestionActivationType::IndicatorClicked) {
        self.view.dataListTextSuggestionsInputView = _pickerView.get();
        [_pickerView selectRow:0 inComponent:0 animated:NO];
    } else
        self.view.dataListTextSuggestions = self.textSuggestions;
}

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)columnIndex
{
    return [self suggestionsCount];
}

- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    return [self suggestionAtIndex:row];
}

- (void)invalidate
{
    if (self.view.dataListTextSuggestionsInputView == _pickerView.get())
        self.view.dataListTextSuggestionsInputView = nil;

    [_pickerView setDelegate:nil];
    [_pickerView setDataSource:nil];
    [_pickerView setControl:nil];
}

@end

@implementation WKDataListSuggestionsPickerView

- (UIView *)controlView
{
    return self;
}

- (void)controlBeginEditing
{
}

- (void)controlEndEditing
{
    [self.control didSelectOptionAtIndex:[self selectedRowInComponent:0]];
}

@end

#pragma mark - WKDataListSuggestionsPopover

@implementation WKDataListSuggestionsPopover  {
    RetainPtr<WKFormRotatingAccessoryPopover> _popover;
    RetainPtr<WKDataListSuggestionsViewController> _suggestionsViewController;
}

- (instancetype)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(WKContentView *)view
{
    if (!(self = [super initWithInformation:WTFMove(information) inView:view]))
        return nil;

    _popover = adoptNS([[WKFormRotatingAccessoryPopover alloc] initWithView:view]);

    return self;
}

- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information
{
    [super updateWithInformation:WTFMove(information)];
    [_suggestionsViewController reloadData];
    self.view.dataListTextSuggestions = self.textSuggestions;
}

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS *)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType
{
    [super showSuggestionsDropdown:dropdown activationType:activationType];

    _suggestionsViewController = adoptNS([[WKDataListSuggestionsViewController alloc] initWithStyle:UITableViewStylePlain]);
    [_suggestionsViewController setControl:self];
    [_suggestionsViewController reloadData];
    self.view.dataListTextSuggestions = self.textSuggestions;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_popover setPopoverController:[[[UIPopoverController alloc] initWithContentViewController:_suggestionsViewController.get()] autorelease]];
ALLOW_DEPRECATED_DECLARATIONS_END

    [_popover presentPopoverAnimated:NO];
}

- (void)invalidate
{
    [_suggestionsViewController setControl:nil];
}

- (void)didSelectOptionAtIndex:(NSInteger)index
{
    [super didSelectOptionAtIndex:index];
    [[_popover popoverController] dismissPopoverAnimated:YES];
    self.view.dataListTextSuggestions = @[ [WKDataListTextSuggestion textSuggestionWithInputText:[self suggestionAtIndex:index]] ];
}

@end

@implementation WKDataListSuggestionsViewController

- (void)reloadData
{
    [self.tableView reloadData];
    [self setPreferredContentSize:CGSizeMake(suggestionsPopoverWidth, maxVisibleSuggestions * suggestionsPopoverCellHeight + suggestionsPopoverCellHeight / 2)];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [self.control suggestionsCount];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:suggestionCellReuseIdentifier];
    if (!cell)
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:suggestionCellReuseIdentifier] autorelease];

    cell.textLabel.text = [self.control suggestionAtIndex:indexPath.row];
    cell.textLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    cell.textLabel.textAlignment = [self.control textAlignment];

    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [self.control didSelectOptionAtIndex:indexPath.row];
}

@end

#endif // ENABLE(DATALIST_ELEMENT) && PLATFORM(IOS_FAMILY)

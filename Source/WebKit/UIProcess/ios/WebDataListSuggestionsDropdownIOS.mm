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

#import "config.h"
#import "WebDataListSuggestionsDropdownIOS.h"

#if ENABLE(DATALIST_ELEMENT) && PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "UserInterfaceIdiom.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPeripheral.h"
#import "WKFormPopover.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebPageProxy.h"

static const CGFloat maxVisibleSuggestions = 5;
static const CGFloat suggestionsPopoverCellHeight = 44;
static const CGFloat suggestionsPopoverWidth = 320;
static NSString * const suggestionCellReuseIdentifier = @"WKDataListSuggestionCell";

@interface WKDataListSuggestionsControl ()

@property (nonatomic, weak) WKContentView *view;
@property (nonatomic) BOOL isShowingSuggestions;

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS&)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType;

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

#if ENABLE(IOS_FORM_CONTROL_REFRESH)
#if USE(UICONTEXTMENU)
@interface WKDataListSuggestionsDropdown : WKDataListSuggestionsControl <UIContextMenuInteractionDelegate>
#else
@interface WKDataListSuggestionsDropdown : WKDataListSuggestionsControl
#endif
@end
#endif

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

#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (m_contentView._shouldUseContextMenusForFormControls) {
        m_suggestionsControl = adoptNS([[WKDataListSuggestionsDropdown alloc] initWithInformation:WTFMove(information) inView:m_contentView]);
        [m_suggestionsControl showSuggestionsDropdown:*this activationType:type];
        return;
    }
#endif

    if (currentUserInterfaceIdiomIsSmallScreen())
        m_suggestionsControl = adoptNS([[WKDataListSuggestionsPicker alloc] initWithInformation:WTFMove(information) inView:m_contentView]);
    else
        m_suggestionsControl = adoptNS([[WKDataListSuggestionsPopover alloc] initWithInformation:WTFMove(information) inView:m_contentView]);

    [m_suggestionsControl showSuggestionsDropdown:*this activationType:type];
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

@implementation WKDataListSuggestionsControl {
    WeakPtr<WebKit::WebDataListSuggestionsDropdownIOS> _dropdown;
    Vector<WebCore::DataListSuggestion> _suggestions;
}

- (instancetype)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _suggestions = WTFMove(information.suggestions);

    [_view _setDataListSuggestionsControl:self];

    return self;
}

- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information
{
    _suggestions = WTFMove(information.suggestions);
}

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS&)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType
{
    _dropdown = dropdown;
}

- (void)didSelectOptionAtIndex:(NSInteger)index
{
    _dropdown->didSelectOption(_suggestions[index].value);
}

- (void)invalidate
{
}

- (NSArray<WKDataListTextSuggestion *> *)textSuggestions
{
    NSMutableArray *suggestions = [NSMutableArray array];

    for (const auto& suggestion : _suggestions) {
        [suggestions addObject:[WKDataListTextSuggestion textSuggestionWithInputText:suggestion.value]];
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
    return _suggestions[index].value;
}

- (NSTextAlignment)textAlignment
{
    return _view.focusedElementInformation.isRTL ? NSTextAlignmentRight : NSTextAlignmentLeft;
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
    [_pickerView setSize:[UIKeyboard defaultSizeForInterfaceOrientation:view.interfaceOrientation]];

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

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS&)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType
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

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS&)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType
{
    [super showSuggestionsDropdown:dropdown activationType:activationType];

    _suggestionsViewController = adoptNS([[WKDataListSuggestionsViewController alloc] initWithStyle:UITableViewStylePlain]);
    [_suggestionsViewController setControl:self];
    [_suggestionsViewController reloadData];
    self.view.dataListTextSuggestions = self.textSuggestions;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_popover setPopoverController:adoptNS([[UIPopoverController alloc] initWithContentViewController:_suggestionsViewController.get()]).get()];
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

    NSInteger suggestionsCount = [self.control suggestionsCount];
    if (suggestionsCount > maxVisibleSuggestions)
        [self setPreferredContentSize:CGSizeMake(suggestionsPopoverWidth, maxVisibleSuggestions * suggestionsPopoverCellHeight + suggestionsPopoverCellHeight / 2)];
    else
        [self setPreferredContentSize:CGSizeMake(suggestionsPopoverWidth, suggestionsCount * suggestionsPopoverCellHeight)];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [self.control suggestionsCount];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    auto cell = retainPtr([tableView dequeueReusableCellWithIdentifier:suggestionCellReuseIdentifier]);
    if (!cell)
        cell = adoptNS([[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:suggestionCellReuseIdentifier]);

    [cell textLabel].text = [self.control suggestionAtIndex:indexPath.row];
    [cell textLabel].lineBreakMode = NSLineBreakByTruncatingTail;
    [cell textLabel].textAlignment = [self.control textAlignment];

    return cell.autorelease();
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [self.control didSelectOptionAtIndex:indexPath.row];
}

@end

#if ENABLE(IOS_FORM_CONTROL_REFRESH)

#pragma mark - WKDataListSuggestionsDropdown

@implementation WKDataListSuggestionsDropdown {
#if USE(UICONTEXTMENU)
    RetainPtr<NSArray<UIMenuElement *>> _suggestionsMenuElements;
    RetainPtr<UIContextMenuInteraction> _suggestionsContextMenuInteraction;
#endif
}

- (instancetype)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(WKContentView *)view
{
    if (!(self = [super initWithInformation:WTFMove(information) inView:view]))
        return nil;

    return self;
}

- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information
{
    auto activationType = information.activationType;

    [super updateWithInformation:WTFMove(information)];
    [self _displayWithActivationType:activationType];
}

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownIOS&)dropdown activationType:(WebCore::DataListSuggestionActivationType)activationType
{
    [super showSuggestionsDropdown:dropdown activationType:activationType];
    [self _displayWithActivationType:activationType];
}

- (void)invalidate
{
#if USE(UICONTEXTMENU)
    [self _removeContextMenuInteraction];
#endif
}

- (void)didSelectOptionAtIndex:(NSInteger)index
{
    [self.view updateFocusedElementFocusedWithDataListDropdown:NO];
    [super didSelectOptionAtIndex:index];
}

- (void)_displayWithActivationType:(WebCore::DataListSuggestionActivationType)activationType
{
    if (activationType == WebCore::DataListSuggestionActivationType::IndicatorClicked)
        [self.view updateFocusedElementFocusedWithDataListDropdown:YES];
    else if (activationType == WebCore::DataListSuggestionActivationType::ControlClicked)
        [self.view updateFocusedElementFocusedWithDataListDropdown:NO];

    [self _updateTextSuggestions];

    if (![UIKeyboard isInHardwareKeyboardMode] && activationType != WebCore::DataListSuggestionActivationType::IndicatorClicked)
        return;

    [self _showSuggestions];
}

- (void)_showSuggestions
{
#if USE(UICONTEXTMENU)
    [self _updateSuggestionsMenuElements];

    if (!_suggestionsContextMenuInteraction) {
        _suggestionsContextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
        [self.view addInteraction:_suggestionsContextMenuInteraction.get()];

        [self.view doAfterEditorStateUpdateAfterFocusingElement:[weakSelf = WeakObjCPtr<WKDataListSuggestionsDropdown>(self)] {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            auto view = [strongSelf view];
            [view presentContextMenu:strongSelf->_suggestionsContextMenuInteraction.get() atLocation:[view lastInteractionLocation]];
        }];
    } else {
        [_suggestionsContextMenuInteraction updateVisibleMenuWithBlock:[&](UIMenu *visibleMenu) -> UIMenu * {
            return [visibleMenu menuByReplacingChildren:_suggestionsMenuElements.get()];
        }];
    }
#endif
}

- (void)_updateTextSuggestions
{
    self.view.dataListTextSuggestions = self.textSuggestions;
}

#if USE(UICONTEXTMENU)

- (void)_updateSuggestionsMenuElements
{
    NSMutableArray *suggestions = [NSMutableArray arrayWithCapacity:self.suggestionsCount];

    for (NSInteger index = 0; index < self.suggestionsCount; index++) {
        UIAction *suggestionAction = [UIAction actionWithTitle:[self suggestionAtIndex:index] image:nil identifier:nil handler:[weakSelf = WeakObjCPtr<WKDataListSuggestionsDropdown>(self), index] (UIAction *) {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            [strongSelf didSelectOptionAtIndex:index];
        }];

        [suggestions addObject:suggestionAction];
    }

    _suggestionsMenuElements = adoptNS([suggestions copy]);
}

- (void)_removeContextMenuInteraction
{
    if (!_suggestionsContextMenuInteraction)
        return;

    [self.view removeInteraction:_suggestionsContextMenuInteraction.get()];
    _suggestionsContextMenuInteraction = nil;
    [self.view _removeContextMenuHintContainerIfPossible];
    [self.view.webView _didDismissContextMenu];
}

- (void)_suggestionsMenuDidPresent
{
    self.isShowingSuggestions = YES;

    [self.view.webView _didShowContextMenu];
}

- (void)_suggestionsMenuDidDismiss
{
    self.isShowingSuggestions = NO;

    [self.view updateFocusedElementFocusedWithDataListDropdown:NO];
    [self _updateTextSuggestions];

    [self _removeContextMenuInteraction];
}

- (UIEdgeInsets)_preferredEdgeInsetsForSuggestionsMenu
{
    CGRect windowBounds = self.view.textEffectsWindow.bounds;
    CGRect elementFrameInWindowCoordinates = [self.view convertRect:self.view.focusedElementInformation.interactionRect toView:nil];

    if (CGRectGetMidY(elementFrameInWindowCoordinates) > CGRectGetMidY(windowBounds))
        return UIEdgeInsetsMake(0, 0, CGRectGetMaxY(windowBounds) - CGRectGetMinY(elementFrameInWindowCoordinates), 0);

    // Use MinY rather than MaxY to account for the hint preview.
    return UIEdgeInsetsMake(CGRectGetMinY(elementFrameInWindowCoordinates), 0, 0, 0);
}

#pragma mark UIContextMenuInteractionDelegate

#if HAVE(UI_CONTEXT_MENU_PREVIEW_ITEM_IDENTIFIER)
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configuration:(UIContextMenuConfiguration *)configuration highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier
#else
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
#endif
{
    return [self.view _createTargetedContextMenuHintPreviewForFocusedElement];
}

- (_UIContextMenuStyle *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction styleForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    _UIContextMenuStyle *style = [_UIContextMenuStyle defaultStyle];
    style.preferredLayout = _UIContextMenuLayoutCompactMenu;
    style.preferredEdgeInsets = [self _preferredEdgeInsetsForSuggestionsMenu];

    return style;
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location
{
    UIContextMenuActionProvider actionMenuProvider = [weakSelf = WeakObjCPtr<WKDataListSuggestionsDropdown>(self)] (NSArray<UIMenuElement *> *) -> UIMenu * {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return nil;

        return [UIMenu menuWithTitle:@"" children:strongSelf->_suggestionsMenuElements.get()];
    };

    return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:actionMenuProvider];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id <UIContextMenuInteractionAnimating>)animator
{
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKDataListSuggestionsDropdown>(self)] {
        if (auto strongSelf = weakSelf.get())
            [strongSelf _suggestionsMenuDidPresent];
    }];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id <UIContextMenuInteractionAnimating>)animator
{
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKDataListSuggestionsDropdown>(self)] {
        if (auto strongSelf = weakSelf.get())
            [strongSelf _suggestionsMenuDidDismiss];
    }];
}

#endif // USE(UICONTEXTMENU)

@end

#endif // ENABLE(IOS_FORM_CONTROL_REFRESH)

#endif // ENABLE(DATALIST_ELEMENT) && PLATFORM(IOS_FAMILY)

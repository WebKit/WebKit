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
#import "WKFormSelectPopover.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPopover.h"
#import "WKFormSelectControl.h"
#import "WebPageProxy.h"
#import <UIKit/UIPickerView.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

static NSString* WKPopoverTableViewCellReuseIdentifier  = @"WKPopoverTableViewCellReuseIdentifier";

@interface UITableViewCell (Internal)
- (CGRect)textRectForContentRect:(CGRect)contentRect;
- (CGRect)contentRectForBounds:(CGRect)bounds;
@end

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static NSString *stringWithWritingDirection(NSString *string, UITextWritingDirection writingDirection, bool override)
{
    if (![string length] || writingDirection == UITextWritingDirectionNatural)
        return string;
    
    if (!override) {
        UCharDirection firstCharacterDirection = u_charDirection([string characterAtIndex:0]);
        if ((firstCharacterDirection == U_LEFT_TO_RIGHT && writingDirection == UITextWritingDirectionLeftToRight)
            || (firstCharacterDirection == U_RIGHT_TO_LEFT && writingDirection == UITextWritingDirectionRightToLeft))
            return string;
    }
    
    const unichar leftToRightEmbedding = 0x202A;
    const unichar rightToLeftEmbedding = 0x202B;
    const unichar popDirectionalFormatting = 0x202C;
    const unichar leftToRightOverride = 0x202D;
    const unichar rightToLeftOverride = 0x202E;
    
    unichar directionalFormattingCharacter;
    if (writingDirection == UITextWritingDirectionLeftToRight)
        directionalFormattingCharacter = (override ? leftToRightOverride : leftToRightEmbedding);
    else
        directionalFormattingCharacter = (override ? rightToLeftOverride : rightToLeftEmbedding);
    
    return [NSString stringWithFormat:@"%C%@%C", directionalFormattingCharacter, string, popDirectionalFormatting];
}
ALLOW_DEPRECATED_DECLARATIONS_END

@class WKSelectPopover;

@interface WKSelectTableViewController : UITableViewController <UIKeyInput>
{
    NSUInteger _singleSelectionIndex;
    NSUInteger _singleSelectionSection;
    NSInteger _numberOfSections;
    BOOL _allowsMultipleSelection;
    
    CGFloat _fontSize;
    CGFloat _maximumTextWidth;
    NSTextAlignment _textAlignment;
    
    WKSelectPopover *_popover;
    WKContentView *_contentView;
}

@property(nonatomic,assign) WKSelectPopover *popover;
@end

@implementation WKSelectTableViewController

- (id)initWithView:(WKContentView *)view hasGroups:(BOOL)hasGroups
{
    if (!(self = [super initWithStyle:UITableViewStylePlain]))
        return nil;
    
    _contentView = view;
    Vector<OptionItem>& selectOptions = [_contentView assistedNodeSelectOptions];
    _allowsMultipleSelection = _contentView.assistedNodeInformation.isMultiSelect;
    
    // Even if the select is empty, there is at least one tableview section.
    _numberOfSections = 1;
    _singleSelectionIndex = NSNotFound;
    NSInteger currentIndex = 0;
    for (size_t i = 0; i < selectOptions.size(); ++i) {
        const OptionItem& item = selectOptions[i];
        if (item.isGroup) {
            _numberOfSections++;
            currentIndex = 0;
            continue;
        }
        if (!_allowsMultipleSelection && item.isSelected) {
            _singleSelectionIndex = currentIndex;
            _singleSelectionSection = item.parentGroupID;
        }
        currentIndex++;
    }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    UITextWritingDirection writingDirection = _contentView.assistedNodeInformation.isRTL ? UITextWritingDirectionRightToLeft : UITextWritingDirectionLeftToRight;
    BOOL override = NO;
    _textAlignment = (writingDirection == UITextWritingDirectionLeftToRight) ? NSTextAlignmentLeft : NSTextAlignmentRight;

    // Typically UIKit apps have their writing direction follow the system
    // language. However WebKit wants to follow the content direction.
    // For that reason we have to override what the system thinks.
    if (writingDirection == UITextWritingDirectionRightToLeft)
        self.view.semanticContentAttribute = UISemanticContentAttributeForceRightToLeft;
    [self setTitle:stringWithWritingDirection(_contentView.assistedNodeInformation.title, writingDirection, override)];
    ALLOW_DEPRECATED_DECLARATIONS_END

    return self;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

    if (_singleSelectionIndex == NSNotFound)
        return;

    if (_singleSelectionSection >= (NSUInteger)[self.tableView numberOfSections])
        return;

    if (_singleSelectionIndex >= (NSUInteger)[self.tableView numberOfRowsInSection:_singleSelectionSection])
        return;

    NSIndexPath *indexPath = [NSIndexPath indexPathForRow:_singleSelectionIndex inSection:_singleSelectionSection];
    [self.tableView scrollToRowAtIndexPath:indexPath atScrollPosition:UITableViewScrollPositionMiddle animated:NO];
}

#pragma mark UITableView delegate methods

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return _numberOfSections;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    if ([_contentView assistedNodeSelectOptions].isEmpty())
        return 1;
    
    int rowCount = 0;
    for (size_t i = 0; i < [_contentView assistedNodeSelectOptions].size(); ++i) {
        const OptionItem& item = [_contentView assistedNodeSelectOptions][i];
        if (item.isGroup)
            continue;
        if (item.parentGroupID == section)
            rowCount++;
        if (item.parentGroupID > section)
            break;
    }
    return rowCount;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    // The first section never has a header. It is for selects without groups.
    if (section == 0)
        return nil;
    
    int groupCount = 0;
    for (size_t i = 0; i < [_contentView assistedNodeSelectOptions].size(); ++i) {
        const OptionItem& item = [_contentView assistedNodeSelectOptions][i];
        if (!item.isGroup)
            continue;
        groupCount++;
        if (item.isGroup && groupCount == section)
            return item.text;
    }
    return nil;
}

- (void)populateCell:(UITableViewCell *)cell withItem:(const OptionItem&)item
{
    [cell.textLabel setText:item.text];
    [cell.textLabel setEnabled:!item.disabled];
    [cell setSelectionStyle:item.disabled ? UITableViewCellSelectionStyleNone : UITableViewCellSelectionStyleBlue];
    [cell setAccessoryType:item.isSelected ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone];
}

- (NSInteger)findItemIndexAt:(NSIndexPath *)indexPath
{
    ASSERT(indexPath.row >= 0);
    ASSERT(indexPath.section <= _numberOfSections);
    
    int optionIndex = 0;
    int rowIndex = 0;
    for (size_t i = 0; i < [_contentView assistedNodeSelectOptions].size(); ++i) {
        const OptionItem& item = [_contentView assistedNodeSelectOptions][i];
        if (item.isGroup) {
            rowIndex = 0;
            continue;
        }
        if (item.parentGroupID == indexPath.section && rowIndex == indexPath.row)
            return optionIndex;
        optionIndex++;
        rowIndex++;
    }
    return NSNotFound;
}

- (OptionItem *)findItemAt:(NSIndexPath *)indexPath
{
    ASSERT(indexPath.row >= 0);
    ASSERT(indexPath.section <= _numberOfSections);

    int index = 0;
    for (size_t i = 0; i < [_contentView assistedNodeSelectOptions].size(); ++i) {
        OptionItem& item = [_contentView assistedNodeSelectOptions][i];
        if (item.isGroup || item.parentGroupID != indexPath.section)
            continue;
        if (index == indexPath.row)
            return &item;
        index++;
    }
    return nil;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:WKPopoverTableViewCellReuseIdentifier];
    if (!cell)
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:WKPopoverTableViewCellReuseIdentifier] autorelease];
    
    cell.semanticContentAttribute = self.view.semanticContentAttribute;
    cell.textLabel.textAlignment = _textAlignment;
    
    if (_contentView.assistedNodeInformation.selectOptions.isEmpty()) {
        cell.textLabel.enabled = NO;
        cell.textLabel.text = WEB_UI_STRING_KEY("No Options", "No Options Select Popover", "Empty select list");
        cell.accessoryType = UITableViewCellAccessoryNone;
        cell.selectionStyle = UITableViewCellSelectionStyleNone;
        return cell;
    }
    
    CGRect textRect = [cell textRectForContentRect:[cell contentRectForBounds:[cell bounds]]];
    ASSERT(textRect.size.width > 0.0);
    
    // Assume all cells have the same available text width.
    UIFont *font = cell.textLabel.font;
    CGFloat initialFontSize = font.pointSize;
    ASSERT(initialFontSize);
    if (textRect.size.width != _maximumTextWidth || _fontSize == 0) {
        _maximumTextWidth = textRect.size.width;
        _fontSize = adjustedFontSize(_maximumTextWidth, font, initialFontSize, _contentView.assistedNodeInformation.selectOptions);
    }
    
    const OptionItem* item = [self findItemAt:indexPath];
    ASSERT(item);
    
    [self populateCell:cell withItem:*item];
    [cell.textLabel setFont:[font fontWithSize:_fontSize]];
    [cell.textLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [cell.textLabel setNumberOfLines:2];
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (_contentView.assistedNodeInformation.selectOptions.isEmpty())
        return;
    
    NSInteger itemIndex = [self findItemIndexAt:indexPath];
    ASSERT(itemIndex != NSNotFound);
    
    if (_allowsMultipleSelection) {
        [tableView deselectRowAtIndexPath:[tableView indexPathForSelectedRow] animated:NO];
        
        UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
        if (!cell.textLabel.enabled)
            return;
        
        BOOL newStateIsSelected = (cell.accessoryType == UITableViewCellAccessoryNone);
        
        cell.accessoryType = newStateIsSelected ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone;
        
        ASSERT(itemIndex != NSNotFound);
        
        // To trigger onchange events programmatically we need to go through this
        // SPI which mimics a user action on the <select>. Normally programmatic
        // changes do not trigger "change" events on such selects.
    
        [_contentView page]->setAssistedNodeSelectedIndex(itemIndex, true);
        OptionItem& item = [_contentView assistedNodeSelectOptions][itemIndex];
        item.isSelected = newStateIsSelected;
    } else {
        [tableView deselectRowAtIndexPath:indexPath animated:NO];
        
        // It is possible for there to be no selection, for example with <select size="2">.
        NSIndexPath *oldIndexPath = nil;
        if (_singleSelectionIndex != NSNotFound) {
            oldIndexPath = [NSIndexPath indexPathForRow:_singleSelectionIndex inSection:_singleSelectionSection];
            if ([indexPath isEqual:oldIndexPath]) {
                [_popover _userActionDismissedPopover:nil];
                return;
            }
        }
        
        UITableViewCell *newCell = [tableView cellForRowAtIndexPath:indexPath];
        
        if (!newCell.textLabel.enabled)
            return;
        
        if (oldIndexPath) {
            UITableViewCell *oldCell = [tableView cellForRowAtIndexPath:oldIndexPath];
            if (oldCell && oldCell.accessoryType == UITableViewCellAccessoryCheckmark)
                oldCell.accessoryType = UITableViewCellAccessoryNone;
        }
        
        if (newCell && newCell.accessoryType == UITableViewCellAccessoryNone) {
            newCell.accessoryType = UITableViewCellAccessoryCheckmark;
            
            _singleSelectionIndex = indexPath.row;
            _singleSelectionSection = indexPath.section;
 
            [_contentView page]->setAssistedNodeSelectedIndex(itemIndex);
            OptionItem& newItem = [_contentView assistedNodeSelectOptions][itemIndex];
            newItem.isSelected = true;
        }
        
        // Need to update the model even if there isn't a cell.
        if (oldIndexPath) {
            if (OptionItem* oldItem = [self findItemAt:oldIndexPath])
                oldItem->isSelected = false;
        }
        
        [_popover _userActionDismissedPopover:nil];
    }
}

#pragma mark UIKeyInput delegate methods

- (BOOL)hasText
{
    return NO;
}

- (void)insertText:(NSString *)text
{
}

- (void)deleteBackward
{
}

@end

@implementation WKSelectPopover {
    RetainPtr<WKSelectTableViewController> _tableViewController;
}

- (instancetype)initWithView:(WKContentView *)view hasGroups:(BOOL)hasGroups
{
    if (!(self = [super initWithView:view]))
        return nil;
    
    CGRect frame;
    frame.origin = CGPointZero;
    frame.size = [UIKeyboard defaultSizeForInterfaceOrientation:[UIApp interfaceOrientation]];

    _tableViewController = adoptNS([[WKSelectTableViewController alloc] initWithView:view hasGroups:hasGroups]);
    [_tableViewController setPopover:self];
    UIViewController *popoverViewController = _tableViewController.get();
    UINavigationController *navController = nil;
    BOOL needsNavigationController = !view.assistedNodeInformation.title.isEmpty();
    if (needsNavigationController) {
        navController = [[UINavigationController alloc] initWithRootViewController:_tableViewController.get()];
        popoverViewController = navController;
    }
    
    CGSize popoverSize = [_tableViewController.get().tableView sizeThatFits:CGSizeMake(320, CGFLOAT_MAX)];
    if (needsNavigationController)
        [(UINavigationController *)popoverViewController topViewController].preferredContentSize = popoverSize;
    else
        popoverViewController.preferredContentSize = popoverSize;
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    self.popoverController = [[[UIPopoverController alloc] initWithContentViewController:popoverViewController] autorelease];
    ALLOW_DEPRECATED_DECLARATIONS_END

    [navController release];
    
    [[UIKeyboardImpl sharedInstance] setDelegate:_tableViewController.get()];
    
    return self;
}

- (void)dealloc
{
    [_tableViewController setPopover:nil];
    _tableViewController.get().tableView.dataSource = nil;
    _tableViewController.get().tableView.delegate = nil;
    
    [super dealloc];
}

- (UIView *)controlView
{
    return nil;
}

- (void)controlBeginEditing
{
    [self presentPopoverAnimated:NO];
}

- (void)controlEndEditing
{
}

- (void)_userActionDismissedPopover:(id)sender
{
    [self accessoryDone];
}

- (UITableViewController *)tableViewController
{
    return _tableViewController.get();
}

@end

@implementation WKSelectPopover(WKTesting)

- (void)selectRow:(NSInteger)rowIndex inComponent:(NSInteger)componentIndex extendingSelection:(BOOL)extendingSelection
{
    NSIndexPath *indexPath = [NSIndexPath indexPathForRow:rowIndex inSection:componentIndex];
    [[_tableViewController tableView] selectRowAtIndexPath:indexPath animated:NO scrollPosition:UITableViewScrollPositionMiddle];
    // Inform the delegate, since -selectRowAtIndexPath:... doesn't do that.
    [_tableViewController tableView:[_tableViewController tableView] didSelectRowAtIndexPath:indexPath];
}

@end

#endif  // PLATFORM(IOS_FAMILY)

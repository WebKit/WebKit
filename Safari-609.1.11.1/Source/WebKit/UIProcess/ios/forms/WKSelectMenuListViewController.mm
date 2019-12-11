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
#import "WKSelectMenuListViewController.h"

#if PLATFORM(WATCHOS)

#import "UIKitSPI.h"
#import <PepperUICore/PUICQuickboardListViewController.h>
#import <PepperUICore/PUICQuickboardListViewControllerSubclass.h>
#import <PepperUICore/PUICResources.h>
#import <PepperUICore/PUICTableView.h>
#import <PepperUICore/PUICTableViewCell.h>
#import <wtf/RetainPtr.h>

static const CGFloat checkmarkImageViewWidth = 32;
static const CGFloat selectMenuItemHorizontalMargin = 9;
static const CGFloat selectMenuItemCellHeight = 44;
static NSString * const selectMenuCellReuseIdentifier = @"WebKitSelectMenuItemCell";

typedef NS_ENUM(NSInteger, PUICQuickboardListSection) {
    PUICQuickboardListSectionHeaderView,
    PUICQuickboardListSectionTrayButtons,
    PUICQuickboardListSectionTextOptions,
    PUICQuickboardListSectionContentUnavailable,
};

@interface WKSelectMenuItemCell : WKQuickboardListItemCell
@property (nonatomic, readonly) UIImageView *imageView;
@end

@implementation WKSelectMenuItemCell {
    RetainPtr<UIImageView> _imageView;
}

- (instancetype) initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    if (!(self = [super initWithStyle:style reuseIdentifier:reuseIdentifier]))
        return nil;

    _imageView = adoptNS([[UIImageView alloc] init]);
    UIImage *checkmarkImage = [PUICResources imageNamed:@"UIPreferencesBlueCheck" inBundle:[NSBundle bundleWithIdentifier:@"com.apple.PepperUICore"] shouldCache:YES];
    [_imageView setImage:[checkmarkImage _flatImageWithColor:[UIColor systemBlueColor]]];
    [_imageView setContentMode:UIViewContentModeCenter];
    [_imageView setHidden:YES];
    [self.contentView addSubview:_imageView.get()];
    return self;
}

- (UIImageView *)imageView
{
    return _imageView.get();
}

@end

@implementation WKSelectMenuListViewController {
    BOOL _isMultipleSelect;
    RetainPtr<NSMutableIndexSet> _indicesOfCheckedOptions;
}

@dynamic delegate;

- (instancetype)initWithDelegate:(id <WKSelectMenuListViewControllerDelegate>)delegate
{
    return [super initWithDelegate:delegate];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.cancelButton.hidden = YES;
    self.showsAcceptButton = YES;

    _isMultipleSelect = [self.delegate selectMenuUsesMultipleSelection:self];
    _indicesOfCheckedOptions = adoptNS([[NSMutableIndexSet alloc] init]);
    for (NSInteger index = 0; index < self.numberOfListItems; ++index) {
        if ([self.delegate selectMenu:self hasSelectedOptionAtIndex:index])
            [_indicesOfCheckedOptions addIndex:index];
    }
}

#pragma mark - Quickboard subclassing

- (void)acceptButtonTappedWithCompletion:(PUICQuickboardCompletionBlock)completion
{
    completion(nil);
}

- (BOOL)shouldShowTrayView
{
    return NO;
}

- (void)didSelectListItem:(NSInteger)itemNumber
{
    NSMutableArray *indexPathsToReload = [NSMutableArray array];
    if (_isMultipleSelect) {
        BOOL addIndex = ![_indicesOfCheckedOptions containsIndex:itemNumber];
        if (addIndex)
            [_indicesOfCheckedOptions addIndex:itemNumber];
        else
            [_indicesOfCheckedOptions removeIndex:itemNumber];
        [self.delegate selectMenu:self didCheckItemAtIndex:itemNumber checked:addIndex];
        [indexPathsToReload addObject:[NSIndexPath indexPathForRow:itemNumber inSection:PUICQuickboardListSectionTextOptions]];
    } else {
        NSInteger previousSelectedIndex = [_indicesOfCheckedOptions firstIndex];
        if (previousSelectedIndex != itemNumber) {
            [_indicesOfCheckedOptions removeAllIndexes];
            [_indicesOfCheckedOptions addIndex:itemNumber];
            [self.delegate selectMenu:self didSelectItemAtIndex:itemNumber];
            if (previousSelectedIndex != NSNotFound)
                [indexPathsToReload addObject:[NSIndexPath indexPathForRow:previousSelectedIndex inSection:PUICQuickboardListSectionTextOptions]];
            [indexPathsToReload addObject:[NSIndexPath indexPathForRow:itemNumber inSection:PUICQuickboardListSectionTextOptions]];
        }
    }

    if (indexPathsToReload.count)
        [self.listView reloadRowsAtIndexPaths:indexPathsToReload withRowAnimation:UITableViewRowAnimationNone];
}

- (NSInteger)numberOfListItems
{
    return [self.delegate numberOfItemsInSelectMenu:self];
}

- (CGFloat)heightForListItem:(NSInteger)itemNumber width:(CGFloat)width
{
    return selectMenuItemCellHeight;
}

- (PUICQuickboardListItemCell *)cellForListItem:(NSInteger)itemNumber
{
    auto reusableCell = retainPtr([self.listView dequeueReusableCellWithIdentifier:selectMenuCellReuseIdentifier]);
    if (!reusableCell) {
        reusableCell = adoptNS([[WKSelectMenuItemCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:selectMenuCellReuseIdentifier]);
        [reusableCell itemLabel].numberOfLines = 1;
        [reusableCell itemLabel].lineBreakMode = NSLineBreakByTruncatingTail;
        [reusableCell itemLabel].allowsDefaultTighteningForTruncation = YES;
        [reusableCell imageView].frame = UIRectInset([reusableCell contentView].bounds, 0, 0, 0, CGRectGetWidth([reusableCell contentView].bounds) - checkmarkImageViewWidth);
    }

    NSString *optionText = [self.delegate selectMenu:self displayTextForItemAtIndex:itemNumber];
    [reusableCell configureForText:optionText width:CGRectGetWidth(self.listView.bounds)];
    [reusableCell setRadioSectionCell:!_isMultipleSelect];

    if ([_indicesOfCheckedOptions containsIndex:itemNumber]) {
        [reusableCell itemLabel].frame = UIRectInset([reusableCell contentView].bounds, 0, selectMenuItemHorizontalMargin + checkmarkImageViewWidth, 0, selectMenuItemHorizontalMargin);
        [reusableCell imageView].hidden = NO;
    } else {
        [reusableCell itemLabel].frame = UIRectInset([reusableCell contentView].bounds, 0, selectMenuItemHorizontalMargin, 0, selectMenuItemHorizontalMargin);
        [reusableCell imageView].hidden = YES;
    }

    return reusableCell.autorelease();
}

- (void)selectItemAtIndex:(NSInteger)index
{
    [self didSelectListItem:index];
}

@end

#endif // PLATFORM(WATCHOS)

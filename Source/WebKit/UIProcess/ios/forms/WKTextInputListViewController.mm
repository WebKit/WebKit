/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKTextInputListViewController.h"

#if PLATFORM(WATCHOS)

#import "WKNumberPadViewController.h"
#import <PepperUICore/PUICQuickboardArouetViewController.h>
#import <PepperUICore/PUICQuickboardListViewControllerSubclass.h>
#import <PepperUICore/PUICQuickboardListViewSpecs.h>
#import <PepperUICore/PUICResources.h>
#import <PepperUICore/PUICTableView.h>
#import <PepperUICore/PUICTableViewCell.h>
#import <wtf/RetainPtr.h>

#if HAVE(QUICKBOARD_COLLECTION_VIEWS)
#import <PepperUICore/PUICQuickboardListCollectionViewItemCell.h>
#endif

static const CGFloat textSuggestionButtonHeight = 44;
static const CGFloat textSuggestionLabelHorizontalMargin = 9;
static NSString *textSuggestionCellReuseIdentifier = @"WebKitQuickboardTextSuggestionCell";

@implementation WKTextInputListViewController {
    RetainPtr<WKNumberPadViewController> _numberPadViewController;
}

@dynamic delegate;

- (instancetype)initWithDelegate:(id <WKTextInputListViewControllerDelegate>)delegate
{
    if (!(self = [super initWithDelegate:delegate]))
        return nil;

    self.textContentType = [self.delegate textContentTypeForListViewController:self];
    return self;
}

- (void)willPresentArouetViewController:(PUICQuickboardArouetViewController *)quickboard
{
    NSString *initialText = [self.delegate initialValueForViewController:self];
    if (initialText.length)
        [quickboard setInputText:initialText selectionRange:NSMakeRange(initialText.length, 0)];

    quickboard.minTextLengthForEnablingAccept = 0;
}

- (NSArray *)additionalTrayButtons
{
    if ([self.delegate numericInputModeForListViewController:self] == WKNumberPadInputModeNone)
        return @[ ];

    UIImage *numberPadButtonIcon = [PUICResources imageNamed:@"QuickboardAddPhoneNumber" inBundle:[NSBundle bundleWithIdentifier:@"com.apple.PepperUICore"] shouldCache:YES];

    auto numberPadButton = adoptNS([[PUICQuickboardListTrayButton alloc] initWithFrame:CGRectZero tintColor:nil defaultHeight:self.specs.defaultButtonHeight]);
    [numberPadButton setImage:numberPadButtonIcon forState:UIControlStateNormal];
    [numberPadButton addTarget:self action:@selector(presentNumberPadViewController) forControlEvents:UIControlEventTouchUpInside];
    return @[ numberPadButton.get() ];
}

- (void)presentNumberPadViewController
{
    if (_numberPadViewController)
        return;

    WKNumberPadInputMode mode = [self.delegate numericInputModeForListViewController:self];
    if (mode == WKNumberPadInputModeNone) {
        ASSERT_NOT_REACHED();
        return;
    }

    NSString *initialText = [self.delegate initialValueForViewController:self];
    _numberPadViewController = adoptNS([[WKNumberPadViewController alloc] initWithDelegate:self.delegate initialText:initialText inputMode:mode]);
    [self presentViewController:_numberPadViewController.get() animated:YES completion:nil];
}

- (void)reloadTextSuggestions
{
    [self reloadListItems];
}

- (void)enterText:(NSString *)text
{
    [self.delegate quickboard:self textEntered:[[[NSAttributedString alloc] initWithString:text] autorelease]];
}

#pragma mark - Quickboard subclassing

- (BOOL)supportsDictationInput
{
    return [self.delegate allowsDictationInputForListViewController:self];
}

- (void)didSelectListItemAtIndexPath:(NSIndexPath *)indexPath
{
    [self _didSelectListItem:indexPath.row];
}

// FIXME: This method can be removed when <rdar://problem/57807445> lands in a build.
- (void)didSelectListItem:(NSInteger)itemNumber
{
    [self _didSelectListItem:itemNumber];
}

- (void)_didSelectListItem:(NSInteger)itemNumber
{
    NSArray<UITextSuggestion *> *textSuggestions = [self.delegate textSuggestionsForListViewController:self];
    if (textSuggestions.count <= static_cast<NSUInteger>(itemNumber)) {
        ASSERT_NOT_REACHED();
        return;
    }

    [self.delegate listViewController:self didSelectTextSuggestion:[textSuggestions objectAtIndex:itemNumber]];
}

- (NSInteger)numberOfListItems
{
    return [self.delegate textSuggestionsForListViewController:self].count;
}

- (CGFloat)heightForListItem:(NSInteger)itemNumber width:(CGFloat)width
{
    return textSuggestionButtonHeight;
}

// FIXME: This method can be removed when <rdar://problem/57807445> lands in a build.
- (PUICQuickboardListItemCell *)cellForListItem:(NSInteger)itemNumber
{
    NSArray<UITextSuggestion *> *textSuggestions = [self.delegate textSuggestionsForListViewController:self];
    if (textSuggestions.count <= static_cast<NSUInteger>(itemNumber)) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    UITextSuggestion *textSuggestion = [textSuggestions objectAtIndex:itemNumber];
    auto reusableCell = retainPtr([self.listView dequeueReusableCellWithIdentifier:textSuggestionCellReuseIdentifier]);
    if (!reusableCell) {
        reusableCell = adoptNS([[WKQuickboardListItemCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:textSuggestionCellReuseIdentifier]);
        [reusableCell itemLabel].numberOfLines = 1;
        [reusableCell itemLabel].lineBreakMode = NSLineBreakByTruncatingTail;
        [reusableCell itemLabel].allowsDefaultTighteningForTruncation = YES;
    }

    CGFloat viewWidth = CGRectGetWidth(self.listView.bounds);
    [reusableCell configureForText:textSuggestion.displayText width:viewWidth];

    // The default behavior of -configureForText:width: causes the text label to run off the end of the cell.
    // Adjust for this by shrinking the label width to actually fit the bounds of the cell.
    [reusableCell itemLabel].frame = UIRectInset([reusableCell contentView].bounds, 0, textSuggestionLabelHorizontalMargin, 0, textSuggestionLabelHorizontalMargin);
    return reusableCell.autorelease();
}

- (NSString *)listItemCellReuseIdentifier
{
    return textSuggestionCellReuseIdentifier;
}

#if HAVE(QUICKBOARD_COLLECTION_VIEWS)

- (Class)listItemCellClass
{
    return [WKQuickboardListCollectionViewItemCell class];
}

- (PUICQuickboardListCollectionViewItemCell *)itemCellForListItem:(NSInteger)itemNumber forIndexPath:(NSIndexPath *)indexPath
{
    NSArray<UITextSuggestion *> *textSuggestions = [self.delegate textSuggestionsForListViewController:self];
    if (textSuggestions.count <= static_cast<NSUInteger>(itemNumber)) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    UITextSuggestion *textSuggestion = [textSuggestions objectAtIndex:itemNumber];
    auto reusableCell = retainPtr([self.collectionView dequeueReusableCellWithReuseIdentifier:textSuggestionCellReuseIdentifier forIndexPath:indexPath]);

    [reusableCell bodyLabel].numberOfLines = 1;
    [reusableCell bodyLabel].lineBreakMode = NSLineBreakByTruncatingTail;
    [reusableCell bodyLabel].allowsDefaultTighteningForTruncation = YES;
    [reusableCell setText:textSuggestion.displayText];

    // The default behavior of -configureForText:width: causes the text label to run off the end of the cell.
    // Adjust for this by shrinking the label width to actually fit the bounds of the cell.
    [reusableCell bodyLabel].frame = UIRectInset([reusableCell contentView].bounds, 0, textSuggestionLabelHorizontalMargin, 0, textSuggestionLabelHorizontalMargin);
    return reusableCell.autorelease();
}

#endif // HAVE(QUICKBOARD_COLLECTION_VIEWS)

- (BOOL)supportsArouetInput
{
    return [self.delegate numericInputModeForListViewController:self] == WKNumberPadInputModeNone;
}

@end

#endif // PLATFORM(WATCHOS)

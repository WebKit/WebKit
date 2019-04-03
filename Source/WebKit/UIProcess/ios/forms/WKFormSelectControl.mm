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

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPopover.h"
#import "WKFormSelectPicker.h"
#import "WKFormSelectPopover.h"
#import "WebPageProxy.h"
#import <UIKit/UIPickerView.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

static const CGFloat minimumOptionFontSize = 12;

CGFloat adjustedFontSize(CGFloat textWidth, UIFont *font, CGFloat initialFontSize, const Vector<OptionItem>& items)
{
    CGFloat adjustedSize = initialFontSize;
    for (size_t i = 0; i < items.size(); ++i) {
        const OptionItem& item = items[i];
        if (item.text.isEmpty())
            continue;

        CGFloat actualFontSize = initialFontSize;
        [(NSString *)item.text _legacy_sizeWithFont:font minFontSize:minimumOptionFontSize actualFontSize:&actualFontSize forWidth:textWidth lineBreakMode:NSLineBreakByWordWrapping];

        if (actualFontSize > 0 && actualFontSize < adjustedSize)
            adjustedSize = actualFontSize;
    }
    return adjustedSize;
}

@implementation WKFormSelectControl {
    RetainPtr<NSObject<WKFormControl>> _control;
}

- (instancetype)initWithView:(WKContentView *)view
{
    bool hasGroups = false;
    for (size_t i = 0; i < view.focusedElementInformation.selectOptions.size(); ++i) {
        if (view.focusedElementInformation.selectOptions[i].isGroup) {
            hasGroups = true;
            break;
        }
    }

    RetainPtr<NSObject <WKFormControl>> control;
    if (currentUserInterfaceIdiomIsPad())
        control = adoptNS([[WKSelectPopover alloc] initWithView:view hasGroups:hasGroups]);
    else if (view.focusedElementInformation.isMultiSelect || hasGroups)
        control = adoptNS([[WKMultipleSelectPicker alloc] initWithView:view]);
    else
        control = adoptNS([[WKSelectSinglePicker alloc] initWithView:view]);

    return [super initWithView:view control:WTFMove(control)];
}

@end

@implementation WKFormSelectControl(WKTesting)

- (void)selectRow:(NSInteger)rowIndex inComponent:(NSInteger)componentIndex extendingSelection:(BOOL)extendingSelection
{
    if ([self.control respondsToSelector:@selector(selectRow:inComponent:extendingSelection:)])
        [id<WKSelectTesting>(self.control) selectRow:rowIndex inComponent:componentIndex extendingSelection:extendingSelection];
}

- (NSString *)selectFormPopoverTitle
{
    if (![self.control isKindOfClass:[WKSelectPopover class]])
        return nil;
    return [(WKSelectPopover *)self.control tableViewController].title;
}

@end

#endif  // PLATFORM(IOS_FAMILY)

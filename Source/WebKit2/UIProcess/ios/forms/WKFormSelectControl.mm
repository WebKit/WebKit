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
#import <UIKit/UIPickerView.h>
#import <UIKit/UIPickerView_Private.h>
#import <UIKit/UIStringDrawing_Private.h>
#import <UIKit/UIWebFormAccessory.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

static const CGFloat minimumOptionFontSize = 12;

CGFloat adjustedFontSize(CGFloat textWidth, UIFont *font, CGFloat initialFontSize, const Vector<WKOptionItem>& items)
{
    CGFloat adjustedSize = initialFontSize;
    for (size_t i = 0; i < items.size(); ++i) {
        const WKOptionItem& item = items[i];
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
    RetainPtr<id<WKFormControl>> _control;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    bool hasGroups = false;
    for (size_t i = 0; i < view.assistedNodeInformation.selectOptions.size(); ++i) {
        if (view.assistedNodeInformation.selectOptions[i].isGroup) {
            hasGroups = true;
            break;
        }
    }

    if (UICurrentUserInterfaceIdiomIsPad())
        _control = adoptNS([[WKSelectPopover alloc] initWithView:view hasGroups:hasGroups]);
    else if (view.assistedNodeInformation.isMultiSelect || hasGroups)
        _control = adoptNS([[WKMultipleSelectPicker alloc] initWithView:view]);
    else
        _control = adoptNS([[WKSelectSinglePicker alloc] initWithView:view]);
        
    return self;
}

+ (WKFormSelectControl *)createPeripheralWithView:(WKContentView *)view
{
    return [[WKFormSelectControl alloc] initWithView:view];
}

- (UIView *)assistantView
{
    return [_control controlView];
}

- (void)beginEditing
{
    [_control controlBeginEditing];
}

- (void)endEditing
{
    [_control controlEndEditing];
}

@end

#endif  // PLATFORM(IOS)

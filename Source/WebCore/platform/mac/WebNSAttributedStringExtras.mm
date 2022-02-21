/*
 * Copyright (C) 2005, 2007, 2008, 2011 Apple Inc. All rights reserved.
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
#import "WebNSAttributedStringExtras.h"

#if PLATFORM(IOS_FAMILY)
#if USE(APPLE_INTERNAL_SDK)
#import <UIKit/NSTextAttachment.h>
#else
enum {
    NSAttachmentCharacter = 0xfffc    /* To denote attachments. */
};
#endif
#endif

namespace WebCore {

NSAttributedString *attributedStringByStrippingAttachmentCharacters(NSAttributedString *attributedString)
{
    NSRange attachmentRange;
    NSString *originalString = [attributedString string];
    static NeverDestroyed attachmentCharString = [] {
        unichar chars[2] = { NSAttachmentCharacter, 0 };
        return adoptNS([[NSString alloc] initWithCharacters:chars length:1]);
    }();

    attachmentRange = [originalString rangeOfString:attachmentCharString.get().get()];
    if (attachmentRange.location != NSNotFound && attachmentRange.length > 0) {
        auto newAttributedString = adoptNS([attributedString mutableCopyWithZone:NULL]);
        
        while (attachmentRange.location != NSNotFound && attachmentRange.length > 0) {
            [newAttributedString replaceCharactersInRange:attachmentRange withString:@""];
            attachmentRange = [[newAttributedString string] rangeOfString:attachmentCharString.get().get()];
        }
        return newAttributedString.autorelease();
    }
    
    return attributedString;
}

}

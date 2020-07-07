/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "CoreTextHelpers.h"

#import <pal/spi/cocoa/CoreTextSPI.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebKit {

CocoaFont *fontWithAttributes(NSDictionary *attributes, CGFloat size)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

#if HAVE(NSFONT_WITH_OPTICAL_SIZING_BUG)
    auto mutableDictionary = adoptNS([attributes mutableCopy]);
    if (id opticalSizeAttribute = [mutableDictionary objectForKey:(__bridge NSString *)kCTFontOpticalSizeAttribute]) {
        if ([opticalSizeAttribute isKindOfClass:[NSString class]]) {
            [mutableDictionary removeObjectForKey:(__bridge NSString *)kCTFontOpticalSizeAttribute];
            if (NSNumber *size = [mutableDictionary objectForKey:(__bridge NSString *)kCTFontSizeAttribute])
                [mutableDictionary setObject:size forKey:(__bridge NSString *)kCTFontOpticalSizeAttribute];
        }
    }

    auto descriptor = [CocoaFontDescriptor fontDescriptorWithFontAttributes:mutableDictionary.get()];
#else
    auto descriptor = [CocoaFontDescriptor fontDescriptorWithFontAttributes:attributes];
#endif

    return [CocoaFont fontWithDescriptor:descriptor size:size];

    END_BLOCK_OBJC_EXCEPTIONS

    return nil;
}

}

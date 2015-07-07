/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FontServicesIOS.h"

#import "FontMetrics.h"
#import <CoreGraphics/CoreGraphics.h>
#import <CoreGraphics/CoreGraphicsPrivate.h>
#import <mach-o/dyld_priv.h>
#import <wtf/RetainPtr.h>

namespace WebCore {

static const float kLineHeightAdjustment = 0.15f;

static bool shouldUseAdjustment(CTFontRef font, bool isiOS7OrLater)
{
    RetainPtr<NSString> familyName = adoptNS((NSString *)CTFontCopyFamilyName(font));
    if (![familyName length])
        return false;

    if ([familyName.get() compare:@"Times" options:NSCaseInsensitiveSearch] == NSOrderedSame
        || [familyName.get() compare:@"Helvetica" options:NSCaseInsensitiveSearch] == NSOrderedSame
        || [familyName.get() compare:@".Helvetica NeueUI" options:NSCaseInsensitiveSearch] == NSOrderedSame)
        return true;
    if (!isiOS7OrLater && [familyName.get() compare:@".Helvetica NeueUILegacy" options:NSCaseInsensitiveSearch] == NSOrderedSame)
        return true;

    return false;
}

static bool isCourier(CTFontRef font)
{
    RetainPtr<NSString> familyName = adoptNS((NSString *)CTFontCopyFamilyName(font));
    if (![familyName length])
        return false;
    return [familyName.get() compare:@"Courier" options:NSCaseInsensitiveSearch] == NSOrderedSame;
}

FontServicesIOS::FontServicesIOS(CTFontRef font)
{
    CGFontDescriptor descriptor;
    RetainPtr<CGFontRef> cgFont = adoptCF(CTFontCopyGraphicsFont(font, NULL));
    m_xHeight = CGFontGetDescriptor(cgFont.get(), &descriptor) ? (descriptor.xHeight / 1000) * CTFontGetSize(font) : 0;
    m_unitsPerEm = CTFontGetUnitsPerEm(font);
    CGFloat lineGap;
    CGFloat ascent;
    CGFloat descent;
    static bool isiOS7OrLater = dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_7_0;
    if (isiOS7OrLater) {
        // Use CoreText API in iOS 7.
        ascent = CTFontGetAscent(font);
        descent = CTFontGetDescent(font);
        lineGap = CTFontGetLeading(font);
    } else {
        float pointSize = CTFontGetSize(font);
        const CGFontHMetrics *metrics = CGFontGetHMetrics(cgFont.get());
        unsigned unitsPerEm = CGFontGetUnitsPerEm(cgFont.get());

        lineGap = (dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_3_0) ? scaleEmToUnits(metrics->lineGap, unitsPerEm) * pointSize : 0.0;

        bool isiOS6OrLater = dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_6_0;
        if (!isiOS6OrLater || !isCourier(font)) {
            ascent = (scaleEmToUnits(metrics->ascent, unitsPerEm) * pointSize);
            descent = (-scaleEmToUnits(metrics->descent, unitsPerEm) * pointSize);
        } else {
            // For Courier, we use Courier New's exact values, because in iOS 5.1 and earlier,
            // Courier New was substituted for Courier by WebKit.
            ascent = (scaleEmToUnits(1705, 2048) * pointSize);
            descent = (scaleEmToUnits(615, 2048) * pointSize);
        }
    }
    CGFloat adjustment = (shouldUseAdjustment(font, isiOS7OrLater)) ? ceil((ascent + descent) * kLineHeightAdjustment) : 0;

    m_ascent = ascent + adjustment;
    m_descent = descent;
    m_lineGap = ceilf(lineGap);
    m_lineSpacing = ceil(ascent) + adjustment + ceil(descent) + m_lineGap;
}


} // namespace WebCore

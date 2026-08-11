/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FEFloodCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "ColorSpaceCG.h"
#import "FEFlood.h"
#import "Filter.h"
#import "Logging.h"
#import <CoreImage/CoreImage.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEFloodCoreImageApplier);

FEFloodCoreImageApplier::FEFloodCoreImageApplier(const FEFlood& effect)
    : Base(effect)
{
}

bool FEFloodCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>>, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto color = m_effect->floodColor().colorWithAlphaMultipliedBy(m_effect->floodOpacity());
    auto [r, g, b, a] = color.toResolvedColorComponentsInColorSpace(m_effect->operatingColorSpace());

    RetainPtr colorSpace = m_effect->operatingColorSpace() == DestinationColorSpace::SRGB() ? sRGBColorSpaceSingleton() : linearSRGBColorSpaceSingleton();
    RetainPtr ciColor = [CIColor colorWithRed:r green:g blue:b alpha:a colorSpace:colorSpace.get()];

    RetainPtr image = [CIImage imageWithColor:ciColor.get()];
    if (!image)
        return false;

    auto cropRect = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());
    image = [image imageByCroppingToRect:cropRect];
    result.setCIImage(WTF::move(image));
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)

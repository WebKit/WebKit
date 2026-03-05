/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "StyleBorderImageData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BorderImageData);

Ref<BorderImageData> BorderImageData::copy() const
{
    return adoptRef(*new BorderImageData(*this));
}

BorderImageData::BorderImageData()
    : borderImage { ComputedStyle::initialBorderImageSource(), ComputedStyle::initialBorderImageSlice(), ComputedStyle::initialBorderImageWidth(), ComputedStyle::initialBorderImageOutset(), ComputedStyle::initialBorderImageRepeat() }
{
}

inline BorderImageData::BorderImageData(const BorderImageData& other)
    : RefCounted<BorderImageData>()
    , borderImage { other.borderImage }
{
}

bool BorderImageData::operator==(const BorderImageData& other) const
{
    return borderImage == other.borderImage;
}

void BorderImageData::dump(TextStream& ts, DumpStyleValues behavior) const
{
    if (behavior == DumpStyleValues::All || borderImage.borderImageSource != ComputedStyle::initialBorderImageSource())
        ts.dumpProperty("border-image-source"_s, borderImage.borderImageSource);
    if (behavior == DumpStyleValues::All || borderImage.borderImageSlice != ComputedStyle::initialBorderImageSlice())
        ts.dumpProperty("border-image-slice"_s, borderImage.borderImageSlice);
    if (behavior == DumpStyleValues::All || borderImage.borderImageWidth != ComputedStyle::initialBorderImageWidth())
        ts.dumpProperty("border-image-width"_s, borderImage.borderImageWidth);
    if (behavior == DumpStyleValues::All || borderImage.borderImageOutset != ComputedStyle::initialBorderImageOutset())
        ts.dumpProperty("border-image-outset"_s, borderImage.borderImageOutset);
    if (behavior == DumpStyleValues::All || borderImage.borderImageRepeat != ComputedStyle::initialBorderImageRepeat())
        ts.dumpProperty("border-image-repeat"_s, borderImage.borderImageRepeat);
}

#if !LOG_DISABLED
void BorderImageData::dumpDifferences(TextStream& ts, const BorderImageData& other) const
{
    LOG_IF_DIFFERENT(borderImage.borderImageSource);
    LOG_IF_DIFFERENT(borderImage.borderImageSlice);
    LOG_IF_DIFFERENT(borderImage.borderImageWidth);
    LOG_IF_DIFFERENT(borderImage.borderImageOutset);
    LOG_IF_DIFFERENT(borderImage.borderImageRepeat);
}
#endif

WTF::TextStream& operator<<(WTF::TextStream& ts, const BorderImageData& borderImageData)
{
    borderImageData.dump(ts);
    return ts;
}

} // namespace Style
} // namespace WebCore

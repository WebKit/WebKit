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
#include "StyleMaskBorderData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MaskBorderData);

Ref<MaskBorderData> MaskBorderData::copy() const
{
    return adoptRef(*new MaskBorderData(*this));
}

MaskBorderData::MaskBorderData()
    : maskBorder { ComputedStyle::initialMaskBorderSource(), ComputedStyle::initialMaskBorderSlice(), ComputedStyle::initialMaskBorderWidth(), ComputedStyle::initialMaskBorderOutset(), ComputedStyle::initialMaskBorderRepeat() }
{
}

inline MaskBorderData::MaskBorderData(const MaskBorderData& other)
    : RefCounted<MaskBorderData>()
    , maskBorder { other.maskBorder }
{
}

bool MaskBorderData::operator==(const MaskBorderData& other) const
{
    return maskBorder == other.maskBorder;
}

void MaskBorderData::dump(TextStream& ts, DumpStyleValues behavior) const
{
    if (behavior == DumpStyleValues::All || maskBorder.maskBorderSource != ComputedStyle::initialMaskBorderSource())
        ts.dumpProperty("mask-border-source"_s, maskBorder.maskBorderSource);
    if (behavior == DumpStyleValues::All || maskBorder.maskBorderSlice != ComputedStyle::initialMaskBorderSlice())
        ts.dumpProperty("mask-border-slice"_s, maskBorder.maskBorderSlice);
    if (behavior == DumpStyleValues::All || maskBorder.maskBorderWidth != ComputedStyle::initialMaskBorderWidth())
        ts.dumpProperty("mask-border-width"_s, maskBorder.maskBorderWidth);
    if (behavior == DumpStyleValues::All || maskBorder.maskBorderOutset != ComputedStyle::initialMaskBorderOutset())
        ts.dumpProperty("mask-border-outset"_s, maskBorder.maskBorderOutset);
    if (behavior == DumpStyleValues::All || maskBorder.maskBorderRepeat != ComputedStyle::initialMaskBorderRepeat())
        ts.dumpProperty("mask-border-repeat"_s, maskBorder.maskBorderRepeat);
}

#if !LOG_DISABLED
void MaskBorderData::dumpDifferences(TextStream& ts, const MaskBorderData& other) const
{
    LOG_IF_DIFFERENT(maskBorder.maskBorderSource);
    LOG_IF_DIFFERENT(maskBorder.maskBorderSlice);
    LOG_IF_DIFFERENT(maskBorder.maskBorderWidth);
    LOG_IF_DIFFERENT(maskBorder.maskBorderOutset);
    LOG_IF_DIFFERENT(maskBorder.maskBorderRepeat);
}
#endif

TextStream& operator<<(TextStream& ts, const MaskBorderData& maskBorderData)
{
    maskBorderData.dump(ts);
    return ts;
}

} // namespace Style
} // namespace WebCore

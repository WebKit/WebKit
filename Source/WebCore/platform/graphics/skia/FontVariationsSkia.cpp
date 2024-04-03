/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "FontVariationsSkia.h"

namespace WebCore {

FontVariationDefaultsMap defaultFontVariationValues(const SkTypeface& typeface)
{
    FontVariationDefaultsMap map;
    int axisCount = typeface.getVariationDesignParameters(nullptr, 0);
    if (!axisCount)
        return map;

    Vector<SkFontParameters::Variation::Axis> axisValues(axisCount);
    if (typeface.getVariationDesignParameters(axisValues.data(), axisValues.size()) == -1)
        return map;

    for (const auto& axisValue : axisValues) {
        FontTag resultKey = { { static_cast<char>((axisValue.tag >> 24)), static_cast<char>((axisValue.tag >> 16)), static_cast<char>((axisValue.tag >> 8)), static_cast<char>(axisValue.tag) } };
        // FIXME: skia doesn't provide the axis name.
        FontVariationDefaults resultValues = { { }, axisValue.def, axisValue.min, axisValue.max };
        map.set(resultKey, resultValues);
    }

    return map;
}

} // namespace WebCore

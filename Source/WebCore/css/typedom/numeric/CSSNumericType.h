/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CSS_TYPED_OM)

#include "CSSNumericBaseType.h"
#include <optional>
#include <wtf/Markable.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

// https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-type
struct CSSNumericType {
    using BaseTypeStorage = Markable<int, IntegralMarkableTraits<int, std::numeric_limits<int>::min()>>;
    BaseTypeStorage length;
    BaseTypeStorage angle;
    BaseTypeStorage time;
    BaseTypeStorage frequency;
    BaseTypeStorage resolution;
    BaseTypeStorage flex;
    BaseTypeStorage percent;
    Markable<CSSNumericBaseType, EnumMarkableTraits<CSSNumericBaseType>> percentHint;

    bool operator==(const CSSNumericType& other) const
    {
        return length == other.length
            && angle == other.angle
            && time == other.time
            && frequency == other.frequency
            && resolution == other.resolution
            && flex == other.flex
            && percent == other.percent
            && percentHint == other.percentHint;
    }

    BaseTypeStorage& valueForType(CSSNumericBaseType type)
    {
        switch (type) {
        case CSSNumericBaseType::Length:
            return length;
        case CSSNumericBaseType::Angle:
            return angle;
        case CSSNumericBaseType::Time:
            return time;
        case CSSNumericBaseType::Frequency:
            return frequency;
        case CSSNumericBaseType::Resolution:
            return resolution;
        case CSSNumericBaseType::Flex:
            return flex;
        case CSSNumericBaseType::Percent:
            return percent;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    const BaseTypeStorage& valueForType(CSSNumericBaseType type) const
    {
        return const_cast<CSSNumericType*>(this)->valueForType(type);
    }

    void applyPercentHint(CSSNumericBaseType hint)
    {
        // https://drafts.css-houdini.org/css-typed-om/#apply-the-percent-hint
        auto& optional = valueForType(hint);
        if (!optional)
            optional = 0;
        if (percent)
            *optional += *std::exchange(percent, 0);
        percentHint = hint;
    }

    size_t nonZeroEntryCount() const
    {
        size_t count { 0 };
        count += length && *length;
        count += angle && *angle;
        count += time && *time;
        count += frequency && *frequency;
        count += resolution && *resolution;
        count += flex && *flex;
        count += percent && *percent;
        return count;
    }

    template<CSSNumericBaseType type>
    bool matches() const
    {
        // https://drafts.css-houdini.org/css-typed-om/#cssnumericvalue-match
        return (type == CSSNumericBaseType::Percent || !percentHint)
            && nonZeroEntryCount() == 1
            && valueForType(type)
            && *valueForType(type);
    }

    bool matchesNumber() const
    {
        // https://drafts.css-houdini.org/css-typed-om/#cssnumericvalue-match
        return !nonZeroEntryCount() && !percentHint;
    }

    template<CSSNumericBaseType type>
    bool matchesTypeOrPercentage() const
    {
        return matches<type>() || matches<CSSNumericBaseType::Percent>();
    }

    String debugString() const
    {
        return makeString("{",
            length ? makeString(" length:", *length) : String(),
            angle ? makeString(" angle:", *angle) : String(),
            time ? makeString(" time:", *time) : String(),
            frequency ? makeString(" frequency:", *frequency) : String(),
            resolution ? makeString(" resolution:", *resolution) : String(),
            flex ? makeString(" flex:", *flex) : String(),
            percent ? makeString(" percent:", *percent) : String(),
            percentHint ? makeString(" percentHint:", WebCore::debugString(*percentHint)) : String(),
        " }");
    }
};

} // namespace WebCore

#endif

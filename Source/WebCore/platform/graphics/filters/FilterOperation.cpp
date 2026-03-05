/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "FilterOperation.h"

#include "AnimationUtilities.h"
#include "ColorBlending.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
    
bool DefaultFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    
    return representedType() == downcast<DefaultFilterOperation>(operation).representedType();
}

FilterOperation::Type DefaultFilterOperation::representedType() const
{
    return m_representedType;
}

double FilterOperation::blendAmounts(double from, double to, const BlendingContext& context) const
{
    auto blendedAmount = [&]() {
        if (context.compositeOperation == CompositeOperation::Accumulate) {
            // The "initial value for interpolation" is 1 for brightness, contrast, opacity and saturate.
            // Accumulation works differently for such operations per https://drafts.fxtf.org/filter-effects/#accumulation.
            switch (m_type) {
            case Type::Brightness:
            case Type::Contrast:
            case Type::Opacity:
            case Type::Saturate:
                return from + to - 1;
            default:
                break;
            }
        }
        return WebCore::blend(from, to, context);
    }();

    // Make sure blended values remain within bounds as specified by
    // https://drafts.fxtf.org/filter-effects/#supported-filter-functions
    switch (m_type) {
    case Type::Grayscale:
    case Type::Invert:
    case Type::Opacity:
    case Type::Sepia:
        return std::clamp(blendedAmount, 0.0, 1.0);
    case Type::Brightness:
    case Type::Contrast:
    case Type::Saturate:
        return std::max(blendedAmount, 0.0);
    default:
        return blendedAmount;
    }
}

RefPtr<FilterOperation> BasicColorMatrixFilterOperation::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    if (blendToPassthrough)
        return BasicColorMatrixFilterOperation::create(blendAmounts(m_amount, passthroughAmount(), context), m_type);

    const BasicColorMatrixFilterOperation* fromOperation = downcast<BasicColorMatrixFilterOperation>(from);
    double fromAmount = fromOperation ? fromOperation->amount() : passthroughAmount();
    auto blendedAmount = blendAmounts(fromAmount, m_amount, context);
    return BasicColorMatrixFilterOperation::create(blendedAmount, m_type);
}

inline bool BasicColorMatrixFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    const BasicColorMatrixFilterOperation& other = downcast<BasicColorMatrixFilterOperation>(operation);
    return m_amount == other.m_amount;
}

double BasicColorMatrixFilterOperation::passthroughAmount() const
{
    switch (m_type) {
    case Type::Grayscale:
    case Type::Sepia:
    case Type::HueRotate:
        return 0;
    case Type::Saturate:
        return 1;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

RefPtr<FilterOperation> BasicComponentTransferFilterOperation::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;
    
    if (blendToPassthrough)
        return BasicComponentTransferFilterOperation::create(blendAmounts(m_amount, passthroughAmount(), context), m_type);
        
    const BasicComponentTransferFilterOperation* fromOperation = downcast<BasicComponentTransferFilterOperation>(from);
    double fromAmount = fromOperation ? fromOperation->amount() : passthroughAmount();
    auto blendedAmount = blendAmounts(fromAmount, m_amount, context);
    return BasicComponentTransferFilterOperation::create(blendedAmount, m_type);
}

inline bool BasicComponentTransferFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    const BasicComponentTransferFilterOperation& other = downcast<BasicComponentTransferFilterOperation>(operation);
    return m_amount == other.m_amount;
}

double BasicComponentTransferFilterOperation::passthroughAmount() const
{
    switch (m_type) {
    case Type::Opacity:
        return 1;
    case Type::Invert:
        return 0;
    case Type::Contrast:
        return 1;
    case Type::Brightness:
        return 1;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

bool BasicComponentTransferFilterOperation::affectsOpacity() const
{
    return m_type == Type::Opacity;
}

bool BlurFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    
    return m_stdDeviation == downcast<BlurFilterOperation>(operation).stdDeviation();
}
    
RefPtr<FilterOperation> BlurFilterOperation::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    if (blendToPassthrough)
        return BlurFilterOperation::create(std::max(0.0f, WebCore::blend(m_stdDeviation, 0.0f, context)));

    const BlurFilterOperation* fromOperation = downcast<BlurFilterOperation>(from);
    auto fromStdDeviation = fromOperation ? fromOperation->m_stdDeviation : 0.0f;
    return BlurFilterOperation::create(std::max(0.0f, WebCore::blend(fromStdDeviation, m_stdDeviation, context)));
}

bool DropShadowFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    const DropShadowFilterOperation& other = downcast<DropShadowFilterOperation>(operation);
    return m_color == other.m_color
        && m_location == other.m_location
        && m_stdDeviation == other.m_stdDeviation;
}

RefPtr<FilterOperation> DropShadowFilterOperation::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    // We should only ever be blending with null or similar operations.
    ASSERT(!from || from->isSameType(*this));

    if (blendToPassthrough) {
        return DropShadowFilterOperation::create(
            WebCore::blend(m_color, Color::transparentBlack, context),
            WebCore::blend(m_location, IntPoint(), context),
            WebCore::blend(m_stdDeviation, 0, context)
        );
    }

    const DropShadowFilterOperation* fromOperation = downcast<DropShadowFilterOperation>(from);
    Color fromColor = fromOperation ? fromOperation->color() : Color::transparentBlack;
    IntPoint fromLocation = fromOperation ? fromOperation->location() : IntPoint();
    int fromStdDeviation = fromOperation ? fromOperation->stdDeviation() : 0;

    return DropShadowFilterOperation::create(
        WebCore::blend(fromColor, m_color, context),
        WebCore::blend(fromLocation, m_location, context),
        std::max(WebCore::blend(fromStdDeviation, m_stdDeviation, context), 0)
    );
}

TextStream& operator<<(TextStream& ts, const FilterOperation& filter)
{
    switch (filter.type()) {
    case FilterOperation::Type::Grayscale: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "grayscale("_s << colorMatrixFilter.amount() << ')';
        break;
    }
    case FilterOperation::Type::Sepia: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "sepia("_s << colorMatrixFilter.amount() << ')';
        break;
    }
    case FilterOperation::Type::Saturate: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "saturate("_s << colorMatrixFilter.amount() << ')';
        break;
    }
    case FilterOperation::Type::HueRotate: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "hue-rotate("_s << colorMatrixFilter.amount() << ')';
        break;
    }
    case FilterOperation::Type::Invert: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "invert("_s << componentTransferFilter.amount() << ')';
        break;
    }
    case FilterOperation::Type::Opacity: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "opacity("_s << componentTransferFilter.amount() << ')';
        break;
    }
    case FilterOperation::Type::Brightness: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "brightness("_s << componentTransferFilter.amount() << ')';
        break;
    }
    case FilterOperation::Type::Contrast: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "contrast("_s << componentTransferFilter.amount() << ')';
        break;
    }
    case FilterOperation::Type::Blur: {
        const auto& blurFilter = downcast<BlurFilterOperation>(filter);
        ts << "blur("_s << blurFilter.stdDeviation() << ')';
        break;
    }
    case FilterOperation::Type::DropShadow: {
        const auto& dropShadowFilter = downcast<DropShadowFilterOperation>(filter);
        ts << "drop-shadow("_s << dropShadowFilter.x() << ' ' << dropShadowFilter.y() << ' ' << dropShadowFilter.location() << ' ' << dropShadowFilter.color() << ')';
        break;
    }
    case FilterOperation::Type::Passthrough:
        ts << "passthrough"_s;
        break;
    case FilterOperation::Type::Default: {
        const auto& defaultFilter = downcast<DefaultFilterOperation>(filter);
        ts << "default type="_s << (int)defaultFilter.representedType();
        break;
    }
    case FilterOperation::Type::None:
        ts << "none"_s;
        break;
    }
    return ts;
}

} // namespace WebCore

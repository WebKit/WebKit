/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "FilterOperations.h"

#include "FEGaussianBlur.h"
#include "IntSize.h"
#include "LengthFunctions.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

bool FilterOperations::operator==(const FilterOperations& other) const
{
    size_t size = m_operations.size();
    if (size != other.m_operations.size())
        return false;
    for (size_t i = 0; i < size; i++) {
        if (*m_operations[i] != *other.m_operations[i])
            return false;
    }
    return true;
}

bool FilterOperations::operationsMatch(const FilterOperations& other) const
{
    size_t size = operations().size();
    if (size != other.operations().size())
        return false;
    for (size_t i = 0; i < size; ++i) {
        if (!operations()[i]->isSameType(*other.operations()[i]))
            return false;
    }
    return true;
}

bool FilterOperations::hasReferenceFilter() const
{
    for (auto& operation : m_operations) {
        if (operation->type() == FilterOperation::REFERENCE)
            return true;
    }
    return false;
}

IntOutsets FilterOperations::outsets() const
{
    IntOutsets totalOutsets;
    for (auto& operation : m_operations) {
        switch (operation->type()) {
        case FilterOperation::BLUR: {
            auto& blurOperation = downcast<BlurFilterOperation>(*operation);
            float stdDeviation = floatValueForLength(blurOperation.stdDeviation(), 0);
            IntSize outsetSize = FEGaussianBlur::calculateOutsetSize({ stdDeviation, stdDeviation });
            IntOutsets outsets(outsetSize.height(), outsetSize.width(), outsetSize.height(), outsetSize.width());
            totalOutsets += outsets;
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            auto& dropShadowOperation = downcast<DropShadowFilterOperation>(*operation);
            float stdDeviation = dropShadowOperation.stdDeviation();
            IntSize outsetSize = FEGaussianBlur::calculateOutsetSize({ stdDeviation, stdDeviation });
            IntOutsets outsets {
                std::max(0, outsetSize.height() - dropShadowOperation.y()),
                std::max(0, outsetSize.width() + dropShadowOperation.x()),
                std::max(0, outsetSize.height() + dropShadowOperation.y()),
                std::max(0, outsetSize.width() - dropShadowOperation.x())
            };
            totalOutsets += outsets;
            break;
        }
        case FilterOperation::REFERENCE:
            ASSERT_NOT_REACHED();
            break;
        default:
            break;
        }
    }
    return totalOutsets;
}

bool FilterOperations::transformColor(Color& color) const
{
    if (isEmpty() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toSRGBALossy<float>();

    for (auto& operation : m_operations) {
        if (!operation->transformColor(sRGBAColor))
            return false;
    }

    color = convertToComponentBytes(sRGBAColor);
    return true;
}

bool FilterOperations::inverseTransformColor(Color& color) const
{
    if (isEmpty() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toSRGBALossy<float>();

    for (auto& operation : m_operations) {
        if (!operation->inverseTransformColor(sRGBAColor))
            return false;
    }

    color = convertToComponentBytes(sRGBAColor);
    return true;
}

bool FilterOperations::hasFilterThatAffectsOpacity() const
{
    for (auto& operation : m_operations) {
        if (operation->affectsOpacity())
            return true;
    }
    return false;
}

bool FilterOperations::hasFilterThatMovesPixels() const
{
    for (auto& operation : m_operations) {
        if (operation->movesPixels())
            return true;
    }
    return false;
}

bool FilterOperations::hasFilterThatShouldBeRestrictedBySecurityOrigin() const
{
    for (auto& operation : m_operations) {
        if (operation->shouldBeRestrictedBySecurityOrigin())
            return true;
    }
    return false;
}

TextStream& operator<<(TextStream& ts, const FilterOperations& filters)
{
    for (size_t i = 0; i < filters.size(); ++i) {
        auto filter = filters.at(i);
        if (filter)
            ts << *filter;
        else
            ts << "(null)";
        if (i < filters.size() - 1)
            ts << " ";
    }
    return ts;
}

} // namespace WebCore

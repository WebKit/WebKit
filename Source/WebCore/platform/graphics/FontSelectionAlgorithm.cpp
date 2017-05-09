/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FontSelectionAlgorithm.h"

namespace WebCore {

auto FontSelectionAlgorithm::stretchDistance(FontSelectionCapabilities capabilities) const -> DistanceResult
{
    auto width = capabilities.width;
    ASSERT(width.isValid());
    if (width.includes(m_request.width))
        return { FontSelectionValue(), m_request.width };

    if (m_request.width > normalStretchValue()) {
        if (width.minimum > m_request.width)
            return { width.minimum - m_request.width, width.minimum };
        ASSERT(width.maximum < m_request.width);
        auto threshold = std::max(m_request.width, m_capabilitiesBounds.width.maximum);
        return { threshold - width.maximum, width.maximum };
    }

    if (width.maximum < m_request.width)
        return { m_request.width - width.maximum, width.maximum };
    ASSERT(width.minimum > m_request.width);
    auto threshold = std::min(m_request.width, m_capabilitiesBounds.width.minimum);
    return { width.minimum - threshold, width.minimum };
}

auto FontSelectionAlgorithm::styleDistance(FontSelectionCapabilities capabilities) const -> DistanceResult
{
    auto slope = capabilities.slope;
    ASSERT(slope.isValid());
    if (slope.includes(m_request.slope))
        return { FontSelectionValue(), m_request.slope };

    if (m_request.slope >= italicThreshold()) {
        if (slope.minimum > m_request.slope)
            return { slope.minimum - m_request.slope, slope.minimum };
        ASSERT(m_request.slope > slope.maximum);
        auto threshold = std::max(m_request.slope, m_capabilitiesBounds.slope.maximum);
        return { threshold - slope.maximum, slope.maximum };
    }

    if (m_request.slope >= FontSelectionValue()) {
        if (slope.maximum >= FontSelectionValue() && slope.maximum < m_request.slope)
            return { m_request.slope - slope.maximum, slope.maximum };
        if (slope.minimum > m_request.slope)
            return { slope.minimum, slope.minimum };
        ASSERT(slope.maximum < FontSelectionValue());
        auto threshold = std::max(m_request.slope, m_capabilitiesBounds.slope.maximum);
        return { threshold - slope.maximum, slope.maximum };
    }

    if (m_request.slope > -italicThreshold()) {
        if (slope.minimum > m_request.slope && slope.minimum <= FontSelectionValue())
            return { slope.minimum - m_request.slope, slope.minimum };
        if (slope.maximum < m_request.slope)
            return { -slope.maximum, slope.maximum };
        ASSERT(slope.minimum > FontSelectionValue());
        auto threshold = std::min(m_request.slope, m_capabilitiesBounds.slope.minimum);
        return { slope.minimum - threshold, slope.minimum };
    }

    if (slope.maximum < m_request.slope)
        return { m_request.slope - slope.maximum, slope.maximum };
    ASSERT(slope.minimum > m_request.slope);
    auto threshold = std::min(m_request.slope, m_capabilitiesBounds.slope.minimum);
    return { slope.minimum - threshold, slope.minimum };
}

auto FontSelectionAlgorithm::weightDistance(FontSelectionCapabilities capabilities) const -> DistanceResult
{
    auto weight = capabilities.weight;
    ASSERT(weight.isValid());
    if (weight.includes(m_request.weight))
        return { FontSelectionValue(), m_request.weight };

    // The spec states: "If the desired weight is 400, 500 is checked first ... If the desired weight is 500, 400 is checked first"
    FontSelectionValue offset(1);
    if (m_request.weight == FontSelectionValue(400) && weight.includes(FontSelectionValue(500)))
        return { offset, FontSelectionValue(500) };
    if (m_request.weight == FontSelectionValue(500) && weight.includes(FontSelectionValue(400)))
        return { offset, FontSelectionValue(400) };

    if (m_request.weight <= weightSearchThreshold()) {
        if (weight.maximum < m_request.weight)
            return { m_request.weight - weight.maximum + offset, weight.maximum };
        ASSERT(weight.minimum > m_request.weight);
        auto threshold = std::min(m_request.weight, m_capabilitiesBounds.weight.minimum);
        return { weight.minimum - threshold + offset, weight.minimum };
    }

    if (weight.minimum > m_request.weight)
        return { weight.minimum - m_request.weight + offset, weight.minimum };
    ASSERT(weight.maximum < m_request.weight);
    auto threshold = std::max(m_request.weight, m_capabilitiesBounds.weight.maximum);
    return { threshold - weight.maximum + offset, weight.maximum };
}

void FontSelectionAlgorithm::filterCapability(DistanceResult(FontSelectionAlgorithm::*computeDistance)(FontSelectionCapabilities) const, FontSelectionRange FontSelectionCapabilities::*inclusionRange)
{
    std::optional<FontSelectionValue> smallestDistance;
    FontSelectionValue closestValue;
    iterateActiveCapabilities([&](FontSelectionCapabilities capabilities, size_t) {
        auto distanceResult = (this->*computeDistance)(capabilities);
        if (!smallestDistance || distanceResult.distance < smallestDistance.value()) {
            smallestDistance = distanceResult.distance;
            closestValue = distanceResult.value;
        }
    });
    ASSERT(smallestDistance);
    iterateActiveCapabilities([&](auto& capabilities, size_t i) {
        if (!(capabilities.*inclusionRange).includes(closestValue))
            m_filter[i] = false;
    });
}

size_t FontSelectionAlgorithm::indexOfBestCapabilities()
{
    filterCapability(&FontSelectionAlgorithm::stretchDistance, &FontSelectionCapabilities::width);
    filterCapability(&FontSelectionAlgorithm::styleDistance, &FontSelectionCapabilities::slope);
    filterCapability(&FontSelectionAlgorithm::weightDistance, &FontSelectionCapabilities::weight);

    auto result = iterateActiveCapabilitiesWithReturn<size_t>([](FontSelectionCapabilities, size_t i) {
        return i;
    });
    ASSERT(result);
    return result.value_or(0);
}

}

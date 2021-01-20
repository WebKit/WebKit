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

FontSelectionAlgorithm::FontSelectionAlgorithm(FontSelectionRequest request, const Vector<Capabilities>& capabilities, Optional<Capabilities> bounds)
    : m_request(request)
    , m_capabilities(capabilities)
{
    ASSERT(!m_capabilities.isEmpty());
    if (bounds)
        m_capabilitiesBounds = bounds.value();
    else {
        for (auto& capabilities : m_capabilities)
            m_capabilitiesBounds.expand(capabilities);
    }
}

auto FontSelectionAlgorithm::stretchDistance(Capabilities capabilities) const -> DistanceResult
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

auto FontSelectionAlgorithm::styleDistance(Capabilities capabilities) const -> DistanceResult
{
    auto slope = capabilities.slope;
    auto requestSlope = m_request.slope.valueOr(normalItalicValue());
    ASSERT(slope.isValid());
    if (slope.includes(requestSlope))
        return { FontSelectionValue(), requestSlope };

    if (requestSlope >= italicThreshold()) {
        if (slope.minimum > requestSlope)
            return { slope.minimum - requestSlope, slope.minimum };
        ASSERT(requestSlope > slope.maximum);
        auto threshold = std::max(requestSlope, m_capabilitiesBounds.slope.maximum);
        return { threshold - slope.maximum, slope.maximum };
    }

    if (requestSlope >= FontSelectionValue()) {
        if (slope.maximum >= FontSelectionValue() && slope.maximum < requestSlope)
            return { requestSlope - slope.maximum, slope.maximum };
        if (slope.minimum > requestSlope)
            return { slope.minimum, slope.minimum };
        ASSERT(slope.maximum < FontSelectionValue());
        auto threshold = std::max(requestSlope, m_capabilitiesBounds.slope.maximum);
        return { threshold - slope.maximum, slope.maximum };
    }

    if (requestSlope > -italicThreshold()) {
        if (slope.minimum > requestSlope && slope.minimum <= FontSelectionValue())
            return { slope.minimum - requestSlope, slope.minimum };
        if (slope.maximum < requestSlope)
            return { -slope.maximum, slope.maximum };
        ASSERT(slope.minimum > FontSelectionValue());
        auto threshold = std::min(requestSlope, m_capabilitiesBounds.slope.minimum);
        return { slope.minimum - threshold, slope.minimum };
    }

    if (slope.maximum < requestSlope)
        return { requestSlope - slope.maximum, slope.maximum };
    ASSERT(slope.minimum > requestSlope);
    auto threshold = std::min(requestSlope, m_capabilitiesBounds.slope.minimum);
    return { slope.minimum - threshold, slope.minimum };
}

auto FontSelectionAlgorithm::weightDistance(Capabilities capabilities) const -> DistanceResult
{
    auto weight = capabilities.weight;
    ASSERT(weight.isValid());
    if (weight.includes(m_request.weight))
        return { FontSelectionValue(), m_request.weight };

    if (m_request.weight >= lowerWeightSearchThreshold() && m_request.weight <= upperWeightSearchThreshold()) {
        if (weight.minimum > m_request.weight && weight.minimum <= upperWeightSearchThreshold())
            return { weight.minimum - m_request.weight, weight.minimum };
        if (weight.maximum < m_request.weight)
            return { upperWeightSearchThreshold() - weight.maximum, weight.maximum };
        ASSERT(weight.minimum > upperWeightSearchThreshold());
        auto threshold = std::min(m_request.weight, m_capabilitiesBounds.weight.minimum);
        return { weight.minimum - threshold, weight.minimum };
    }
    if (m_request.weight < lowerWeightSearchThreshold()) {
        if (weight.maximum < m_request.weight)
            return { m_request.weight - weight.maximum, weight.maximum };
        ASSERT(weight.minimum > m_request.weight);
        auto threshold = std::min(m_request.weight, m_capabilitiesBounds.weight.minimum);
        return { weight.minimum - threshold, weight.minimum };
    }
    ASSERT(m_request.weight >= upperWeightSearchThreshold());
    if (weight.minimum > m_request.weight)
        return { weight.minimum - m_request.weight, weight.minimum };
    ASSERT(weight.maximum < m_request.weight);
    auto threshold = std::max(m_request.weight, m_capabilitiesBounds.weight.maximum);
    return { threshold - weight.maximum, weight.maximum };
}

FontSelectionValue FontSelectionAlgorithm::bestValue(const bool eliminated[], DistanceFunction computeDistance) const
{
    Optional<DistanceResult> smallestDistance;
    for (size_t i = 0, size = m_capabilities.size(); i < size; ++i) {
        if (eliminated[i])
            continue;
        auto distanceResult = (this->*computeDistance)(m_capabilities[i]);
        if (!smallestDistance || distanceResult.distance < smallestDistance.value().distance)
            smallestDistance = distanceResult;
    }
    return smallestDistance.value().value;
}

void FontSelectionAlgorithm::filterCapability(bool eliminated[], DistanceFunction computeDistance, CapabilitiesRange inclusionRange)
{
    auto value = bestValue(eliminated, computeDistance);
    for (size_t i = 0, size = m_capabilities.size(); i < size; ++i) {
        eliminated[i] = eliminated[i]
            || !(m_capabilities[i].*inclusionRange).includes(value);
    }
}

size_t FontSelectionAlgorithm::indexOfBestCapabilities()
{
    Vector<bool, 256> eliminated(m_capabilities.size(), false);
    filterCapability(eliminated.data(), &FontSelectionAlgorithm::stretchDistance, &Capabilities::width);
    filterCapability(eliminated.data(), &FontSelectionAlgorithm::styleDistance, &Capabilities::slope);
    filterCapability(eliminated.data(), &FontSelectionAlgorithm::weightDistance, &Capabilities::weight);
    return eliminated.find(false);
}

}

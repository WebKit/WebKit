/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WillChangeData.h"

namespace WebCore {

bool WillChangeData::operator==(const WillChangeData& other) const
{
    return m_animatableFeatures == other.m_animatableFeatures;
}

bool WillChangeData::containsScrollPosition() const
{
    for (const auto& feature : m_animatableFeatures) {
        if (feature.feature() == ScrollPosition)
            return true;
    }
    return false;
}

bool WillChangeData::containsContents() const
{
    for (const auto& feature : m_animatableFeatures) {
        if (feature.feature() == Contents)
            return true;
    }
    return false;
}

bool WillChangeData::containsProperty(CSSPropertyID property) const
{
    for (const auto& feature : m_animatableFeatures) {
        if (feature.property() == property)
            return true;
    }
    return false;
}

void WillChangeData::addFeature(Feature feature, CSSPropertyID propertyID)
{
    ASSERT(feature == Property || propertyID == CSSPropertyInvalid);
    m_animatableFeatures.append(AnimatableFeature(feature, propertyID));
}

WillChangeData::FeaturePropertyPair WillChangeData::featureAt(size_t index) const
{
    if (index >= m_animatableFeatures.size())
        return FeaturePropertyPair(Invalid, CSSPropertyInvalid);

    return m_animatableFeatures[index].featurePropertyPair();
}

} // namespace WebCore

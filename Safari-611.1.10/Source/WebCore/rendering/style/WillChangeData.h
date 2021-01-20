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

#pragma once

#include "CSSPropertyNames.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class WillChangeData : public RefCounted<WillChangeData> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<WillChangeData> create()
    {
        return adoptRef(*new WillChangeData);
    }
    
    bool operator==(const WillChangeData&) const;
    bool operator!=(const WillChangeData& o) const
    {
        return !(*this == o);
    }

    bool isAuto() const { return m_animatableFeatures.isEmpty(); }
    size_t numFeatures() const { return m_animatableFeatures.size(); }

    bool containsScrollPosition() const;
    bool containsContents() const;
    bool containsProperty(CSSPropertyID) const;

    bool canCreateStackingContext() const { return m_canCreateStackingContext; }
    bool canTriggerCompositing() const { return m_canTriggerCompositing; }
    bool canTriggerCompositingOnInline() const { return m_canTriggerCompositingOnInline; }

    enum Feature {
        ScrollPosition,
        Contents,
        Property,
        Invalid
    };

    void addFeature(Feature, CSSPropertyID = CSSPropertyInvalid);
    
    typedef std::pair<Feature, CSSPropertyID> FeaturePropertyPair;
    FeaturePropertyPair featureAt(size_t) const;

    static bool propertyCreatesStackingContext(CSSPropertyID);

private:
    WillChangeData()
    {
    }

    struct AnimatableFeature {
        static const int numCSSPropertyIDBits = 14;
        COMPILE_ASSERT(numCSSProperties < (1 << numCSSPropertyIDBits), CSSPropertyID_should_fit_in_14_bits);

        unsigned m_feature : 2;
        unsigned m_cssPropertyID : numCSSPropertyIDBits;

        Feature feature() const
        {
            return static_cast<Feature>(m_feature);
        }

        CSSPropertyID property() const
        {
            return feature() == Property ? static_cast<CSSPropertyID>(m_cssPropertyID) : CSSPropertyInvalid;
        }
        
        FeaturePropertyPair featurePropertyPair() const
        {
            return FeaturePropertyPair(feature(), property());
        }

        AnimatableFeature(Feature willChange, CSSPropertyID willChangeProperty = CSSPropertyInvalid)
        {
            switch (willChange) {
            case Property:
                ASSERT(willChangeProperty != CSSPropertyInvalid);
                m_cssPropertyID = willChangeProperty;
                FALLTHROUGH;
            case ScrollPosition:
            case Contents:
                m_feature = static_cast<unsigned>(willChange);
                break;
            case Invalid:
                ASSERT_NOT_REACHED();
                break;
            }
        }
        
        bool operator==(const AnimatableFeature& other) const
        {
            return m_feature == other.m_feature && m_cssPropertyID == other.m_cssPropertyID;
        }
    };

    Vector<AnimatableFeature, 1> m_animatableFeatures;
    bool m_canCreateStackingContext { false };
    bool m_canTriggerCompositing { false };
    bool m_canTriggerCompositingOnInline { false };
};

} // namespace WebCore

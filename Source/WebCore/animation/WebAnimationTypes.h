/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/Markable.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class AnimationEventBase;
class AnimationList;
class CSSAnimation;
class CSSTransition;
class WebAnimation;

struct WebAnimationsMarkableDoubleTraits {
    static bool isEmptyValue(double value)
    {
        return std::isnan(value);
    }

    static constexpr double emptyValue()
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
};

enum class AnimationImpact {
    RequiresRecomposite     = 1 << 0,
    ForcesStackingContext   = 1 << 1
};

enum class UseAcceleratedAction : uint8_t { Yes, No };

using MarkableDouble = Markable<double, WebAnimationsMarkableDoubleTraits>;

using AnimationCollection = ListHashSet<RefPtr<WebAnimation>>;
using AnimationEvents = Vector<Ref<AnimationEventBase>>;
using CSSAnimationCollection = ListHashSet<RefPtr<CSSAnimation>>;

using AnimatableProperty = std::variant<CSSPropertyID, AtomString>;
using AnimatablePropertyToTransitionMap = HashMap<AnimatableProperty, RefPtr<CSSTransition>>;

} // namespace WebCore

namespace WTF {

template<> struct DefaultHash<WebCore::AnimatableProperty> {
    static unsigned hash(const WebCore::AnimatableProperty& key) {
        return WTF::switchOn(key,
            [] (WebCore::CSSPropertyID property) {
                return DefaultHash<WebCore::CSSPropertyID>::hash(property);
            },
            [] (const AtomString& string) {
                return DefaultHash<AtomString>::hash(string);
            }
        );
    }
    static bool equal(const WebCore::AnimatableProperty& a, const WebCore::AnimatableProperty& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebCore::AnimatableProperty> : GenericHashTraits<WebCore::AnimatableProperty> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(WebCore::AnimatableProperty& slot) {
        WebCore::CSSPropertyID property;
        HashTraits<WebCore::CSSPropertyID>::constructDeletedValue(property);
        new (NotNull, &slot) WebCore::AnimatableProperty(property);
    }
    static bool isDeletedValue(const WebCore::AnimatableProperty& value) {
        return WTF::switchOn(value,
            [] (WebCore::CSSPropertyID property) {
                return HashTraits<WebCore::CSSPropertyID>::isDeletedValue(property);
            },
            [] (const AtomString&) {
                return false;
            }
        );
    }
};

} // namespace WTF

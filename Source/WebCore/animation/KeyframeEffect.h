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

#include "AnimationEffect.h"
#include "AnimationEffectTimingProperties.h"
#include "CSSPropertyBlendingClient.h"
#include "CompositeOperation.h"
#include "IterationCompositeOperation.h"
#include "KeyframeEffectOptions.h"
#include "KeyframeList.h"
#include "RenderStyle.h"
#include <wtf/Ref.h>

namespace WebCore {

class Element;

class KeyframeEffect final : public AnimationEffect
    , public CSSPropertyBlendingClient {
public:
    static ExceptionOr<Ref<KeyframeEffect>> create(JSC::ExecState&, Element*, JSC::Strong<JSC::JSObject>&&, std::optional<Variant<double, KeyframeEffectOptions>>&&);
    ~KeyframeEffect() { }

    struct BasePropertyIndexedKeyframe {
        Variant<std::nullptr_t, Vector<std::optional<double>>, double> offset = Vector<std::optional<double>>();
        Variant<Vector<String>, String> easing = Vector<String>();
        Variant<Vector<CompositeOperation>, CompositeOperation> composite = Vector<CompositeOperation>();
    };

    struct PropertyAndValues {
        CSSPropertyID property;
        Vector<String> values;
    };

    struct KeyframeLikeObject {
        BasePropertyIndexedKeyframe baseProperties;
        Vector<PropertyAndValues> propertiesAndValues;
    };

    struct ProcessedKeyframe {
        String easing;
        std::optional<double> offset;
        std::optional<double> computedOffset;
        std::optional<CompositeOperation> composite;
        HashMap<CSSPropertyID, String> cssPropertiesAndValues;
    };

    struct BaseComputedKeyframe {
        std::optional<double> offset;
        double computedOffset;
        String easing { "linear" };
        std::optional<CompositeOperation> composite;
    };

    Element* target() const { return m_target.get(); }
    Vector<JSC::Strong<JSC::JSObject>> getKeyframes(JSC::ExecState&);
    ExceptionOr<void> setKeyframes(JSC::ExecState&, JSC::Strong<JSC::JSObject>&&);

    IterationCompositeOperation iterationComposite() const { return m_iterationCompositeOperation; }
    void setIterationComposite(IterationCompositeOperation iterationCompositeOperation) { m_iterationCompositeOperation = iterationCompositeOperation; }
    CompositeOperation composite() const { return m_compositeOperation; }
    void setComposite(CompositeOperation compositeOperation) { m_compositeOperation = compositeOperation; }

    void getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle);
    void apply(RenderStyle&) override;
    void startOrStopAccelerated();
    bool isRunningAccelerated() const { return m_startedAccelerated; }

    RenderElement* renderer() const override;
    const RenderStyle& currentStyle() const override;
    bool isAccelerated() const override { return false; }
    bool filterFunctionListsMatch() const override { return false; }
    bool transformFunctionListsMatch() const override { return false; }
#if ENABLE(FILTERS_LEVEL_2)
    bool backdropFilterFunctionListsMatch() const override { return false; }
#endif

private:
    KeyframeEffect(Element*);
    ExceptionOr<void> processKeyframes(JSC::ExecState&, JSC::Strong<JSC::JSObject>&&);

    void setAnimatedPropertiesInStyle(RenderStyle&, double);
    void computeStackingContextImpact();
    bool shouldRunAccelerated();

    RefPtr<Element> m_target;
    IterationCompositeOperation m_iterationCompositeOperation { IterationCompositeOperation::Replace };
    CompositeOperation m_compositeOperation { CompositeOperation::Replace };
    KeyframeList m_keyframes;
    Vector<std::optional<double>> m_offsets;
    Vector<RefPtr<TimingFunction>> m_timingFunctions;
    Vector<std::optional<CompositeOperation>> m_compositeOperations;
    bool m_triggersStackingContext { false };
    bool m_started { false };
    bool m_startedAccelerated { false };

};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_EFFECT(KeyframeEffect, isKeyframeEffect());

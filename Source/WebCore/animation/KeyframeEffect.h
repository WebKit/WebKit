/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "CSSPropertyBlendingClient.h"
#include "CompositeOperation.h"
#include "EffectTiming.h"
#include "Element.h"
#include "IterationCompositeOperation.h"
#include "KeyframeEffectOptions.h"
#include "KeyframeList.h"
#include "RenderStyle.h"
#include "StyleProperties.h"
#include <wtf/Ref.h>

namespace WebCore {

class Element;
class FilterOperations;

class KeyframeEffect : public AnimationEffect
    , public CSSPropertyBlendingClient {
public:
    static ExceptionOr<Ref<KeyframeEffect>> create(JSC::ExecState&, Element*, JSC::Strong<JSC::JSObject>&&, std::optional<Variant<double, KeyframeEffectOptions>>&&);
    static ExceptionOr<Ref<KeyframeEffect>> create(JSC::ExecState&, Ref<KeyframeEffect>&&);
    static Ref<KeyframeEffect> create(const Element&);
    ~KeyframeEffect() { }

    bool isKeyframeEffect() const final { return true; }

    struct BasePropertyIndexedKeyframe {
        Variant<std::nullptr_t, Vector<std::optional<double>>, double> offset = Vector<std::optional<double>>();
        Variant<Vector<String>, String> easing = Vector<String>();
        Variant<std::nullptr_t, Vector<std::optional<CompositeOperation>>, CompositeOperation> composite = Vector<std::optional<CompositeOperation>>();
    };

    struct BaseKeyframe {
        std::optional<double> offset;
        String easing { "linear" };
        std::optional<CompositeOperation> composite;
    };

    struct PropertyAndValues {
        CSSPropertyID property;
        Vector<String> values;
    };

    struct KeyframeLikeObject {
        BasePropertyIndexedKeyframe baseProperties;
        Vector<PropertyAndValues> propertiesAndValues;
    };

    struct ParsedKeyframe {
        std::optional<double> offset;
        double computedOffset;
        std::optional<CompositeOperation> composite;
        String easing;
        RefPtr<TimingFunction> timingFunction;
        Ref<MutableStyleProperties> style;
        HashMap<CSSPropertyID, String> unparsedStyle;

        ParsedKeyframe()
            : style(MutableStyleProperties::create())
        {
        }
    };

    struct BaseComputedKeyframe {
        std::optional<double> offset;
        double computedOffset;
        String easing { "linear" };
        std::optional<CompositeOperation> composite;
    };

    Element* target() const { return m_target.get(); }
    void setTarget(RefPtr<Element>&&);

    Vector<JSC::Strong<JSC::JSObject>> getKeyframes(JSC::ExecState&);
    ExceptionOr<void> setKeyframes(JSC::ExecState&, JSC::Strong<JSC::JSObject>&&);

    IterationCompositeOperation iterationComposite() const { return m_iterationCompositeOperation; }
    void setIterationComposite(IterationCompositeOperation iterationCompositeOperation) { m_iterationCompositeOperation = iterationCompositeOperation; }
    CompositeOperation composite() const { return m_compositeOperation; }
    void setComposite(CompositeOperation compositeOperation) { m_compositeOperation = compositeOperation; }

    void getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle);
    void apply(RenderStyle&) override;
    void invalidate() override;
    void animationDidSeek() final;
    void animationSuspensionStateDidChange(bool) final;
    void applyPendingAcceleratedActions();
    bool isRunningAccelerated() const { return m_lastRecordedAcceleratedAction != AcceleratedAction::Stop; }
    bool hasPendingAcceleratedAction() const { return !m_pendingAcceleratedActions.isEmpty() && isRunningAccelerated(); }

    RenderElement* renderer() const override;
    const RenderStyle& currentStyle() const override;
    bool isAccelerated() const override { return m_shouldRunAccelerated; }
    bool filterFunctionListsMatch() const override { return m_filterFunctionListsMatch; }
    bool transformFunctionListsMatch() const override { return m_transformFunctionListsMatch; }
#if ENABLE(FILTERS_LEVEL_2)
    bool backdropFilterFunctionListsMatch() const override { return m_backdropFilterFunctionListsMatch; }
#endif
    bool colorFilterFunctionListsMatch() const override { return m_colorFilterFunctionListsMatch; }

    void computeDeclarativeAnimationBlendingKeyframes(const RenderStyle* oldStyle, const RenderStyle& newStyle);
    bool hasBlendingKeyframes() const { return m_blendingKeyframes.size(); }
    const HashSet<CSSPropertyID>& animatedProperties() const { return m_blendingKeyframes.properties(); }

    bool computeExtentOfTransformAnimation(LayoutRect&) const;
    bool computeTransformedExtentViaTransformList(const FloatRect&, const RenderStyle&, LayoutRect&) const;
    bool computeTransformedExtentViaMatrix(const FloatRect&, const RenderStyle&, LayoutRect&) const;
    bool forceLayoutIfNeeded();

private:
    KeyframeEffect(Element*);

    enum class AcceleratedAction { Play, Pause, Seek, Stop };

    void copyPropertiesFromSource(Ref<KeyframeEffect>&&);
    ExceptionOr<void> processKeyframes(JSC::ExecState&, JSC::Strong<JSC::JSObject>&&);
    void addPendingAcceleratedAction(AcceleratedAction);
    void updateAcceleratedAnimationState();
    void setAnimatedPropertiesInStyle(RenderStyle&, double);
    TimingFunction* timingFunctionForKeyframeAtIndex(size_t);
    Ref<const Animation> backingAnimationForCompositedRenderer() const;
    void computedNeedsForcedLayout();
    void computeStackingContextImpact();
    void updateBlendingKeyframes(RenderStyle&);
    void computeCSSAnimationBlendingKeyframes();
    void computeCSSTransitionBlendingKeyframes(const RenderStyle* oldStyle, const RenderStyle& newStyle);
    void computeShouldRunAccelerated();
    void setBlendingKeyframes(KeyframeList&);
    void checkForMatchingTransformFunctionLists();
    void checkForMatchingFilterFunctionLists();
#if ENABLE(FILTERS_LEVEL_2)
    void checkForMatchingBackdropFilterFunctionLists();
#endif
    void checkForMatchingColorFilterFunctionLists();

    bool checkForMatchingFilterFunctionLists(CSSPropertyID, const std::function<const FilterOperations& (const RenderStyle&)>&) const;

    AcceleratedAction m_lastRecordedAcceleratedAction { AcceleratedAction::Stop };
    bool m_shouldRunAccelerated { false };
    bool m_needsForcedLayout { false };
    bool m_triggersStackingContext { false };
    bool m_transformFunctionListsMatch { false };
    bool m_filterFunctionListsMatch { false };
#if ENABLE(FILTERS_LEVEL_2)
    bool m_backdropFilterFunctionListsMatch { false };
#endif
    bool m_colorFilterFunctionListsMatch { false };

    RefPtr<Element> m_target;
    KeyframeList m_blendingKeyframes;
    Vector<ParsedKeyframe> m_parsedKeyframes;
    IterationCompositeOperation m_iterationCompositeOperation { IterationCompositeOperation::Replace };
    CompositeOperation m_compositeOperation { CompositeOperation::Replace };

    Vector<AcceleratedAction> m_pendingAcceleratedActions;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_EFFECT(KeyframeEffect, isKeyframeEffect());

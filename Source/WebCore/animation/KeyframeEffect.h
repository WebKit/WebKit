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
#include "AnimationEffectPhase.h"
#include "CSSPropertyBlendingClient.h"
#include "CompositeOperation.h"
#include "CompositeOperationOrAuto.h"
#include "EffectTiming.h"
#include "Element.h"
#include "IterationCompositeOperation.h"
#include "KeyframeEffectOptions.h"
#include "KeyframeList.h"
#include "RenderStyle.h"
#include "StyleProperties.h"
#include "WebAnimationTypes.h"
#include <wtf/Ref.h>

namespace WebCore {

class Element;
class FilterOperations;

class KeyframeEffect : public AnimationEffect
    , public CSSPropertyBlendingClient {
public:
    static ExceptionOr<Ref<KeyframeEffect>> create(JSC::JSGlobalObject&, Element*, JSC::Strong<JSC::JSObject>&&, Optional<Variant<double, KeyframeEffectOptions>>&&);
    static ExceptionOr<Ref<KeyframeEffect>> create(JSC::JSGlobalObject&, Ref<KeyframeEffect>&&);
    static Ref<KeyframeEffect> create(const Element&, PseudoId);
    ~KeyframeEffect() { }

    bool isKeyframeEffect() const final { return true; }

    struct BasePropertyIndexedKeyframe {
        Variant<std::nullptr_t, Vector<Optional<double>>, double> offset = Vector<Optional<double>>();
        Variant<Vector<String>, String> easing = Vector<String>();
        Variant<Vector<CompositeOperationOrAuto>, CompositeOperationOrAuto> composite = Vector<CompositeOperationOrAuto>();
    };

    struct BaseKeyframe {
        MarkableDouble offset;
        String easing { "linear" };
        CompositeOperationOrAuto composite { CompositeOperationOrAuto::Auto };
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
        MarkableDouble offset;
        double computedOffset;
        CompositeOperationOrAuto composite { CompositeOperationOrAuto::Auto };
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
        MarkableDouble offset;
        double computedOffset;
        String easing { "linear" };
        CompositeOperationOrAuto composite { CompositeOperationOrAuto::Auto };
    };

    const Vector<ParsedKeyframe>& parsedKeyframes() const { return m_parsedKeyframes; }

    Element* target() const { return m_target.get(); }
    Element* targetElementOrPseudoElement() const;
    void setTarget(RefPtr<Element>&&);

    bool targetsPseudoElement() const;
    const String pseudoElement() const;
    ExceptionOr<void> setPseudoElement(const String&);

    Vector<JSC::Strong<JSC::JSObject>> getBindingsKeyframes(JSC::JSGlobalObject&);
    Vector<JSC::Strong<JSC::JSObject>> getKeyframes(JSC::JSGlobalObject&);
    ExceptionOr<void> setBindingsKeyframes(JSC::JSGlobalObject&, JSC::Strong<JSC::JSObject>&&);
    ExceptionOr<void> setKeyframes(JSC::JSGlobalObject&, JSC::Strong<JSC::JSObject>&&);

    IterationCompositeOperation iterationComposite() const { return m_iterationCompositeOperation; }
    void setIterationComposite(IterationCompositeOperation iterationCompositeOperation) { m_iterationCompositeOperation = iterationCompositeOperation; }
    CompositeOperation composite() const { return m_compositeOperation; }
    void setComposite(CompositeOperation compositeOperation) { m_compositeOperation = compositeOperation; }

    void getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle);
    void apply(RenderStyle&, Optional<Seconds> = WTF::nullopt) override;
    void invalidate() override;
    void animationDidTick() final;
    void animationDidPlay() final;
    void animationDidChangeTimingProperties() final;
    void animationWasCanceled() final;
    void animationSuspensionStateDidChange(bool) final;
    void animationTimelineDidChange(AnimationTimeline*) final;
    void animationTimingDidChange();
    void applyPendingAcceleratedActions();

    void willChangeRenderer();

    void setAnimation(WebAnimation*) final;

    RenderElement* renderer() const override;
    const RenderStyle& currentStyle() const override;
    bool triggersStackingContext() const { return m_triggersStackingContext; }
    bool isRunningAccelerated() const { return m_isRunningAccelerated; }
    bool isAboutToRunAccelerated() const { return m_acceleratedPropertiesState != AcceleratedProperties::None && m_lastRecordedAcceleratedAction != AcceleratedAction::Stop; }
    bool isCompletelyAccelerated() const { return m_acceleratedPropertiesState == AcceleratedProperties::All; }
    bool filterFunctionListsMatch() const override { return m_filterFunctionListsMatch; }
    bool transformFunctionListsMatch() const override { return m_transformFunctionListsMatch; }
#if ENABLE(FILTERS_LEVEL_2)
    bool backdropFilterFunctionListsMatch() const override { return m_backdropFilterFunctionListsMatch; }
#endif
    bool colorFilterFunctionListsMatch() const override { return m_colorFilterFunctionListsMatch; }

    void computeDeclarativeAnimationBlendingKeyframes(const RenderStyle* oldStyle, const RenderStyle& newStyle);
    const KeyframeList& blendingKeyframes() const { return m_blendingKeyframes; }
    const HashSet<CSSPropertyID>& animatedProperties() const { return m_blendingKeyframes.properties(); }
    bool animatesProperty(CSSPropertyID) const;

    bool computeExtentOfTransformAnimation(LayoutRect&) const;
    bool computeTransformedExtentViaTransformList(const FloatRect&, const RenderStyle&, LayoutRect&) const;
    bool computeTransformedExtentViaMatrix(const FloatRect&, const RenderStyle&, LayoutRect&) const;
    bool forceLayoutIfNeeded();

    enum class Accelerated : uint8_t { Yes, No };
    bool isCurrentlyAffectingProperty(CSSPropertyID, Accelerated = Accelerated::No) const;
    bool isRunningAcceleratedAnimationForProperty(CSSPropertyID) const;

    bool requiresPseudoElement() const;

private:
    KeyframeEffect(Element*, PseudoId);

    enum class AcceleratedAction : uint8_t { Play, Pause, UpdateTiming, Stop };
    enum class BlendingKeyframesSource : uint8_t { CSSAnimation, CSSTransition, WebAnimation };
    enum class AcceleratedProperties : uint8_t { None, Some, All };

    Document* document() const;
    void updateEffectStackMembership();
    void copyPropertiesFromSource(Ref<KeyframeEffect>&&);
    void didChangeTargetElementOrPseudoElement(Element*);
    ExceptionOr<void> processKeyframes(JSC::JSGlobalObject&, JSC::Strong<JSC::JSObject>&&);
    void addPendingAcceleratedAction(AcceleratedAction);
    void updateAcceleratedActions();
    void setAnimatedPropertiesInStyle(RenderStyle&, double);
    TimingFunction* timingFunctionForKeyframeAtIndex(size_t) const;
    Ref<const Animation> backingAnimationForCompositedRenderer() const;
    void computedNeedsForcedLayout();
    void computeStackingContextImpact();
    void computeSomeKeyframesUseStepsTimingFunction();
    void clearBlendingKeyframes();
    void updateBlendingKeyframes(RenderStyle&);
    void computeCSSAnimationBlendingKeyframes(const RenderStyle&);
    void computeCSSTransitionBlendingKeyframes(const RenderStyle* oldStyle, const RenderStyle& newStyle);
    void computeAcceleratedPropertiesState();
    void setBlendingKeyframes(KeyframeList&);
    Optional<double> progressUntilNextStep(double) const final;
    void checkForMatchingTransformFunctionLists();
    void checkForMatchingFilterFunctionLists();
    void checkForMatchingColorFilterFunctionLists();
    bool checkForMatchingFilterFunctionLists(CSSPropertyID, const std::function<const FilterOperations& (const RenderStyle&)>&) const;
#if ENABLE(FILTERS_LEVEL_2)
    void checkForMatchingBackdropFilterFunctionLists();
#endif

    KeyframeList m_blendingKeyframes { emptyString() };
    Vector<ParsedKeyframe> m_parsedKeyframes;
    Vector<AcceleratedAction> m_pendingAcceleratedActions;
    RefPtr<Element> m_target;
    PseudoId m_pseudoId { PseudoId::None };

    AcceleratedAction m_lastRecordedAcceleratedAction { AcceleratedAction::Stop };
    BlendingKeyframesSource m_blendingKeyframesSource { BlendingKeyframesSource::WebAnimation };
    IterationCompositeOperation m_iterationCompositeOperation { IterationCompositeOperation::Replace };
    CompositeOperation m_compositeOperation { CompositeOperation::Replace };
    AcceleratedProperties m_acceleratedPropertiesState { AcceleratedProperties::None };
    AnimationEffectPhase m_phaseAtLastApplication { AnimationEffectPhase::Idle };
    bool m_isRunningAccelerated { false };
    bool m_needsForcedLayout { false };
    bool m_triggersStackingContext { false };
    bool m_transformFunctionListsMatch { false };
    bool m_filterFunctionListsMatch { false };
#if ENABLE(FILTERS_LEVEL_2)
    bool m_backdropFilterFunctionListsMatch { false };
#endif
    bool m_colorFilterFunctionListsMatch { false };
    bool m_inTargetEffectStack { false };
    bool m_someKeyframesUseStepsTimingFunction { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_EFFECT(KeyframeEffect, isKeyframeEffect());

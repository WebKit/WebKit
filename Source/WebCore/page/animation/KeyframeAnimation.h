/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AnimationBase.h"
#include "Document.h"
#include "KeyframeList.h"

namespace WebCore {

class FilterOperations;
class RenderStyle;

// A KeyframeAnimation tracks the state of an explicit animation for a single RenderElement.
class KeyframeAnimation final : public AnimationBase {
public:
    static Ref<KeyframeAnimation> create(const Animation& animation, Element& element, CompositeAnimation& compositeAnimation, const RenderStyle& unanimatedStyle)
    {
        return adoptRef(*new KeyframeAnimation(animation, element, compositeAnimation, unanimatedStyle));
    }
    
    OptionSet<AnimateChange> animate(CompositeAnimation&, const RenderStyle& targetStyle, std::unique_ptr<RenderStyle>& animatedStyle);
    void getAnimatedStyle(std::unique_ptr<RenderStyle>&) override;

    bool computeExtentOfTransformAnimation(LayoutRect&) const override;

    const KeyframeList& keyframes() const { return m_keyframes; }

    const AtomicString& name() const { return m_keyframes.animationName(); }

    bool hasAnimationForProperty(CSSPropertyID) const;

    bool triggersStackingContext() const { return m_triggersStackingContext; }
    bool dependsOnLayout() const { return m_dependsOnLayout; }
    bool affectsAcceleratedProperty() const { return m_hasAcceleratedProperty; }

    void setUnanimatedStyle(std::unique_ptr<RenderStyle> style) { m_unanimatedStyle = WTFMove(style); }
    const RenderStyle& unanimatedStyle() const override { return *m_unanimatedStyle; }

    Optional<Seconds> timeToNextService() override;

protected:
    void onAnimationStart(double elapsedTime) override;
    void onAnimationIteration(double elapsedTime) override;
    void onAnimationEnd(double elapsedTime) override;
    bool startAnimation(double timeOffset) override;
    void pauseAnimation(double timeOffset) override;
    void endAnimation(bool fillingForwards = false) override;

    void overrideAnimations() override;
    void resumeOverriddenAnimations() override;

    bool shouldSendEventForListener(Document::ListenerType inListenerType) const;
    bool sendAnimationEvent(const AtomicString&, double elapsedTime);

    bool affectsProperty(CSSPropertyID) const override;

    bool computeExtentOfAnimationForMatrixAnimation(const FloatRect& rendererBox, LayoutRect&) const;

    bool computeExtentOfAnimationForMatchingTransformLists(const FloatRect& rendererBox, LayoutRect&) const;

    void computeLayoutDependency();
    void resolveKeyframeStyles();
    void validateTransformFunctionList();
    void checkForMatchingFilterFunctionLists();
#if ENABLE(FILTERS_LEVEL_2)
    void checkForMatchingBackdropFilterFunctionLists();
#endif
    void checkForMatchingColorFilterFunctionLists();
    bool checkForMatchingFilterFunctionLists(CSSPropertyID, const std::function<const FilterOperations& (const RenderStyle&)>&) const;

private:
    KeyframeAnimation(const Animation&, Element&, CompositeAnimation&, const RenderStyle& unanimatedStyle);
    virtual ~KeyframeAnimation();
    
    // Get the styles for the given property surrounding the current animation time and the progress between them.
    void fetchIntervalEndpointsForProperty(CSSPropertyID, const RenderStyle*& fromStyle, const RenderStyle*& toStyle, double& progress) const;

    KeyframeList m_keyframes;
    std::unique_ptr<RenderStyle> m_unanimatedStyle; // The style just before we started animation

    bool m_startEventDispatched { false };
    bool m_triggersStackingContext { false };
    bool m_hasAcceleratedProperty { false };
    bool m_dependsOnLayout { false };
};

} // namespace WebCore

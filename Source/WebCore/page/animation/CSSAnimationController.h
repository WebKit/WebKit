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
#include "CSSPropertyNames.h"
#include "RenderStyle.h"
#include <wtf/Forward.h>

namespace WebCore {

class CSSAnimationControllerPrivate;
class Document;
class Element;
class Frame;
class LayoutRect;
class RenderElement;

struct AnimationUpdate {
    std::unique_ptr<RenderStyle> style;
    bool animationChangeRequiresRecomposite { false };
};

class CSSAnimationController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CSSAnimationController(Frame&);
    ~CSSAnimationController();

    void cancelAnimations(Element&);
    AnimationUpdate updateAnimations(Element&, const RenderStyle& newStyle, const RenderStyle* oldStyle);
    std::unique_ptr<RenderStyle> animatedStyleForRenderer(RenderElement&);

    // If possible, compute the visual extent of any transform animation on the given renderer
    // using the given rect, returning the result in the rect. Return false if there is some
    // transform animation but we were unable to cheaply compute its affect on the extent.
    bool computeExtentOfAnimation(RenderElement&, LayoutRect&) const;

    // This is called when an accelerated animation or transition has actually started to animate.
    void notifyAnimationStarted(RenderElement&, MonotonicTime startTime);

    WEBCORE_EXPORT bool pauseAnimationAtTime(Element&, const AtomicString& name, double t); // To be used only for testing
    WEBCORE_EXPORT bool pauseTransitionAtTime(Element&, const String& property, double t); // To be used only for testing
    WEBCORE_EXPORT unsigned numberOfActiveAnimations(Document*) const; // To be used only for testing
    
    // "Running" here means the animation is running or paused.
    bool isRunningAnimationOnRenderer(RenderElement&, CSSPropertyID) const;
    bool isRunningAcceleratedAnimationOnRenderer(RenderElement&, CSSPropertyID) const;

    WEBCORE_EXPORT bool isSuspended() const;
    WEBCORE_EXPORT void suspendAnimations();
    WEBCORE_EXPORT void resumeAnimations();
    void serviceAnimations();

    void updateThrottlingState();
    WEBCORE_EXPORT Seconds animationInterval() const;

    WEBCORE_EXPORT void suspendAnimationsForDocument(Document*);
    WEBCORE_EXPORT void resumeAnimationsForDocument(Document*);
    WEBCORE_EXPORT bool animationsAreSuspendedForDocument(Document*);
    void detachFromDocument(Document*);
    void startAnimationsIfNotSuspended(Document*);

    void beginAnimationUpdate();
    void endAnimationUpdate();

    WEBCORE_EXPORT bool allowsNewAnimationsWhileSuspended() const;
    WEBCORE_EXPORT void setAllowsNewAnimationsWhileSuspended(bool);
    
    static bool supportsAcceleratedAnimationOfProperty(CSSPropertyID);

    bool hasAnimations() const;

private:
    const std::unique_ptr<CSSAnimationControllerPrivate> m_data;
};

class AnimationUpdateBlock {
public:
    AnimationUpdateBlock(CSSAnimationController* animationController)
        : m_animationController(animationController)
    {
        if (m_animationController)
            m_animationController->beginAnimationUpdate();
    }
    
    ~AnimationUpdateBlock()
    {
        if (m_animationController)
            m_animationController->endAnimationUpdate();
    }
    
    CSSAnimationController* m_animationController;
};

} // namespace WebCore

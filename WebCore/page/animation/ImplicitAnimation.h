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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef ImplicitAnimation_h
#define ImplicitAnimation_h

#include "AnimationBase.h"
#include "Document.h"

namespace WebCore {

// An ImplicitAnimation tracks the state of a transition of a specific CSS property
// for a single RenderObject.
class ImplicitAnimation : public AnimationBase {
public:
    ImplicitAnimation(const Animation* transition, int animatingProperty, RenderObject* renderer, CompositeAnimation* compAnim);    
    virtual ~ImplicitAnimation()
    {
        ASSERT(!m_fromStyle && !m_toStyle);
        
        // If we were waiting for an end event, we need to finish the animation to make sure no old
        // animations stick around in the lower levels
        if (waitingForEndEvent() && m_object)
            ASSERT(0);
        
        // Do the cleanup here instead of in the base class so the specialized methods get called
        if (!postActive())
            updateStateMachine(STATE_INPUT_END_ANIMATION, -1);     
    }
    
    int transitionProperty() const { return m_transitionProperty; }
    int animatingProperty() const { return m_animatingProperty; }
    
    virtual void onAnimationEnd(double inElapsedTime);
    
    virtual void animate(CompositeAnimation*, RenderObject*, const RenderStyle* currentStyle, 
                         const RenderStyle* targetStyle, RenderStyle*& animatedStyle);
    virtual void reset(RenderObject* renderer, const RenderStyle* from = 0, const RenderStyle* to = 0);
    
    void setOverridden(bool b);
    virtual bool overridden() const { return m_overridden; }
    
    virtual bool shouldFireEvents() const { return true; }
    
    virtual bool affectsProperty(int property) const;
    
    bool hasStyle() const { return m_fromStyle && m_toStyle; }
    
    bool isTargetPropertyEqual(int prop, const RenderStyle* targetStyle);

    void blendPropertyValueInStyle(int prop, RenderStyle* currentStyle);
    
protected:
    bool shouldSendEventForListener(Document::ListenerType inListenerType);    
    bool sendTransitionEvent(const AtomicString& inEventType, double inElapsedTime);
    
private:
    int m_transitionProperty;   // Transition property as specified in the RenderStyle. May be cAnimateAll
    int m_animatingProperty;    // Specific property for this ImplicitAnimation
    bool m_overridden;          // true when there is a keyframe animation that overrides the transitioning property
    
    // The two styles that we are blending.
    RenderStyle* m_fromStyle;
    RenderStyle* m_toStyle;
};

}

#endif

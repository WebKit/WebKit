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

#ifndef KeyframeAnimation_h
#define KeyframeAnimation_h

#include "AnimationBase.h"
#include "Document.h"
#include "RenderStyle.h"

namespace WebCore {

// A KeyframeAnimation tracks the state of an explicit animation
// for a single RenderObject.
class KeyframeAnimation : public AnimationBase {
public:
    KeyframeAnimation(const Animation* animation, RenderObject* renderer, int index, CompositeAnimation* compAnim)
    : AnimationBase(animation, renderer, compAnim)
    , m_keyframes(animation->keyframeList())
    , m_name(animation->name())
    , m_index(index)
    {
    }

    virtual ~KeyframeAnimation()
    {
        // Do the cleanup here instead of in the base class so the specialized methods get called
        if (!postActive())
            updateStateMachine(AnimationStateInputEndAnimation, -1);
    }

    virtual void animate(CompositeAnimation*, RenderObject*, const RenderStyle* currentStyle,
                         const RenderStyle* targetStyle, RenderStyle*& animatedStyle);

    void setName(const String& s) { m_name = s; }
    const AtomicString& name() const { return m_name; }
    int index() const { return m_index; }

    virtual bool shouldFireEvents() const { return true; }

protected:
    virtual void onAnimationStart(double inElapsedTime);
    virtual void onAnimationIteration(double inElapsedTime);
    virtual void onAnimationEnd(double inElapsedTime);
    virtual void endAnimation(bool reset);

    virtual void overrideAnimations();
    virtual void resumeOverriddenAnimations();
    
    bool shouldSendEventForListener(Document::ListenerType inListenerType);    
    bool sendAnimationEvent(const AtomicString& inEventType, double inElapsedTime);
    
    virtual bool affectsProperty(int property) const;

private:
    // The keyframes that we are blending.
    RefPtr<KeyframeList> m_keyframes;
    AtomicString m_name;
    // The order in which this animation appears in the animation-name style.
    int m_index;
};

}

#endif

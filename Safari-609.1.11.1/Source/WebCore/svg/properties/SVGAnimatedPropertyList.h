/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#include "SVGAnimatedProperty.h"

namespace WebCore {

template<typename ListType>
class SVGAnimatedPropertyList : public SVGAnimatedProperty {
public:
    template<typename... Arguments>
    static Ref<SVGAnimatedPropertyList> create(SVGElement* contextElement, Arguments&&... arguments)
    {
        return adoptRef(*new SVGAnimatedPropertyList(contextElement, std::forward<Arguments>(arguments)...));
    }

    ~SVGAnimatedPropertyList()
    {
        m_baseVal->detach();
        if (m_animVal)
            m_animVal->detach();
    }

    // Used by the DOM.
    const Ref<ListType>& baseVal() const { return m_baseVal; }

    // Used by SVGElement::parseAttribute().
    Ref<ListType>& baseVal() { return m_baseVal; }

    // Used by the DOM.
    const RefPtr<ListType>& animVal() const { return const_cast<SVGAnimatedPropertyList*>(this)->ensureAnimVal(); }

    // Called by SVGAnimatedPropertyAnimator to pass the animVal to the SVGAnimationFunction::progress.
    RefPtr<ListType>& animVal() { return ensureAnimVal(); }

    // Used when committing a change from the SVGAnimatedProperty to the attribute.
    String baseValAsString() const override { return m_baseVal->valueAsString(); }

    // Used to apply the SVGAnimator change to the target element.
    String animValAsString() const override
    {
        ASSERT(isAnimating());
        return m_animVal->valueAsString();
    }

    // Managing the relationship with the owner.
    void setDirty() override { m_baseVal->setDirty(); }
    bool isDirty() const override { return m_baseVal->isDirty(); }
    Optional<String> synchronize() override { return m_baseVal->synchronize(); }

    // Used by RenderSVGElements and DumpRenderTree.
    const ListType& currentValue() const
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return isAnimating() ? *m_animVal : m_baseVal.get();
    }

    // Controlling the animation.
    void startAnimation() override
    {
        if (m_animVal)
            *m_animVal = m_baseVal;
        else
            ensureAnimVal();
        SVGAnimatedProperty::startAnimation();
    }

    void stopAnimation() override
    {
        if (m_animVal)
            *m_animVal = m_baseVal;
        SVGAnimatedProperty::stopAnimation();
    }

    // Controlling the instance animation.
    void instanceStartAnimation(SVGAnimatedProperty& animated) override
    {
        m_animVal = static_cast<SVGAnimatedPropertyList&>(animated).animVal();
        SVGAnimatedProperty::instanceStartAnimation(animated);
    }

    void instanceStopAnimation() override
    {
        m_animVal = nullptr;
        SVGAnimatedProperty::instanceStopAnimation();
    }

    // Visual Studio doesn't seem to see these private constructors from subclasses.
    // FIXME: See what it takes to remove this hack.
#if !COMPILER(MSVC)
protected:
#endif
    template<typename... Arguments>
    SVGAnimatedPropertyList(SVGElement* contextElement, Arguments&&... arguments)
        : SVGAnimatedProperty(contextElement)
        , m_baseVal(ListType::create(this, SVGPropertyAccess::ReadWrite, std::forward<Arguments>(arguments)...))
    {
    }

    RefPtr<ListType>& ensureAnimVal()
    {
        if (!m_animVal)
            m_animVal = ListType::create(m_baseVal, SVGPropertyAccess::ReadOnly);
        return m_animVal;
    }

    // Called when m_baseVal changes or an item in m_baseVal changes.
    void commitPropertyChange(SVGProperty* property) override
    {
        if (m_animVal)
            *m_animVal = m_baseVal;
        SVGAnimatedProperty::commitPropertyChange(property);
    }

    Ref<ListType> m_baseVal;
    mutable RefPtr<ListType> m_animVal;
};

}

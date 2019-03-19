/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
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

#include "SVGPropertyOwner.h"
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    
class SVGElement;

class SVGAnimatedProperty : public RefCounted<SVGAnimatedProperty>, public SVGPropertyOwner {
public:
    virtual ~SVGAnimatedProperty() = default;
    
    // Manage the relationship with the owner.
    bool isAttached() const { return m_contextElement; }
    void detach() { m_contextElement = nullptr; }
    SVGElement* contextElement() const { return m_contextElement; }
    
    virtual String baseValAsString() const { return emptyString(); }
    virtual String animValAsString() const { return emptyString(); }
    
    // Control the synchronization between the attribute and its reflection in baseVal.
    virtual bool isDirty() const { return false; }
    virtual void setDirty() { }
    virtual Optional<String> synchronize() { return WTF::nullopt; }
    
    // Control the animation life cycle.
    bool isAnimating() const { return m_isAnimating; }
    virtual void startAnimation() { m_isAnimating = true; }
    virtual void stopAnimation() { m_isAnimating = false; }
    
    // Attach/Detach the animVal of the traget element's property by the instance element's property.
    virtual void instanceStartAnimation(SVGAnimatedProperty&) { m_isAnimating = true; }
    virtual void instanceStopAnimation() { m_isAnimating = false; }
    
protected:
    SVGAnimatedProperty(SVGElement* contextElement)
        : m_contextElement(contextElement)
    {
    }
    
    SVGPropertyOwner* owner() const override;
    void commitPropertyChange(SVGProperty*) override;
    
    SVGElement* m_contextElement { nullptr };
    bool m_isAnimating { false };
};

} // namespace WebCore


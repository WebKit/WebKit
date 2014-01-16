/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef AccessibilitySlider_h
#define AccessibilitySlider_h

#include "AccessibilityMockObject.h"
#include "AccessibilityRenderObject.h"

namespace WebCore {

class HTMLInputElement;

class AccessibilitySlider : public AccessibilityRenderObject {
    
public:
    static PassRefPtr<AccessibilitySlider> create(RenderObject*);
    virtual ~AccessibilitySlider() { }

protected:
    explicit AccessibilitySlider(RenderObject*);

private:
    HTMLInputElement* inputElement() const;
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const override;

    virtual AccessibilityRole roleValue() const override { return SliderRole; }
    virtual bool isSlider() const override { return true; }
    virtual bool isInputSlider() const override { return true; }
    virtual bool isControl() const override { return true; }
    
    virtual void addChildren() override;
    
    virtual bool canSetValueAttribute() const override { return true; }
    const AtomicString& getAttribute(const QualifiedName& attribute) const;
    
    virtual void setValue(const String&) override;
    virtual float valueForRange() const override;
    virtual float maxValueForRange() const override;
    virtual float minValueForRange() const override;
    virtual AccessibilityOrientation orientation() const override;
};

class AccessibilitySliderThumb : public AccessibilityMockObject {
    
public:
    static PassRefPtr<AccessibilitySliderThumb> create();
    virtual ~AccessibilitySliderThumb() { }

    virtual bool isSliderThumb() const override final { return true; }

    virtual AccessibilityRole roleValue() const override { return SliderThumbRole; }

    virtual LayoutRect elementRect() const override;

private:
    AccessibilitySliderThumb();

    virtual bool computeAccessibilityIsIgnored() const override;
};

ACCESSIBILITY_OBJECT_TYPE_CASTS(AccessibilitySliderThumb, isSliderThumb())

} // namespace WebCore

#endif // AccessibilitySlider_h

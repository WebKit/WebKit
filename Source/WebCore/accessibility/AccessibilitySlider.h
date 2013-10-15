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
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const OVERRIDE;

    virtual AccessibilityRole roleValue() const OVERRIDE { return SliderRole; }
    virtual bool isSlider() const OVERRIDE { return true; }
    virtual bool isInputSlider() const OVERRIDE { return true; }
    virtual bool isControl() const OVERRIDE { return true; }
    
    virtual void addChildren() OVERRIDE;
    
    virtual bool canSetValueAttribute() const OVERRIDE { return true; }
    const AtomicString& getAttribute(const QualifiedName& attribute) const;
    
    virtual void setValue(const String&) OVERRIDE;
    virtual float valueForRange() const OVERRIDE;
    virtual float maxValueForRange() const OVERRIDE;
    virtual float minValueForRange() const OVERRIDE;
    virtual AccessibilityOrientation orientation() const OVERRIDE;
};

class AccessibilitySliderThumb : public AccessibilityMockObject {
    
public:
    static PassRefPtr<AccessibilitySliderThumb> create();
    virtual ~AccessibilitySliderThumb() { }

    virtual AccessibilityRole roleValue() const OVERRIDE { return SliderThumbRole; }

    virtual LayoutRect elementRect() const OVERRIDE;

private:
    AccessibilitySliderThumb();

    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};


} // namespace WebCore

#endif // AccessibilitySlider_h

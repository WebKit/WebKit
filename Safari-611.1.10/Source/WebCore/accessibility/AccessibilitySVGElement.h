/*
 * Copyright (C) 2016 Igalia, S.L.
 * All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AccessibilityRenderObject.h"

namespace WebCore {

class AccessibilitySVGElement : public AccessibilityRenderObject {

public:
    static Ref<AccessibilitySVGElement> create(RenderObject*);
    virtual ~AccessibilitySVGElement();

protected:
    explicit AccessibilitySVGElement(RenderObject*);

private:
    String accessibilityDescription() const final;
    String helpText() const final;
    void accessibilityText(Vector<AccessibilityText>&) const final;
    AccessibilityRole determineAccessibilityRole() final;
    AccessibilityRole determineAriaRoleAttribute() const final;
    bool inheritsPresentationalRole() const final;
    bool isAccessibilitySVGElement() const final { return true; }
    bool computeAccessibilityIsIgnored() const final;

    AccessibilityObject* targetForUseElement() const;

    template <typename ChildrenType>
    Element* childElementWithMatchingLanguage(ChildrenType&) const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilitySVGElement, isAccessibilitySVGElement())

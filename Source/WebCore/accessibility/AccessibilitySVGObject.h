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
#include <wtf/WeakPtr.h>

namespace WebCore {

class AccessibilitySVGObject : public AccessibilityRenderObject {
public:
    static Ref<AccessibilitySVGObject> create(AXID, RenderObject&, AXObjectCache&, bool isSVGRoot = false);
    static Ref<AccessibilitySVGObject> create(AXID, Element&, AXObjectCache&, bool isSVGRoot = false);
    virtual ~AccessibilitySVGObject();

    AccessibilityObject* parentObject() const override;
    void setParent(AccessibilityRenderObject*);
    bool isRootWithAccessibleContent() const;

    bool isAccessibilitySVGObjectInstance() const final { return true; }
    bool isAccessibilitySVGRoot() const final { return m_isSVGRoot; }

protected:
    explicit AccessibilitySVGObject(AXID, RenderObject&, AXObjectCache&, bool isSVGRoot);
    explicit AccessibilitySVGObject(AXID, Element&, AXObjectCache&, bool isSVGRoot);
    AccessibilityRole determineAriaRoleAttribute() const final;

private:
    String description() const final;
    String helpText() const final;
    void accessibilityText(Vector<AccessibilityText>&) const final;
    AccessibilityRole determineAccessibilityRole() override;
    bool inheritsPresentationalRole() const final;
    bool computeIsIgnored() const final;

    AccessibilityObject* targetForUseElement() const;

    // Returns true if the SVG element associated with this object has a <title> or <desc> child.
    bool hasTitleOrDescriptionChild() const;
    template <typename ChildrenType>
    Element* childElementWithMatchingLanguage(ChildrenType&) const;

    // Set for remote SVG resources, on the root.
    WeakPtr<AccessibilityRenderObject> m_parent;
};

inline void AccessibilitySVGObject::setParent(AccessibilityRenderObject* parent)
{
    ASSERT(m_isSVGRoot);
    m_parent = parent;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilitySVGObject) \
    static bool isType(const WebCore::AccessibilityObject& object) { return object.isAccessibilitySVGObjectInstance(); } \
SPECIALIZE_TYPE_TRAITS_END()

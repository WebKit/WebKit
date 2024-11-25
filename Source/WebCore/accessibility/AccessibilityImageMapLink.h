/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "AccessibilityMockObject.h"
#include "HTMLAreaElement.h"
#include "HTMLMapElement.h"

namespace WebCore {
    
class AccessibilityImageMapLink final : public AccessibilityMockObject {
public:
    static Ref<AccessibilityImageMapLink> create(AXID);
    virtual ~AccessibilityImageMapLink();
    
    void setHTMLAreaElement(HTMLAreaElement*);
    HTMLAreaElement* areaElement() const { return m_areaElement.get(); }
    
    void setHTMLMapElement(HTMLMapElement* element) { m_mapElement = element; }    
    HTMLMapElement* mapElement() const { return m_mapElement.get(); }
    
    Node* node() const final { return m_areaElement.get(); }

    AccessibilityRole determineAccessibilityRole() final;
    bool isEnabled() const final { return true; }

    Element* anchorElement() const final;
    Element* actionElement() const final;
    URL url() const final;
    String title() const final;
    String description() const final;
    AccessibilityObject* parentObject() const final;

    LayoutRect elementRect() const final;

private:
    explicit AccessibilityImageMapLink(AXID);

    void detachFromParent() final;
    Path elementPath() const final;
    RenderElement* imageMapLinkRenderer() const;
    void accessibilityText(Vector<AccessibilityText>&) const final;
    bool isImageMapLink() const final { return true; }
    bool supportsPath() const final { return true; }

    WeakPtr<HTMLAreaElement, WeakPtrImplWithEventTargetData> m_areaElement;
    WeakPtr<HTMLMapElement, WeakPtrImplWithEventTargetData> m_mapElement;
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilityImageMapLink)
    static bool isType(const WebCore::AXCoreObject& object)
    {
        auto* accessibilityObject = dynamicDowncast<WebCore::AccessibilityObject>(object);
        return accessibilityObject && accessibilityObject->isImageMapLink();
    }
    static bool isType(const WebCore::AccessibilityObject& object) { return object.isImageMapLink(); }
SPECIALIZE_TYPE_TRAITS_END()

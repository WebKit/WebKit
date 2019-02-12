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
    static Ref<AccessibilityImageMapLink> create();
    virtual ~AccessibilityImageMapLink();
    
    void setHTMLAreaElement(HTMLAreaElement* element) { m_areaElement = element; }
    HTMLAreaElement* areaElement() const { return m_areaElement.get(); }
    
    void setHTMLMapElement(HTMLMapElement* element) { m_mapElement = element; }    
    HTMLMapElement* mapElement() const { return m_mapElement.get(); }
    
    Node* node() const override { return m_areaElement.get(); }
        
    AccessibilityRole roleValue() const override;
    bool isEnabled() const override { return true; }
    
    Element* anchorElement() const override;
    Element* actionElement() const override;
    URL url() const override;
    bool isLink() const override { return true; }
    bool isLinked() const override { return true; }
    String title() const override;
    String accessibilityDescription() const override;
    AccessibilityObject* parentObject() const override;
    
    String stringValueForMSAA() const override;
    String nameForMSAA() const override;

    LayoutRect elementRect() const override;

private:
    AccessibilityImageMapLink();

    void detachFromParent() override;
    Path elementPath() const override;
    RenderElement* imageMapLinkRenderer() const;
    void accessibilityText(Vector<AccessibilityText>&) const override;
    bool isImageMapLink() const override { return true; }
    bool supportsPath() const override { return true; }

    RefPtr<HTMLAreaElement> m_areaElement;
    RefPtr<HTMLMapElement> m_mapElement;
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityImageMapLink, isImageMapLink())

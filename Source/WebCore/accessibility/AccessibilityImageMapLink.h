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

#ifndef AccessibilityImageMapLink_h
#define AccessibilityImageMapLink_h

#include "AccessibilityMockObject.h"
#include "HTMLAreaElement.h"
#include "HTMLMapElement.h"

namespace WebCore {
    
class AccessibilityImageMapLink : public AccessibilityMockObject {
        
private:
    AccessibilityImageMapLink();
public:
    static PassRefPtr<AccessibilityImageMapLink> create();
    virtual ~AccessibilityImageMapLink();
    
    void setHTMLAreaElement(HTMLAreaElement* element) { m_areaElement = element; }
    HTMLAreaElement* areaElement() const { return m_areaElement.get(); }
    
    void setHTMLMapElement(HTMLMapElement* element) { m_mapElement = element; }    
    HTMLMapElement* mapElement() const { return m_mapElement.get(); }
    
    virtual Node* node() const OVERRIDE { return m_areaElement.get(); }
        
    virtual AccessibilityRole roleValue() const OVERRIDE;
    virtual bool isEnabled() const OVERRIDE { return true; }
    
    virtual Element* anchorElement() const OVERRIDE;
    virtual Element* actionElement() const OVERRIDE;
    virtual URL url() const OVERRIDE;
    virtual bool isLink() const OVERRIDE { return true; }
    virtual bool isLinked() const OVERRIDE { return true; }
    virtual String title() const OVERRIDE;
    virtual String accessibilityDescription() const OVERRIDE;
    virtual AccessibilityObject* parentObject() const OVERRIDE;
    
    virtual String stringValueForMSAA() const OVERRIDE;
    virtual String nameForMSAA() const OVERRIDE;

    virtual LayoutRect elementRect() const OVERRIDE;

private:    
    RefPtr<HTMLAreaElement> m_areaElement;
    RefPtr<HTMLMapElement> m_mapElement;

    virtual void detachFromParent() OVERRIDE;

    virtual Path elementPath() const OVERRIDE;
    RenderElement* imageMapLinkRenderer() const;
    virtual void accessibilityText(Vector<AccessibilityText>&) OVERRIDE;
    virtual bool isImageMapLink() const OVERRIDE { return true; }
    virtual bool supportsPath() const OVERRIDE { return true; }
};
    
} // namespace WebCore

#endif // AccessibilityImageMapLink_h

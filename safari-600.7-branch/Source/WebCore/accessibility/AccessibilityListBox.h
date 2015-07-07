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

#ifndef AccessibilityListBox_h
#define AccessibilityListBox_h

#include "AccessibilityRenderObject.h"

namespace WebCore {

class AccessibilityListBox : public AccessibilityRenderObject {

private:
    explicit AccessibilityListBox(RenderObject*);
public:
    static PassRefPtr<AccessibilityListBox> create(RenderObject*);
    virtual ~AccessibilityListBox();
    
    virtual bool isListBox() const override { return true; }
    
    virtual bool canSetSelectedChildrenAttribute() const override;
    void setSelectedChildren(const AccessibilityChildrenVector&);
    virtual AccessibilityRole roleValue() const override { return ListBoxRole; }
        
    virtual void selectedChildren(AccessibilityChildrenVector&) override;
    virtual void visibleChildren(AccessibilityChildrenVector&) override;
    
    virtual void addChildren() override;

private:    
    AccessibilityObject* listBoxOptionAccessibilityObject(HTMLElement*) const;
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const override;
};

ACCESSIBILITY_OBJECT_TYPE_CASTS(AccessibilityListBox, isListBox())
    
} // namespace WebCore

#endif // AccessibilityListBox_h

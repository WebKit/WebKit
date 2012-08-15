/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#ifndef AccessibilityNodeObject_h
#define AccessibilityNodeObject_h

#include "AccessibilityObject.h"
#include "LayoutTypes.h"
#include <wtf/Forward.h>

namespace WebCore {
    
class AXObjectCache;
class Element;
class Frame;
class FrameView;
class HitTestResult;
class HTMLAnchorElement;
class HTMLAreaElement;
class HTMLElement;
class HTMLLabelElement;
class HTMLMapElement;
class HTMLSelectElement;
class IntPoint;
class IntSize;
class Node;
class RenderListBox;
class RenderTextControl;
class RenderView;
class VisibleSelection;
class Widget;
    
class AccessibilityNodeObject : public AccessibilityObject {
protected:
    explicit AccessibilityNodeObject(Node*);
public:
    static PassRefPtr<AccessibilityNodeObject> create(Node*);
    virtual ~AccessibilityNodeObject();

    virtual void init();
    
    virtual bool isAccessibilityNodeObject() const { return true; }

    virtual bool canSetFocusAttribute() const;
    
    virtual AccessibilityObject* firstChild() const;
    virtual AccessibilityObject* lastChild() const;
    virtual AccessibilityObject* previousSibling() const;
    virtual AccessibilityObject* nextSibling() const;
    virtual AccessibilityObject* parentObject() const;
    virtual AccessibilityObject* parentObjectIfExists() const;

    void setNode(Node*);
    virtual Node* node() const { return m_node; }
    virtual Document* document() const;

    virtual void detach();
    virtual void childrenChanged();
    virtual void updateAccessibilityRole();

    virtual LayoutRect elementRect() const;

protected:
    AccessibilityRole m_ariaRole;
    bool m_childrenDirty;
    mutable AccessibilityRole m_roleForMSAA;

    virtual bool isDetached() const { return !m_node; }

    virtual AccessibilityRole determineAccessibilityRole();
    virtual void addChildren();
    virtual bool accessibilityIsIgnored() const;
    AccessibilityRole ariaRoleAttribute() const;
    AccessibilityRole determineAriaRoleAttribute() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;

private:
    Node* m_node;
};

inline AccessibilityNodeObject* toAccessibilityNodeObject(AccessibilityObject* object)
{
    ASSERT(!object || object->isAccessibilityNodeObject());
    return static_cast<AccessibilityNodeObject*>(object);
}

inline const AccessibilityNodeObject* toAccessibilityNodeObject(const AccessibilityObject* object)
{
    ASSERT(!object || object->isAccessibilityNodeObject());
    return static_cast<const AccessibilityNodeObject*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toAccessibilityNodeObject(const AccessibilityNodeObject*);

} // namespace WebCore

#endif // AccessibilityNodeObject_h

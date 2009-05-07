/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLOptGroupElement_h
#define HTMLOptGroupElement_h

#include "HTMLFormControlElement.h"
#include "OptionGroupElement.h"

namespace WebCore {
    
class HTMLSelectElement;

class HTMLOptGroupElement : public HTMLFormControlElement, public OptionGroupElement {
public:
    HTMLOptGroupElement(const QualifiedName&, Document*, HTMLFormElement* = 0);

    virtual bool checkDTD(const Node*);
    virtual const AtomicString& formControlType() const;
    virtual bool isFocusable() const;
    virtual void parseMappedAttribute(MappedAttribute*);
    virtual bool rendererIsNeeded(RenderStyle*) { return false; }
    virtual void attach();
    virtual void detach();
    virtual void setRenderStyle(PassRefPtr<RenderStyle>);

    virtual bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool removeChild(Node* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool removeChildren();
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    String label() const;
    void setLabel(const String&);
    
    virtual String groupLabelText() const;
    HTMLSelectElement* ownerSelectElement() const;
    virtual void accessKeyAction(bool sendToAnyElement);
    
private:
    virtual RenderStyle* nonRendererRenderStyle() const;
    
    void recalcSelectOptions();

    RefPtr<RenderStyle> m_style;
};

} //namespace

#endif

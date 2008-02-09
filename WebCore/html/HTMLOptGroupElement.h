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

#include "HTMLGenericFormElement.h"

namespace WebCore {

class HTMLOptGroupElement : public HTMLGenericFormElement {
public:
    HTMLOptGroupElement(Document*, HTMLFormElement* = 0);

    virtual bool checkDTD(const Node*);
    virtual const AtomicString& type() const;
    virtual bool isFocusable() const;
    virtual void parseMappedAttribute(MappedAttribute*);
    virtual bool rendererIsNeeded(RenderStyle*) { return false; }
    virtual void attach();
    virtual void detach();
    virtual RenderStyle* renderStyle() const { return m_style; }
    virtual void setRenderStyle(RenderStyle*);

    virtual bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&);
    virtual bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&);
    virtual bool removeChild(Node* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&);
    virtual bool removeChildren();
    virtual void childrenChanged(bool changedByParser = false);

    String label() const;
    void setLabel(const String&);
    
    String groupLabelText() const;
    
private:
    void recalcSelectOptions();

    RenderStyle* m_style;
};

} //namespace

#endif

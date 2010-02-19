/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#ifndef HTMLOptionElement_h
#define HTMLOptionElement_h

#include "HTMLFormControlElement.h"
#include "OptionElement.h"

namespace WebCore {

class HTMLSelectElement;
class HTMLFormElement;
class MappedAttribute;

class HTMLOptionElement : public HTMLFormControlElement, public OptionElement {
    friend class HTMLSelectElement;
    friend class RenderMenuList;

public:
    HTMLOptionElement(const QualifiedName&, Document*, HTMLFormElement* = 0);

    static PassRefPtr<HTMLOptionElement> createForJSConstructor(Document*, const String& data, const String& value,
       bool defaultSelected, bool selected, ExceptionCode&);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 2; }
    virtual bool checkDTD(const Node* newChild);
    virtual bool supportsFocus() const;
    virtual bool isFocusable() const;
    virtual bool rendererIsNeeded(RenderStyle*) { return false; }
    virtual void attach();
    virtual void detach();
    virtual void setRenderStyle(PassRefPtr<RenderStyle>);

    virtual const AtomicString& formControlType() const;

    virtual String text() const;
    void setText(const String&, ExceptionCode&);

    int index() const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual String value() const;
    void setValue(const String&);

    virtual bool selected() const;
    void setSelected(bool);
    virtual void setSelectedState(bool);

    HTMLSelectElement* ownerSelectElement() const;

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    bool defaultSelected() const;
    void setDefaultSelected(bool);

    String label() const;
    void setLabel(const String&);

    virtual String textIndentedToRespectGroupLabel() const;

    bool ownElementDisabled() const { return HTMLFormControlElement::disabled(); }
    virtual bool disabled() const;

    virtual void insertedIntoTree(bool);
    virtual void accessKeyAction(bool);

private:
    virtual RenderStyle* nonRendererRenderStyle() const;

    OptionElementData m_data;
    RefPtr<RenderStyle> m_style;
};

} //namespace

#endif

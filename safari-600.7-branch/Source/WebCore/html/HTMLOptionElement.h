/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010, 2011 Apple Inc. All rights reserved.
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

#include "HTMLElement.h"

namespace WebCore {

class HTMLDataListElement;
class HTMLSelectElement;

class HTMLOptionElement final : public HTMLElement {
public:
    static PassRefPtr<HTMLOptionElement> create(Document&);
    static PassRefPtr<HTMLOptionElement> create(const QualifiedName&, Document&);
    static PassRefPtr<HTMLOptionElement> createForJSConstructor(Document&, const String& data, const String& value,
       bool defaultSelected, bool selected, ExceptionCode&);

    virtual String text() const;
    void setText(const String&, ExceptionCode&);

    int index() const;

    String value() const;
    void setValue(const String&);

    bool selected();
    void setSelected(bool);

#if ENABLE(DATALIST_ELEMENT)
    HTMLDataListElement* ownerDataListElement() const;
#endif
    HTMLSelectElement* ownerSelectElement() const;

    String label() const;
    void setLabel(const String&);

    bool ownElementDisabled() const { return m_disabled; }

    virtual bool isDisabledFormControl() const override;

    String textIndentedToRespectGroupLabel() const;

    void setSelectedState(bool);

private:
    HTMLOptionElement(const QualifiedName&, Document&);

    virtual bool isFocusable() const override;
    virtual bool rendererIsNeeded(const RenderStyle&) override { return false; }
    virtual void didAttachRenderers() override;
    virtual void willDetachRenderers() override;

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void accessKeyAction(bool) override;

    virtual void childrenChanged(const ChildChange&) override;

    // <option> never has a renderer so we manually manage a cached style.
    void updateNonRenderStyle(RenderStyle& parentStyle);
    virtual RenderStyle* nonRendererStyle() const override;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer(RenderStyle& parentStyle) override;

    virtual void didRecalcStyle(Style::Change) override;

    String collectOptionInnerText() const;

    bool m_disabled;
    bool m_isSelected;
    RefPtr<RenderStyle> m_style;
};

NODE_TYPE_CASTS(HTMLOptionElement)

} // namespace

#endif

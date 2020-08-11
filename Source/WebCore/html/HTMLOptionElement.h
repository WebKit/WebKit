/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "HTMLElement.h"

namespace WebCore {

class HTMLSelectElement;

class HTMLOptionElement final : public HTMLElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLOptionElement);
public:
    static Ref<HTMLOptionElement> create(Document&);
    static Ref<HTMLOptionElement> create(const QualifiedName&, Document&);
    static ExceptionOr<Ref<HTMLOptionElement>> createForJSConstructor(Document&, const String& text, const String& value, bool defaultSelected, bool selected);

    WEBCORE_EXPORT String text() const;
    void setText(const String&);

    WEBCORE_EXPORT int index() const;

    WEBCORE_EXPORT String value() const;
    WEBCORE_EXPORT void setValue(const String&);

    WEBCORE_EXPORT bool selected() const;
    WEBCORE_EXPORT void setSelected(bool);

    WEBCORE_EXPORT HTMLSelectElement* ownerSelectElement() const;

    WEBCORE_EXPORT String label() const;
    String displayLabel() const;
    WEBCORE_EXPORT void setLabel(const String&);

    bool ownElementDisabled() const { return m_disabled; }

    WEBCORE_EXPORT bool isDisabledFormControl() const final;

    String textIndentedToRespectGroupLabel() const;

    void setSelectedState(bool);

private:
    HTMLOptionElement(const QualifiedName&, Document&);

    bool isFocusable() const final;
    bool rendererIsNeeded(const RenderStyle&) final { return false; }
    bool matchesDefaultPseudoClass() const final;

    void parseAttribute(const QualifiedName&, const AtomString&) final;

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    bool accessKeyAction(bool) final;

    void childrenChanged(const ChildChange&) final;

    void willResetComputedStyle() final;

    String collectOptionInnerText() const;

    bool m_disabled;
    bool m_isSelected;
};

} // namespace

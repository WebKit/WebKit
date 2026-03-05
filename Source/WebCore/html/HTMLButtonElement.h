/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010, 2016 Apple Inc. All rights reserved.
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

#include <WebCore/HTMLFormControlElement.h>

namespace WebCore {

class RenderButton;

class HTMLButtonElement final : public HTMLFormControlElement {
    WTF_MAKE_TZONE_ALLOCATED(HTMLButtonElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLButtonElement);
public:
    static Ref<HTMLButtonElement> create(const QualifiedName&, Document&, HTMLFormElement*);
    static Ref<HTMLButtonElement> create(Document&);
    
    const AtomString& value() const;
    const AtomString& command() const;

    RefPtr<Element> commandForElement() const;
    CommandType commandType() const;

    bool willRespondToMouseClickEventsWithEditability(Editability) const final;

    RenderButton* renderer() const;

    bool isExplicitlySetSubmitButton() const;

    bool isDevolvableWidget() const override { return true; }

private:
    HTMLButtonElement(const QualifiedName& tagName, Document&, HTMLFormElement*);

    enum class Type : uint8_t { Submit, Reset, Button };

    const AtomString& formControlType() const final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    int defaultTabIndex() const final;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode& parentOfInsertedTree) final;
    void removedFromAncestor(RemovalType, ContainerNode& oldParentOfRemovedTree) final;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const final;
    void defaultEventHandler(Event&) final;

    void handleCommand();

    bool appendFormData(DOMFormData&) final;

    bool NODELETE isEnumeratable() const final { return true; }
    bool isLabelable() const final { return true; }
    bool isInteractiveContent() const final { return true; }

    bool isSuccessfulSubmitButton() const final;
    bool matchesDefaultPseudoClass() const final;
    bool isActivatedSubmit() const final;
    void setActivatedSubmit(bool flag) final;

    bool NODELETE isURLAttribute(const Attribute&) const final;

    bool canStartSelection() const final { return false; }

    bool NODELETE isOptionalFormControl() const final { return true; }
    bool computeWillValidate() const final;

    bool isSubmitButton() const final;

    void computeType(const AtomString& typeAttrValue);

    Type m_type;
    bool m_isActivatedSubmit;
};

} // namespace

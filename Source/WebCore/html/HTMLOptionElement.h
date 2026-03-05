/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2026 Apple Inc. All rights reserved.
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

#include <WebCore/HTMLElement.h>

namespace WebCore {

class HTMLSelectElement;
class HTMLSlotElement;
class HTMLSpanElement;

enum class AllowStyleInvalidation : bool { No, Yes };

class HTMLOptionElement final : public HTMLElement {
    WTF_MAKE_TZONE_ALLOCATED(HTMLOptionElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLOptionElement);
public:
    static Ref<HTMLOptionElement> create(Document&);
    static Ref<HTMLOptionElement> create(const QualifiedName&, Document&);
    static ExceptionOr<Ref<HTMLOptionElement>> createForLegacyFactoryFunction(Document&, String&& text, const AtomString& value, bool defaultSelected, bool selected);

    void finishParsingChildren() final;

    WEBCORE_EXPORT String text() const;
    void setText(String&&);

    WEBCORE_EXPORT HTMLFormElement* form() const;
    RefPtr<HTMLFormElement> formForBindings() const;

    WEBCORE_EXPORT int index() const;

    WEBCORE_EXPORT String value() const;

    WEBCORE_EXPORT bool selected(AllowStyleInvalidation = AllowStyleInvalidation::Yes) const;
    WEBCORE_EXPORT void setSelected(bool);

    WEBCORE_EXPORT HTMLSelectElement* NODELETE ownerSelectElement() const;

    WEBCORE_EXPORT String label() const;
    WEBCORE_EXPORT String displayLabel() const;

    bool ownElementDisabled() const { return m_disabled; }

    WEBCORE_EXPORT bool isDisabledFormControl() const final;

    String textIndentedToRespectGroupLabel() const;

    void setSelectedState(bool, AllowStyleInvalidation = AllowStyleInvalidation::Yes);
    bool selectedWithoutUpdate() const { return m_isSelected; }

    void cloneIntoSelectedContent(HTMLSelectedContentElement&);

    void updateUserAgentShadowTree() final;

private:
    HTMLOptionElement(const QualifiedName&, Document&);

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode& oldParentOfRemovedTree) final;

    bool supportsFocus() const final;
    bool isFocusable() const final;
    bool matchesDefaultPseudoClass() const final { return m_isDefault; }

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

    void didAddUserAgentShadowRoot(ShadowRoot&) final;

    bool accessKeyAction(bool) final;

    void defaultEventHandler(Event&) final;

    void childrenChanged(const ChildChange&) final;

    void willResetComputedStyle() final;

    String collectOptionInnerText() const;
    String collectOptionInnerTextCollapsingWhitespace() const;

    void invalidateShadowTree();

    bool m_disabled { false };
    bool m_isSelected { false };
    bool m_isDefault { false };
    bool m_shadowTreeNeedsUpdate { false };
    WeakPtr<HTMLSelectElement, WeakPtrImplWithEventTargetData> m_ownerSelect;
    WeakPtr<HTMLSpanElement, WeakPtrImplWithEventTargetData> m_labelContainer;
    WeakPtr<HTMLSlotElement, WeakPtrImplWithEventTargetData> m_slot;
};

} // namespace

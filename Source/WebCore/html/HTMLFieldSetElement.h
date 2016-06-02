/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#include "HTMLFormControlElement.h"
#include <wtf/HashSet.h>

namespace WebCore {

class FormAssociatedElement;
class HTMLFormControlsCollection;
class RenderFieldSet;

class HTMLFieldSetElement final : public HTMLFormControlElement {
public:
    static Ref<HTMLFieldSetElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    HTMLLegendElement* legend() const;

    Ref<HTMLFormControlsCollection> elements();
    Ref<HTMLCollection> elementsForNativeBindings();

    const Vector<FormAssociatedElement*>& associatedElements() const;
    unsigned length() const;

    void addInvalidDescendant(const HTMLFormControlElement&);
    void removeInvalidDescendant(const HTMLFormControlElement&);

    RenderFieldSet* renderer() const;

private:
    HTMLFieldSetElement(const QualifiedName&, Document&, HTMLFormElement*);
    ~HTMLFieldSetElement();

    bool isEnumeratable() const override { return true; }
    bool supportsFocus() const override;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    const AtomicString& formControlType() const override;
    bool computeWillValidate() const override { return false; }
    void disabledAttributeChanged() override;
    void disabledStateChanged() override;
    void childrenChanged(const ChildChange&) override;
    void didMoveToNewDocument(Document* oldDocument) override;

    bool matchesValidPseudoClass() const override;
    bool matchesInvalidPseudoClass() const override;

    void updateAssociatedElements() const;

    mutable Vector<FormAssociatedElement*> m_associatedElements;
    // When the DOM tree is modified, we have to refresh the m_associatedElements array.
    mutable uint64_t m_documentVersion { 0 };
    HashSet<const HTMLFormControlElement*> m_invalidDescendants;
    bool m_hasDisabledAttribute { false };
};

} // namespace

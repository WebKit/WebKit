/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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

#include "FormListedElement.h"
#include "HTMLPlugInImageElement.h"

namespace WebCore {

class HTMLFormElement;

class HTMLObjectElement final : public HTMLPlugInImageElement, public FormListedElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLObjectElement);
public:
    static Ref<HTMLObjectElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    bool isExposed() const { return m_isExposed; }
    bool containsJavaApplet() const;

    bool hasFallbackContent() const;
    bool useFallbackContent() const final { return m_useFallbackContent; }
    void renderFallbackContent();

    bool willValidate() const final { return false; }

    // Implementation of constraint validation API.
    // Note that the object elements are always barred from constraint validation.
    static bool checkValidity() { return true; }
    static bool reportValidity() { return true; }

    void setCustomValidity(const String&) final { }
    String validationMessage() const final { return String(); }

    using HTMLPlugInImageElement::ref;
    using HTMLPlugInImageElement::deref;

    HTMLFormElement* form() const final { return FormListedElement::form(); }

private:
    HTMLObjectElement(const QualifiedName&, Document&, HTMLFormElement*);
    ~HTMLObjectElement();

    int defaultTabIndex() const final;

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const final;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void didFinishInsertingNode() final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;

    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;

    void childrenChanged(const ChildChange&) final;

    bool isURLAttribute(const Attribute&) const final;
    const AtomString& imageSourceURL() const final;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    void updateWidget(CreatePlugins) final;
    void updateExposedState();

    // FIXME: This function should not deal with url or serviceType
    // so that we can better share code between <object> and <embed>.
    void parametersForPlugin(Vector<AtomString>& paramNames, Vector<AtomString>& paramValues, String& url, String& serviceType);

    bool hasValidClassId();

    void refFormAssociatedElement() final { ref(); }
    void derefFormAssociatedElement() final { deref(); }

    FormAssociatedElement* asFormAssociatedElement() final { return this; }
    FormListedElement* asFormListedElement() final { return this; }

    // These functions can be called concurrently for ValidityState.
    HTMLObjectElement& asHTMLElement() final { return *this; }
    const HTMLObjectElement& asHTMLElement() const final { return *this; }

    bool isFormControlElement() const final { return false; }

    bool isEnumeratable() const final { return true; }

    bool canContainRangeEndPoint() const final;

    bool m_isExposed { true };
    bool m_useFallbackContent { false };
};

} // namespace WebCore

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLObjectElement_h
#define HTMLObjectElement_h

#include "FormAssociatedElement.h"
#include "HTMLPlugInImageElement.h"

namespace WebCore {

class HTMLFormElement;

class HTMLObjectElement final : public HTMLPlugInImageElement, public FormAssociatedElement {
public:
    static PassRefPtr<HTMLObjectElement> create(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);
    virtual ~HTMLObjectElement();

    bool isDocNamedItem() const { return m_docNamedItem; }
    bool containsJavaApplet() const;

    bool hasFallbackContent() const;
    virtual bool useFallbackContent() const override { return m_useFallbackContent; }
    void renderFallbackContent();

    // Implementation of constraint validation API.
    // Note that the object elements are always barred from constraint validation.
    static bool checkValidity() { return true; }
    virtual void setCustomValidity(const String&) override { }
    virtual String validationMessage() const override { return String(); }

    using HTMLPlugInImageElement::ref;
    using HTMLPlugInImageElement::deref;

    using FormAssociatedElement::form;

private:
    HTMLObjectElement(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;

    virtual void didMoveToNewDocument(Document* oldDocument) override;

    virtual void childrenChanged(const ChildChange&) override;

    virtual bool isURLAttribute(const Attribute&) const override;
    virtual const AtomicString& imageSourceURL() const override;

    virtual RenderWidget* renderWidgetForJSBindings() const override;

    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    virtual void updateWidget(PluginCreationOption) override;
    void updateDocNamedItem();

    // FIXME: This function should not deal with url or serviceType
    // so that we can better share code between <object> and <embed>.
    void parametersForPlugin(Vector<String>& paramNames, Vector<String>& paramValues, String& url, String& serviceType);
    
    bool shouldAllowQuickTimeClassIdQuirk();
    bool hasValidClassId();

    virtual void refFormAssociatedElement() override { ref(); }
    virtual void derefFormAssociatedElement() override { deref(); }
    virtual HTMLFormElement* virtualForm() const override;

    virtual FormNamedItem* asFormNamedItem() override final { return this; }
    virtual HTMLObjectElement& asHTMLElement() override final { return *this; }
    virtual const HTMLObjectElement& asHTMLElement() const override final { return *this; }

    virtual bool isFormControlElement() const override { return false; }

    virtual bool isEnumeratable() const override { return true; }
    virtual bool appendFormData(FormDataList&, bool) override;

    virtual bool canContainRangeEndPoint() const override;

    bool m_docNamedItem : 1;
    bool m_useFallbackContent : 1;
};

NODE_TYPE_CASTS(HTMLObjectElement)

}

#endif

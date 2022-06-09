/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "SVGElement.h"
#include "SVGURIReference.h"
#include "ScriptElement.h"
#include "XLinkNames.h"

namespace WebCore {

class SVGScriptElement final : public SVGElement, public SVGURIReference, public ScriptElement {
    WTF_MAKE_ISO_ALLOCATED(SVGScriptElement);
public:
    static Ref<SVGScriptElement> create(const QualifiedName&, Document&, bool wasInsertedByParser);

    using SVGElement::ref;
    using SVGElement::deref;

private:
    SVGScriptElement(const QualifiedName&, Document&, bool wasInsertedByParser, bool alreadyStarted);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGScriptElement, SVGElement, SVGURIReference>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void didFinishInsertingNode() final;
    void childrenChanged(const ChildChange&) final;

    bool isURLAttribute(const Attribute& attribute) const final { return attribute.name() == sourceAttributeValue(); }
    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    Ref<Element> cloneElementWithoutAttributesAndChildren(Document&) final;
    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    // ScriptElement
    String sourceAttributeValue() const final { return href(); }
    String charsetAttributeValue() const final { return String(); }
    String typeAttributeValue() const final { return getAttribute(SVGNames::typeAttr).string(); }
    String languageAttributeValue() const final { return String(); }
    String forAttributeValue() const final { return String(); }
    String eventAttributeValue() const final { return String(); }
    bool hasAsyncAttribute() const final { return false; }
    bool hasDeferAttribute() const final { return false; }
    bool hasNoModuleAttribute() const final { return false; }
    ReferrerPolicy referrerPolicy() const final { return ReferrerPolicy::EmptyString; }
    bool hasSourceAttribute() const final { return hasAttribute(SVGNames::hrefAttr) || hasAttribute(XLinkNames::hrefAttr); }
    void dispatchLoadEvent() final { SVGURIReference::dispatchLoadEvent(); }
    void dispatchErrorEvent() final;

    // SVGElement
    bool haveLoadedRequiredResources() final { return SVGURIReference::haveLoadedRequiredResources(); }
    Timer* loadEventTimer() final { return &m_loadEventTimer; }

    // SVGURIReference
    bool haveFiredLoadEvent() const final { return ScriptElement::haveFiredLoadEvent(); }
    void setHaveFiredLoadEvent(bool haveFiredLoadEvent) final { ScriptElement::setHaveFiredLoadEvent(haveFiredLoadEvent); }
    bool errorOccurred() const final { return ScriptElement::errorOccurred(); }
    void setErrorOccurred(bool errorOccurred) final { ScriptElement::setErrorOccurred(errorOccurred); }

#ifndef NDEBUG
    bool filterOutAnimatableAttribute(const QualifiedName& name) const final { return name == SVGNames::typeAttr; }
#endif

    PropertyRegistry m_propertyRegistry { *this };
    Timer m_loadEventTimer;
};

} // namespace WebCore

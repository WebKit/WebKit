/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedString.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGURIReference.h"
#include "ScriptElement.h"

namespace WebCore {

class SVGScriptElement final : public SVGElement, public SVGURIReference, public SVGExternalResourcesRequired, public ScriptElement {
public:
    static Ref<SVGScriptElement> create(const QualifiedName&, Document&, bool wasInsertedByParser);

    using SVGElement::ref;
    using SVGElement::deref;

private:
    SVGScriptElement(const QualifiedName&, Document&, bool wasInsertedByParser, bool alreadyStarted);

    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void didFinishInsertingNode() final;
    void childrenChanged(const ChildChange&) final;

    void svgAttributeChanged(const QualifiedName&) final;
    bool isURLAttribute(const Attribute&) const final;
    void finishParsingChildren() final;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    bool haveLoadedRequiredResources() final { return SVGExternalResourcesRequired::haveLoadedRequiredResources(); }

    String sourceAttributeValue() const final;
    String charsetAttributeValue() const final;
    String typeAttributeValue() const final;
    String languageAttributeValue() const final;
    String forAttributeValue() const final;
    String eventAttributeValue() const final;
    bool hasAsyncAttribute() const final;
    bool hasDeferAttribute() const final;
    bool hasNoModuleAttribute() const final;
    bool hasSourceAttribute() const final;

    void dispatchLoadEvent() final { SVGExternalResourcesRequired::dispatchLoadEvent(this); }

    Ref<Element> cloneElementWithoutAttributesAndChildren(Document&) final;
    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    // SVGExternalResourcesRequired
    void setHaveFiredLoadEvent(bool haveFiredLoadEvent) final { ScriptElement::setHaveFiredLoadEvent(haveFiredLoadEvent); }
    bool isParserInserted() const final { return ScriptElement::isParserInserted(); }
    bool haveFiredLoadEvent() const final { return ScriptElement::haveFiredLoadEvent(); }
    Timer* svgLoadEventTimer() final { return &m_svgLoadEventTimer; }

#ifndef NDEBUG
    bool filterOutAnimatableAttribute(const QualifiedName&) const final;
#endif

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGScriptElement)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    Timer m_svgLoadEventTimer;
};

} // namespace WebCore

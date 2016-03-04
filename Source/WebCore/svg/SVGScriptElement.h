/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGScriptElement_h
#define SVGScriptElement_h

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedString.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGURIReference.h"
#include "ScriptElement.h"

namespace WebCore {

class SVGScriptElement final : public SVGElement
                             , public SVGURIReference
                             , public SVGExternalResourcesRequired
                             , public ScriptElement {
public:
    static Ref<SVGScriptElement> create(const QualifiedName&, Document&, bool wasInsertedByParser);

private:
    SVGScriptElement(const QualifiedName&, Document&, bool wasInsertedByParser, bool alreadyStarted);

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    InsertionNotificationRequest insertedInto(ContainerNode&) override;
    void finishedInsertingSubtree() override;
    void childrenChanged(const ChildChange&) override;

    void svgAttributeChanged(const QualifiedName&) override;
    bool isURLAttribute(const Attribute&) const override;
    void finishParsingChildren() override;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    bool haveLoadedRequiredResources() override { return SVGExternalResourcesRequired::haveLoadedRequiredResources(); }

    String sourceAttributeValue() const override;
    String charsetAttributeValue() const override;
    String typeAttributeValue() const override;
    String languageAttributeValue() const override;
    String forAttributeValue() const override;
    String eventAttributeValue() const override;
    bool asyncAttributeValue() const override;
    bool deferAttributeValue() const override;
    bool hasSourceAttribute() const override;

    void dispatchLoadEvent() override { SVGExternalResourcesRequired::dispatchLoadEvent(this); }

    Ref<Element> cloneElementWithoutAttributesAndChildren(Document&) override;
    bool rendererIsNeeded(const RenderStyle&) override { return false; }

    // SVGExternalResourcesRequired
    void setHaveFiredLoadEvent(bool haveFiredLoadEvent) override { ScriptElement::setHaveFiredLoadEvent(haveFiredLoadEvent); }
    bool isParserInserted() const override { return ScriptElement::isParserInserted(); }
    bool haveFiredLoadEvent() const override { return ScriptElement::haveFiredLoadEvent(); }
    Timer* svgLoadEventTimer() override { return &m_svgLoadEventTimer; }

#ifndef NDEBUG
    bool filterOutAnimatableAttribute(const QualifiedName&) const override;
#endif

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGScriptElement)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    Timer m_svgLoadEventTimer;
};

} // namespace WebCore

#endif

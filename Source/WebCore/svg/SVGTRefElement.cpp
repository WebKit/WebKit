/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "config.h"

#if ENABLE(SVG)
#include "SVGTRefElement.h"

#include "EventListener.h"
#include "EventNames.h"
#include "MutationEvent.h"
#include "RenderSVGInline.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGResource.h"
#include "ShadowRoot.h"
#include "SVGDocument.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "Text.h"
#include "XLinkNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGTRefElement, XLinkNames::hrefAttr, Href, href)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGTRefElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTextPositioningElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGTRefElement::SVGTRefElement(const QualifiedName& tagName, Document* document)
    : SVGTextPositioningElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::trefTag));
    registerAnimatedPropertiesForSVGTRefElement();
}

PassRefPtr<SVGTRefElement> SVGTRefElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGTRefElement(tagName, document));
}

class SubtreeModificationEventListener : public EventListener {
public:
    static PassRefPtr<SubtreeModificationEventListener> create(SVGTRefElement* trefElement, String targetId)
    {
        return adoptRef(new SubtreeModificationEventListener(trefElement, targetId));
    }

    static const SubtreeModificationEventListener* cast(const EventListener* listener)
    {
        return listener->type() == CPPEventListenerType ? static_cast<const SubtreeModificationEventListener*>(listener) : 0;
    }

    virtual bool operator==(const EventListener&);

    void clear()
    {
        Element* target = m_trefElement->treeScope()->getElementById(m_targetId);
        if (target && target->parentNode())
            target->parentNode()->removeEventListener(eventNames().DOMSubtreeModifiedEvent, this, false);
        
        m_trefElement = 0;
        m_targetId = String();
    }

private:
    SubtreeModificationEventListener(SVGTRefElement* trefElement, String targetId)
        : EventListener(CPPEventListenerType)
        , m_trefElement(trefElement)
        , m_targetId(targetId)
    {
    }

    virtual void handleEvent(ScriptExecutionContext*, Event*);

    SVGTRefElement* m_trefElement;
    String m_targetId;
};

bool SubtreeModificationEventListener::operator==(const EventListener& listener)
{
    if (const SubtreeModificationEventListener* subtreeModificationEventListener = SubtreeModificationEventListener::cast(&listener))
        return m_trefElement == subtreeModificationEventListener->m_trefElement;
    return false;
}

void SubtreeModificationEventListener::handleEvent(ScriptExecutionContext*, Event* event)
{
    if (m_trefElement && event->type() == eventNames().DOMSubtreeModifiedEvent && m_trefElement != event->target())
        m_trefElement->updateReferencedText();
}

class SVGShadowText : public Text {
public:
    static PassRefPtr<SVGShadowText> create(Document* document, const String& data)
    {
        return adoptRef(new SVGShadowText(document, data));
    }
private:
    SVGShadowText(Document* document, const String& data)
        : Text(document, data)
    {
         setHasCustomWillOrDidRecalcStyle();
    }
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void willRecalcTextStyle(StyleChange);
};

RenderObject* SVGShadowText::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGInlineText(this, dataImpl());
}

void SVGShadowText::willRecalcTextStyle(StyleChange change)
{
    if (change != NoChange && parentNode()->shadowHost()) {
        if (renderer())
            renderer()->setStyle(parentNode()->shadowHost()->renderer()->style());
    }
}

SVGTRefElement::~SVGTRefElement()
{
    clearEventListener();
}

void SVGTRefElement::updateReferencedText()
{
    Element* target = SVGURIReference::targetElementFromIRIString(href(), document());
    ASSERT(target);
    String textContent;
    if (target->parentNode())
        textContent = target->textContent();
    ExceptionCode ignore = 0;
    if (!ensureShadowRoot()->firstChild())
        shadowRoot()->appendChild(SVGShadowText::create(document(), textContent), ignore);
    else
        shadowRoot()->firstChild()->setTextContent(textContent, ignore);
}

bool SVGTRefElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        SVGURIReference::addSupportedAttributes(supportedAttributes);
    return supportedAttributes.contains<QualifiedName, SVGAttributeHashTranslator>(attrName);
}

void SVGTRefElement::parseAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGTextPositioningElement::parseAttribute(attr);
        return;
    }

    if (SVGURIReference::parseAttribute(attr)) {
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGTRefElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGTextPositioningElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (SVGURIReference::isKnownAttribute(attrName)) {
        buildPendingResource();
        if (RenderObject* renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

RenderObject* SVGTRefElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGInline(this);
}

bool SVGTRefElement::childShouldCreateRenderer(Node* child) const
{
    return child->isInShadowTree();
}

bool SVGTRefElement::rendererIsNeeded(const NodeRenderingContext& context)
{
    if (parentNode()
        && (parentNode()->hasTagName(SVGNames::aTag)
#if ENABLE(SVG_FONTS)
            || parentNode()->hasTagName(SVGNames::altGlyphTag)
#endif
            || parentNode()->hasTagName(SVGNames::textTag)
            || parentNode()->hasTagName(SVGNames::textPathTag)
            || parentNode()->hasTagName(SVGNames::tspanTag)))
        return StyledElement::rendererIsNeeded(context);

    return false;
}

void SVGTRefElement::buildPendingResource()
{
    // Remove any existing event listener.
    clearEventListener();

    String id;
    Element* target = SVGURIReference::targetElementFromIRIString(href(), document(), &id);
    if (!target) {
        if (hasPendingResources() || id.isEmpty())
            return;

        ASSERT(!hasPendingResources());
        document()->accessSVGExtensions()->addPendingResource(id, this);
        ASSERT(hasPendingResources());
        return;
    }

    updateReferencedText();

    // We should not add the event listener if we are not in document yet.
    if (!inDocument())
        return;

    m_eventListener = SubtreeModificationEventListener::create(this, id);
    ASSERT(target->parentNode());
    target->parentNode()->addEventListener(eventNames().DOMSubtreeModifiedEvent, m_eventListener.get(), false);
}

void SVGTRefElement::insertedIntoDocument()
{
    SVGStyledElement::insertedIntoDocument();
    buildPendingResource();
}

void SVGTRefElement::removedFromDocument()
{
    SVGStyledElement::removedFromDocument();
    clearEventListener();
}

void SVGTRefElement::clearEventListener()
{
    if (m_eventListener) {
        m_eventListener->clear();
        m_eventListener = 0;
    }
}

}

#endif // ENABLE(SVG)

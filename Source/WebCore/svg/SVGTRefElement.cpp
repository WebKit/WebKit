/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGTRefElement.h"

#include "EventListener.h"
#include "EventNames.h"
#include "MutationEvent.h"
#include "RenderSVGInline.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGResource.h"
#include "ShadowRoot.h"
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#include "SVGNames.h"
#include "ScriptDisallowedScope.h"
#include "StyleInheritedData.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTRefElement);

Ref<SVGTRefElement> SVGTRefElement::create(const QualifiedName& tagName, Document& document)
{
    Ref<SVGTRefElement> element = adoptRef(*new SVGTRefElement(tagName, document));
    element->ensureUserAgentShadowRoot();
    return element;
}

class SVGTRefTargetEventListener final : public EventListener {
public:
    static Ref<SVGTRefTargetEventListener> create(SVGTRefElement& trefElement)
    {
        return adoptRef(*new SVGTRefTargetEventListener(trefElement));
    }

    static const SVGTRefTargetEventListener* cast(const EventListener* listener)
    {
        return listener->type() == SVGTRefTargetEventListenerType ? static_cast<const SVGTRefTargetEventListener*>(listener) : nullptr;
    }

    void attach(RefPtr<Element>&& target);
    void detach();
    bool isAttached() const { return m_target.get(); }

private:
    explicit SVGTRefTargetEventListener(SVGTRefElement& trefElement);

    void handleEvent(ScriptExecutionContext&, Event&) final;
    bool operator==(const EventListener&) const final;

    SVGTRefElement& m_trefElement;
    RefPtr<Element> m_target;
};

SVGTRefTargetEventListener::SVGTRefTargetEventListener(SVGTRefElement& trefElement)
    : EventListener(SVGTRefTargetEventListenerType)
    , m_trefElement(trefElement)
    , m_target(nullptr)
{
}

void SVGTRefTargetEventListener::attach(RefPtr<Element>&& target)
{
    ASSERT(!isAttached());
    ASSERT(target.get());
    ASSERT(target->isConnected());

    target->addEventListener(eventNames().DOMSubtreeModifiedEvent, *this, false);
    target->addEventListener(eventNames().DOMNodeRemovedFromDocumentEvent, *this, false);
    m_target = WTFMove(target);
}

void SVGTRefTargetEventListener::detach()
{
    if (!isAttached())
        return;

    m_target->removeEventListener(eventNames().DOMSubtreeModifiedEvent, *this, false);
    m_target->removeEventListener(eventNames().DOMNodeRemovedFromDocumentEvent, *this, false);
    m_target = nullptr;
}

bool SVGTRefTargetEventListener::operator==(const EventListener& listener) const
{
    if (const SVGTRefTargetEventListener* targetListener = SVGTRefTargetEventListener::cast(&listener))
        return &m_trefElement == &targetListener->m_trefElement;
    return false;
}

void SVGTRefTargetEventListener::handleEvent(ScriptExecutionContext&, Event& event)
{
    if (!isAttached())
        return;

    if (event.type() == eventNames().DOMSubtreeModifiedEvent && &m_trefElement != event.target())
        m_trefElement.updateReferencedText(m_target.get());
    else if (event.type() == eventNames().DOMNodeRemovedFromDocumentEvent)
        m_trefElement.detachTarget();
}

inline SVGTRefElement::SVGTRefElement(const QualifiedName& tagName, Document& document)
    : SVGTextPositioningElement(tagName, document)
    , SVGURIReference(this)
    , m_targetListener(SVGTRefTargetEventListener::create(*this))
{
    ASSERT(hasTagName(SVGNames::trefTag));
}

SVGTRefElement::~SVGTRefElement()
{
    m_targetListener->detach();
}

void SVGTRefElement::updateReferencedText(Element* target)
{
    String textContent;
    if (target)
        textContent = target->textContent();

    auto root = userAgentShadowRoot();
    ASSERT(root);
    ScriptDisallowedScope::EventAllowedScope allowedScope(*root);
    if (!root->firstChild())
        root->appendChild(Text::create(document(), textContent));
    else {
        ASSERT(root->firstChild()->isTextNode());
        root->firstChild()->setTextContent(textContent);
    }
}

void SVGTRefElement::detachTarget()
{
    // Remove active listeners and clear the text content.
    m_targetListener->detach();

    String emptyContent;

    ASSERT(shadowRoot());
    auto container = makeRefPtr(shadowRoot()->firstChild());
    if (container)
        container->setTextContent(emptyContent);

    if (!isConnected())
        return;

    // Mark the referenced ID as pending.
    auto target = SVGURIReference::targetElementFromIRIString(href(), document());
    if (!target.identifier.isEmpty())
        document().accessSVGExtensions().addPendingResource(target.identifier, *this);
}

void SVGTRefElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGTextPositioningElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
}

void SVGTRefElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (SVGURIReference::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        buildPendingResource();
        if (auto renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    SVGTextPositioningElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGTRefElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGInline>(*this, WTFMove(style));
}

bool SVGTRefElement::childShouldCreateRenderer(const Node& child) const
{
    return child.isInShadowTree();
}

bool SVGTRefElement::rendererIsNeeded(const RenderStyle& style)
{
    if (parentNode()
        && (parentNode()->hasTagName(SVGNames::aTag)
#if ENABLE(SVG_FONTS)
            || parentNode()->hasTagName(SVGNames::altGlyphTag)
#endif
            || parentNode()->hasTagName(SVGNames::textTag)
            || parentNode()->hasTagName(SVGNames::textPathTag)
            || parentNode()->hasTagName(SVGNames::tspanTag)))
        return StyledElement::rendererIsNeeded(style);

    return false;
}

void SVGTRefElement::clearTarget()
{
    m_targetListener->detach();
}

void SVGTRefElement::buildPendingResource()
{
    // Remove any existing event listener.
    m_targetListener->detach();

    // If we're not yet in a document, this function will be called again from insertedIntoAncestor().
    if (!isConnected())
        return;

    auto target = SVGURIReference::targetElementFromIRIString(href(), treeScope());
    if (!target.element) {
        if (target.identifier.isEmpty())
            return;

        document().accessSVGExtensions().addPendingResource(target.identifier, *this);
        ASSERT(hasPendingResources());
        return;
    }

    // Don't set up event listeners if this is a shadow tree node.
    // SVGUseElement::transferEventListenersToShadowTree() handles this task, and addEventListener()
    // expects every element instance to have an associated shadow tree element - which is not the
    // case when we land here from SVGUseElement::buildShadowTree().
    if (!isInShadowTree())
        m_targetListener->attach(target.element.copyRef());

    updateReferencedText(target.element.get());
}

Node::InsertedIntoAncestorResult SVGTRefElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    return InsertedIntoAncestorResult::Done;
}

void SVGTRefElement::didFinishInsertingNode()
{
    buildPendingResource();
}

void SVGTRefElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        m_targetListener->detach();
}

}

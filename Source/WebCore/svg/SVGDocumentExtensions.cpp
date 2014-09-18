/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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
#include "SVGDocumentExtensions.h"

#include "DOMWindow.h"
#include "Document.h"
#include "EventListener.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"
#include "SMILTimeContainer.h"
#include "SVGElement.h"
#include "SVGResourcesCache.h"
#include "SVGSMILElement.h"
#include "SVGSVGElement.h"
#include "ScriptableDocumentParser.h"
#include "ShadowRoot.h"
#include "XLinkNames.h"
#include <wtf/text/AtomicString.h>

namespace WebCore {

SVGDocumentExtensions::SVGDocumentExtensions(Document* document)
    : m_document(document)
    , m_resourcesCache(std::make_unique<SVGResourcesCache>())
{
}

SVGDocumentExtensions::~SVGDocumentExtensions()
{
}

void SVGDocumentExtensions::addTimeContainer(SVGSVGElement* element)
{
    m_timeContainers.add(element);
}

void SVGDocumentExtensions::removeTimeContainer(SVGSVGElement* element)
{
    m_timeContainers.remove(element);
}

void SVGDocumentExtensions::addResource(const AtomicString& id, RenderSVGResourceContainer* resource)
{
    ASSERT(resource);

    if (id.isEmpty())
        return;

    // Replaces resource if already present, to handle potential id changes
    m_resources.set(id, resource);
}

void SVGDocumentExtensions::removeResource(const AtomicString& id)
{
    if (id.isEmpty())
        return;

    m_resources.remove(id);
}

RenderSVGResourceContainer* SVGDocumentExtensions::resourceById(const AtomicString& id) const
{
    if (id.isEmpty())
        return 0;

    return m_resources.get(id);
}

void SVGDocumentExtensions::startAnimations()
{
    // FIXME: Eventually every "Time Container" will need a way to latch on to some global timer
    // starting animations for a document will do this "latching"
    // FIXME: We hold a ref pointers to prevent a shadow tree from getting removed out from underneath us.
    // In the future we should refactor the use-element to avoid this. See https://webkit.org/b/53704
    Vector<RefPtr<SVGSVGElement>> timeContainers;
    timeContainers.appendRange(m_timeContainers.begin(), m_timeContainers.end());
    auto end = timeContainers.end();
    for (auto it = timeContainers.begin(); it != end; ++it)
        (*it)->timeContainer()->begin();
}

void SVGDocumentExtensions::pauseAnimations()
{
    auto end = m_timeContainers.end();
    for (auto it = m_timeContainers.begin(); it != end; ++it)
        (*it)->pauseAnimations();
}

void SVGDocumentExtensions::unpauseAnimations()
{
    auto end = m_timeContainers.end();
    for (auto it = m_timeContainers.begin(); it != end; ++it)
        (*it)->unpauseAnimations();
}

void SVGDocumentExtensions::dispatchSVGLoadEventToOutermostSVGElements()
{
    Vector<RefPtr<SVGSVGElement>> timeContainers;
    timeContainers.appendRange(m_timeContainers.begin(), m_timeContainers.end());

    auto end = timeContainers.end();
    for (auto it = timeContainers.begin(); it != end; ++it) {
        SVGSVGElement* outerSVG = (*it).get();
        if (!outerSVG->isOutermostSVGSVGElement())
            continue;
        outerSVG->sendSVGLoadEventIfPossible();
    }
}

static void reportMessage(Document* document, MessageLevel level, const String& message)
{
    if (document->frame())
        document->addConsoleMessage(MessageSource::Rendering, level, message);
}

void SVGDocumentExtensions::reportWarning(const String& message)
{
    reportMessage(m_document, MessageLevel::Warning, "Warning: " + message);
}

void SVGDocumentExtensions::reportError(const String& message)
{
    reportMessage(m_document, MessageLevel::Error, "Error: " + message);
}

void SVGDocumentExtensions::addPendingResource(const AtomicString& id, Element* element)
{
    ASSERT(element);

    if (id.isEmpty())
        return;

    auto result = m_pendingResources.add(id, nullptr);
    if (result.isNewEntry)
        result.iterator->value = std::make_unique<PendingElements>();
    result.iterator->value->add(element);

    element->setHasPendingResources();
}

bool SVGDocumentExtensions::isIdOfPendingResource(const AtomicString& id) const
{
    if (id.isEmpty())
        return false;

    return m_pendingResources.contains(id);
}

bool SVGDocumentExtensions::isElementWithPendingResources(Element* element) const
{
    // This algorithm takes time proportional to the number of pending resources and need not.
    // If performance becomes an issue we can keep a counted set of elements and answer the question efficiently.
    ASSERT(element);
    auto end = m_pendingResources.end();
    for (auto it = m_pendingResources.begin(); it != end; ++it) {
        PendingElements* elements = it->value.get();
        ASSERT(elements);

        if (elements->contains(element))
            return true;
    }
    return false;
}

bool SVGDocumentExtensions::isPendingResource(Element* element, const AtomicString& id) const
{
    ASSERT(element);

    if (!isIdOfPendingResource(id))
        return false;

    return m_pendingResources.get(id)->contains(element);
}

void SVGDocumentExtensions::clearHasPendingResourcesIfPossible(Element* element)
{
    if (!isElementWithPendingResources(element))
        element->clearHasPendingResources();
}

void SVGDocumentExtensions::removeElementFromPendingResources(Element* element)
{
    ASSERT(element);

    // Remove the element from pending resources.
    if (!m_pendingResources.isEmpty() && element->hasPendingResources()) {
        Vector<AtomicString> toBeRemoved;
        auto end = m_pendingResources.end();
        for (auto it = m_pendingResources.begin(); it != end; ++it) {
            PendingElements* elements = it->value.get();
            ASSERT(elements);
            ASSERT(!elements->isEmpty());

            elements->remove(element);
            if (elements->isEmpty())
                toBeRemoved.append(it->key);
        }

        clearHasPendingResourcesIfPossible(element);

        // We use the removePendingResource function here because it deals with set lifetime correctly.
        auto vectorEnd = toBeRemoved.end();
        for (auto it = toBeRemoved.begin(); it != vectorEnd; ++it)
            removePendingResource(*it);
    }

    // Remove the element from pending resources that were scheduled for removal.
    if (!m_pendingResourcesForRemoval.isEmpty()) {
        Vector<AtomicString> toBeRemoved;
        auto end = m_pendingResourcesForRemoval.end();
        for (auto it = m_pendingResourcesForRemoval.begin(); it != end; ++it) {
            PendingElements* elements = it->value.get();
            ASSERT(elements);
            ASSERT(!elements->isEmpty());

            elements->remove(element);
            if (elements->isEmpty())
                toBeRemoved.append(it->key);
        }

        // We use the removePendingResourceForRemoval function here because it deals with set lifetime correctly.
        auto vectorEnd = toBeRemoved.end();
        for (auto it = toBeRemoved.begin(); it != vectorEnd; ++it)
            removePendingResourceForRemoval(*it);
    }
}

std::unique_ptr<SVGDocumentExtensions::PendingElements> SVGDocumentExtensions::removePendingResource(const AtomicString& id)
{
    ASSERT(m_pendingResources.contains(id));
    return m_pendingResources.take(id);
}

std::unique_ptr<SVGDocumentExtensions::PendingElements> SVGDocumentExtensions::removePendingResourceForRemoval(const AtomicString& id)
{
    ASSERT(m_pendingResourcesForRemoval.contains(id));
    return m_pendingResourcesForRemoval.take(id);
}

void SVGDocumentExtensions::markPendingResourcesForRemoval(const AtomicString& id)
{
    if (id.isEmpty())
        return;

    ASSERT(!m_pendingResourcesForRemoval.contains(id));

    std::unique_ptr<PendingElements> existing = m_pendingResources.take(id);
    if (existing && !existing->isEmpty())
        m_pendingResourcesForRemoval.add(id, WTF::move(existing));
}

Element* SVGDocumentExtensions::removeElementFromPendingResourcesForRemovalMap(const AtomicString& id)
{
    if (id.isEmpty())
        return 0;

    PendingElements* resourceSet = m_pendingResourcesForRemoval.get(id);
    if (!resourceSet || resourceSet->isEmpty())
        return 0;

    auto firstElement = resourceSet->begin();
    Element* element = *firstElement;

    resourceSet->remove(firstElement);

    if (resourceSet->isEmpty())
        removePendingResourceForRemoval(id);

    return element;
}

HashSet<SVGElement*>* SVGDocumentExtensions::setOfElementsReferencingTarget(SVGElement* referencedElement) const
{
    ASSERT(referencedElement);
    const auto it = m_elementDependencies.find(referencedElement);
    if (it == m_elementDependencies.end())
        return 0;
    return it->value.get();
}

void SVGDocumentExtensions::addElementReferencingTarget(SVGElement* referencingElement, SVGElement* referencedElement)
{
    ASSERT(referencingElement);
    ASSERT(referencedElement);

    if (HashSet<SVGElement*>* elements = m_elementDependencies.get(referencedElement)) {
        elements->add(referencingElement);
        return;
    }

    auto elements = std::make_unique<HashSet<SVGElement*>>();
    elements->add(referencingElement);
    m_elementDependencies.set(referencedElement, WTF::move(elements));
}

void SVGDocumentExtensions::removeAllTargetReferencesForElement(SVGElement* referencingElement)
{
    Vector<SVGElement*> toBeRemoved;

    auto end = m_elementDependencies.end();
    for (auto it = m_elementDependencies.begin(); it != end; ++it) {
        SVGElement* referencedElement = it->key;
        HashSet<SVGElement*>& referencingElements = *it->value;
        referencingElements.remove(referencingElement);
        if (referencingElements.isEmpty())
            toBeRemoved.append(referencedElement);
    }

    auto vectorEnd = toBeRemoved.end();
    for (auto it = toBeRemoved.begin(); it != vectorEnd; ++it)
        m_elementDependencies.remove(*it);
}

void SVGDocumentExtensions::rebuildElements()
{
    Vector<SVGElement*> shadowRebuildElements = WTF::move(m_rebuildElements);
    for (auto* element : shadowRebuildElements)
        element->svgAttributeChanged(XLinkNames::hrefAttr);
}

void SVGDocumentExtensions::clearTargetDependencies(SVGElement& referencedElement)
{
    if (referencedElement.isInShadowTree()) {
        // The host element (e.g. <use>) of the shadow root will rebuild the shadow tree
        // and all its references.
        ASSERT(referencedElement.shadowRoot());
        ASSERT(m_rebuildElements.contains(referencedElement.shadowRoot()->hostElement()));
        return;
    }
    auto it = m_elementDependencies.find(&referencedElement);
    if (it == m_elementDependencies.end())
        return;
    ASSERT(it->key == &referencedElement);
    HashSet<SVGElement*>* referencingElements = it->value.get();
    for (auto* element : *referencingElements) {
        m_rebuildElements.append(element);
        element->callClearTarget();
    }
}

void SVGDocumentExtensions::rebuildAllElementReferencesForTarget(SVGElement& referencedElement)
{
    auto it = m_elementDependencies.find(&referencedElement);
    if (it == m_elementDependencies.end())
        return;
    ASSERT(it->key == &referencedElement);

    HashSet<SVGElement*>* referencingElements = it->value.get();
    Vector<SVGElement*> elementsToRebuild;
    elementsToRebuild.reserveInitialCapacity(referencingElements->size());
    for (auto* element : *referencingElements)
        elementsToRebuild.uncheckedAppend(element);

    for (auto* element : elementsToRebuild)
        element->svgAttributeChanged(XLinkNames::hrefAttr);
}

void SVGDocumentExtensions::removeAllElementReferencesForTarget(SVGElement* referencedElement)
{
    m_elementDependencies.remove(referencedElement);

    size_t position = m_rebuildElements.find(referencedElement);
    if (position != notFound)
        m_rebuildElements.remove(position);
}

#if ENABLE(SVG_FONTS)
void SVGDocumentExtensions::registerSVGFontFaceElement(SVGFontFaceElement* element)
{
    m_svgFontFaceElements.add(element);
}

void SVGDocumentExtensions::unregisterSVGFontFaceElement(SVGFontFaceElement* element)
{
    ASSERT(m_svgFontFaceElements.contains(element));
    m_svgFontFaceElements.remove(element);
}
#endif

}

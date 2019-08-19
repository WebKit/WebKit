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
#include <wtf/text/AtomString.h>

namespace WebCore {

SVGDocumentExtensions::SVGDocumentExtensions(Document& document)
    : m_document(document)
    , m_resourcesCache(makeUnique<SVGResourcesCache>())
    , m_areAnimationsPaused(!document.page() || !document.page()->isVisible())
{
}

SVGDocumentExtensions::~SVGDocumentExtensions() = default;

void SVGDocumentExtensions::addTimeContainer(SVGSVGElement& element)
{
    m_timeContainers.add(&element);
    if (m_areAnimationsPaused)
        element.pauseAnimations();
}

void SVGDocumentExtensions::removeTimeContainer(SVGSVGElement& element)
{
    m_timeContainers.remove(&element);
}

void SVGDocumentExtensions::addResource(const AtomString& id, RenderSVGResourceContainer& resource)
{
    if (id.isEmpty())
        return;

    // Replaces resource if already present, to handle potential id changes
    m_resources.set(id, &resource);
}

void SVGDocumentExtensions::removeResource(const AtomString& id)
{
    if (id.isEmpty())
        return;

    m_resources.remove(id);
}

RenderSVGResourceContainer* SVGDocumentExtensions::resourceById(const AtomString& id) const
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
    for (auto& element : timeContainers)
        element->timeContainer().begin();
}

void SVGDocumentExtensions::pauseAnimations()
{
    for (auto& container : m_timeContainers)
        container->pauseAnimations();
    m_areAnimationsPaused = true;
}

void SVGDocumentExtensions::unpauseAnimations()
{
    for (auto& container : m_timeContainers)
        container->unpauseAnimations();
    m_areAnimationsPaused = false;
}

void SVGDocumentExtensions::dispatchSVGLoadEventToOutermostSVGElements()
{
    Vector<RefPtr<SVGSVGElement>> timeContainers;
    timeContainers.appendRange(m_timeContainers.begin(), m_timeContainers.end());

    for (auto& container : timeContainers) {
        if (!container->isOutermostSVGSVGElement())
            continue;
        container->sendSVGLoadEventIfPossible();
    }
}

static void reportMessage(Document& document, MessageLevel level, const String& message)
{
    if (document.frame())
        document.addConsoleMessage(MessageSource::Rendering, level, message);
}

void SVGDocumentExtensions::reportWarning(const String& message)
{
    reportMessage(m_document, MessageLevel::Warning, "Warning: " + message);
}

void SVGDocumentExtensions::reportError(const String& message)
{
    reportMessage(m_document, MessageLevel::Error, "Error: " + message);
}

void SVGDocumentExtensions::addPendingResource(const AtomString& id, Element& element)
{
    if (id.isEmpty())
        return;

    auto result = m_pendingResources.add(id, nullptr);
    if (result.isNewEntry)
        result.iterator->value = makeUnique<PendingElements>();
    result.iterator->value->add(&element);

    element.setHasPendingResources();
}

bool SVGDocumentExtensions::isIdOfPendingResource(const AtomString& id) const
{
    if (id.isEmpty())
        return false;

    return m_pendingResources.contains(id);
}

bool SVGDocumentExtensions::isElementWithPendingResources(Element& element) const
{
    // This algorithm takes time proportional to the number of pending resources and need not.
    // If performance becomes an issue we can keep a counted set of elements and answer the question efficiently.
    for (auto& elements : m_pendingResources.values()) {
        ASSERT(elements);
        if (elements->contains(&element))
            return true;
    }
    return false;
}

bool SVGDocumentExtensions::isPendingResource(Element& element, const AtomString& id) const
{
    if (!isIdOfPendingResource(id))
        return false;

    return m_pendingResources.get(id)->contains(&element);
}

void SVGDocumentExtensions::clearHasPendingResourcesIfPossible(Element& element)
{
    if (!isElementWithPendingResources(element))
        element.clearHasPendingResources();
}

void SVGDocumentExtensions::removeElementFromPendingResources(Element& element)
{
    // Remove the element from pending resources.
    if (!m_pendingResources.isEmpty() && element.hasPendingResources()) {
        Vector<AtomString> toBeRemoved;
        for (auto& resource : m_pendingResources) {
            PendingElements* elements = resource.value.get();
            ASSERT(elements);
            ASSERT(!elements->isEmpty());

            elements->remove(&element);
            if (elements->isEmpty())
                toBeRemoved.append(resource.key);
        }

        clearHasPendingResourcesIfPossible(element);

        // We use the removePendingResource function here because it deals with set lifetime correctly.
        for (auto& resource : toBeRemoved)
            removePendingResource(resource);
    }

    // Remove the element from pending resources that were scheduled for removal.
    if (!m_pendingResourcesForRemoval.isEmpty()) {
        Vector<AtomString> toBeRemoved;
        for (auto& resource : m_pendingResourcesForRemoval) {
            PendingElements* elements = resource.value.get();
            ASSERT(elements);
            ASSERT(!elements->isEmpty());

            elements->remove(&element);
            if (elements->isEmpty())
                toBeRemoved.append(resource.key);
        }

        // We use the removePendingResourceForRemoval function here because it deals with set lifetime correctly.
        for (auto& resource : toBeRemoved)
            removePendingResourceForRemoval(resource);
    }
}

std::unique_ptr<SVGDocumentExtensions::PendingElements> SVGDocumentExtensions::removePendingResource(const AtomString& id)
{
    ASSERT(m_pendingResources.contains(id));
    return m_pendingResources.take(id);
}

std::unique_ptr<SVGDocumentExtensions::PendingElements> SVGDocumentExtensions::removePendingResourceForRemoval(const AtomString& id)
{
    ASSERT(m_pendingResourcesForRemoval.contains(id));
    return m_pendingResourcesForRemoval.take(id);
}

void SVGDocumentExtensions::markPendingResourcesForRemoval(const AtomString& id)
{
    if (id.isEmpty())
        return;

    ASSERT(!m_pendingResourcesForRemoval.contains(id));

    std::unique_ptr<PendingElements> existing = m_pendingResources.take(id);
    if (existing && !existing->isEmpty())
        m_pendingResourcesForRemoval.add(id, WTFMove(existing));
}

RefPtr<Element> SVGDocumentExtensions::removeElementFromPendingResourcesForRemovalMap(const AtomString& id)
{
    if (id.isEmpty())
        return 0;

    PendingElements* resourceSet = m_pendingResourcesForRemoval.get(id);
    if (!resourceSet || resourceSet->isEmpty())
        return 0;

    auto firstElement = resourceSet->begin();
    RefPtr<Element> element = *firstElement;

    resourceSet->remove(firstElement);

    if (resourceSet->isEmpty())
        removePendingResourceForRemoval(id);

    return element;
}

HashSet<SVGElement*>* SVGDocumentExtensions::setOfElementsReferencingTarget(SVGElement& referencedElement) const
{
    return m_elementDependencies.get(&referencedElement);
}

void SVGDocumentExtensions::addElementReferencingTarget(SVGElement& referencingElement, SVGElement& referencedElement)
{
    auto result = m_elementDependencies.ensure(&referencedElement, [&referencingElement] {
        return makeUnique<HashSet<SVGElement*>>(std::initializer_list<SVGElement*> { &referencingElement });
    });
    if (!result.isNewEntry)
        result.iterator->value->add(&referencingElement);
}

void SVGDocumentExtensions::removeAllTargetReferencesForElement(SVGElement& referencingElement)
{
    Vector<SVGElement*> toBeRemoved;

    for (auto& dependency : m_elementDependencies) {
        auto& referencingElements = *dependency.value;
        referencingElements.remove(&referencingElement);
        if (referencingElements.isEmpty())
            toBeRemoved.append(dependency.key);
    }

    for (auto& element : toBeRemoved)
        m_elementDependencies.remove(element);
}

void SVGDocumentExtensions::rebuildElements()
{
    Vector<SVGElement*> shadowRebuildElements = WTFMove(m_rebuildElements);
    for (auto* element : shadowRebuildElements)
        element->svgAttributeChanged(SVGNames::hrefAttr);
}

void SVGDocumentExtensions::clearTargetDependencies(SVGElement& referencedElement)
{
    auto* referencingElements = m_elementDependencies.get(&referencedElement);
    if (!referencingElements)
        return;
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
        element->svgAttributeChanged(SVGNames::hrefAttr);
}

void SVGDocumentExtensions::removeAllElementReferencesForTarget(SVGElement& referencedElement)
{
    m_elementDependencies.remove(&referencedElement);
    m_rebuildElements.removeFirst(&referencedElement);
}

#if ENABLE(SVG_FONTS)

void SVGDocumentExtensions::registerSVGFontFaceElement(SVGFontFaceElement& element)
{
    m_svgFontFaceElements.add(&element);
}

void SVGDocumentExtensions::unregisterSVGFontFaceElement(SVGFontFaceElement& element)
{
    ASSERT(m_svgFontFaceElements.contains(&element));
    m_svgFontFaceElements.remove(&element);
}

#endif

}

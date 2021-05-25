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
#include "SVGFontFaceElement.h"
#include "SVGResourcesCache.h"
#include "SVGSMILElement.h"
#include "SVGSVGElement.h"
#include "SVGUseElement.h"
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

SVGDocumentExtensions::~SVGDocumentExtensions()
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_useElementsWithPendingShadowTreeUpdate.computesEmpty());
}

void SVGDocumentExtensions::addTimeContainer(SVGSVGElement& element)
{
    m_timeContainers.add(element);
    if (m_areAnimationsPaused)
        element.pauseAnimations();
}

void SVGDocumentExtensions::removeTimeContainer(SVGSVGElement& element)
{
    m_timeContainers.remove(element);
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


void SVGDocumentExtensions::addUseElementWithPendingShadowTreeUpdate(SVGUseElement& element)
{
    auto result = m_useElementsWithPendingShadowTreeUpdate.add(element);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(result.isNewEntry);
}

void SVGDocumentExtensions::removeUseElementWithPendingShadowTreeUpdate(SVGUseElement& element)
{
    m_useElementsWithPendingShadowTreeUpdate.remove(element);
    // FIXME: Assert that element was in m_svgUseElements once re-entrancy to update style and layout have been removed.
}


void SVGDocumentExtensions::startAnimations()
{
    // FIXME: Eventually every "Time Container" will need a way to latch on to some global timer
    // starting animations for a document will do this "latching"
    // FIXME: We hold a ref pointers to prevent a shadow tree from getting removed out from underneath us.
    // In the future we should refactor the use-element to avoid this. See https://webkit.org/b/53704
    auto timeContainers = copyToVectorOf<Ref<SVGSVGElement>>(m_timeContainers);
    for (auto& element : timeContainers)
        element->timeContainer().begin();
}

void SVGDocumentExtensions::pauseAnimations()
{
    for (auto& container : m_timeContainers)
        container.pauseAnimations();
    m_areAnimationsPaused = true;
}

void SVGDocumentExtensions::unpauseAnimations()
{
    for (auto& container : m_timeContainers)
        container.unpauseAnimations();
    m_areAnimationsPaused = false;
}

void SVGDocumentExtensions::dispatchLoadEventToOutermostSVGElements()
{
    auto timeContainers = copyToVectorOf<Ref<SVGSVGElement>>(m_timeContainers);
    for (auto& container : timeContainers) {
        if (!container->isOutermostSVGSVGElement())
            continue;
        container->sendLoadEventIfPossible();
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

    auto result = m_pendingResources.add(id, WeakHashSet<Element> { });
    result.iterator->value.add(element);

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
    return WTF::anyOf(m_pendingResources.values(), [&] (auto& elements) {
        return elements.contains(element);
    });
}

bool SVGDocumentExtensions::isPendingResource(Element& element, const AtomString& id) const
{
    if (id.isEmpty())
        return false;

    auto it = m_pendingResources.find(id);
    if (it == m_pendingResources.end())
        return false;

    return it->value.contains(element);
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
            auto& elements = resource.value;
            elements.remove(element);
            if (elements.computesEmpty())
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
            auto& elements = resource.value;
            elements.remove(element);
            if (elements.computesEmpty())
                toBeRemoved.append(resource.key);
        }

        // We use the removePendingResourceForRemoval function here because it deals with set lifetime correctly.
        for (auto& resource : toBeRemoved)
            m_pendingResourcesForRemoval.remove(resource);
    }
}

void SVGDocumentExtensions::markPendingResourcesForRemoval(const AtomString& id)
{
    if (id.isEmpty())
        return;

    ASSERT(!m_pendingResourcesForRemoval.contains(id));

    auto existing = m_pendingResources.take(id);
    if (!existing.computesEmpty())
        m_pendingResourcesForRemoval.add(id, WTFMove(existing));
}

RefPtr<Element> SVGDocumentExtensions::takeElementFromPendingResourcesForRemovalMap(const AtomString& id)
{
    if (id.isEmpty())
        return nullptr;

    auto it = m_pendingResourcesForRemoval.find(id);
    if (it == m_pendingResourcesForRemoval.end())
        return nullptr;

    auto& resourceSet = it->value;
    auto firstElement = makeRefPtr(resourceSet.begin().get());
    if (!firstElement)
        return nullptr;

    resourceSet.remove(*firstElement);

    if (resourceSet.computesEmpty())
        m_pendingResourcesForRemoval.remove(id);

    return firstElement;
}

void SVGDocumentExtensions::addElementToRebuild(SVGElement& element)
{
    m_rebuildElements.append(element);
}

void SVGDocumentExtensions::removeElementToRebuild(SVGElement& element)
{
    m_rebuildElements.removeFirst(element);
}

void SVGDocumentExtensions::rebuildElements()
{
    auto shadowRebuildElements = std::exchange(m_rebuildElements, { });
    for (auto& element : shadowRebuildElements)
        element->svgAttributeChanged(SVGNames::hrefAttr);
}

void SVGDocumentExtensions::clearTargetDependencies(SVGElement& referencedElement)
{
    for (auto& element : referencedElement.referencingElements()) {
        m_rebuildElements.append(element.get());
        element->callClearTarget();
    }
}

void SVGDocumentExtensions::rebuildAllElementReferencesForTarget(SVGElement& referencedElement)
{
    for (auto& element : referencedElement.referencingElements())
        element->svgAttributeChanged(SVGNames::hrefAttr);
}

void SVGDocumentExtensions::registerSVGFontFaceElement(SVGFontFaceElement& element)
{
    m_svgFontFaceElements.add(element);
}

void SVGDocumentExtensions::unregisterSVGFontFaceElement(SVGFontFaceElement& element)
{
    ASSERT(m_svgFontFaceElements.contains(element));
    m_svgFontFaceElements.remove(element);
}

}

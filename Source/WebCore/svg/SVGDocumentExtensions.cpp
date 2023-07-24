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

#include "Document.h"
#include "EventListener.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
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

static bool animationsPausedForDocument(Document& document)
{
    return !document.page() || !document.page()->isVisible() || !document.page()->imageAnimationEnabled();
}

SVGDocumentExtensions::SVGDocumentExtensions(Document& document)
    : m_document(document)
    , m_resourcesCache(makeUnique<SVGResourcesCache>())
    , m_areAnimationsPaused(animationsPausedForDocument(document))
{
}

SVGDocumentExtensions::~SVGDocumentExtensions() = default;

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

Vector<Ref<SVGSVGElement>> SVGDocumentExtensions::allSVGSVGElements() const
{
    return copyToVectorOf<Ref<SVGSVGElement>>(m_timeContainers);
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
    // If animations are paused at the document level, don't allow `this` to be unpaused.
    if (animationsPausedForDocument(m_document))
        return;

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

void SVGDocumentExtensions::addElementToRebuild(SVGElement& element)
{
    m_rebuildElements.append(element);
}

void SVGDocumentExtensions::removeElementToRebuild(SVGElement& element)
{
    m_rebuildElements.removeFirstMatching([&](auto& item) { return item.ptr() == &element; });
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

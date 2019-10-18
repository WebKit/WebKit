/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class Document;
class Element;
class RenderSVGResourceContainer;
class SVGElement;
class SVGFontFaceElement;
class SVGResourcesCache;
class SVGSMILElement;
class SVGSVGElement;

class SVGDocumentExtensions {
    WTF_MAKE_NONCOPYABLE(SVGDocumentExtensions); WTF_MAKE_FAST_ALLOCATED;
public:
    typedef HashSet<Element*> PendingElements;
    explicit SVGDocumentExtensions(Document&);
    ~SVGDocumentExtensions();
    
    void addTimeContainer(SVGSVGElement&);
    void removeTimeContainer(SVGSVGElement&);

    void addResource(const AtomString& id, RenderSVGResourceContainer&);
    void removeResource(const AtomString& id);
    RenderSVGResourceContainer* resourceById(const AtomString& id) const;

    void startAnimations();
    void pauseAnimations();
    void unpauseAnimations();
    void dispatchLoadEventToOutermostSVGElements();
    bool areAnimationsPaused() const { return m_areAnimationsPaused; }

    void reportWarning(const String&);
    void reportError(const String&);

    SVGResourcesCache& resourcesCache() { return *m_resourcesCache; }

    HashSet<SVGElement*>* setOfElementsReferencingTarget(SVGElement& referencedElement) const;
    void addElementReferencingTarget(SVGElement& referencingElement, SVGElement& referencedElement);
    void removeAllTargetReferencesForElement(SVGElement&);
    void rebuildAllElementReferencesForTarget(SVGElement&);
    void removeAllElementReferencesForTarget(SVGElement&);

    void clearTargetDependencies(SVGElement&);
    void rebuildElements();

#if ENABLE(SVG_FONTS)
    const HashSet<SVGFontFaceElement*>& svgFontFaceElements() const { return m_svgFontFaceElements; }
    void registerSVGFontFaceElement(SVGFontFaceElement&);
    void unregisterSVGFontFaceElement(SVGFontFaceElement&);
#endif

private:
    Document& m_document;
    HashSet<SVGSVGElement*> m_timeContainers; // For SVG 1.2 support this will need to be made more general.
#if ENABLE(SVG_FONTS)
    HashSet<SVGFontFaceElement*> m_svgFontFaceElements;
#endif
    HashMap<AtomString, RenderSVGResourceContainer*> m_resources;
    HashMap<AtomString, std::unique_ptr<PendingElements>> m_pendingResources; // Resources that are pending.
    HashMap<AtomString, std::unique_ptr<PendingElements>> m_pendingResourcesForRemoval; // Resources that are pending and scheduled for removal.
    HashMap<SVGElement*, std::unique_ptr<HashSet<SVGElement*>>> m_elementDependencies;
    std::unique_ptr<SVGResourcesCache> m_resourcesCache;

    Vector<SVGElement*> m_rebuildElements;
    bool m_areAnimationsPaused;

public:
    // This HashMap contains a list of pending resources. Pending resources, are such
    // which are referenced by any object in the SVG document, but do NOT exist yet.
    // For instance, dynamically build gradients / patterns / clippers...
    void addPendingResource(const AtomString& id, Element&);
    bool isIdOfPendingResource(const AtomString& id) const;
    bool isPendingResource(Element&, const AtomString& id) const;
    void clearHasPendingResourcesIfPossible(Element&);
    void removeElementFromPendingResources(Element&);
    std::unique_ptr<PendingElements> removePendingResource(const AtomString& id);

    // The following two functions are used for scheduling a pending resource to be removed.
    void markPendingResourcesForRemoval(const AtomString&);
    RefPtr<Element> removeElementFromPendingResourcesForRemovalMap(const AtomString&);

private:
    bool isElementWithPendingResources(Element&) const;
    std::unique_ptr<PendingElements> removePendingResourceForRemoval(const AtomString&);
};

} // namespace WebCore

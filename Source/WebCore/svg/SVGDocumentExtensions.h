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
#include <wtf/RobinHoodHashMap.h>
#include <wtf/WeakHashSet.h>
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
class SVGUseElement;
class WeakPtrImplWithEventTargetData;

class SVGDocumentExtensions {
    WTF_MAKE_NONCOPYABLE(SVGDocumentExtensions); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SVGDocumentExtensions(Document&);
    ~SVGDocumentExtensions();

    void addTimeContainer(SVGSVGElement&);
    void removeTimeContainer(SVGSVGElement&);
    WEBCORE_EXPORT Vector<Ref<SVGSVGElement>> allSVGSVGElements() const;

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

    void addElementToRebuild(SVGElement&);
    void removeElementToRebuild(SVGElement&);
    void rebuildElements();
    void clearTargetDependencies(SVGElement&);
    void rebuildAllElementReferencesForTarget(SVGElement&);

    const WeakHashSet<SVGFontFaceElement, WeakPtrImplWithEventTargetData>& svgFontFaceElements() const { return m_svgFontFaceElements; }
    void registerSVGFontFaceElement(SVGFontFaceElement&);
    void unregisterSVGFontFaceElement(SVGFontFaceElement&);

private:
    Document& m_document;
    WeakHashSet<SVGSVGElement, WeakPtrImplWithEventTargetData> m_timeContainers; // For SVG 1.2 support this will need to be made more general.
    WeakHashSet<SVGFontFaceElement, WeakPtrImplWithEventTargetData> m_svgFontFaceElements;
    MemoryCompactRobinHoodHashMap<AtomString, RenderSVGResourceContainer*> m_resources;
    MemoryCompactRobinHoodHashMap<AtomString, WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>> m_pendingResources; // Resources that are pending.
    MemoryCompactRobinHoodHashMap<AtomString, WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>> m_pendingResourcesForRemoval; // Resources that are pending and scheduled for removal.
    std::unique_ptr<SVGResourcesCache> m_resourcesCache;

    Vector<Ref<SVGElement>> m_rebuildElements;
    bool m_areAnimationsPaused;

public:
    // This HashMap contains a list of pending resources. Pending resources, are such
    // which are referenced by any object in the SVG document, but do NOT exist yet.
    // For instance, dynamically build gradients / patterns / clippers...
    void addPendingResource(const AtomString& id, SVGElement&);
    bool isIdOfPendingResource(const AtomString& id) const;
    bool isPendingResource(SVGElement&, const AtomString& id) const;
    void clearHasPendingResourcesIfPossible(SVGElement&);
    void removeElementFromPendingResources(SVGElement&);
    WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> removePendingResource(const AtomString& id) { return m_pendingResources.take(id); }

    // The following two functions are used for scheduling a pending resource to be removed.
    void markPendingResourcesForRemoval(const AtomString&);
    RefPtr<SVGElement> takeElementFromPendingResourcesForRemovalMap(const AtomString&);

private:
    bool isElementWithPendingResources(SVGElement&) const;
};

} // namespace WebCore

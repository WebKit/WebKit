/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class CSSStyleSheet;
class CSSStyleSheetObservableArray;
class ContainerNode;
class Document;
class Element;
class FloatPoint;
class JSDOMGlobalObject;
class HTMLImageElement;
class HTMLLabelElement;
class HTMLMapElement;
class LayoutPoint;
class LegacyRenderSVGResourceContainer;
class IdTargetObserverRegistry;
class Node;
class RadioButtonGroups;
class SVGElement;
class ShadowRoot;
class TreeScopeOrderedMap;
class WeakPtrImplWithEventTargetData;
struct SVGResourcesMap;

class TreeScope {
    friend class Document;

public:
    TreeScope* parentTreeScope() const { return m_parentTreeScope; }
    void setParentTreeScope(TreeScope&);

    // For CheckedPtr / CheckedRef use.
    void incrementPtrCount() const;
    void decrementPtrCount() const;
#if CHECKED_POINTER_DEBUG
    void registerCheckedPtr(const void*) const;
    void copyCheckedPtr(const void* source, const void* destination) const;
    void moveCheckedPtr(const void* source, const void* destination) const;
    void unregisterCheckedPtr(const void*) const;
#endif // CHECKED_POINTER_DEBUG

    Element* focusedElementInScope();
    Element* pointerLockElement() const;

    WEBCORE_EXPORT RefPtr<Element> getElementById(const AtomString&) const;
    WEBCORE_EXPORT RefPtr<Element> getElementById(const String&) const;
    RefPtr<Element> getElementById(StringView) const;
    const Vector<WeakRef<Element, WeakPtrImplWithEventTargetData>>* getAllElementsById(const AtomString&) const;
    inline bool hasElementWithId(const AtomString&) const; // Defined in TreeScopeInlines.h.
    inline bool containsMultipleElementsWithId(const AtomString& id) const; // Defined in TreeScopeInlines.h.
    void addElementById(const AtomString& elementId, Element&, bool notifyObservers = true);
    void removeElementById(const AtomString& elementId, Element&, bool notifyObservers = true);

    WEBCORE_EXPORT RefPtr<Element> getElementByName(const AtomString&) const;
    inline bool hasElementWithName(const AtomString&) const; // Defined in TreeScopeInlines.h.
    inline bool containsMultipleElementsWithName(const AtomString&) const; // Defined in TreeScopeInlines.h.
    void addElementByName(const AtomString&, Element&);
    void removeElementByName(const AtomString&, Element&);

    Document& documentScope() const { return m_documentScope.get(); }
    Ref<Document> protectedDocumentScope() const;
    static ptrdiff_t documentScopeMemoryOffset() { return OBJECT_OFFSETOF(TreeScope, m_documentScope); }

    // https://dom.spec.whatwg.org/#retarget
    Ref<Node> retargetToScope(Node&) const;

    WEBCORE_EXPORT Node* ancestorNodeInThisScope(Node*) const;
    WEBCORE_EXPORT Element* ancestorElementInThisScope(Element*) const;

    void addImageMap(HTMLMapElement&);
    void removeImageMap(HTMLMapElement&);
    RefPtr<HTMLMapElement> getImageMap(const AtomString&) const;

    void addImageElementByUsemap(const AtomString&, HTMLImageElement&);
    void removeImageElementByUsemap(const AtomString&, HTMLImageElement&);
    RefPtr<HTMLImageElement> imageElementByUsemap(const AtomString&) const;

    // For accessibility.
    bool shouldCacheLabelsByForAttribute() const { return !!m_labelsByForAttribute; }
    void addLabel(const AtomString& forAttributeValue, HTMLLabelElement&);
    void removeLabel(const AtomString& forAttributeValue, HTMLLabelElement&);
    const Vector<WeakRef<Element, WeakPtrImplWithEventTargetData>>* labelElementsForId(const AtomString& forAttributeValue);

    WEBCORE_EXPORT RefPtr<Element> elementFromPoint(double clientX, double clientY);
    WEBCORE_EXPORT Vector<RefPtr<Element>> elementsFromPoint(double clientX, double clientY);
    WEBCORE_EXPORT Vector<RefPtr<Element>> elementsFromPoint(const FloatPoint&);

    // Find first anchor with the given name.
    // First searches for an element with the given ID, but if that fails, then looks
    // for an anchor with the given name. ID matching is always case sensitive, but
    // Anchor name matching is case sensitive in strict mode and not case sensitive in
    // quirks mode for historical compatibility reasons.
    RefPtr<Element> findAnchor(StringView name);

    ContainerNode& rootNode() const { return m_rootNode; }
    Ref<ContainerNode> protectedRootNode() const;

    IdTargetObserverRegistry& idTargetObserverRegistry() { return m_idTargetObserverRegistry.get(); }
    const IdTargetObserverRegistry& idTargetObserverRegistry() const { return m_idTargetObserverRegistry.get(); }

    RadioButtonGroups& radioButtonGroups();

    JSC::JSValue adoptedStyleSheetWrapper(JSDOMGlobalObject&);
    std::span<const RefPtr<CSSStyleSheet>> adoptedStyleSheets() const;
    ExceptionOr<void> setAdoptedStyleSheets(Vector<RefPtr<CSSStyleSheet>>&&);

    void addSVGResource(const AtomString& id, LegacyRenderSVGResourceContainer&);
    void removeSVGResource(const AtomString& id);
    LegacyRenderSVGResourceContainer* lookupLegacySVGResoureById(const AtomString& id) const;

    void addPendingSVGResource(const AtomString& id, SVGElement&);
    bool isIdOfPendingSVGResource(const AtomString& id) const;
    bool isPendingSVGResource(SVGElement&, const AtomString& id) const;
    void clearHasPendingSVGResourcesIfPossible(SVGElement&);
    void removeElementFromPendingSVGResources(SVGElement&);
    WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> removePendingSVGResource(const AtomString&);
    void markPendingSVGResourcesForRemoval(const AtomString&);
    RefPtr<SVGElement> takeElementFromPendingSVGResourcesForRemovalMap(const AtomString&);

protected:
    TreeScope(ShadowRoot&, Document&);
    explicit TreeScope(Document&);
    ~TreeScope();

    void destroyTreeScopeData();
    void setDocumentScope(Document& document)
    {
        m_documentScope = document;
    }

    RefPtr<Node> nodeFromPoint(const LayoutPoint& clientPoint, LayoutPoint* localPoint);

private:
    CSSStyleSheetObservableArray& ensureAdoptedStyleSheets();

    SVGResourcesMap& svgResourcesMap() const;
    bool isElementWithPendingSVGResources(SVGElement&) const;

    ContainerNode& m_rootNode;
    std::reference_wrapper<Document> m_documentScope;
    TreeScope* m_parentTreeScope;

    std::unique_ptr<TreeScopeOrderedMap> m_elementsById;
    std::unique_ptr<TreeScopeOrderedMap> m_elementsByName;
    std::unique_ptr<TreeScopeOrderedMap> m_imageMapsByName;
    std::unique_ptr<TreeScopeOrderedMap> m_imagesByUsemap;
    std::unique_ptr<TreeScopeOrderedMap> m_labelsByForAttribute;

    UniqueRef<IdTargetObserverRegistry> m_idTargetObserverRegistry;

    std::unique_ptr<RadioButtonGroups> m_radioButtonGroups;
    RefPtr<CSSStyleSheetObservableArray> m_adoptedStyleSheets;

    std::unique_ptr<SVGResourcesMap> m_svgResourcesMap;
};

TreeScope* commonTreeScope(Node*, Node*);

} // namespace WebCore

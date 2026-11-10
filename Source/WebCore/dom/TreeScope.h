/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#include <WebCore/HitTestSource.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/NoVirtualDestructorBase.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/AtomString.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class CSSStyleSheet;
class CSSStyleSheetObservableArray;
class ContainerNode;
class CustomElementRegistry;
class Document;
class Element;
class FloatPoint;
class JSDOMGlobalObject;
class HTMLAnchorElement;
class HTMLImageElement;
class HTMLLabelElement;
class HTMLMapElement;
class LayoutPoint;
class LegacyRenderSVGResourceContainer;
class IdTargetObserverRegistry;
class Node;
class QualifiedName;
class RadioButtonGroups;
class SVGElement;
class ShadowRoot;
class TreeScopeOrderedMap;
class WeakPtrImplWithEventTargetData;
struct SVGResourcesMap;
template<typename> class ExceptionOr;

class TreeScope : public NoVirtualDestructorBase {
    friend class Document;

public:
    TreeScope* parentTreeScope() const { return m_parentTreeScope.get(); }
    void setParentTreeScope(TreeScope&);

    NODELETE WEBCORE_EXPORT void ref() const;
    WEBCORE_EXPORT void deref() const;

    uint32_t checkedPtrCount() const;
    uint32_t checkedPtrCountWithoutThreadCheck() const;
    void incrementCheckedPtrCount() const;
    void decrementCheckedPtrCount() const;
    void setDidBeginCheckedPtrDeletion();

    NODELETE Element* focusedElementInScope();
    NODELETE Element* pointerLockElement() const;

    void setCustomElementRegistry(RefPtr<CustomElementRegistry>&&);
    CustomElementRegistry* customElementRegistry() const { return m_customElementRegistry.get(); }

    WEBCORE_EXPORT RefPtr<Element> getElementById(AtomStringImpl*) const;
    WEBCORE_EXPORT RefPtr<Element> getElementById(StringImpl*) const;
    RefPtr<Element> getElementById(StringView) const;
    // Pointer and StringView overloads are ambiguous for smart pointer arguments.
    RefPtr<Element> getElementById(const RefPtr<AtomStringImpl>& elementId) const { return getElementById(elementId.get()); }
    RefPtr<Element> getElementById(const RefPtr<StringImpl>& elementId) const { return getElementById(elementId.get()); }
    RefPtr<Element> elementByIdResolvingReferenceTarget(AtomStringImpl*) const;
    const Vector<WeakRef<Element, WeakPtrImplWithEventTargetData>>* getAllElementsById(AtomStringImpl*) const;
    inline bool hasElementWithId(AtomStringImpl*) const; // Defined in TreeScopeInlines.h.
    inline bool containsMultipleElementsWithId(AtomStringImpl* id) const; // Defined in TreeScopeInlines.h.
    void addElementById(AtomStringImpl* elementId, Element&, bool notifyObservers = true);
    void removeElementById(AtomStringImpl* elementId, Element&, bool notifyObservers = true);

    WEBCORE_EXPORT RefPtr<Element> getElementByName(AtomStringImpl*) const;
    inline bool hasElementWithName(AtomStringImpl*) const; // Defined in TreeScopeInlines.h.
    inline bool containsMultipleElementsWithName(AtomStringImpl*) const; // Defined in TreeScopeInlines.h.
    void addElementByName(AtomStringImpl*, Element&);
    void removeElementByName(AtomStringImpl*, Element&);

    NODELETE Document& documentScope() const { return m_documentScope.get(); }
    static constexpr ptrdiff_t documentScopeMemoryOffset() { return OBJECT_OFFSETOF(TreeScope, m_documentScope); }

    // https://dom.spec.whatwg.org/#retarget
    NODELETE Ref<Node> retargetToScope(Node&) const;

    NODELETE WEBCORE_EXPORT Node* ancestorNodeInThisScope(Node*) const;
    NODELETE WEBCORE_EXPORT Element* ancestorElementInThisScope(Element*) const;

    void addImageMap(HTMLMapElement&, AtomStringImpl* name, AtomStringImpl* id);
    void removeImageMap(HTMLMapElement&, AtomStringImpl* name, AtomStringImpl* id);
    RefPtr<HTMLMapElement> getImageMap(AtomStringImpl*) const;

    void addImageElementByUsemap(AtomStringImpl*, HTMLImageElement&);
    void removeImageElementByUsemap(AtomStringImpl*, HTMLImageElement&);
    RefPtr<HTMLImageElement> imageElementByUsemap(AtomStringImpl*) const;

    // For accessibility.
    bool shouldCacheLabelsByForAttribute() const { return !!m_labelsByForAttribute; }
    void addLabel(AtomStringImpl* forAttributeValue, HTMLLabelElement&);
    void removeLabel(AtomStringImpl* forAttributeValue, HTMLLabelElement&);
    const Vector<WeakRef<Element, WeakPtrImplWithEventTargetData>>* labelElementsForId(AtomStringImpl* forAttributeValue);

    WEBCORE_EXPORT RefPtr<Element> elementFromPoint(double clientX, double clientY, HitTestSource = HitTestSource::Script);
    WEBCORE_EXPORT Vector<Ref<Element>> elementsFromPoint(double clientX, double clientY, HitTestSource = HitTestSource::Script);

    // Find first anchor with the given name.
    // First searches for an element with the given ID, but if that fails, then looks
    // for an anchor with the given name. ID matching is always case sensitive, but
    // Anchor name matching is case sensitive in strict mode and not case sensitive in
    // quirks mode for historical compatibility reasons.
    RefPtr<Element> findAnchor(StringView name);
    NODELETE bool isMatchingAnchor(HTMLAnchorElement&, StringView name);

    inline ContainerNode& rootNode() const; // Defined in TreeScopeInlines.h

    inline IdTargetObserverRegistry& idTargetObserverRegistry();
    IdTargetObserverRegistry* idTargetObserverRegistryIfExists() { return m_idTargetObserverRegistry.get(); }

    RadioButtonGroups& radioButtonGroups();

    JSC::JSValue adoptedStyleSheetWrapper(JSDOMGlobalObject&);
    NODELETE std::span<const Ref<CSSStyleSheet>> adoptedStyleSheets() const;
    ExceptionOr<void> setAdoptedStyleSheets(Vector<Ref<CSSStyleSheet>>&&);

    void addSVGResource(AtomStringImpl* id, LegacyRenderSVGResourceContainer&);
    void removeSVGResource(AtomStringImpl* id, LegacyRenderSVGResourceContainer&);
    LegacyRenderSVGResourceContainer* lookupLegacySVGResoureById(AtomStringImpl* id) const;

    void addPendingSVGResource(AtomStringImpl* id, SVGElement&);
    bool isIdOfPendingSVGResource(AtomStringImpl* id) const;
    bool isPendingSVGResource(SVGElement&, AtomStringImpl* id) const;
    void clearHasPendingSVGResourcesIfPossible(SVGElement&);
    void removeElementFromPendingSVGResources(SVGElement&);
    WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> removePendingSVGResource(AtomStringImpl*);
    void markPendingSVGResourcesForRemoval(AtomStringImpl*);
    RefPtr<SVGElement> takeElementFromPendingSVGResourcesForRemovalMap(AtomStringImpl*);

    // FIXME: Add a ThreadSafetyAnalysis annotated Lock subclass that allows for asserted reads from the mutator
    static ALWAYS_INLINE Lock& treeScopeMutationLock();

protected:
    TreeScope(ShadowRoot&, Document&, RefPtr<CustomElementRegistry>&&);
    explicit TreeScope(Document&);
    ~TreeScope();

    void destroyTreeScopeData();
    inline void setDocumentScope(Document&);

    RefPtr<Node> nodeFromPoint(const LayoutPoint& clientPoint, LayoutPoint* localPoint, HitTestSource);

private:
    IdTargetObserverRegistry& ensureIdTargetObserverRegistry();
    CSSStyleSheetObservableArray& ensureAdoptedStyleSheets();

    SVGResourcesMap& svgResourcesMap() const;
    bool isElementWithPendingSVGResources(SVGElement&) const;

    const CheckedRef<ContainerNode> m_rootNode;
    std::reference_wrapper<Document> m_documentScope;
    CheckedPtr<TreeScope> m_parentTreeScope;

    RefPtr<CustomElementRegistry> m_customElementRegistry;

    std::unique_ptr<TreeScopeOrderedMap> m_elementsById;
    std::unique_ptr<TreeScopeOrderedMap> m_elementsByName;
    std::unique_ptr<TreeScopeOrderedMap> m_imageMapsByName;
    std::unique_ptr<TreeScopeOrderedMap> m_imagesByUsemap;
    std::unique_ptr<TreeScopeOrderedMap> m_labelsByForAttribute;

    const std::unique_ptr<IdTargetObserverRegistry> m_idTargetObserverRegistry;

    const std::unique_ptr<RadioButtonGroups> m_radioButtonGroups;
    const RefPtr<CSSStyleSheetObservableArray> m_adoptedStyleSheets;

    const std::unique_ptr<SVGResourcesMap> m_svgResourcesMap;
};

NODELETE TreeScope* commonTreeScope(Node*, Node*);

} // namespace WebCore

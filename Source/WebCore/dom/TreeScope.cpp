/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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

#include "config.h"
#include "TreeScope.h"

#include "Attr.h"
#include "CSSStyleSheet.h"
#include "CSSStyleSheetObservableArray.h"
#include "CustomElementRegistry.h"
#include "FocusController.h"
#include "HTMLAnchorElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HitTestResult.h"
#include "IdTargetObserverRegistry.h"
#include "JSObservableArray.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NodeRareData.h"
#include "Page.h"
#include "PointerLockController.h"
#include "PseudoElement.h"
#include "RadioButtonGroups.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TreeScopeOrderedMap.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/RobinHoodHashMap.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/CString.h>

namespace WebCore {

struct SameSizeAsTreeScope {
    void* pointers[13];
};

static_assert(sizeof(TreeScope) == sizeof(SameSizeAsTreeScope), "treescope should stay small");

using namespace HTMLNames;

struct SVGResourcesMap {
    WTF_MAKE_NONCOPYABLE(SVGResourcesMap);
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    SVGResourcesMap() = default;

    MemoryCompactRobinHoodHashMap<AtomString, WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>> pendingResources;
    MemoryCompactRobinHoodHashMap<AtomString, WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>> pendingResourcesForRemoval;
    MemoryCompactRobinHoodHashMap<AtomString, LegacyRenderSVGResourceContainer*> legacyResources;
};

TreeScope::TreeScope(ShadowRoot& shadowRoot, Document& document, RefPtr<CustomElementRegistry>&& registry)
    : m_rootNode(shadowRoot)
    , m_documentScope(document)
    , m_parentTreeScope(&document)
    , m_customElementRegistry(WTFMove(registry))
{
    shadowRoot.setTreeScope(*this);
}

TreeScope::TreeScope(Document& document)
    : m_rootNode(document)
    , m_documentScope(document)
    , m_parentTreeScope(nullptr)
{
    document.setTreeScope(*this);
}

TreeScope::~TreeScope() = default;

void TreeScope::ref() const
{
    if (auto* document = dynamicDowncast<Document>(m_rootNode.get()))
        document->ref();
    else
        downcast<ShadowRoot>(m_rootNode.get()).ref();
}

void TreeScope::deref() const
{
    if (auto* document = dynamicDowncast<Document>(m_rootNode.get()))
        document->deref();
    else
        downcast<ShadowRoot>(m_rootNode.get()).deref();
}

IdTargetObserverRegistry& TreeScope::ensureIdTargetObserverRegistry()
{
    if (!m_idTargetObserverRegistry)
        m_idTargetObserverRegistry = makeUnique<IdTargetObserverRegistry>();
    return *m_idTargetObserverRegistry;
}

void TreeScope::destroyTreeScopeData()
{
    m_elementsById = nullptr;
    m_elementsByName = nullptr;
    m_imageMapsByName = nullptr;
    m_imagesByUsemap = nullptr;
    m_labelsByForAttribute = nullptr;
}

void TreeScope::setParentTreeScope(TreeScope& newParentScope)
{
    // A document node cannot be re-parented.
    ASSERT(!m_rootNode->isDocumentNode());

    m_parentTreeScope = &newParentScope;
    setDocumentScope(newParentScope.documentScope());
}

void TreeScope::setCustomElementRegistry(Ref<CustomElementRegistry>&& registry)
{
    if (!m_customElementRegistry)
        m_customElementRegistry = WTFMove(registry);
}

ExceptionOr<Ref<Node>> TreeScope::importNode(Node& nodeToImport, bool deep)
{
    switch (nodeToImport.nodeType()) {
    case Node::DOCUMENT_FRAGMENT_NODE:
        if (nodeToImport.isShadowRoot())
            break;
        FALLTHROUGH;
    case Node::ELEMENT_NODE:
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::PROCESSING_INSTRUCTION_NODE:
    case Node::COMMENT_NODE:
        return nodeToImport.cloneNodeInternal(*this, deep ? Node::CloningOperation::Everything : Node::CloningOperation::OnlySelf);

    case Node::ATTRIBUTE_NODE: {
        auto& attribute = uncheckedDowncast<Attr>(nodeToImport);
        return Ref<Node> { Attr::create(documentScope(), attribute.qualifiedName(), attribute.value()) };
    }
    case Node::DOCUMENT_NODE: // Can't import a document into another document.
    case Node::DOCUMENT_TYPE_NODE: // FIXME: Support cloning a DocumentType node per DOM4.
        break;
    }
    return Exception { ExceptionCode::NotSupportedError };
}

RefPtr<Element> TreeScope::getElementById(const AtomString& elementId) const
{
    if (elementId.isEmpty())
        return nullptr;
    if (!m_elementsById)
        return nullptr;
    return m_elementsById->getElementById(elementId, *this);
}

RefPtr<Element> TreeScope::getElementById(const String& elementId) const
{
    if (!m_elementsById)
        return nullptr;

    if (auto atomElementId = elementId.toExistingAtomString(); !atomElementId.isNull())
        return m_elementsById->getElementById(atomElementId, *this);

    return nullptr;
}

RefPtr<Element> TreeScope::getElementById(StringView elementId) const
{
    if (!m_elementsById)
        return nullptr;

    if (auto atomElementId = elementId.toExistingAtomString(); !atomElementId.isNull())
        return m_elementsById->getElementById(atomElementId, *this);

    return nullptr;
}

const Vector<WeakRef<Element, WeakPtrImplWithEventTargetData>>* TreeScope::getAllElementsById(const AtomString& elementId) const
{
    if (elementId.isEmpty())
        return nullptr;
    if (!m_elementsById)
        return nullptr;
    return m_elementsById->getAllElementsById(elementId, *this);
}

void TreeScope::addElementById(const AtomString& elementId, Element& element, bool notifyObservers)
{
    if (!m_elementsById)
        m_elementsById = makeUnique<TreeScopeOrderedMap>();
    m_elementsById->add(elementId, element, *this);
    if (m_idTargetObserverRegistry && notifyObservers)
        m_idTargetObserverRegistry->notifyObservers(elementId);
}

void TreeScope::removeElementById(const AtomString& elementId, Element& element, bool notifyObservers)
{
    if (!m_elementsById)
        return;
    m_elementsById->remove(elementId, element);
    if (m_idTargetObserverRegistry && notifyObservers)
        m_idTargetObserverRegistry->notifyObservers(elementId);
}

RefPtr<Element> TreeScope::getElementByName(const AtomString& name) const
{
    if (name.isEmpty())
        return nullptr;
    if (!m_elementsByName)
        return nullptr;
    return m_elementsByName->getElementByName(name, *this);
}

void TreeScope::addElementByName(const AtomString& name, Element& element)
{
    if (!m_elementsByName)
        m_elementsByName = makeUnique<TreeScopeOrderedMap>();
    m_elementsByName->add(name, element, *this);
}

void TreeScope::removeElementByName(const AtomString& name, Element& element)
{
    if (!m_elementsByName)
        return;
    m_elementsByName->remove(name, element);
}


Ref<Node> TreeScope::retargetToScope(Node& node) const
{
    auto& scope = node.treeScope();
    if (LIKELY(this == &scope || !node.isInShadowTree()))
        return node;
    ASSERT(is<ShadowRoot>(scope.rootNode()));

    Vector<TreeScope*, 8> nodeTreeScopes;
    for (auto* currentScope = &scope; currentScope; currentScope = currentScope->parentTreeScope())
        nodeTreeScopes.append(currentScope);
    ASSERT(nodeTreeScopes.size() >= 2);

    Vector<const TreeScope*, 8> ancestorScopes;
    for (auto* currentScope = this; currentScope; currentScope = currentScope->parentTreeScope())
        ancestorScopes.append(currentScope);

    auto i = nodeTreeScopes.size();
    auto j = ancestorScopes.size();
    while (i > 0 && j > 0 && nodeTreeScopes[i - 1] == ancestorScopes[j - 1]) {
        --i;
        --j;
    }

    bool nodeIsInOuterTreeScope = !i;
    if (nodeIsInOuterTreeScope)
        return node;

    auto& shadowRootInLowestCommonTreeScope = downcast<ShadowRoot>(nodeTreeScopes[i - 1]->rootNode());
    return *shadowRootInLowestCommonTreeScope.host();
}

Node* TreeScope::ancestorNodeInThisScope(Node* node) const
{
    for (; node; node = node->shadowHost()) {
        if (&node->treeScope() == this)
            return node;
        if (!node->isInShadowTree())
            return nullptr;
    }
    return nullptr;
}

Element* TreeScope::ancestorElementInThisScope(Element* element) const
{
    for (; element; element = element->shadowHost()) {
        if (&element->treeScope() == this)
            return element;
        if (!element->isInShadowTree())
            return nullptr;
    }
    return nullptr;
}

void TreeScope::addImageMap(HTMLMapElement& imageMap)
{
    auto name = imageMap.getName();
    if (name.isNull())
        return;
    if (!m_imageMapsByName)
        m_imageMapsByName = makeUnique<TreeScopeOrderedMap>();
    m_imageMapsByName->add(name, imageMap, *this);
}

void TreeScope::removeImageMap(HTMLMapElement& imageMap)
{
    if (!m_imageMapsByName)
        return;
    auto name = imageMap.getName();
    if (name.isNull())
        return;
    m_imageMapsByName->remove(name, imageMap);
}

RefPtr<HTMLMapElement> TreeScope::getImageMap(const AtomString& name) const
{
    if (!m_imageMapsByName || name.isNull())
        return nullptr;
    return m_imageMapsByName->getElementByMapName(name, *this);
}

void TreeScope::addImageElementByUsemap(const AtomString& name, HTMLImageElement& element)
{
    if (!m_imagesByUsemap)
        m_imagesByUsemap = makeUnique<TreeScopeOrderedMap>();
    return m_imagesByUsemap->add(name, element, *this);
}

void TreeScope::removeImageElementByUsemap(const AtomString& name, HTMLImageElement& element)
{
    if (!m_imagesByUsemap)
        return;
    m_imagesByUsemap->remove(name, element);
}

RefPtr<HTMLImageElement> TreeScope::imageElementByUsemap(const AtomString& name) const
{
    if (!m_imagesByUsemap)
        return nullptr;
    return m_imagesByUsemap->getElementByUsemap(name, *this);
}

void TreeScope::addLabel(const AtomString& forAttributeValue, HTMLLabelElement& element)
{
    ASSERT(m_labelsByForAttribute);
    m_labelsByForAttribute->add(forAttributeValue, element, *this);
}

void TreeScope::removeLabel(const AtomString& forAttributeValue, HTMLLabelElement& element)
{
    ASSERT(m_labelsByForAttribute);
    m_labelsByForAttribute->remove(forAttributeValue, element);
}

const Vector<WeakRef<Element, WeakPtrImplWithEventTargetData>>* TreeScope::labelElementsForId(const AtomString& forAttributeValue)
{
    if (forAttributeValue.isEmpty())
        return nullptr;

    if (!m_labelsByForAttribute) {
        // Populate the map on first access.
        m_labelsByForAttribute = makeUnique<TreeScopeOrderedMap>();

        for (Ref label : descendantsOfType<HTMLLabelElement>(m_rootNode.get())) {
            const AtomString& forValue = label->attributeWithoutSynchronization(forAttr);
            if (!forValue.isEmpty())
                addLabel(forValue, label);
        }
    }

    return m_labelsByForAttribute->getElementsByLabelForAttribute(forAttributeValue, *this);
}

static std::optional<LayoutPoint> absolutePointIfNotClipped(Document& document, const LayoutPoint& clientPoint)
{
    if (!document.frame() || !document.view())
        return std::nullopt;

    const auto& settings = document.frame()->settings();
    if (settings.visualViewportEnabled() && settings.clientCoordinatesRelativeToLayoutViewport()) {
        document.updateLayout();
        if (!document.view() || !document.hasLivingRenderTree())
            return std::nullopt;
        auto* view = document.view();
        FloatPoint layoutViewportPoint = view->clientToLayoutViewportPoint(clientPoint);
        FloatRect layoutViewportBounds({ }, view->layoutViewportRect().size());
        if (!layoutViewportBounds.contains(layoutViewportPoint))
            return std::nullopt;
        return LayoutPoint(view->layoutViewportToAbsolutePoint(layoutViewportPoint));
    }

    auto* frame = document.frame();
    auto* view = document.view();
    float scaleFactor = frame->pageZoomFactor() * frame->frameScaleFactor();

    LayoutPoint absolutePoint = clientPoint;
    absolutePoint.scale(scaleFactor);
    absolutePoint.moveBy(view->contentsScrollPosition());

    LayoutRect visibleRect;
#if PLATFORM(IOS_FAMILY)
    visibleRect = view->unobscuredContentRect();
#else
    visibleRect = view->visibleContentRect();
#endif
    if (visibleRect.contains(absolutePoint))
        return absolutePoint;
    return std::nullopt;
}

RefPtr<Node> TreeScope::nodeFromPoint(const LayoutPoint& clientPoint, LayoutPoint* localPoint, HitTestSource source)
{
    Ref document = protectedDocumentScope();
    auto absolutePoint = absolutePointIfNotClipped(document, clientPoint);
    if (!absolutePoint)
        return nullptr;

    HitTestResult result(absolutePoint.value());
    document->hitTest({ source, HitTestRequest::defaultTypes }, result);
    if (localPoint)
        *localPoint = result.localPoint();
    return result.innerNode();
}

RefPtr<Element> TreeScope::elementFromPoint(double clientX, double clientY, HitTestSource source)
{
    if (!protectedDocumentScope()->hasLivingRenderTree())
        return nullptr;

    auto node = nodeFromPoint(LayoutPoint { clientX, clientY }, nullptr, source);
    if (!node)
        return nullptr;

    node = retargetToScope(*node);
    while (!is<Element>(*node)) {
        node = node->parentInComposedTree();
        if (!node)
            break;
        node = retargetToScope(*node);
    }

    return static_pointer_cast<Element>(WTFMove(node));
}

Vector<RefPtr<Element>> TreeScope::elementsFromPoint(double clientX, double clientY, HitTestSource source)
{
    Vector<RefPtr<Element>> elements;

    Ref document = protectedDocumentScope();
    if (!document->hasLivingRenderTree())
        return elements;

    auto absolutePoint = absolutePointIfNotClipped(document, LayoutPoint(clientX, clientY));
    if (!absolutePoint)
        return elements;

    static constexpr OptionSet hitTypes {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::Active,
        HitTestRequest::Type::DisallowUserAgentShadowContent,
        HitTestRequest::Type::CollectMultipleElements,
        HitTestRequest::Type::IncludeAllElementsUnderPoint
    };

    HitTestResult result { absolutePoint.value() };
    document->hitTest({ source, hitTypes }, result);

    RefPtr<Node> lastNode;
    auto& nodeSet = result.listBasedTestResult();
    elements.reserveInitialCapacity(nodeSet.size());
    for (auto& listBasedNode : nodeSet) {
        RefPtr<Node> node = retargetToScope(listBasedNode);
        while (!is<Element>(node)) {
            node = node->parentInComposedTree();
            if (!node)
                break;
            node = retargetToScope(*node);
        }

        if (!node)
            continue;

        if (auto pseudoElement = dynamicDowncast<PseudoElement>(*node))
            node = pseudoElement->hostElement();

        // Prune duplicate entries. A pseudo ::before content above its parent
        // node should only result in one entry.
        if (node == lastNode)
            continue;

        elements.append(static_pointer_cast<Element>(node));
        lastNode = node;
    }

    if (auto* rootDocument = dynamicDowncast<Document>(m_rootNode.get())) {
        if (Element* rootElement = rootDocument->documentElement()) {
            if (elements.isEmpty() || elements.last() != rootElement)
                elements.append(rootElement);
        }
    }

    return elements;
}

// FIXME: Would be nice to change this to take a StringView, since that's what callers have
// and there is no particular advantage to already having a String.
RefPtr<Element> TreeScope::findAnchor(StringView name)
{
    if (name.isEmpty())
        return nullptr;
    if (RefPtr element = getElementById(name))
        return element;
    auto inQuirksMode = documentScope().inQuirksMode();
    Ref rootNode = m_rootNode.get();
    for (Ref anchor : descendantsOfType<HTMLAnchorElement>(rootNode)) {
        if (inQuirksMode) {
            // Quirks mode, ASCII case-insensitive comparison of names.
            // FIXME: This behavior is not mentioned in the HTML specification.
            // We should either remove this or get this into the specification.
            if (equalIgnoringASCIICase(anchor->name(), name))
                return anchor;
        } else {
            // Strict mode, names need to match exactly.
            if (anchor->name() == name)
                return anchor;
        }
    }
    return nullptr;
}

static Element* focusedFrameOwnerElement(Frame* focusedFrame, LocalFrame* currentFrame)
{
    for (; focusedFrame; focusedFrame = focusedFrame->tree().parent()) {
        if (focusedFrame->tree().parent() == currentFrame)
            return focusedFrame->ownerElement();
    }
    return nullptr;
}

Element* TreeScope::focusedElementInScope()
{
    Ref document = protectedDocumentScope();
    RefPtr element = document->focusedElement();

    if (!element && document->page())
        element = focusedFrameOwnerElement(document->page()->focusController().focusedFrame(), document->frame());

    return ancestorElementInThisScope(element.get());
}

#if ENABLE(POINTER_LOCK)

Element* TreeScope::pointerLockElement() const
{
    Document& document = documentScope();
    Page* page = document.page();
    if (!page || page->pointerLockController().lockPending())
        return nullptr;
    auto* element = page->pointerLockController().element();
    if (!element || &element->document() != &document)
        return nullptr;
    return ancestorElementInThisScope(element);
}

#endif

static void listTreeScopes(Node* node, Vector<TreeScope*, 5>& treeScopes)
{
    while (true) {
        treeScopes.append(&node->treeScope());
        Element* ancestor = node->shadowHost();
        if (!ancestor)
            break;
        node = ancestor;
    }
}

TreeScope* commonTreeScope(Node* nodeA, Node* nodeB)
{
    if (!nodeA || !nodeB)
        return nullptr;

    if (&nodeA->treeScope() == &nodeB->treeScope())
        return &nodeA->treeScope();

    Vector<TreeScope*, 5> treeScopesA;
    listTreeScopes(nodeA, treeScopesA);

    Vector<TreeScope*, 5> treeScopesB;
    listTreeScopes(nodeB, treeScopesB);

    size_t indexA = treeScopesA.size();
    size_t indexB = treeScopesB.size();

    for (; indexA > 0 && indexB > 0 && treeScopesA[indexA - 1] == treeScopesB[indexB - 1]; --indexA, --indexB) { }

    // If the nodes had no common tree scope, return immediately.
    if (indexA == treeScopesA.size())
        return nullptr;
    
    return treeScopesA[indexA] == treeScopesB[indexB] ? treeScopesA[indexA] : nullptr;
}

RadioButtonGroups& TreeScope::radioButtonGroups()
{
    if (!m_radioButtonGroups)
        m_radioButtonGroups = makeUnique<RadioButtonGroups>();
    return *m_radioButtonGroups;
}

CSSStyleSheetObservableArray& TreeScope::ensureAdoptedStyleSheets()
{
    if (UNLIKELY(!m_adoptedStyleSheets))
        m_adoptedStyleSheets = CSSStyleSheetObservableArray::create(m_rootNode.get());
    return *m_adoptedStyleSheets;
}

std::span<const Ref<CSSStyleSheet>> TreeScope::adoptedStyleSheets() const
{
    return m_adoptedStyleSheets ? m_adoptedStyleSheets->sheets().span() : std::span<const Ref<CSSStyleSheet>> { };
}

JSC::JSValue TreeScope::adoptedStyleSheetWrapper(JSDOMGlobalObject& lexicalGlobalObject)
{
    return JSC::JSObservableArray::create(&lexicalGlobalObject, ensureAdoptedStyleSheets());
}

ExceptionOr<void> TreeScope::setAdoptedStyleSheets(Vector<Ref<CSSStyleSheet>>&& sheets)
{
    if (!m_adoptedStyleSheets && sheets.isEmpty())
        return { };
    return ensureAdoptedStyleSheets().setSheets(WTFMove(sheets));
}

SVGResourcesMap& TreeScope::svgResourcesMap() const
{
    if (!m_svgResourcesMap)
        const_cast<TreeScope&>(*this).m_svgResourcesMap = makeUnique<SVGResourcesMap>();
    return *m_svgResourcesMap;
}

void TreeScope::addSVGResource(const AtomString& id, LegacyRenderSVGResourceContainer& resource)
{
    if (id.isEmpty())
        return;

    // Replaces resource if already present, to handle potential id changes
    svgResourcesMap().legacyResources.set(id, &resource);
}

void TreeScope::removeSVGResource(const AtomString& id)
{
    if (id.isEmpty())
        return;

    svgResourcesMap().legacyResources.remove(id);
}

LegacyRenderSVGResourceContainer* TreeScope::lookupLegacySVGResoureById(const AtomString& id) const
{
    if (id.isEmpty())
        return nullptr;

    return svgResourcesMap().legacyResources.get(id);
}

void TreeScope::addPendingSVGResource(const AtomString& id, SVGElement& element)
{
    if (id.isEmpty())
        return;

    auto result = svgResourcesMap().pendingResources.add(id, WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> { });
    result.iterator->value.add(element);

    element.setHasPendingResources();
}

bool TreeScope::isIdOfPendingSVGResource(const AtomString& id) const
{
    if (id.isEmpty())
        return false;

    return svgResourcesMap().pendingResources.contains(id);
}

bool TreeScope::isElementWithPendingSVGResources(SVGElement& element) const
{
    // This algorithm takes time proportional to the number of pending resources and need not.
    // If performance becomes an issue we can keep a counted set of elements and answer the question efficiently.
    return WTF::anyOf(svgResourcesMap().pendingResources.values(), [&] (auto& elements) {
        return elements.contains(element);
    });
}

bool TreeScope::isPendingSVGResource(SVGElement& element, const AtomString& id) const
{
    if (id.isEmpty())
        return false;

    auto& pendingResources = svgResourcesMap().pendingResources;
    auto it = pendingResources.find(id);
    if (it == pendingResources.end())
        return false;

    return it->value.contains(element);
}

void TreeScope::clearHasPendingSVGResourcesIfPossible(SVGElement& element)
{
    if (!isElementWithPendingSVGResources(element))
        element.clearHasPendingResources();
}

void TreeScope::removeElementFromPendingSVGResources(SVGElement& element)
{
    if (!svgResourcesMap().pendingResources.isEmpty() && element.hasPendingResources()) {
        Vector<AtomString> toBeRemoved;
        for (auto& resource : svgResourcesMap().pendingResources) {
            auto& elements = resource.value;
            elements.remove(element);
            if (elements.isEmptyIgnoringNullReferences())
                toBeRemoved.append(resource.key);
        }

        clearHasPendingSVGResourcesIfPossible(element);

        // We use the removePendingResource function here because it deals with set lifetime correctly.
        for (auto& resource : toBeRemoved)
            removePendingSVGResource(resource);
    }

    if (!svgResourcesMap().pendingResourcesForRemoval.isEmpty()) {
        Vector<AtomString> toBeRemoved;
        for (auto& resource : svgResourcesMap().pendingResourcesForRemoval) {
            auto& elements = resource.value;
            elements.remove(element);
            if (elements.isEmptyIgnoringNullReferences())
                toBeRemoved.append(resource.key);
        }

        // We use m_pendingResourcesForRemoval here because it deals with set lifetime correctly.
        for (auto& resource : toBeRemoved)
            svgResourcesMap().pendingResourcesForRemoval.remove(resource);
    }
}

WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> TreeScope::removePendingSVGResource(const AtomString& id)
{
    return svgResourcesMap().pendingResources.take(id);
}

void TreeScope::markPendingSVGResourcesForRemoval(const AtomString& id)
{
    if (id.isEmpty())
        return;

    ASSERT(!svgResourcesMap().pendingResourcesForRemoval.contains(id));

    auto existing = svgResourcesMap().pendingResources.take(id);
    if (!existing.isEmptyIgnoringNullReferences())
        svgResourcesMap().pendingResourcesForRemoval.add(id, WTFMove(existing));
}

RefPtr<SVGElement> TreeScope::takeElementFromPendingSVGResourcesForRemovalMap(const AtomString& id)
{
    if (id.isEmpty())
        return nullptr;

    auto it = svgResourcesMap().pendingResourcesForRemoval.find(id);
    if (it == svgResourcesMap().pendingResourcesForRemoval.end())
        return nullptr;

    auto& resourceSet = it->value;
    RefPtr firstElement = resourceSet.begin().get();
    if (!firstElement)
        return nullptr;

    resourceSet.remove(*firstElement);

    if (resourceSet.isEmptyIgnoringNullReferences())
        svgResourcesMap().pendingResourcesForRemoval.remove(id);

    return firstElement;
}

Ref<Document> TreeScope::protectedDocumentScope() const
{
    return m_documentScope.get();
}

} // namespace WebCore

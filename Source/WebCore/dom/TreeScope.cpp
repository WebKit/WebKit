/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include "DOMWindow.h"
#include "ElementIterator.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HitTestResult.h"
#include "IdTargetObserverRegistry.h"
#include "NodeRareData.h"
#include "Page.h"
#include "PointerLockController.h"
#include "PseudoElement.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include <wtf/text/CString.h>

namespace WebCore {

struct SameSizeAsTreeScope {
    void* pointers[9];
};

COMPILE_ASSERT(sizeof(TreeScope) == sizeof(SameSizeAsTreeScope), treescope_should_stay_small);

using namespace HTMLNames;

TreeScope::TreeScope(ShadowRoot& shadowRoot, Document& document)
    : m_rootNode(shadowRoot)
    , m_documentScope(document)
    , m_parentTreeScope(&document)
    , m_idTargetObserverRegistry(std::make_unique<IdTargetObserverRegistry>())
{
    shadowRoot.setTreeScope(*this);
}

TreeScope::TreeScope(Document& document)
    : m_rootNode(document)
    , m_documentScope(document)
    , m_parentTreeScope(nullptr)
    , m_idTargetObserverRegistry(std::make_unique<IdTargetObserverRegistry>())
{
    document.setTreeScope(*this);
}

TreeScope::~TreeScope() = default;

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
    ASSERT(!m_rootNode.isDocumentNode());

    m_parentTreeScope = &newParentScope;
    setDocumentScope(newParentScope.documentScope());
}

Element* TreeScope::getElementById(const AtomicString& elementId) const
{
    if (elementId.isNull())
        return nullptr;
    if (!m_elementsById)
        return nullptr;
    return m_elementsById->getElementById(*elementId.impl(), *this);
}

Element* TreeScope::getElementById(const String& elementId) const
{
    if (!m_elementsById)
        return nullptr;

    if (RefPtr<AtomicStringImpl> atomicElementId = AtomicStringImpl::lookUp(elementId.impl()))
        return m_elementsById->getElementById(*atomicElementId, *this);

    return nullptr;
}

Element* TreeScope::getElementById(StringView elementId) const
{
    if (!m_elementsById)
        return nullptr;

    if (auto atomicElementId = elementId.toExistingAtomicString())
        return m_elementsById->getElementById(*atomicElementId, *this);

    return nullptr;
}

const Vector<Element*>* TreeScope::getAllElementsById(const AtomicString& elementId) const
{
    if (elementId.isEmpty())
        return nullptr;
    if (!m_elementsById)
        return nullptr;
    return m_elementsById->getAllElementsById(*elementId.impl(), *this);
}

void TreeScope::addElementById(const AtomicStringImpl& elementId, Element& element, bool notifyObservers)
{
    if (!m_elementsById)
        m_elementsById = std::make_unique<TreeScopeOrderedMap>();
    m_elementsById->add(elementId, element, *this);
    if (notifyObservers)
        m_idTargetObserverRegistry->notifyObservers(elementId);
}

void TreeScope::removeElementById(const AtomicStringImpl& elementId, Element& element, bool notifyObservers)
{
    if (!m_elementsById)
        return;
    m_elementsById->remove(elementId, element);
    if (notifyObservers)
        m_idTargetObserverRegistry->notifyObservers(elementId);
}

Element* TreeScope::getElementByName(const AtomicString& name) const
{
    if (name.isEmpty())
        return nullptr;
    if (!m_elementsByName)
        return nullptr;
    return m_elementsByName->getElementByName(*name.impl(), *this);
}

void TreeScope::addElementByName(const AtomicStringImpl& name, Element& element)
{
    if (!m_elementsByName)
        m_elementsByName = std::make_unique<TreeScopeOrderedMap>();
    m_elementsByName->add(name, element, *this);
}

void TreeScope::removeElementByName(const AtomicStringImpl& name, Element& element)
{
    if (!m_elementsByName)
        return;
    m_elementsByName->remove(name, element);
}


Node& TreeScope::retargetToScope(Node& node) const
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

    size_t i = nodeTreeScopes.size();
    size_t j = ancestorScopes.size();
    while (i > 0 && j > 0 && nodeTreeScopes[i - 1] == ancestorScopes[j - 1]) {
        --i;
        --j;
    }

    bool nodeIsInOuterTreeScope = !i;
    if (nodeIsInOuterTreeScope)
        return node;

    ShadowRoot& shadowRootInLowestCommonTreeScope = downcast<ShadowRoot>(nodeTreeScopes[i - 1]->rootNode());
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
    AtomicStringImpl* name = imageMap.getName().impl();
    if (!name)
        return;
    if (!m_imageMapsByName)
        m_imageMapsByName = std::make_unique<TreeScopeOrderedMap>();
    m_imageMapsByName->add(*name, imageMap, *this);
}

void TreeScope::removeImageMap(HTMLMapElement& imageMap)
{
    if (!m_imageMapsByName)
        return;
    AtomicStringImpl* name = imageMap.getName().impl();
    if (!name)
        return;
    m_imageMapsByName->remove(*name, imageMap);
}

HTMLMapElement* TreeScope::getImageMap(const AtomicString& name) const
{
    if (!m_imageMapsByName || !name.impl())
        return nullptr;
    return m_imageMapsByName->getElementByMapName(*name.impl(), *this);
}

void TreeScope::addImageElementByUsemap(const AtomicStringImpl& name, HTMLImageElement& element)
{
    if (!m_imagesByUsemap)
        m_imagesByUsemap = std::make_unique<TreeScopeOrderedMap>();
    return m_imagesByUsemap->add(name, element, *this);
}

void TreeScope::removeImageElementByUsemap(const AtomicStringImpl& name, HTMLImageElement& element)
{
    if (!m_imagesByUsemap)
        return;
    m_imagesByUsemap->remove(name, element);
}

HTMLImageElement* TreeScope::imageElementByUsemap(const AtomicStringImpl& name) const
{
    if (!m_imagesByUsemap)
        return nullptr;
    return m_imagesByUsemap->getElementByUsemap(name, *this);
}

void TreeScope::addLabel(const AtomicStringImpl& forAttributeValue, HTMLLabelElement& element)
{
    ASSERT(m_labelsByForAttribute);
    m_labelsByForAttribute->add(forAttributeValue, element, *this);
}

void TreeScope::removeLabel(const AtomicStringImpl& forAttributeValue, HTMLLabelElement& element)
{
    ASSERT(m_labelsByForAttribute);
    m_labelsByForAttribute->remove(forAttributeValue, element);
}

HTMLLabelElement* TreeScope::labelElementForId(const AtomicString& forAttributeValue)
{
    if (forAttributeValue.isEmpty())
        return nullptr;

    if (!m_labelsByForAttribute) {
        // Populate the map on first access.
        m_labelsByForAttribute = std::make_unique<TreeScopeOrderedMap>();

        for (auto& label : descendantsOfType<HTMLLabelElement>(m_rootNode)) {
            const AtomicString& forValue = label.attributeWithoutSynchronization(forAttr);
            if (!forValue.isEmpty())
                addLabel(*forValue.impl(), label);
        }
    }

    return m_labelsByForAttribute->getElementByLabelForAttribute(*forAttributeValue.impl(), *this);
}

static Optional<LayoutPoint> absolutePointIfNotClipped(Document& document, const LayoutPoint& clientPoint)
{
    if (!document.frame() || !document.view())
        return WTF::nullopt;

    const auto& settings = document.frame()->settings();
    if (settings.visualViewportEnabled() && settings.clientCoordinatesRelativeToLayoutViewport()) {
        document.updateLayout();
        if (!document.view() || !document.hasLivingRenderTree())
            return WTF::nullopt;
        auto* view = document.view();
        FloatPoint layoutViewportPoint = view->clientToLayoutViewportPoint(clientPoint);
        FloatRect layoutViewportBounds({ }, view->layoutViewportRect().size());
        if (!layoutViewportBounds.contains(layoutViewportPoint))
            return WTF::nullopt;
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
    return WTF::nullopt;
}

Node* TreeScope::nodeFromPoint(const LayoutPoint& clientPoint, LayoutPoint* localPoint)
{
    auto absolutePoint = absolutePointIfNotClipped(documentScope(), clientPoint);
    if (!absolutePoint)
        return nullptr;

    HitTestResult result(absolutePoint.value());
    documentScope().renderView()->hitTest(HitTestRequest(), result);

    if (localPoint)
        *localPoint = result.localPoint();

    return result.innerNode();
}

RefPtr<Element> TreeScope::elementFromPoint(double clientX, double clientY)
{
    Document& document = documentScope();
    if (!document.hasLivingRenderTree())
        return nullptr;

    Node* node = nodeFromPoint(LayoutPoint(clientX, clientY), nullptr);
    if (!node)
        return nullptr;

    node = &retargetToScope(*node);
    while (!is<Element>(*node)) {
        node = node->parentInComposedTree();
        if (!node)
            break;
        node = &retargetToScope(*node);
    }

    return downcast<Element>(node);
}

Vector<RefPtr<Element>> TreeScope::elementsFromPoint(double clientX, double clientY)
{
    Vector<RefPtr<Element>> elements;

    Document& document = documentScope();
    if (!document.hasLivingRenderTree())
        return elements;

    auto absolutePoint = absolutePointIfNotClipped(document, LayoutPoint(clientX, clientY));
    if (!absolutePoint)
        return elements;

    HitTestRequest request(HitTestRequest::ReadOnly
        | HitTestRequest::Active
        | HitTestRequest::DisallowUserAgentShadowContent
        | HitTestRequest::CollectMultipleElements
        | HitTestRequest::IncludeAllElementsUnderPoint);
    HitTestResult result(absolutePoint.value());
    documentScope().renderView()->hitTest(request, result);

    Node* lastNode = nullptr;
    for (const auto& listBasedNode : result.listBasedTestResult()) {
        Node* node = listBasedNode.get();
        node = &retargetToScope(*node);
        while (!is<Element>(*node)) {
            node = node->parentInComposedTree();
            if (!node)
                break;
            node = &retargetToScope(*node);
        }

        if (!node)
            continue;

        if (is<PseudoElement>(node))
            node = downcast<PseudoElement>(*node).hostElement();

        // Prune duplicate entries. A pseudo ::before content above its parent
        // node should only result in one entry.
        if (node == lastNode)
            continue;

        elements.append(downcast<Element>(node));
        lastNode = node;
    }

    if (m_rootNode.isDocumentNode()) {
        if (Element* rootElement = downcast<Document>(m_rootNode).documentElement()) {
            if (elements.isEmpty() || elements.last() != rootElement)
                elements.append(rootElement);
        }
    }

    return elements;
}

Element* TreeScope::findAnchor(const String& name)
{
    if (name.isEmpty())
        return nullptr;
    if (Element* element = getElementById(name))
        return element;
    for (auto& anchor : descendantsOfType<HTMLAnchorElement>(m_rootNode)) {
        if (m_rootNode.document().inQuirksMode()) {
            // Quirks mode, ASCII case-insensitive comparison of names.
            // FIXME: This behavior is not mentioned in the HTML specification.
            // We should either remove this or get this into the specification.
            if (equalIgnoringASCIICase(anchor.name(), name))
                return &anchor;
        } else {
            // Strict mode, names need to match exactly.
            if (anchor.name() == name)
                return &anchor;
        }
    }
    return nullptr;
}

static Element* focusedFrameOwnerElement(Frame* focusedFrame, Frame* currentFrame)
{
    for (; focusedFrame; focusedFrame = focusedFrame->tree().parent()) {
        if (focusedFrame->tree().parent() == currentFrame)
            return focusedFrame->ownerElement();
    }
    return nullptr;
}

Element* TreeScope::focusedElementInScope()
{
    Document& document = documentScope();
    Element* element = document.focusedElement();

    if (!element && document.page())
        element = focusedFrameOwnerElement(document.page()->focusController().focusedFrame(), document.frame());

    return ancestorElementInThisScope(element);
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

} // namespace WebCore

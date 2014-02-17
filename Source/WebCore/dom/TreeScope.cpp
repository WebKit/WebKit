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

#include "DOMSelection.h"
#include "DOMWindow.h"
#include "ElementIterator.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HitTestResult.h"
#include "IdTargetObserverRegistry.h"
#include "Page.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "ShadowRoot.h"
#include "TreeScopeAdopter.h"
#include <wtf/text/CString.h>

namespace WebCore {

struct SameSizeAsTreeScope {
    void* pointers[9];
};

COMPILE_ASSERT(sizeof(TreeScope) == sizeof(SameSizeAsTreeScope), treescope_should_stay_small);

using namespace HTMLNames;

TreeScope::TreeScope(ShadowRoot& shadowRoot, Document& document)
    : m_rootNode(shadowRoot)
    , m_documentScope(&document)
    , m_parentTreeScope(&document)
    , m_idTargetObserverRegistry(std::make_unique<IdTargetObserverRegistry>())
{
    shadowRoot.setTreeScope(*this);
}

TreeScope::TreeScope(Document& document)
    : m_rootNode(document)
    , m_documentScope(&document)
    , m_parentTreeScope(nullptr)
    , m_idTargetObserverRegistry(std::make_unique<IdTargetObserverRegistry>())
{
    document.setTreeScope(*this);
}

TreeScope::~TreeScope()
{
    if (m_selection) {
        m_selection->clearTreeScope();
        m_selection = nullptr;
    }
}

void TreeScope::destroyTreeScopeData()
{
    m_elementsById = nullptr;
    m_imageMapsByName = nullptr;
    m_labelsByForAttribute = nullptr;
}

void TreeScope::setParentTreeScope(TreeScope* newParentScope)
{
    // A document node cannot be re-parented.
    ASSERT(!m_rootNode.isDocumentNode());
    // Every scope other than document needs a parent scope.
    ASSERT(newParentScope);

    m_parentTreeScope = newParentScope;
    setDocumentScope(&newParentScope->documentScope());
}

Element* TreeScope::getElementById(const AtomicString& elementId) const
{
    if (elementId.isEmpty())
        return nullptr;
    if (!m_elementsById)
        return nullptr;
    return m_elementsById->getElementById(*elementId.impl(), *this);
}

const Vector<Element*>* TreeScope::getAllElementsById(const AtomicString& elementId) const
{
    if (elementId.isEmpty())
        return nullptr;
    if (!m_elementsById)
        return nullptr;
    return m_elementsById->getAllElementsById(*elementId.impl(), *this);
}

void TreeScope::addElementById(const AtomicStringImpl& elementId, Element& element)
{
    if (!m_elementsById)
        m_elementsById = std::make_unique<DocumentOrderedMap>();
    m_elementsById->add(elementId, element, *this);
    m_idTargetObserverRegistry->notifyObservers(elementId);
}

void TreeScope::removeElementById(const AtomicStringImpl& elementId, Element& element)
{
    if (!m_elementsById)
        return;
    m_elementsById->remove(elementId, element);
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
        m_elementsByName = std::make_unique<DocumentOrderedMap>();
    m_elementsByName->add(name, element, *this);
}

void TreeScope::removeElementByName(const AtomicStringImpl& name, Element& element)
{
    if (!m_elementsByName)
        return;
    m_elementsByName->remove(name, element);
}

Node* TreeScope::ancestorInThisScope(Node* node) const
{
    for (; node; node = node->shadowHost()) {
        if (&node->treeScope() == this)
            return node;
        if (!node->isInShadowTree())
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
        m_imageMapsByName = std::make_unique<DocumentOrderedMap>();
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

HTMLMapElement* TreeScope::getImageMap(const String& url) const
{
    if (url.isNull())
        return nullptr;
    if (!m_imageMapsByName)
        return nullptr;
    size_t hashPos = url.find('#');
    String name = (hashPos == notFound ? url : url.substring(hashPos + 1)).impl();
    if (name.isEmpty())
        return nullptr;
    if (m_rootNode.document().isHTMLDocument()) {
        AtomicString lowercasedName = name.lower();
        return m_imageMapsByName->getElementByLowercasedMapName(*lowercasedName.impl(), *this);
    }
    return m_imageMapsByName->getElementByMapName(*AtomicString(name).impl(), *this);
}

Node* nodeFromPoint(Document* document, int x, int y, LayoutPoint* localPoint)
{
    Frame* frame = document->frame();

    if (!frame)
        return nullptr;
    FrameView* frameView = frame->view();
    if (!frameView)
        return nullptr;

    float scaleFactor = frame->pageZoomFactor() * frame->frameScaleFactor();
#if !PLATFORM(IOS)
    IntPoint point = roundedIntPoint(FloatPoint(x * scaleFactor  + frameView->scrollX(), y * scaleFactor + frameView->scrollY()));

    if (!frameView->visibleContentRect().contains(point))
        return nullptr;
#else
    IntPoint point = roundedIntPoint(FloatPoint(x * scaleFactor  + frameView->actualScrollX(), y * scaleFactor + frameView->actualScrollY()));

    if (!frameView->actualVisibleContentRect().contains(point))
        return nullptr;
#endif
    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);
    HitTestResult result(point);
    document->renderView()->hitTest(request, result);

    if (localPoint)
        *localPoint = result.localPoint();

    return result.innerNode();
}

Element* TreeScope::elementFromPoint(int x, int y) const
{
    Node* node = nodeFromPoint(&m_rootNode.document(), x, y);
    while (node && !node->isElementNode())
        node = node->parentNode();
    if (node)
        node = ancestorInThisScope(node);
    return toElement(node);
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
        m_labelsByForAttribute = std::make_unique<DocumentOrderedMap>();

        for (auto& label : descendantsOfType<HTMLLabelElement>(m_rootNode)) {
            const AtomicString& forValue = label.fastGetAttribute(forAttr);
            if (!forValue.isEmpty())
                addLabel(*forValue.impl(), label);
        }
    }

    return m_labelsByForAttribute->getElementByLabelForAttribute(*forAttributeValue.impl(), *this);
}

DOMSelection* TreeScope::getSelection() const
{
    if (!m_rootNode.document().frame())
        return nullptr;

    if (m_selection)
        return m_selection.get();

    if (this != &m_rootNode.document())
        return m_rootNode.document().getSelection();

    m_selection = DOMSelection::create(&m_rootNode.document());
    return m_selection.get();
}

Element* TreeScope::findAnchor(const String& name)
{
    if (name.isEmpty())
        return nullptr;
    if (Element* element = getElementById(name))
        return element;
    for (auto& anchor : descendantsOfType<HTMLAnchorElement>(m_rootNode)) {
        if (m_rootNode.document().inQuirksMode()) {
            // Quirks mode, case insensitive comparison of names.
            if (equalIgnoringCase(anchor.name(), name))
                return &anchor;
        } else {
            // Strict mode, names need to match exactly.
            if (anchor.name() == name)
                return &anchor;
        }
    }
    return nullptr;
}

void TreeScope::adoptIfNeeded(Node* node)
{
    ASSERT(this);
    ASSERT(node);
    ASSERT(!node->isDocumentNode());
    ASSERT(!node->m_deletionHasBegun);
    TreeScopeAdopter adopter(node, *this);
    if (adopter.needsScopeChange())
        adopter.execute();
}

static Element* focusedFrameOwnerElement(Frame* focusedFrame, Frame* currentFrame)
{
    for (; focusedFrame; focusedFrame = focusedFrame->tree().parent()) {
        if (focusedFrame->tree().parent() == currentFrame)
            return focusedFrame->ownerElement();
    }
    return nullptr;
}

Element* TreeScope::focusedElement()
{
    Document& document = m_rootNode.document();
    Element* element = document.focusedElement();

    if (!element && document.page())
        element = focusedFrameOwnerElement(document.page()->focusController().focusedFrame(), document.frame());
    if (!element)
        return nullptr;
    TreeScope* treeScope = &element->treeScope();
    while (treeScope != this && treeScope != &document) {
        element = toShadowRoot(treeScope->rootNode()).hostElement();
        treeScope = &element->treeScope();
    }
    if (this != treeScope)
        return nullptr;
    return element;
}

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

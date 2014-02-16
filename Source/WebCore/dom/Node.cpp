/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "Node.h"

#include "AXObjectCache.h"
#include "Attr.h"
#include "BeforeLoadEvent.h"
#include "ChildListMutationScope.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CSSParser.h"
#include "CSSRule.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "ContainerNodeAlgorithms.h"
#include "ContextMenuController.h"
#include "DOMImplementation.h"
#include "DocumentType.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "EventDispatcher.h"
#include "EventException.h"
#include "EventHandler.h"
#include "FlowThreadController.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLStyleElement.h"
#include "InsertionPoint.h"
#include "KeyboardEvent.h"
#include "Logging.h"
#include "MutationEvent.h"
#include "NodeRenderStyle.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ProcessingInstruction.h"
#include "ProgressEvent.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "ScopedEventQueue.h"
#include "Settings.h"
#include "StorageEvent.h"
#include "StyleResolver.h"
#include "TemplateContentDocumentFragment.h"
#include "TextEvent.h"
#include "TouchEvent.h"
#include "TreeScopeAdopter.h"
#include "WheelEvent.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include <runtime/JSCInlines.h>
#include <runtime/VM.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(INDIE_UI)
#include "UIRequestEvent.h"
#endif

#if ENABLE(INSPECTOR)
#include "InspectorController.h"
#endif

namespace WebCore {

using namespace HTMLNames;

bool Node::isSupported(const String& feature, const String& version)
{
    return DOMImplementation::hasFeature(feature, version);
}

#if DUMP_NODE_STATISTICS
static HashSet<Node*> liveNodeSet;
#endif

void Node::dumpStatistics()
{
#if DUMP_NODE_STATISTICS
    size_t nodesWithRareData = 0;

    size_t elementNodes = 0;
    size_t attrNodes = 0;
    size_t textNodes = 0;
    size_t cdataNodes = 0;
    size_t commentNodes = 0;
    size_t entityReferenceNodes = 0;
    size_t entityNodes = 0;
    size_t piNodes = 0;
    size_t documentNodes = 0;
    size_t docTypeNodes = 0;
    size_t fragmentNodes = 0;
    size_t notationNodes = 0;
    size_t xpathNSNodes = 0;
    size_t shadowRootNodes = 0;

    HashMap<String, size_t> perTagCount;

    size_t attributes = 0;
    size_t attributesWithAttr = 0;
    size_t elementsWithAttributeStorage = 0;
    size_t elementsWithRareData = 0;
    size_t elementsWithNamedNodeMap = 0;

    for (HashSet<Node*>::iterator it = liveNodeSet.begin(); it != liveNodeSet.end(); ++it) {
        Node* node = *it;

        if (node->hasRareData()) {
            ++nodesWithRareData;
            if (node->isElementNode()) {
                ++elementsWithRareData;
                if (toElement(node)->hasNamedNodeMap())
                    ++elementsWithNamedNodeMap;
            }
        }

        switch (node->nodeType()) {
            case ELEMENT_NODE: {
                ++elementNodes;

                // Tag stats
                Element* element = toElement(node);
                HashMap<String, size_t>::AddResult result = perTagCount.add(element->tagName(), 1);
                if (!result.isNewEntry)
                    result.iterator->value++;

                if (ElementData* elementData = element->elementData()) {
                    unsigned length = elementData->length();
                    attributes += length;
                    ++elementsWithAttributeStorage;
                    for (unsigned i = 0; i < length; ++i) {
                        Attribute& attr = elementData->attributeAt(i);
                        if (attr.attr())
                            ++attributesWithAttr;
                    }
                }
                break;
            }
            case ATTRIBUTE_NODE: {
                ++attrNodes;
                break;
            }
            case TEXT_NODE: {
                ++textNodes;
                break;
            }
            case CDATA_SECTION_NODE: {
                ++cdataNodes;
                break;
            }
            case COMMENT_NODE: {
                ++commentNodes;
                break;
            }
            case ENTITY_REFERENCE_NODE: {
                ++entityReferenceNodes;
                break;
            }
            case ENTITY_NODE: {
                ++entityNodes;
                break;
            }
            case PROCESSING_INSTRUCTION_NODE: {
                ++piNodes;
                break;
            }
            case DOCUMENT_NODE: {
                ++documentNodes;
                break;
            }
            case DOCUMENT_TYPE_NODE: {
                ++docTypeNodes;
                break;
            }
            case DOCUMENT_FRAGMENT_NODE: {
                if (node->isShadowRoot())
                    ++shadowRootNodes;
                else
                    ++fragmentNodes;
                break;
            }
            case NOTATION_NODE: {
                ++notationNodes;
                break;
            }
            case XPATH_NAMESPACE_NODE: {
                ++xpathNSNodes;
                break;
            }
        }
    }

    printf("Number of Nodes: %d\n\n", liveNodeSet.size());
    printf("Number of Nodes with RareData: %zu\n\n", nodesWithRareData);

    printf("NodeType distribution:\n");
    printf("  Number of Element nodes: %zu\n", elementNodes);
    printf("  Number of Attribute nodes: %zu\n", attrNodes);
    printf("  Number of Text nodes: %zu\n", textNodes);
    printf("  Number of CDATASection nodes: %zu\n", cdataNodes);
    printf("  Number of Comment nodes: %zu\n", commentNodes);
    printf("  Number of EntityReference nodes: %zu\n", entityReferenceNodes);
    printf("  Number of Entity nodes: %zu\n", entityNodes);
    printf("  Number of ProcessingInstruction nodes: %zu\n", piNodes);
    printf("  Number of Document nodes: %zu\n", documentNodes);
    printf("  Number of DocumentType nodes: %zu\n", docTypeNodes);
    printf("  Number of DocumentFragment nodes: %zu\n", fragmentNodes);
    printf("  Number of Notation nodes: %zu\n", notationNodes);
    printf("  Number of XPathNS nodes: %zu\n", xpathNSNodes);
    printf("  Number of ShadowRoot nodes: %zu\n", shadowRootNodes);

    printf("Element tag name distibution:\n");
    for (HashMap<String, size_t>::iterator it = perTagCount.begin(); it != perTagCount.end(); ++it)
        printf("  Number of <%s> tags: %zu\n", it->key.utf8().data(), it->value);

    printf("Attributes:\n");
    printf("  Number of Attributes (non-Node and Node): %zu [%zu]\n", attributes, sizeof(Attribute));
    printf("  Number of Attributes with an Attr: %zu\n", attributesWithAttr);
    printf("  Number of Elements with attribute storage: %zu [%zu]\n", elementsWithAttributeStorage, sizeof(ElementData));
    printf("  Number of Elements with RareData: %zu\n", elementsWithRareData);
    printf("  Number of Elements with NamedNodeMap: %zu [%zu]\n", elementsWithNamedNodeMap, sizeof(NamedNodeMap));
#endif
}

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, nodeCounter, ("WebCoreNode"));
DEFINE_DEBUG_ONLY_GLOBAL(HashSet<Node*>, ignoreSet, );

#ifndef NDEBUG
static bool shouldIgnoreLeaks = false;
#endif

void Node::startIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = true;
#endif
}

void Node::stopIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = false;
#endif
}

void Node::trackForDebugging()
{
#ifndef NDEBUG
    if (shouldIgnoreLeaks)
        ignoreSet.add(this);
    else
        nodeCounter.increment();
#endif

#if DUMP_NODE_STATISTICS
    liveNodeSet.add(this);
#endif
}

Node::~Node()
{
#ifndef NDEBUG
    if (!ignoreSet.remove(this))
        nodeCounter.decrement();
#endif

#if DUMP_NODE_STATISTICS
    liveNodeSet.remove(this);
#endif

    ASSERT(!renderer());
    ASSERT(!parentNode());
    ASSERT(!m_previous);
    ASSERT(!m_next);

    if (hasRareData())
        clearRareData();

    if (!isContainerNode())
        willBeDeletedFrom(&document());

    document().selfOnlyDeref();

    InspectorCounters::decrementCounter(InspectorCounters::NodeCounter);
}

void Node::willBeDeletedFrom(Document* document)
{
    if (hasEventTargetData()) {
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS)
        if (document)
            document->removeTouchEventListener(this, true);
#endif
        clearEventTargetData();
    }

    if (document) {
        if (AXObjectCache* cache = document->existingAXObjectCache())
            cache->remove(this);
    }
}

NodeRareData* Node::rareData() const
{
    ASSERT_WITH_SECURITY_IMPLICATION(hasRareData());
    return static_cast<NodeRareData*>(m_data.m_rareData);
}

NodeRareData& Node::ensureRareData()
{
    if (hasRareData())
        return *rareData();

    NodeRareData* data;
    if (isElementNode())
        data = ElementRareData::create(toRenderElement(m_data.m_renderer)).leakPtr();
    else
        data = NodeRareData::create(m_data.m_renderer).leakPtr();
    ASSERT(data);

    m_data.m_rareData = data;
    setFlag(HasRareDataFlag);
    return *data;
}

void Node::clearRareData()
{
    ASSERT(hasRareData());
    ASSERT(!transientMutationObserverRegistry() || transientMutationObserverRegistry()->isEmpty());

    RenderObject* renderer = m_data.m_rareData->renderer();
    if (isElementNode())
        delete static_cast<ElementRareData*>(m_data.m_rareData);
    else
        delete static_cast<NodeRareData*>(m_data.m_rareData);
    m_data.m_renderer = renderer;
    clearFlag(HasRareDataFlag);
}

Node* Node::toNode()
{
    return this;
}

HTMLInputElement* Node::toInputElement()
{
    // If one of the below ASSERTs trigger, you are calling this function
    // directly or indirectly from a constructor or destructor of this object.
    // Don't do this!
    ASSERT(!(isHTMLElement() && hasTagName(inputTag)));
    return 0;
}

String Node::nodeValue() const
{
    return String();
}

void Node::setNodeValue(const String& /*nodeValue*/, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // By default, setting nodeValue has no effect.
}

PassRefPtr<NodeList> Node::childNodes()
{
    if (isContainerNode())
        return ensureRareData().ensureNodeLists().ensureChildNodeList(toContainerNode(*this));
    return ensureRareData().ensureNodeLists().ensureEmptyChildNodeList(*this);
}

Node *Node::lastDescendant() const
{
    Node *n = const_cast<Node *>(this);
    while (n && n->lastChild())
        n = n->lastChild();
    return n;
}

Node* Node::firstDescendant() const
{
    Node *n = const_cast<Node *>(this);
    while (n && n->firstChild())
        n = n->firstChild();
    return n;
}

bool Node::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec)
{
    if (!isContainerNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return false;
    }
    return toContainerNode(this)->insertBefore(newChild, refChild, ec);
}

bool Node::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& ec)
{
    if (!isContainerNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return false;
    }
    return toContainerNode(this)->replaceChild(newChild, oldChild, ec);
}

bool Node::removeChild(Node* oldChild, ExceptionCode& ec)
{
    if (!isContainerNode()) {
        ec = NOT_FOUND_ERR;
        return false;
    }
    return toContainerNode(this)->removeChild(oldChild, ec);
}

bool Node::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec)
{
    if (!isContainerNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return false;
    }
    return toContainerNode(this)->appendChild(newChild, ec);
}

void Node::remove(ExceptionCode& ec)
{
    if (ContainerNode* parent = parentNode())
        parent->removeChild(this, ec);
}

void Node::normalize()
{
    // Go through the subtree beneath us, normalizing all nodes. This means that
    // any two adjacent text nodes are merged and any empty text nodes are removed.

    RefPtr<Node> node = this;
    while (Node* firstChild = node->firstChild())
        node = firstChild;
    while (node) {
        NodeType type = node->nodeType();
        if (type == ELEMENT_NODE)
            toElement(node.get())->normalizeAttributes();

        if (node == this)
            break;

        if (type != TEXT_NODE) {
            node = NodeTraversal::nextPostOrder(node.get());
            continue;
        }

        RefPtr<Text> text = toText(node.get());

        // Remove empty text nodes.
        if (!text->length()) {
            // Care must be taken to get the next node before removing the current node.
            node = NodeTraversal::nextPostOrder(node.get());
            text->remove(IGNORE_EXCEPTION);
            continue;
        }

        // Merge text nodes.
        while (Node* nextSibling = node->nextSibling()) {
            if (nextSibling->nodeType() != TEXT_NODE)
                break;
            RefPtr<Text> nextText = toText(nextSibling);

            // Remove empty text nodes.
            if (!nextText->length()) {
                nextText->remove(IGNORE_EXCEPTION);
                continue;
            }

            // Both non-empty text nodes. Merge them.
            unsigned offset = text->length();
            text->appendData(nextText->data(), IGNORE_EXCEPTION);
            document().textNodesMerged(nextText.get(), offset);
            nextText->remove(IGNORE_EXCEPTION);
        }

        node = NodeTraversal::nextPostOrder(node.get());
    }
}

const AtomicString& Node::prefix() const
{
    // For nodes other than elements and attributes, the prefix is always null
    return nullAtom;
}

void Node::setPrefix(const AtomicString& /*prefix*/, ExceptionCode& ec)
{
    // The spec says that for nodes other than elements and attributes, prefix is always null.
    // It does not say what to do when the user tries to set the prefix on another type of
    // node, however Mozilla throws a NAMESPACE_ERR exception.
    ec = NAMESPACE_ERR;
}

const AtomicString& Node::localName() const
{
    return nullAtom;
}

const AtomicString& Node::namespaceURI() const
{
    return nullAtom;
}

bool Node::isContentEditable(UserSelectAllTreatment treatment)
{
    document().updateStyleIfNeeded();
    return hasEditableStyle(Editable, treatment);
}

bool Node::isContentRichlyEditable()
{
    document().updateStyleIfNeeded();
    return hasEditableStyle(RichlyEditable, UserSelectAllIsAlwaysNonEditable);
}

void Node::inspect()
{
#if ENABLE(INSPECTOR)
    if (document().page())
        document().page()->inspectorController().inspect(this);
#endif
}

bool Node::hasEditableStyle(EditableLevel editableLevel, UserSelectAllTreatment treatment) const
{
    if (!document().hasLivingRenderTree())
        return false;
    if (document().frame() && document().frame()->page() && document().frame()->page()->isEditable() && !containingShadowRoot())
        return true;

    if (isPseudoElement())
        return false;

    // Ideally we'd call ASSERT(!needsStyleRecalc()) here, but
    // ContainerNode::setFocus() calls setNeedsStyleRecalc(), so the assertion
    // would fire in the middle of Document::setFocusedElement().

    for (const Node* node = this; node; node = node->parentNode()) {
        RenderStyle* style = node->isDocumentNode() ? node->renderStyle() : const_cast<Node*>(node)->computedStyle();
        if (!style)
            continue;
        if (style->display() == NONE)
            continue;
#if ENABLE(USERSELECT_ALL)
        // Elements with user-select: all style are considered atomic
        // therefore non editable.
        if (treatment == UserSelectAllIsAlwaysNonEditable && style->userSelect() == SELECT_ALL)
            return false;
#else
        UNUSED_PARAM(treatment);
#endif
        switch (style->userModify()) {
        case READ_ONLY:
            return false;
        case READ_WRITE:
            return true;
        case READ_WRITE_PLAINTEXT_ONLY:
            return editableLevel != RichlyEditable;
        }
        ASSERT_NOT_REACHED();
        return false;
    }
    return false;
}

bool Node::isEditableToAccessibility(EditableLevel editableLevel) const
{
    if (hasEditableStyle(editableLevel))
        return true;

    // FIXME: Respect editableLevel for ARIA editable elements.
    if (editableLevel == RichlyEditable)
        return false;

    ASSERT(AXObjectCache::accessibilityEnabled());
    ASSERT(document().existingAXObjectCache());

    if (AXObjectCache* cache = document().existingAXObjectCache())
        return cache->rootAXEditableElement(this);

    return false;
}

RenderBox* Node::renderBox() const
{
    RenderObject* renderer = this->renderer();
    return renderer && renderer->isBox() ? toRenderBox(renderer) : 0;
}

RenderBoxModelObject* Node::renderBoxModelObject() const
{
    RenderObject* renderer = this->renderer();
    return renderer && renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : 0;
}

LayoutRect Node::boundingBox() const
{
    if (renderer())
        return renderer()->absoluteBoundingBoxRect();
    return LayoutRect();
}
    
LayoutRect Node::renderRect(bool* isReplaced)
{    
    RenderObject* hitRenderer = this->renderer();
    ASSERT(hitRenderer);
    RenderObject* renderer = hitRenderer;
    while (renderer && !renderer->isBody() && !renderer->isRoot()) {
        if (renderer->isRenderBlock() || renderer->isInlineBlockOrInlineTable() || renderer->isReplaced()) {
            *isReplaced = renderer->isReplaced();
            return renderer->absoluteBoundingBoxRect();
        }
        renderer = renderer->parent();
    }
    return LayoutRect();    
}

void Node::markAncestorsWithChildNeedsStyleRecalc()
{
    ContainerNode* ancestor = isPseudoElement() ? toPseudoElement(this)->hostElement() : parentOrShadowHostNode();
    for (; ancestor && !ancestor->childNeedsStyleRecalc(); ancestor = ancestor->parentOrShadowHostNode())
        ancestor->setChildNeedsStyleRecalc();

    if (document().childNeedsStyleRecalc())
        document().scheduleStyleRecalc();
}

void Node::refEventTarget()
{
    ref();
}

void Node::derefEventTarget()
{
    deref();
}

void Node::setNeedsStyleRecalc(StyleChangeType changeType)
{
    ASSERT(changeType != NoStyleChange);
    if (!inRenderedDocument())
        return;

    StyleChangeType existingChangeType = styleChangeType();
    if (changeType > existingChangeType)
        setStyleChange(changeType);

    if (existingChangeType == NoStyleChange || changeType == ReconstructRenderTree)
        markAncestorsWithChildNeedsStyleRecalc();
}

unsigned Node::nodeIndex() const
{
    Node *_tempNode = previousSibling();
    unsigned count=0;
    for ( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
}

template<unsigned type>
bool shouldInvalidateNodeListCachesForAttr(const unsigned nodeListCounts[], const QualifiedName& attrName)
{
    if (nodeListCounts[type] && shouldInvalidateTypeOnAttributeChange(static_cast<NodeListInvalidationType>(type), attrName))
        return true;
    return shouldInvalidateNodeListCachesForAttr<type + 1>(nodeListCounts, attrName);
}

template<>
bool shouldInvalidateNodeListCachesForAttr<numNodeListInvalidationTypes>(const unsigned[], const QualifiedName&)
{
    return false;
}

bool Document::shouldInvalidateNodeListAndCollectionCaches(const QualifiedName* attrName) const
{
    if (attrName)
        return shouldInvalidateNodeListCachesForAttr<DoNotInvalidateOnAttributeChanges + 1>(m_nodeListAndCollectionCounts, *attrName);

    for (int type = 0; type < numNodeListInvalidationTypes; type++) {
        if (m_nodeListAndCollectionCounts[type])
            return true;
    }

    return false;
}

void Document::invalidateNodeListAndCollectionCaches(const QualifiedName* attrName)
{
    for (HashSet<LiveNodeList*>::iterator it = m_listsInvalidatedAtDocument.begin(), end = m_listsInvalidatedAtDocument.end(); it != end; ++it)
        (*it)->invalidateCache(attrName);
    for (HashSet<HTMLCollection*>::iterator it = m_collectionsInvalidatedAtDocument.begin(), end = m_collectionsInvalidatedAtDocument.end(); it != end; ++it)
        (*it)->invalidateCache(attrName);
}

void Node::invalidateNodeListAndCollectionCachesInAncestors(const QualifiedName* attrName, Element* attributeOwnerElement)
{
    if (hasRareData() && (!attrName || isAttributeNode())) {
        if (NodeListsNodeData* lists = rareData()->nodeLists())
            lists->clearChildNodeListCache();
    }

    // Modifications to attributes that are not associated with an Element can't invalidate NodeList caches.
    if (attrName && !attributeOwnerElement)
        return;

    if (!document().shouldInvalidateNodeListAndCollectionCaches(attrName))
        return;

    document().invalidateNodeListAndCollectionCaches(attrName);

    for (Node* node = this; node; node = node->parentNode()) {
        if (!node->hasRareData())
            continue;
        NodeRareData* data = node->rareData();
        if (data->nodeLists())
            data->nodeLists()->invalidateCaches(attrName);
    }
}

NodeListsNodeData* Node::nodeLists()
{
    return hasRareData() ? rareData()->nodeLists() : 0;
}

void Node::clearNodeLists()
{
    rareData()->clearNodeLists();
}

void Node::checkSetPrefix(const AtomicString& prefix, ExceptionCode& ec)
{
    // Perform error checking as required by spec for setting Node.prefix. Used by
    // Element::setPrefix() and Attr::setPrefix()

    if (!prefix.isEmpty() && !Document::isValidName(prefix)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }

    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // FIXME: Raise NAMESPACE_ERR if prefix is malformed per the Namespaces in XML specification.

    const AtomicString& nodeNamespaceURI = namespaceURI();
    if ((nodeNamespaceURI.isEmpty() && !prefix.isEmpty())
        || (prefix == xmlAtom && nodeNamespaceURI != XMLNames::xmlNamespaceURI)) {
        ec = NAMESPACE_ERR;
        return;
    }
    // Attribute-specific checks are in Attr::setPrefix().
}

bool Node::isDescendantOf(const Node* other) const
{
    // Return true if other is an ancestor of this, otherwise false
    if (!other || !other->hasChildNodes() || inDocument() != other->inDocument())
        return false;
    if (other->isDocumentNode())
        return &document() == other && !isDocumentNode() && inDocument();
    for (const ContainerNode* n = parentNode(); n; n = n->parentNode()) {
        if (n == other)
            return true;
    }
    return false;
}

bool Node::isDescendantOrShadowDescendantOf(const Node* other) const
{
    if (!other) 
        return false;
    if (isDescendantOf(other))
        return true;
    const Node* shadowAncestorNode = deprecatedShadowAncestorNode();
    if (!shadowAncestorNode)
        return false;
    return shadowAncestorNode == other || shadowAncestorNode->isDescendantOf(other);
}

bool Node::contains(const Node* node) const
{
    if (!node)
        return false;
    return this == node || node->isDescendantOf(this);
}

bool Node::containsIncludingShadowDOM(const Node* node) const
{
    for (; node; node = node->parentOrShadowHostNode()) {
        if (node == this)
            return true;
    }
    return false;
}

bool Node::containsIncludingHostElements(const Node* node) const
{
#if ENABLE(TEMPLATE_ELEMENT)
    while (node) {
        if (node == this)
            return true;
        if (node->isDocumentFragment() && static_cast<const DocumentFragment*>(node)->isTemplateContent())
            node = static_cast<const TemplateContentDocumentFragment*>(node)->host();
        else
            node = node->parentOrShadowHostNode();
    }
    return false;
#else
    return containsIncludingShadowDOM(node);
#endif
}

Node* Node::pseudoAwarePreviousSibling() const
{
    Element* parentOrHost = isPseudoElement() ? toPseudoElement(this)->hostElement() : parentElement();
    if (parentOrHost && !previousSibling()) {
        if (isAfterPseudoElement() && parentOrHost->lastChild())
            return parentOrHost->lastChild();
        if (!isBeforePseudoElement())
            return parentOrHost->beforePseudoElement();
    }
    return previousSibling();
}

Node* Node::pseudoAwareNextSibling() const
{
    Element* parentOrHost = isPseudoElement() ? toPseudoElement(this)->hostElement() : parentElement();
    if (parentOrHost && !nextSibling()) {
        if (isBeforePseudoElement() && parentOrHost->firstChild())
            return parentOrHost->firstChild();
        if (!isAfterPseudoElement())
            return parentOrHost->afterPseudoElement();
    }
    return nextSibling();
}

Node* Node::pseudoAwareFirstChild() const
{
    if (isElementNode()) {
        const Element* currentElement = toElement(this);
        Node* first = currentElement->beforePseudoElement();
        if (first)
            return first;
        first = currentElement->firstChild();
        if (!first)
            first = currentElement->afterPseudoElement();
        return first;
    }
    return firstChild();
}

Node* Node::pseudoAwareLastChild() const
{
    if (isElementNode()) {
        const Element* currentElement = toElement(this);
        Node* last = currentElement->afterPseudoElement();
        if (last)
            return last;
        last = currentElement->lastChild();
        if (!last)
            last = currentElement->beforePseudoElement();
        return last;
    }
    return lastChild();
}

RenderStyle* Node::computedStyle(PseudoId pseudoElementSpecifier)
{
    for (Node* node = this; node; node = node->parentOrShadowHostNode()) {
        if (node->isElementNode())
            return toElement(node)->computedStyle(pseudoElementSpecifier);
    }
    return nullptr;
}

int Node::maxCharacterOffset() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks questions about HTML in the core DOM class
// is obviously misplaced.
bool Node::canStartSelection() const
{
    if (hasEditableStyle())
        return true;

    if (renderer()) {
        const RenderStyle& style = renderer()->style();
        // We allow selections to begin within an element that has -webkit-user-select: none set,
        // but if the element is draggable then dragging should take priority over selection.
        if (style.userDrag() == DRAG_ELEMENT && style.userSelect() == SELECT_NONE)
            return false;
    }
    return parentOrShadowHostNode() ? parentOrShadowHostNode()->canStartSelection() : true;
}

Element* Node::shadowHost() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return root->hostElement();
    return 0;
}

Node* Node::deprecatedShadowAncestorNode() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return root->hostElement();

    return const_cast<Node*>(this);
}

ShadowRoot* Node::containingShadowRoot() const
{
    ContainerNode* root = treeScope().rootNode();
    return root && root->isShadowRoot() ? toShadowRoot(root) : 0;
}

Node* Node::nonBoundaryShadowTreeRootNode()
{
    ASSERT(!isShadowRoot());
    Node* root = this;
    while (root) {
        if (root->isShadowRoot())
            return root;
        Node* parent = root->parentNodeGuaranteedHostFree();
        if (parent && parent->isShadowRoot())
            return root;
        root = parent;
    }
    return 0;
}

ContainerNode* Node::nonShadowBoundaryParentNode() const
{
    ContainerNode* parent = parentNode();
    return parent && !parent->isShadowRoot() ? parent : 0;
}

Element* Node::parentOrShadowHostElement() const
{
    ContainerNode* parent = parentOrShadowHostNode();
    if (!parent)
        return 0;

    if (parent->isShadowRoot())
        return toShadowRoot(parent)->hostElement();

    if (!parent->isElementNode())
        return 0;

    return toElement(parent);
}

Node* Node::insertionParentForBinding() const
{
    return findInsertionPointOf(this);
}

Node::InsertionNotificationRequest Node::insertedInto(ContainerNode& insertionPoint)
{
    ASSERT(insertionPoint.inDocument() || isContainerNode());
    if (insertionPoint.inDocument())
        setFlag(InDocumentFlag);
    if (parentOrShadowHostNode()->isInShadowTree())
        setFlag(IsInShadowTreeFlag);
    return InsertionDone;
}

void Node::removedFrom(ContainerNode& insertionPoint)
{
    ASSERT(insertionPoint.inDocument() || isContainerNode());
    if (insertionPoint.inDocument())
        clearFlag(InDocumentFlag);
    if (isInShadowTree() && !treeScope().rootNode()->isShadowRoot())
        clearFlag(IsInShadowTreeFlag);
}

bool Node::isRootEditableElement() const
{
    return hasEditableStyle() && isElementNode() && (!parentNode() || !parentNode()->hasEditableStyle()
        || !parentNode()->isElementNode() || hasTagName(bodyTag));
}

Element* Node::rootEditableElement(EditableType editableType) const
{
    if (editableType == HasEditableAXRole) {
        if (AXObjectCache* cache = document().existingAXObjectCache())
            return const_cast<Element*>(cache->rootAXEditableElement(this));
    }
    
    return rootEditableElement();
}

Element* Node::rootEditableElement() const
{
    Element* result = 0;
    for (Node* n = const_cast<Node*>(this); n && n->hasEditableStyle(); n = n->parentNode()) {
        if (n->isElementNode())
            result = toElement(n);
        if (n->hasTagName(bodyTag))
            break;
    }
    return result;
}

// FIXME: End of obviously misplaced HTML editing functions.  Try to move these out of Node.

Document* Node::ownerDocument() const
{
    Document* document = &this->document();
    return document == this ? nullptr : document;
}

URL Node::baseURI() const
{
    return parentNode() ? parentNode()->baseURI() : URL();
}

bool Node::isEqualNode(Node* other) const
{
    if (!other)
        return false;
    
    NodeType nodeType = this->nodeType();
    if (nodeType != other->nodeType())
        return false;
    
    if (nodeName() != other->nodeName())
        return false;
    
    if (localName() != other->localName())
        return false;
    
    if (namespaceURI() != other->namespaceURI())
        return false;
    
    if (prefix() != other->prefix())
        return false;
    
    if (nodeValue() != other->nodeValue())
        return false;
    
    if (isElementNode() && !toElement(this)->hasEquivalentAttributes(toElement(other)))
        return false;
    
    Node* child = firstChild();
    Node* otherChild = other->firstChild();
    
    while (child) {
        if (!child->isEqualNode(otherChild))
            return false;
        
        child = child->nextSibling();
        otherChild = otherChild->nextSibling();
    }
    
    if (otherChild)
        return false;
    
    if (nodeType == DOCUMENT_TYPE_NODE) {
        const DocumentType* documentTypeThis = static_cast<const DocumentType*>(this);
        const DocumentType* documentTypeOther = static_cast<const DocumentType*>(other);
        
        if (documentTypeThis->publicId() != documentTypeOther->publicId())
            return false;

        if (documentTypeThis->systemId() != documentTypeOther->systemId())
            return false;

        if (documentTypeThis->internalSubset() != documentTypeOther->internalSubset())
            return false;

        // FIXME: We don't compare entities or notations because currently both are always empty.
    }
    
    return true;
}

bool Node::isDefaultNamespace(const AtomicString& namespaceURIMaybeEmpty) const
{
    const AtomicString& namespaceURI = namespaceURIMaybeEmpty.isEmpty() ? nullAtom : namespaceURIMaybeEmpty;

    switch (nodeType()) {
        case ELEMENT_NODE: {
            const Element* elem = toElement(this);
            
            if (elem->prefix().isNull())
                return elem->namespaceURI() == namespaceURI;

            if (elem->hasAttributes()) {
                for (const Attribute& attribute : elem->attributesIterator()) {
                    if (attribute.localName() == xmlnsAtom)
                        return attribute.value() == namespaceURI;
                }
            }

            if (Element* ancestor = ancestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);

            return false;
        }
        case DOCUMENT_NODE:
            if (Element* de = toDocument(this)->documentElement())
                return de->isDefaultNamespace(namespaceURI);
            return false;
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            return false;
        case ATTRIBUTE_NODE: {
            const Attr* attr = static_cast<const Attr*>(this);
            if (attr->ownerElement())
                return attr->ownerElement()->isDefaultNamespace(namespaceURI);
            return false;
        }
        default:
            if (Element* ancestor = ancestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);
            return false;
    }
}

String Node::lookupPrefix(const AtomicString &namespaceURI) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespacePrefixAlgo
    
    if (namespaceURI.isEmpty())
        return String();
    
    switch (nodeType()) {
        case ELEMENT_NODE:
            return lookupNamespacePrefix(namespaceURI, static_cast<const Element *>(this));
        case DOCUMENT_NODE:
            if (Element* de = toDocument(this)->documentElement())
                return de->lookupPrefix(namespaceURI);
            return String();
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_FRAGMENT_NODE:
        case DOCUMENT_TYPE_NODE:
            return String();
        case ATTRIBUTE_NODE: {
            const Attr *attr = static_cast<const Attr *>(this);
            if (attr->ownerElement())
                return attr->ownerElement()->lookupPrefix(namespaceURI);
            return String();
        }
        default:
            if (Element* ancestor = ancestorElement())
                return ancestor->lookupPrefix(namespaceURI);
            return String();
    }
}

String Node::lookupNamespaceURI(const String &prefix) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespaceURIAlgo
    
    if (!prefix.isNull() && prefix.isEmpty())
        return String();
    
    switch (nodeType()) {
        case ELEMENT_NODE: {
            const Element *elem = static_cast<const Element *>(this);
            
            if (!elem->namespaceURI().isNull() && elem->prefix() == prefix)
                return elem->namespaceURI();
            
            if (elem->hasAttributes()) {
                for (const Attribute& attribute : elem->attributesIterator()) {
                    
                    if (attribute.prefix() == xmlnsAtom && attribute.localName() == prefix) {
                        if (!attribute.value().isEmpty())
                            return attribute.value();
                        
                        return String();
                    }
                    if (attribute.localName() == xmlnsAtom && prefix.isNull()) {
                        if (!attribute.value().isEmpty())
                            return attribute.value();
                        
                        return String();
                    }
                }
            }
            if (Element* ancestor = ancestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            return String();
        }
        case DOCUMENT_NODE:
            if (Element* de = toDocument(this)->documentElement())
                return de->lookupNamespaceURI(prefix);
            return String();
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            return String();
        case ATTRIBUTE_NODE: {
            const Attr *attr = static_cast<const Attr *>(this);
            
            if (attr->ownerElement())
                return attr->ownerElement()->lookupNamespaceURI(prefix);
            else
                return String();
        }
        default:
            if (Element* ancestor = ancestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            return String();
    }
}

String Node::lookupNamespacePrefix(const AtomicString &_namespaceURI, const Element *originalElement) const
{
    if (_namespaceURI.isNull())
        return String();
            
    if (originalElement->lookupNamespaceURI(prefix()) == _namespaceURI)
        return prefix();
    
    ASSERT(isElementNode());
    const Element* thisElement = toElement(this);
    if (thisElement->hasAttributes()) {
        for (const Attribute& attribute : thisElement->attributesIterator()) {
            if (attribute.prefix() == xmlnsAtom && attribute.value() == _namespaceURI
                && originalElement->lookupNamespaceURI(attribute.localName()) == _namespaceURI)
                return attribute.localName();
        }
    }
    
    if (Element* ancestor = ancestorElement())
        return ancestor->lookupNamespacePrefix(_namespaceURI, originalElement);
    return String();
}

static void appendTextContent(const Node* node, bool convertBRsToNewlines, bool& isNullString, StringBuilder& content)
{
    switch (node->nodeType()) {
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::COMMENT_NODE:
        isNullString = false;
        content.append(static_cast<const CharacterData*>(node)->data());
        break;

    case Node::PROCESSING_INSTRUCTION_NODE:
        isNullString = false;
        content.append(static_cast<const ProcessingInstruction*>(node)->data());
        break;
    
    case Node::ELEMENT_NODE:
        if (node->hasTagName(brTag) && convertBRsToNewlines) {
            isNullString = false;
            content.append('\n');
            break;
        }
        FALLTHROUGH;
    case Node::ATTRIBUTE_NODE:
    case Node::ENTITY_NODE:
    case Node::ENTITY_REFERENCE_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        isNullString = false;
        for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
            if (child->nodeType() == Node::COMMENT_NODE || child->nodeType() == Node::PROCESSING_INSTRUCTION_NODE)
                continue;
            appendTextContent(child, convertBRsToNewlines, isNullString, content);
        }
        break;

    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_TYPE_NODE:
    case Node::NOTATION_NODE:
    case Node::XPATH_NAMESPACE_NODE:
        break;
    }
}

String Node::textContent(bool convertBRsToNewlines) const
{
    StringBuilder content;
    bool isNullString = true;
    appendTextContent(this, convertBRsToNewlines, isNullString, content);
    return isNullString ? String() : content.toString();
}

void Node::setTextContent(const String& text, ExceptionCode& ec)
{           
    switch (nodeType()) {
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
            setNodeValue(text, ec);
            return;
        case ELEMENT_NODE:
        case ATTRIBUTE_NODE:
        case ENTITY_NODE:
        case ENTITY_REFERENCE_NODE:
        case DOCUMENT_FRAGMENT_NODE: {
            Ref<ContainerNode> container(*toContainerNode(this));
            ChildListMutationScope mutation(container.get());
            container->removeChildren();
            if (!text.isEmpty())
                container->appendChild(document().createTextNode(text), ec);
            return;
        }
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case NOTATION_NODE:
        case XPATH_NAMESPACE_NODE:
            // Do nothing.
            return;
    }
    ASSERT_NOT_REACHED();
}

Element* Node::ancestorElement() const
{
    // In theory, there can be EntityReference nodes between elements, but this is currently not supported.
    for (ContainerNode* n = parentNode(); n; n = n->parentNode()) {
        if (n->isElementNode())
            return toElement(n);
    }
    return 0;
}

bool Node::offsetInCharacters() const
{
    return false;
}

static inline unsigned short compareDetachedElementsPosition(Node* firstNode, Node* secondNode)
{
    // If the 2 nodes are not in the same tree, return the result of adding DOCUMENT_POSITION_DISCONNECTED,
    // DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC, and either DOCUMENT_POSITION_PRECEDING or
    // DOCUMENT_POSITION_FOLLOWING, with the constraint that this is to be consistent. Whether to return
    // DOCUMENT_POSITION_PRECEDING or DOCUMENT_POSITION_FOLLOWING is implemented here via pointer
    // comparison.
    // See step 3 in http://www.w3.org/TR/2012/WD-dom-20121206/#dom-node-comparedocumentposition
    unsigned short direction = (firstNode > secondNode) ? Node::DOCUMENT_POSITION_PRECEDING : Node::DOCUMENT_POSITION_FOLLOWING;
    return Node::DOCUMENT_POSITION_DISCONNECTED | Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
}

unsigned short Node::compareDocumentPosition(Node* otherNode)
{
    // It is not clear what should be done if |otherNode| is 0.
    if (!otherNode)
        return DOCUMENT_POSITION_DISCONNECTED;

    if (otherNode == this)
        return DOCUMENT_POSITION_EQUIVALENT;
    
    Attr* attr1 = isAttributeNode() ? toAttr(this) : nullptr;
    Attr* attr2 = otherNode->isAttributeNode() ? toAttr(otherNode) : nullptr;
    
    Node* start1 = attr1 ? attr1->ownerElement() : this;
    Node* start2 = attr2 ? attr2->ownerElement() : otherNode;
    
    // If either of start1 or start2 is null, then we are disconnected, since one of the nodes is
    // an orphaned attribute node.
    if (!start1 || !start2)
        return compareDetachedElementsPosition(this, otherNode);

    Vector<Node*, 16> chain1;
    Vector<Node*, 16> chain2;
    if (attr1)
        chain1.append(attr1);
    if (attr2)
        chain2.append(attr2);
    
    if (attr1 && attr2 && start1 == start2 && start1) {
        // We are comparing two attributes on the same node. Crawl our attribute map and see which one we hit first.
        Element* owner1 = attr1->ownerElement();
        owner1->synchronizeAllAttributes();
        for (const Attribute& attribute : owner1->attributesIterator()) {
            // If neither of the two determining nodes is a child node and nodeType is the same for both determining nodes, then an
            // implementation-dependent order between the determining nodes is returned. This order is stable as long as no nodes of
            // the same nodeType are inserted into or removed from the direct container. This would be the case, for example, 
            // when comparing two attributes of the same element, and inserting or removing additional attributes might change 
            // the order between existing attributes.
            if (attr1->qualifiedName() == attribute.name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_FOLLOWING;
            if (attr2->qualifiedName() == attribute.name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_PRECEDING;
        }
        
        ASSERT_NOT_REACHED();
        return DOCUMENT_POSITION_DISCONNECTED;
    }

    // If one node is in the document and the other is not, we must be disconnected.
    // If the nodes have different owning documents, they must be disconnected.  Note that we avoid
    // comparing Attr nodes here, since they return false from inDocument() all the time (which seems like a bug).
    if (start1->inDocument() != start2->inDocument() ||
        &start1->treeScope() != &start2->treeScope())
        return compareDetachedElementsPosition(this, otherNode);

    // We need to find a common ancestor container, and then compare the indices of the two immediate children.
    Node* current;
    for (current = start1; current; current = current->parentNode())
        chain1.append(current);
    for (current = start2; current; current = current->parentNode())
        chain2.append(current);

    unsigned index1 = chain1.size();
    unsigned index2 = chain2.size();

    // If the two elements don't have a common root, they're not in the same tree.
    if (chain1[index1 - 1] != chain2[index2 - 1])
        return compareDetachedElementsPosition(this, otherNode);

    // Walk the two chains backwards and look for the first difference.
    for (unsigned i = std::min(index1, index2); i; --i) {
        Node* child1 = chain1[--index1];
        Node* child2 = chain2[--index2];
        if (child1 != child2) {
            // If one of the children is an attribute, it wins.
            if (child1->nodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_FOLLOWING;
            if (child2->nodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_PRECEDING;
            
            if (!child2->nextSibling())
                return DOCUMENT_POSITION_FOLLOWING;
            if (!child1->nextSibling())
                return DOCUMENT_POSITION_PRECEDING;

            // Otherwise we need to see which node occurs first.  Crawl backwards from child2 looking for child1.
            for (Node* child = child2->previousSibling(); child; child = child->previousSibling()) {
                if (child == child1)
                    return DOCUMENT_POSITION_FOLLOWING;
            }
            return DOCUMENT_POSITION_PRECEDING;
        }
    }
    
    // There was no difference between the two parent chains, i.e., one was a subset of the other.  The shorter
    // chain is the ancestor.
    return index1 < index2 ? 
               DOCUMENT_POSITION_FOLLOWING | DOCUMENT_POSITION_CONTAINED_BY :
               DOCUMENT_POSITION_PRECEDING | DOCUMENT_POSITION_CONTAINS;
}

FloatPoint Node::convertToPage(const FloatPoint& p) const
{
    // If there is a renderer, just ask it to do the conversion
    if (renderer())
        return renderer()->localToAbsolute(p, UseTransforms);
    
    // Otherwise go up the tree looking for a renderer
    Element *parent = ancestorElement();
    if (parent)
        return parent->convertToPage(p);

    // No parent - no conversion needed
    return p;
}

FloatPoint Node::convertFromPage(const FloatPoint& p) const
{
    // If there is a renderer, just ask it to do the conversion
    if (renderer())
        return renderer()->absoluteToLocal(p, UseTransforms);

    // Otherwise go up the tree looking for a renderer
    Element *parent = ancestorElement();
    if (parent)
        return parent->convertFromPage(p);

    // No parent - no conversion needed
    return p;
}

#ifndef NDEBUG

static void appendAttributeDesc(const Node* node, StringBuilder& stringBuilder, const QualifiedName& name, const char* attrDesc)
{
    if (!node->isElementNode())
        return;

    String attr = toElement(node)->getAttribute(name);
    if (attr.isEmpty())
        return;

    stringBuilder.append(attrDesc);
    stringBuilder.append(attr);
}

void Node::showNode(const char* prefix) const
{
    if (!prefix)
        prefix = "";
    if (isTextNode()) {
        String value = nodeValue();
        value.replaceWithLiteral('\\', "\\\\");
        value.replaceWithLiteral('\n', "\\n");
        fprintf(stderr, "%s%s\t%p \"%s\"\n", prefix, nodeName().utf8().data(), this, value.utf8().data());
    } else {
        StringBuilder attrs;
        appendAttributeDesc(this, attrs, classAttr, " CLASS=");
        appendAttributeDesc(this, attrs, styleAttr, " STYLE=");
        fprintf(stderr, "%s%s\t%p%s\n", prefix, nodeName().utf8().data(), this, attrs.toString().utf8().data());
    }
}

void Node::showTreeForThis() const
{
    showTreeAndMark(this, "*");
}

void Node::showNodePathForThis() const
{
    Vector<const Node*, 16> chain;
    const Node* node = this;
    while (node->parentOrShadowHostNode()) {
        chain.append(node);
        node = node->parentOrShadowHostNode();
    }
    for (unsigned index = chain.size(); index > 0; --index) {
        const Node* node = chain[index - 1];
        if (node->isShadowRoot()) {
            int count = 0;
            for (const ShadowRoot* shadowRoot = toShadowRoot(node); shadowRoot && shadowRoot != node; shadowRoot = shadowRoot->shadowRoot())
                ++count;
            fprintf(stderr, "/#shadow-root[%d]", count);
            continue;
        }

        switch (node->nodeType()) {
        case ELEMENT_NODE: {
            fprintf(stderr, "/%s", node->nodeName().utf8().data());

            const Element* element = toElement(node);
            const AtomicString& idattr = element->getIdAttribute();
            bool hasIdAttr = !idattr.isNull() && !idattr.isEmpty();
            if (node->previousSibling() || node->nextSibling()) {
                int count = 0;
                for (Node* previous = node->previousSibling(); previous; previous = previous->previousSibling())
                    if (previous->nodeName() == node->nodeName())
                        ++count;
                if (hasIdAttr)
                    fprintf(stderr, "[@id=\"%s\" and position()=%d]", idattr.string().utf8().data(), count);
                else
                    fprintf(stderr, "[%d]", count);
            } else if (hasIdAttr)
                fprintf(stderr, "[@id=\"%s\"]", idattr.string().utf8().data());
            break;
        }
        case TEXT_NODE:
            fprintf(stderr, "/text()");
            break;
        case ATTRIBUTE_NODE:
            fprintf(stderr, "/@%s", node->nodeName().utf8().data());
            break;
        default:
            break;
        }
    }
    fprintf(stderr, "\n");
}

static void traverseTreeAndMark(const String& baseIndent, const Node* rootNode, const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2)
{
    for (const Node* node = rootNode; node; node = NodeTraversal::next(node)) {
        if (node == markedNode1)
            fprintf(stderr, "%s", markedLabel1);
        if (node == markedNode2)
            fprintf(stderr, "%s", markedLabel2);

        StringBuilder indent;
        indent.append(baseIndent);
        for (const Node* tmpNode = node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentOrShadowHostNode())
            indent.append('\t');
        fprintf(stderr, "%s", indent.toString().utf8().data());
        node->showNode();
        indent.append('\t');
        if (!node->isShadowRoot()) {
            if (ShadowRoot* shadowRoot = node->shadowRoot())
                traverseTreeAndMark(indent.toString(), shadowRoot, markedNode1, markedLabel1, markedNode2, markedLabel2);
        }
    }
}

void Node::showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2) const
{
    const Node* rootNode;
    const Node* node = this;
    while (node->parentOrShadowHostNode() && !node->hasTagName(bodyTag))
        node = node->parentOrShadowHostNode();
    rootNode = node;

    String startingIndent;
    traverseTreeAndMark(startingIndent, rootNode, markedNode1, markedLabel1, markedNode2, markedLabel2);
}

void Node::formatForDebugger(char* buffer, unsigned length) const
{
    String result;
    String s;

    s = nodeName();
    if (s.isEmpty())
        result = "<none>";
    else
        result = s;

    strncpy(buffer, result.utf8().data(), length - 1);
}

static ContainerNode* parentOrShadowHostOrFrameOwner(const Node* node)
{
    ContainerNode* parent = node->parentOrShadowHostNode();
    if (!parent && node->document().frame())
        parent = node->document().frame()->ownerElement();
    return parent;
}

static void showSubTreeAcrossFrame(const Node* node, const Node* markedNode, const String& indent)
{
    if (node == markedNode)
        fputs("*", stderr);
    fputs(indent.utf8().data(), stderr);
    node->showNode();
    if (!node->isShadowRoot()) {
        if (node->isFrameOwnerElement())
            showSubTreeAcrossFrame(static_cast<const HTMLFrameOwnerElement*>(node)->contentDocument(), markedNode, indent + "\t");
        if (ShadowRoot* shadowRoot = node->shadowRoot())
            showSubTreeAcrossFrame(shadowRoot, markedNode, indent + "\t");
    }
    for (Node* child = node->firstChild(); child; child = child->nextSibling())
        showSubTreeAcrossFrame(child, markedNode, indent + "\t");
}

void Node::showTreeForThisAcrossFrame() const
{
    Node* rootNode = const_cast<Node*>(this);
    while (parentOrShadowHostOrFrameOwner(rootNode))
        rootNode = parentOrShadowHostOrFrameOwner(rootNode);
    showSubTreeAcrossFrame(rootNode, this, "");
}

#endif

// --------

void NodeListsNodeData::invalidateCaches(const QualifiedName* attrName)
{
    for (auto& atomicName : m_atomicNameCaches)
        atomicName.value->invalidateCache(attrName);

    for (auto& name : m_nameCaches)
        name.value->invalidateCache(attrName);

    for (auto& collection : m_cachedCollections)
        collection.value->invalidateCache(attrName);

    if (attrName)
        return;

    for (auto& tagNodeList : m_tagNodeListCacheNS)
        tagNodeList.value->invalidateCache();
}

void Node::getSubresourceURLs(ListHashSet<URL>& urls) const
{
    addSubresourceAttributeURLs(urls);
}

Element* Node::enclosingLinkEventParentOrSelf()
{
    for (Node* node = this; node; node = node->parentOrShadowHostNode()) {
        // For imagemaps, the enclosing link element is the associated area element not the image itself.
        // So we don't let images be the enclosing link element, even though isLink sometimes returns
        // true for them.
        if (node->isLink() && !isHTMLImageElement(node))
            return toElement(node);
    }

    return 0;
}

EventTargetInterface Node::eventTargetInterface() const
{
    return NodeEventTargetInterfaceType;
}

void Node::didMoveToNewDocument(Document* oldDocument)
{
    TreeScopeAdopter::ensureDidMoveToNewDocumentWasCalled(oldDocument);

    if (const EventTargetData* eventTargetData = this->eventTargetData()) {
        const EventListenerMap& listenerMap = eventTargetData->eventListenerMap;
        if (!listenerMap.isEmpty()) {
            Vector<AtomicString> types = listenerMap.eventTypes();
            for (unsigned i = 0; i < types.size(); ++i)
                document().addListenerTypeIfNeeded(types[i]);
        }
    }

    if (AXObjectCache::accessibilityEnabled() && oldDocument)
        if (AXObjectCache* cache = oldDocument->existingAXObjectCache())
            cache->remove(this);

    const EventListenerVector& mousewheelListeners = getEventListeners(eventNames().mousewheelEvent);
    for (size_t i = 0; i < mousewheelListeners.size(); ++i) {
        oldDocument->didRemoveWheelEventHandler();
        document().didAddWheelEventHandler();
    }

    const EventListenerVector& wheelListeners = getEventListeners(eventNames().wheelEvent);
    for (size_t i = 0; i < wheelListeners.size(); ++i) {
        oldDocument->didRemoveWheelEventHandler();
        document().didAddWheelEventHandler();
    }

    Vector<AtomicString> touchEventNames = eventNames().touchEventNames();
    for (size_t i = 0; i < touchEventNames.size(); ++i) {
        const EventListenerVector& listeners = getEventListeners(touchEventNames[i]);
        for (size_t j = 0; j < listeners.size(); ++j) {
            oldDocument->didRemoveTouchEventHandler(this);
            document().didAddTouchEventHandler(this);
        }
    }

    if (Vector<OwnPtr<MutationObserverRegistration>>* registry = mutationObserverRegistry()) {
        for (size_t i = 0; i < registry->size(); ++i) {
            document().addMutationObserverTypes(registry->at(i)->mutationTypes());
        }
    }

    if (HashSet<MutationObserverRegistration*>* transientRegistry = transientMutationObserverRegistry()) {
        for (HashSet<MutationObserverRegistration*>::iterator iter = transientRegistry->begin(); iter != transientRegistry->end(); ++iter) {
            document().addMutationObserverTypes((*iter)->mutationTypes());
        }
    }
}

static inline bool tryAddEventListener(Node* targetNode, const AtomicString& eventType, PassRefPtr<EventListener> prpListener, bool useCapture)
{
    RefPtr<EventListener> listener = prpListener;

    if (!targetNode->EventTarget::addEventListener(eventType, listener, useCapture))
        return false;

    targetNode->document().addListenerTypeIfNeeded(eventType);
    if (eventType == eventNames().wheelEvent || eventType == eventNames().mousewheelEvent)
        targetNode->document().didAddWheelEventHandler();
    else if (eventNames().isTouchEventType(eventType))
        targetNode->document().didAddTouchEventHandler(targetNode);

#if PLATFORM(IOS)
    if (targetNode == &targetNode->document() && eventType == eventNames().scrollEvent)
        targetNode->document().domWindow()->incrementScrollEventListenersCount();

    // FIXME: Would it be sufficient to special-case this code for <body> and <frameset>?
    //
    // This code was added to address <rdar://problem/5846492> Onorientationchange event not working for document.body.
    // Forward this call to addEventListener() to the window since these are window-only events.
    if (eventType == eventNames().orientationchangeEvent || eventType == eventNames().resizeEvent)
        targetNode->document().domWindow()->addEventListener(eventType, listener, useCapture);

#if ENABLE(TOUCH_EVENTS)
    if (eventNames().isTouchEventType(eventType))
        targetNode->document().addTouchEventListener(targetNode);
#endif
#endif // PLATFORM(IOS)

#if ENABLE(IOS_GESTURE_EVENTS) && ENABLE(TOUCH_EVENTS)
    if (eventType == eventNames().gesturestartEvent || eventType == eventNames().gesturechangeEvent || eventType == eventNames().gestureendEvent)
        targetNode->document().addTouchEventListener(targetNode);
#endif

    return true;
}

bool Node::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    return tryAddEventListener(this, eventType, listener, useCapture);
}

static inline bool tryRemoveEventListener(Node* targetNode, const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!targetNode->EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    // FIXME: Notify Document that the listener has vanished. We need to keep track of a number of
    // listeners for each type, not just a bool - see https://bugs.webkit.org/show_bug.cgi?id=33861
    if (eventType == eventNames().wheelEvent || eventType == eventNames().mousewheelEvent)
        targetNode->document().didRemoveWheelEventHandler();
    else if (eventNames().isTouchEventType(eventType))
        targetNode->document().didRemoveTouchEventHandler(targetNode);

#if PLATFORM(IOS)
    if (targetNode == &targetNode->document() && eventType == eventNames().scrollEvent)
        targetNode->document().domWindow()->decrementScrollEventListenersCount();

    // FIXME: Would it be sufficient to special-case this code for <body> and <frameset>? See <rdar://problem/15647823>.
    // This code was added to address <rdar://problem/5846492> Onorientationchange event not working for document.body.
    // Forward this call to removeEventListener() to the window since these are window-only events.
    if (eventType == eventNames().orientationchangeEvent || eventType == eventNames().resizeEvent)
        targetNode->document().domWindow()->removeEventListener(eventType, listener, useCapture);

#if ENABLE(TOUCH_EVENTS)
    if (eventNames().isTouchEventType(eventType))
        targetNode->document().removeTouchEventListener(targetNode);
#endif
#endif // PLATFORM(IOS)

#if ENABLE(IOS_GESTURE_EVENTS) && ENABLE(TOUCH_EVENTS)
    if (eventType == eventNames().gesturestartEvent || eventType == eventNames().gesturechangeEvent || eventType == eventNames().gestureendEvent)
        targetNode->document().removeTouchEventListener(targetNode);
#endif

    return true;
}

bool Node::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    return tryRemoveEventListener(this, eventType, listener, useCapture);
}

typedef HashMap<Node*, OwnPtr<EventTargetData>> EventTargetDataMap;

static EventTargetDataMap& eventTargetDataMap()
{
    DEFINE_STATIC_LOCAL(EventTargetDataMap, map, ());
    return map;
}

EventTargetData* Node::eventTargetData()
{
    return hasEventTargetData() ? eventTargetDataMap().get(this) : 0;
}

EventTargetData& Node::ensureEventTargetData()
{
    if (hasEventTargetData())
        return *eventTargetDataMap().get(this);
    setHasEventTargetData(true);
    EventTargetData* data = new EventTargetData;
    eventTargetDataMap().set(this, adoptPtr(data));
    return *data;
}

void Node::clearEventTargetData()
{
    eventTargetDataMap().remove(this);
}

Vector<OwnPtr<MutationObserverRegistration>>* Node::mutationObserverRegistry()
{
    if (!hasRareData())
        return 0;
    NodeMutationObserverData* data = rareData()->mutationObserverData();
    if (!data)
        return 0;
    return &data->registry;
}

HashSet<MutationObserverRegistration*>* Node::transientMutationObserverRegistry()
{
    if (!hasRareData())
        return 0;
    NodeMutationObserverData* data = rareData()->mutationObserverData();
    if (!data)
        return 0;
    return &data->transientRegistry;
}

template<typename Registry>
static inline void collectMatchingObserversForMutation(HashMap<MutationObserver*, MutationRecordDeliveryOptions>& observers, Registry* registry, Node* target, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    if (!registry)
        return;
    for (typename Registry::iterator iter = registry->begin(); iter != registry->end(); ++iter) {
        const MutationObserverRegistration& registration = **iter;
        if (registration.shouldReceiveMutationFrom(target, type, attributeName)) {
            MutationRecordDeliveryOptions deliveryOptions = registration.deliveryOptions();
            HashMap<MutationObserver*, MutationRecordDeliveryOptions>::AddResult result = observers.add(registration.observer(), deliveryOptions);
            if (!result.isNewEntry)
                result.iterator->value |= deliveryOptions;
        }
    }
}

void Node::getRegisteredMutationObserversOfType(HashMap<MutationObserver*, MutationRecordDeliveryOptions>& observers, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    ASSERT((type == MutationObserver::Attributes && attributeName) || !attributeName);
    collectMatchingObserversForMutation(observers, mutationObserverRegistry(), this, type, attributeName);
    collectMatchingObserversForMutation(observers, transientMutationObserverRegistry(), this, type, attributeName);
    for (Node* node = parentNode(); node; node = node->parentNode()) {
        collectMatchingObserversForMutation(observers, node->mutationObserverRegistry(), this, type, attributeName);
        collectMatchingObserversForMutation(observers, node->transientMutationObserverRegistry(), this, type, attributeName);
    }
}

void Node::registerMutationObserver(MutationObserver* observer, MutationObserverOptions options, const HashSet<AtomicString>& attributeFilter)
{
    MutationObserverRegistration* registration = 0;
    Vector<OwnPtr<MutationObserverRegistration>>& registry = ensureRareData().ensureMutationObserverData().registry;
    for (size_t i = 0; i < registry.size(); ++i) {
        if (registry[i]->observer() == observer) {
            registration = registry[i].get();
            registration->resetObservation(options, attributeFilter);
        }
    }

    if (!registration) {
        registry.append(MutationObserverRegistration::create(observer, this, options, attributeFilter));
        registration = registry.last().get();
    }

    document().addMutationObserverTypes(registration->mutationTypes());
}

void Node::unregisterMutationObserver(MutationObserverRegistration* registration)
{
    Vector<OwnPtr<MutationObserverRegistration>>* registry = mutationObserverRegistry();
    ASSERT(registry);
    if (!registry)
        return;

    size_t index = registry->find(registration);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    registry->remove(index);
}

void Node::registerTransientMutationObserver(MutationObserverRegistration* registration)
{
    ensureRareData().ensureMutationObserverData().transientRegistry.add(registration);
}

void Node::unregisterTransientMutationObserver(MutationObserverRegistration* registration)
{
    HashSet<MutationObserverRegistration*>* transientRegistry = transientMutationObserverRegistry();
    ASSERT(transientRegistry);
    if (!transientRegistry)
        return;

    ASSERT(transientRegistry->contains(registration));
    transientRegistry->remove(registration);
}

void Node::notifyMutationObserversNodeWillDetach()
{
    if (!document().hasMutationObservers())
        return;

    for (Node* node = parentNode(); node; node = node->parentNode()) {
        if (Vector<OwnPtr<MutationObserverRegistration>>* registry = node->mutationObserverRegistry()) {
            const size_t size = registry->size();
            for (size_t i = 0; i < size; ++i)
                registry->at(i)->observedSubtreeNodeWillDetach(this);
        }

        if (HashSet<MutationObserverRegistration*>* transientRegistry = node->transientMutationObserverRegistry()) {
            for (HashSet<MutationObserverRegistration*>::iterator iter = transientRegistry->begin(); iter != transientRegistry->end(); ++iter)
                (*iter)->observedSubtreeNodeWillDetach(this);
        }
    }
}

void Node::handleLocalEvents(Event& event)
{
    if (!hasEventTargetData())
        return;

    if (isElementNode() && toElement(*this).isDisabledFormControl() && event.isMouseEvent())
        return;

    fireEventListeners(&event);
}

void Node::dispatchScopedEvent(PassRefPtr<Event> event)
{
    EventDispatcher::dispatchScopedEvent(*this, event);
}

bool Node::dispatchEvent(PassRefPtr<Event> event)
{
#if ENABLE(TOUCH_EVENTS) && !PLATFORM(IOS)
    if (event->isTouchEvent())
        return dispatchTouchEvent(adoptRef(toTouchEvent(event.leakRef())));
#endif
    return EventDispatcher::dispatchEvent(this, event);
}

void Node::dispatchSubtreeModifiedEvent()
{
    if (isInShadowTree())
        return;

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    if (!document().hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return;
    const AtomicString& subtreeModifiedEventName = eventNames().DOMSubtreeModifiedEvent;
    if (!parentNode() && !hasEventListeners(subtreeModifiedEventName))
        return;

    dispatchScopedEvent(MutationEvent::create(subtreeModifiedEventName, true));
}

bool Node::dispatchDOMActivateEvent(int detail, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    RefPtr<UIEvent> event = UIEvent::create(eventNames().DOMActivateEvent, true, true, document().defaultView(), detail);
    event->setUnderlyingEvent(underlyingEvent);
    dispatchScopedEvent(event);
    return event->defaultHandled();
}

#if ENABLE(TOUCH_EVENTS) && !PLATFORM(IOS)
bool Node::dispatchTouchEvent(PassRefPtr<TouchEvent> event)
{
    return EventDispatcher::dispatchEvent(this, event);
}
#endif

#if ENABLE(INDIE_UI)
bool Node::dispatchUIRequestEvent(PassRefPtr<UIRequestEvent> event)
{
    EventDispatcher::dispatchEvent(this, event);
    return event->defaultHandled() || event->defaultPrevented();
}
#endif
    
bool Node::dispatchBeforeLoadEvent(const String& sourceURL)
{
    if (!document().hasListenerType(Document::BEFORELOAD_LISTENER))
        return true;

    Ref<Node> protect(*this);
    RefPtr<BeforeLoadEvent> beforeLoadEvent = BeforeLoadEvent::create(sourceURL);
    dispatchEvent(beforeLoadEvent.get());
    return !beforeLoadEvent->defaultPrevented();
}

void Node::dispatchInputEvent()
{
    dispatchScopedEvent(Event::create(eventNames().inputEvent, true, false));
}

void Node::defaultEventHandler(Event* event)
{
    if (event->target() != this)
        return;
    const AtomicString& eventType = event->type();
    if (eventType == eventNames().keydownEvent || eventType == eventNames().keypressEvent) {
        if (event->isKeyboardEvent())
            if (Frame* frame = document().frame())
                frame->eventHandler().defaultKeyboardEventHandler(toKeyboardEvent(event));
    } else if (eventType == eventNames().clickEvent) {
        int detail = event->isUIEvent() ? toUIEvent(event)->detail() : 0;
        if (dispatchDOMActivateEvent(detail, event))
            event->setDefaultHandled();
#if ENABLE(CONTEXT_MENUS)
    } else if (eventType == eventNames().contextmenuEvent) {
        if (Frame* frame = document().frame())
            if (Page* page = frame->page())
                page->contextMenuController().handleContextMenuEvent(event);
#endif
    } else if (eventType == eventNames().textInputEvent) {
        if (event->eventInterface() == TextEventInterfaceType)
            if (Frame* frame = document().frame())
                frame->eventHandler().defaultTextInputEventHandler(toTextEvent(event));
#if ENABLE(PAN_SCROLLING)
    } else if (eventType == eventNames().mousedownEvent && event->isMouseEvent()) {
        if (toMouseEvent(event)->button() == MiddleButton) {
            if (enclosingLinkEventParentOrSelf())
                return;

            RenderObject* renderer = this->renderer();
            while (renderer && (!renderer->isBox() || !toRenderBox(renderer)->canBeScrolledAndHasScrollableArea()))
                renderer = renderer->parent();

            if (renderer) {
                if (Frame* frame = document().frame())
                    frame->eventHandler().startPanScrolling(toRenderBox(renderer));
            }
        }
#endif
    } else if ((eventType == eventNames().wheelEvent || eventType == eventNames().mousewheelEvent) && event->eventInterface() == WheelEventInterfaceType) {

        // If we don't have a renderer, send the wheel event to the first node we find with a renderer.
        // This is needed for <option> and <optgroup> elements so that <select>s get a wheel scroll.
        Node* startNode = this;
        while (startNode && !startNode->renderer())
            startNode = startNode->parentOrShadowHostNode();
        
        if (startNode && startNode->renderer())
            if (Frame* frame = document().frame())
                frame->eventHandler().defaultWheelEventHandler(startNode, toWheelEvent(event));
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS)
    } else if (event->eventInterface() == TouchEventInterfaceType && eventNames().isTouchEventType(eventType)) {
        RenderObject* renderer = this->renderer();
        while (renderer && (!renderer->isBox() || !toRenderBox(renderer)->canBeScrolledAndHasScrollableArea()))
            renderer = renderer->parent();

        if (renderer && renderer->node()) {
            if (Frame* frame = document().frame())
                frame->eventHandler().defaultTouchEventHandler(renderer->node(), toTouchEvent(event));
        }
#endif
    } else if (event->type() == eventNames().webkitEditableContentChangedEvent) {
        dispatchInputEvent();
    }
}

bool Node::willRespondToMouseMoveEvents()
{
    // FIXME: Why is the iOS code path different from the non-iOS code path?
#if !PLATFORM(IOS)
    if (!isElementNode())
        return false;
    if (toElement(this)->isDisabledFormControl())
        return false;
#endif
    return hasEventListeners(eventNames().mousemoveEvent) || hasEventListeners(eventNames().mouseoverEvent) || hasEventListeners(eventNames().mouseoutEvent);
}

bool Node::willRespondToMouseClickEvents()
{
    // FIXME: Why is the iOS code path different from the non-iOS code path?
#if PLATFORM(IOS)
    return isContentEditable() || hasEventListeners(eventNames().mouseupEvent) || hasEventListeners(eventNames().mousedownEvent) || hasEventListeners(eventNames().clickEvent);
#else
    if (!isElementNode())
        return false;
    if (toElement(this)->isDisabledFormControl())
        return false;
    return isContentEditable(UserSelectAllIsAlwaysNonEditable) || hasEventListeners(eventNames().mouseupEvent) || hasEventListeners(eventNames().mousedownEvent) || hasEventListeners(eventNames().clickEvent) || hasEventListeners(eventNames().DOMActivateEvent);
#endif
}

bool Node::willRespondToMouseWheelEvents()
{
    return hasEventListeners(eventNames().mousewheelEvent);
}

// It's important not to inline removedLastRef, because we don't want to inline the code to
// delete a Node at each deref call site.
void Node::removedLastRef()
{
    // An explicit check for Document here is better than a virtual function since it is
    // faster for non-Document nodes, and because the call to removedLastRef that is inlined
    // at all deref call sites is smaller if it's a non-virtual function.
    if (isDocumentNode()) {
        toDocument(*this).removedLastRef();
        return;
    }

#ifndef NDEBUG
    m_deletionHasBegun = true;
#endif
    delete this;
}

void Node::textRects(Vector<IntRect>& rects) const
{
    RefPtr<Range> range = Range::create(document());
    range->selectNodeContents(const_cast<Node*>(this), IGNORE_EXCEPTION);
    range->textRects(rects);
}

unsigned Node::connectedSubframeCount() const
{
    return hasRareData() ? rareData()->connectedSubframeCount() : 0;
}

void Node::incrementConnectedSubframeCount(unsigned amount)
{
    ASSERT(isContainerNode());
    ensureRareData().incrementConnectedSubframeCount(amount);
}

void Node::decrementConnectedSubframeCount(unsigned amount)
{
    rareData()->decrementConnectedSubframeCount(amount);
}

void Node::updateAncestorConnectedSubframeCountForRemoval() const
{
    unsigned count = connectedSubframeCount();

    if (!count)
        return;

    for (Node* node = parentOrShadowHostNode(); node; node = node->parentOrShadowHostNode())
        node->decrementConnectedSubframeCount(count);
}

void Node::updateAncestorConnectedSubframeCountForInsertion() const
{
    unsigned count = connectedSubframeCount();

    if (!count)
        return;

    for (Node* node = parentOrShadowHostNode(); node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount(count);
}

bool Node::inRenderedDocument() const
{
    return inDocument() && document().hasLivingRenderTree();
}

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::Node* node)
{
    if (node)
        node->showTreeForThis();
}

void showNodePath(const WebCore::Node* node)
{
    if (node)
        node->showNodePathForThis();
}

#endif

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "Attr.h"
#include "CSSParser.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "CString.h"
#include "ChildNodeList.h"
#include "ClassNodeList.h"
#include "DOMImplementation.h"
#include "Document.h"
#include "DynamicNodeList.h"
#include "Element.h"
#include "Event.h"
#include "EventException.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "InspectorTimelineAgent.h"
#include "KeyboardEvent.h"
#include "Logging.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "NameNodeList.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ProcessingInstruction.h"
#include "ProgressEvent.h"
#include "RegisteredEventListener.h"
#include "RenderObject.h"
#include "ScriptController.h"
#include "SelectorNodeList.h"
#include "StringBuilder.h"
#include "TagNodeList.h"
#include "Text.h"
#include "TextEvent.h"
#include "UIEvent.h"
#include "UIEventWithKeyState.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"
#include "WheelEvent.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include <wtf/HashSet.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/UnusedParam.h>

#if ENABLE(DOM_STORAGE)
#include "StorageEvent.h"
#endif

#if ENABLE(SVG)
#include "SVGElementInstance.h"
#include "SVGUseElement.h"
#endif

#if ENABLE(XHTMLMP)
#include "HTMLNoScriptElement.h"
#endif

#define DUMP_NODE_STATISTICS 0

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static HashSet<Node*>* gNodesDispatchingSimulatedClicks = 0;

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

    HashMap<String, size_t> perTagCount;

    size_t attributes = 0;
    size_t mappedAttributes = 0;
    size_t mappedAttributesWithStyleDecl = 0;
    size_t attributesWithAttr = 0;
    size_t attrMaps = 0;
    size_t mappedAttrMaps = 0;

    for (HashSet<Node*>::const_iterator it = liveNodeSet.begin(); it != liveNodeSet.end(); ++it) {
        Node* node = *it;

        if (node->hasRareData())
            ++nodesWithRareData;

        switch (node->nodeType()) {
            case ELEMENT_NODE: {
                ++elementNodes;

                // Tag stats
                Element* element = static_cast<Element*>(node);
                pair<HashMap<String, size_t>::iterator, bool> result = perTagCount.add(element->tagName(), 1);
                if (!result.second)
                    result.first->second++;

                // AttributeMap stats
                if (NamedNodeMap* attrMap = element->attributes(true)) {
                    attributes += attrMap->length();
                    ++attrMaps;
                    if (attrMap->isMappedAttributeMap())
                        ++mappedAttrMaps;
                    for (unsigned i = 0; i < attrMap->length(); ++i) {
                        Attribute* attr = attrMap->attributeItem(i);
                        if (attr->attr())
                            ++attributesWithAttr;
                        if (attr->isMappedAttribute()) {
                            ++mappedAttributes;
                            if (attr->style())
                                ++mappedAttributesWithStyleDecl;
                        }
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

    printf("NodeType distrubution:\n");
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

    printf("Element tag name distibution:\n");
    for (HashMap<String, size_t>::const_iterator it = perTagCount.begin(); it != perTagCount.end(); ++it)
        printf("  Number of <%s> tags: %zu\n", it->first.utf8().data(), it->second);

    printf("Attribute Maps:\n");
    printf("  Number of Attributes (non-Node and Node): %zu [%zu]\n", attributes, sizeof(Attribute));
    printf("  Number of MappedAttributes: %zu [%zu]\n", mappedAttributes, sizeof(MappedAttribute));
    printf("  Number of MappedAttributes with a StyleDeclaration: %zu\n", mappedAttributesWithStyleDecl);
    printf("  Number of Attributes with an Attr: %zu\n", attributesWithAttr);
    printf("  Number of NamedNodeMaps: %zu\n", attrMaps);
    printf("  Number of NamedMappedAttrMap: %zu\n", mappedAttrMaps);
#endif
}

#ifndef NDEBUG
static WTF::RefCountedLeakCounter nodeCounter("WebCoreNode");

static bool shouldIgnoreLeaks = false;
static HashSet<Node*> ignoreSet;
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

Node::StyleChange Node::diff(const RenderStyle* s1, const RenderStyle* s2)
{
    // FIXME: The behavior of this function is just totally wrong.  It doesn't handle
    // explicit inheritance of non-inherited properties and so you end up not re-resolving
    // style in cases where you need to.
    StyleChange ch = NoInherit;
    EDisplay display1 = s1 ? s1->display() : NONE;
    bool fl1 = s1 && s1->hasPseudoStyle(FIRST_LETTER);
    EDisplay display2 = s2 ? s2->display() : NONE;
    bool fl2 = s2 && s2->hasPseudoStyle(FIRST_LETTER);
        
    if (display1 != display2 || fl1 != fl2 || (s1 && s2 && !s1->contentDataEquivalent(s2)))
        ch = Detach;
    else if (!s1 || !s2)
        ch = Inherit;
    else if (*s1 == *s2)
        ch = NoChange;
    else if (s1->inheritedNotEqual(s2))
        ch = Inherit;
    
    // For nth-child and other positional rules, treat styles as different if they have
    // changed positionally in the DOM. This way subsequent sibling resolutions won't be confused
    // by the wrong child index and evaluate to incorrect results.
    if (ch == NoChange && s1->childIndex() != s2->childIndex())
        ch = NoInherit;

    // If the pseudoStyles have changed, we want any StyleChange that is not NoChange
    // because setStyle will do the right thing with anything else.
    if (ch == NoChange && s1->hasPseudoStyle(BEFORE)) {
        RenderStyle* ps2 = s2->getCachedPseudoStyle(BEFORE);
        if (!ps2)
            ch = NoInherit;
        else {
            RenderStyle* ps1 = s1->getCachedPseudoStyle(BEFORE);
            ch = ps1 && *ps1 == *ps2 ? NoChange : NoInherit;
        }
    }
    if (ch == NoChange && s1->hasPseudoStyle(AFTER)) {
        RenderStyle* ps2 = s2->getCachedPseudoStyle(AFTER);
        if (!ps2)
            ch = NoInherit;
        else {
            RenderStyle* ps1 = s1->getCachedPseudoStyle(AFTER);
            ch = ps2 && *ps1 == *ps2 ? NoChange : NoInherit;
        }
    }
    
    return ch;
}

inline bool Node::initialRefCount(ConstructionType type)
{
    switch (type) {
        case CreateContainer:
        case CreateElement:
        case CreateOther:
        case CreateText:
            return 1;
        case CreateElementZeroRefCount:
            return 0;
    }
    ASSERT_NOT_REACHED();
    return 1;
}

inline bool Node::isContainer(ConstructionType type)
{
    switch (type) {
        case CreateContainer:
        case CreateElement:
        case CreateElementZeroRefCount:
            return true;
        case CreateOther:
        case CreateText:
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

inline bool Node::isElement(ConstructionType type)
{
    switch (type) {
        case CreateContainer:
        case CreateOther:
        case CreateText:
            return false;
        case CreateElement:
        case CreateElementZeroRefCount:
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

inline bool Node::isText(ConstructionType type)
{
    switch (type) {
        case CreateContainer:
        case CreateElement:
        case CreateElementZeroRefCount:
        case CreateOther:
            return false;
        case CreateText:
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

Node::Node(Document* document, ConstructionType type)
    : TreeShared<Node>(initialRefCount(type))
    , m_document(document)
    , m_previous(0)
    , m_next(0)
    , m_renderer(0)
    , m_styleChange(NoStyleChange)
    , m_hasId(false)
    , m_hasClass(false)
    , m_attached(false)
    , m_childNeedsStyleRecalc(false)
    , m_inDocument(false)
    , m_isLink(false)
    , m_active(false)
    , m_hovered(false)
    , m_inActiveChain(false)
    , m_inDetach(false)
    , m_inSubtreeMark(false)
    , m_hasRareData(false)
    , m_isElement(isElement(type))
    , m_isContainer(isContainer(type))
    , m_isText(isText(type))
    , m_parsingChildrenFinished(true)
    , m_isStyleAttributeValid(true)
    , m_synchronizingStyleAttribute(false)
#if ENABLE(SVG)
    , m_areSVGAttributesValid(true)
    , m_synchronizingSVGAttributes(false)
#endif
{
    if (m_document)
        m_document->selfOnlyRef();

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
    HashSet<Node*>::iterator it = ignoreSet.find(this);
    if (it != ignoreSet.end())
        ignoreSet.remove(it);
    else
        nodeCounter.decrement();
#endif

#if DUMP_NODE_STATISTICS
    liveNodeSet.remove(this);
#endif

    if (!eventListeners().isEmpty() && !inDocument())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    if (!hasRareData())
        ASSERT(!NodeRareData::rareDataMap().contains(this));
    else {
        if (m_document && rareData()->nodeLists())
            m_document->removeNodeListCache();
        
        NodeRareData::NodeRareDataMap& dataMap = NodeRareData::rareDataMap();
        NodeRareData::NodeRareDataMap::iterator it = dataMap.find(this);
        ASSERT(it != dataMap.end());
        delete it->second;
        dataMap.remove(it);
    }

    if (renderer())
        detach();

    if (m_previous)
        m_previous->setNextSibling(0);
    if (m_next)
        m_next->setPreviousSibling(0);

    if (m_document)
        m_document->selfOnlyDeref();
}

#ifdef NDEBUG

static inline void setWillMoveToNewOwnerDocumentWasCalled(bool)
{
}

static inline void setDidMoveToNewOwnerDocumentWasCalled(bool)
{
}

#else
    
static bool willMoveToNewOwnerDocumentWasCalled;
static bool didMoveToNewOwnerDocumentWasCalled;

static void setWillMoveToNewOwnerDocumentWasCalled(bool wasCalled)
{
    willMoveToNewOwnerDocumentWasCalled = wasCalled;
}

static void setDidMoveToNewOwnerDocumentWasCalled(bool wasCalled)
{
    didMoveToNewOwnerDocumentWasCalled = wasCalled;
}
    
#endif
    
void Node::setDocument(Document* document)
{
    if (inDocument() || m_document == document)
        return;

    document->selfOnlyRef();

    setWillMoveToNewOwnerDocumentWasCalled(false);
    willMoveToNewOwnerDocument();
    ASSERT(willMoveToNewOwnerDocumentWasCalled);

#if USE(JSC)
    updateDOMNodeDocument(this, m_document, document);
#endif

    if (m_document)
        m_document->selfOnlyDeref();

    m_document = document;

    setDidMoveToNewOwnerDocumentWasCalled(false);
    didMoveToNewOwnerDocument();
    ASSERT(didMoveToNewOwnerDocumentWasCalled);
}

NodeRareData* Node::rareData() const
{
    ASSERT(hasRareData());
    return NodeRareData::rareDataFromMap(this);
}

NodeRareData* Node::ensureRareData()
{
    if (hasRareData())
        return rareData();
    
    ASSERT(!NodeRareData::rareDataMap().contains(this));
    NodeRareData* data = createRareData();
    NodeRareData::rareDataMap().set(this, data);
    m_hasRareData = true;
    return data;
}
    
NodeRareData* Node::createRareData()
{
    return new NodeRareData;
}
    
short Node::tabIndex() const
{
    return hasRareData() ? rareData()->tabIndex() : 0;
}
    
void Node::setTabIndexExplicitly(short i)
{
    ensureRareData()->setTabIndexExplicitly(i);
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
    NodeRareData* data = ensureRareData();
    if (!data->nodeLists()) {
        data->setNodeLists(NodeListsNodeData::create());
        if (document())
            document()->addNodeListCache();
    }

    return ChildNodeList::create(this, data->nodeLists()->m_childNodeListCaches.get());
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

bool Node::insertBefore(PassRefPtr<Node>, Node*, ExceptionCode& ec, bool)
{
    ec = HIERARCHY_REQUEST_ERR;
    return false;
}

bool Node::replaceChild(PassRefPtr<Node>, Node*, ExceptionCode& ec, bool)
{
    ec = HIERARCHY_REQUEST_ERR;
    return false;
}

bool Node::removeChild(Node*, ExceptionCode& ec)
{
    ec = NOT_FOUND_ERR;
    return false;
}

bool Node::appendChild(PassRefPtr<Node>, ExceptionCode& ec, bool)
{
    ec = HIERARCHY_REQUEST_ERR;
    return false;
}

void Node::remove(ExceptionCode& ec)
{
    ref();
    if (Node *p = parentNode())
        p->removeChild(this, ec);
    else
        ec = HIERARCHY_REQUEST_ERR;
    deref();
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
            static_cast<Element*>(node.get())->normalizeAttributes();

        if (node == this)
            break;

        if (type != TEXT_NODE) {
            node = node->traverseNextNodePostOrder();
            continue;
        }

        Text* text = static_cast<Text*>(node.get());

        // Remove empty text nodes.
        if (!text->length()) {
            // Care must be taken to get the next node before removing the current node.
            node = node->traverseNextNodePostOrder();
            ExceptionCode ec;
            text->remove(ec);
            continue;
        }

        // Merge text nodes.
        while (Node* nextSibling = node->nextSibling()) {
            if (!nextSibling->isTextNode())
                break;
            RefPtr<Text> nextText = static_cast<Text*>(nextSibling);

            // Remove empty text nodes.
            if (!nextText->length()) {
                ExceptionCode ec;
                nextText->remove(ec);
                continue;
            }

            // Both non-empty text nodes. Merge them.
            unsigned offset = text->length();
            ExceptionCode ec;
            text->appendData(nextText->data(), ec);
            document()->textNodesMerged(nextText.get(), offset);
            nextText->remove(ec);
        }

        node = node->traverseNextNodePostOrder();
    }
}

const AtomicString& Node::virtualPrefix() const
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

const AtomicString& Node::virtualLocalName() const
{
    return nullAtom;
}

const AtomicString& Node::virtualNamespaceURI() const
{
    return nullAtom;
}

ContainerNode* Node::addChild(PassRefPtr<Node>)
{
    return 0;
}

bool Node::isContentEditable() const
{
    return parent() && parent()->isContentEditable();
}

bool Node::isContentRichlyEditable() const
{
    return parent() && parent()->isContentRichlyEditable();
}

bool Node::shouldUseInputMethod() const
{
    return isContentEditable();
}

RenderBox* Node::renderBox() const
{
    return m_renderer && m_renderer->isBox() ? toRenderBox(m_renderer) : 0;
}

RenderBoxModelObject* Node::renderBoxModelObject() const
{
    return m_renderer && m_renderer->isBoxModelObject() ? toRenderBoxModelObject(m_renderer) : 0;
}

IntRect Node::getRect() const
{
    // FIXME: broken with transforms
    if (renderer())
        return renderer()->absoluteBoundingBoxRect();
    return IntRect();
}

void Node::setNeedsStyleRecalc(StyleChangeType changeType)
{
    if ((changeType != NoStyleChange) && !attached()) // changed compared to what?
        return;

    if (!(changeType == InlineStyleChange && (m_styleChange == FullStyleChange || m_styleChange == SyntheticStyleChange)))
        m_styleChange = changeType;

    if (m_styleChange != NoStyleChange) {
        for (Node* p = parentNode(); p && !p->childNeedsStyleRecalc(); p = p->parentNode())
            p->setChildNeedsStyleRecalc(true);
        if (document()->childNeedsStyleRecalc())
            document()->scheduleStyleRecalc();
    }
}

static Node* outermostLazyAttachedAncestor(Node* start)
{
    Node* p = start;
    for (Node* next = p->parentNode(); !next->renderer(); p = next, next = next->parentNode()) {}
    return p;
}

void Node::lazyAttach()
{
    bool mustDoFullAttach = false;

    for (Node* n = this; n; n = n->traverseNextNode(this)) {
        if (!n->canLazyAttach()) {
            mustDoFullAttach = true;
            break;
        }

        if (n->firstChild())
            n->setChildNeedsStyleRecalc(true);
        n->m_styleChange = FullStyleChange;
        n->m_attached = true;
    }

    if (mustDoFullAttach) {
        Node* lazyAttachedAncestor = outermostLazyAttachedAncestor(this);
        if (lazyAttachedAncestor->attached())
            lazyAttachedAncestor->detach();
        lazyAttachedAncestor->attach();
    } else {
        for (Node* p = parentNode(); p && !p->childNeedsStyleRecalc(); p = p->parentNode())
            p->setChildNeedsStyleRecalc(true);
        if (document()->childNeedsStyleRecalc())
            document()->scheduleStyleRecalc();
    }
}

bool Node::canLazyAttach()
{
    return shadowAncestorNode() == this;
}
    
void Node::setFocus(bool b)
{ 
    if (b || hasRareData())
        ensureRareData()->setFocused(b);
}

bool Node::rareDataFocused() const
{
    ASSERT(hasRareData());
    return rareData()->isFocused();
}

bool Node::supportsFocus() const
{
    return hasRareData() && rareData()->tabIndexSetExplicitly();
}
    
bool Node::isFocusable() const
{
    if (!inDocument() || !supportsFocus())
        return false;
    
    if (renderer())
        ASSERT(!renderer()->needsLayout());
    else
        // If the node is in a display:none tree it might say it needs style recalc but
        // the whole document is atually up to date.
        ASSERT(!document()->childNeedsStyleRecalc());
    
    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Hyatt wants to fix that some day with a "has visible content" flag or the like.
    if (!renderer() || renderer()->style()->visibility() != VISIBLE)
        return false;

    return true;
}

bool Node::isKeyboardFocusable(KeyboardEvent*) const
{
    return isFocusable() && tabIndex() >= 0;
}

bool Node::isMouseFocusable() const
{
    return isFocusable();
}

unsigned Node::nodeIndex() const
{
    Node *_tempNode = previousSibling();
    unsigned count=0;
    for ( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
}

void Node::registerDynamicNodeList(DynamicNodeList* list)
{
    NodeRareData* data = ensureRareData();
    if (!data->nodeLists()) {
        data->setNodeLists(NodeListsNodeData::create());
        document()->addNodeListCache();
    } else if (!m_document || !m_document->hasNodeListCaches()) {
        // We haven't been receiving notifications while there were no registered lists, so the cache is invalid now.
        data->nodeLists()->invalidateCaches();
    }

    if (list->hasOwnCaches())
        data->nodeLists()->m_listsWithCaches.add(list);
}

void Node::unregisterDynamicNodeList(DynamicNodeList* list)
{
    ASSERT(rareData());
    ASSERT(rareData()->nodeLists());
    if (list->hasOwnCaches()) {
        NodeRareData* data = rareData();
        data->nodeLists()->m_listsWithCaches.remove(list);
        if (data->nodeLists()->isEmpty()) {
            data->clearNodeLists();
            if (document())
                document()->removeNodeListCache();
        }
    }
}

void Node::notifyLocalNodeListsAttributeChanged()
{
    if (!hasRareData())
        return;
    NodeRareData* data = rareData();
    if (!data->nodeLists())
        return;

    data->nodeLists()->invalidateCachesThatDependOnAttributes();

    if (data->nodeLists()->isEmpty()) {
        data->clearNodeLists();
        document()->removeNodeListCache();
    }
}

void Node::notifyNodeListsAttributeChanged()
{
    for (Node *n = this; n; n = n->parentNode())
        n->notifyLocalNodeListsAttributeChanged();
}

void Node::notifyLocalNodeListsChildrenChanged()
{
    if (!hasRareData())
        return;
    NodeRareData* data = rareData();
    if (!data->nodeLists())
        return;

    data->nodeLists()->invalidateCaches();

    NodeListsNodeData::NodeListSet::iterator end = data->nodeLists()->m_listsWithCaches.end();
    for (NodeListsNodeData::NodeListSet::iterator i = data->nodeLists()->m_listsWithCaches.begin(); i != end; ++i)
        (*i)->invalidateCache();

    if (data->nodeLists()->isEmpty()) {
        data->clearNodeLists();
        document()->removeNodeListCache();
    }
}

void Node::notifyNodeListsChildrenChanged()
{
    for (Node* n = this; n; n = n->parentNode())
        n->notifyLocalNodeListsChildrenChanged();
}

Node *Node::traverseNextNode(const Node *stayWithin) const
{
    if (firstChild())
        return firstChild();
    if (this == stayWithin)
        return 0;
    if (nextSibling())
        return nextSibling();
    const Node *n = this;
    while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return 0;
}

Node *Node::traverseNextSibling(const Node *stayWithin) const
{
    if (this == stayWithin)
        return 0;
    if (nextSibling())
        return nextSibling();
    const Node *n = this;
    while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return 0;
}

Node* Node::traverseNextNodePostOrder() const
{
    Node* next = nextSibling();
    if (!next)
        return parentNode();
    while (Node* firstChild = next->firstChild())
        next = firstChild;
    return next;
}

Node *Node::traversePreviousNode(const Node *stayWithin) const
{
    if (this == stayWithin)
        return 0;
    if (previousSibling()) {
        Node *n = previousSibling();
        while (n->lastChild())
            n = n->lastChild();
        return n;
    }
    return parentNode();
}

Node *Node::traversePreviousNodePostOrder(const Node *stayWithin) const
{
    if (lastChild())
        return lastChild();
    if (this == stayWithin)
        return 0;
    if (previousSibling())
        return previousSibling();
    const Node *n = this;
    while (n && !n->previousSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n)
        return n->previousSibling();
    return 0;
}

Node* Node::traversePreviousSiblingPostOrder(const Node* stayWithin) const
{
    if (this == stayWithin)
        return 0;
    if (previousSibling())
        return previousSibling();
    const Node *n = this;
    while (n && !n->previousSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n)
        return n->previousSibling();
    return 0;
}

void Node::checkSetPrefix(const AtomicString&, ExceptionCode& ec)
{
    // Perform error checking as required by spec for setting Node.prefix. Used by
    // Element::setPrefix() and Attr::setPrefix()

    // FIXME: Implement support for INVALID_CHARACTER_ERR: Raised if the specified prefix contains an illegal character.
    
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // FIXME: Implement NAMESPACE_ERR: - Raised if the specified prefix is malformed
    // We have to comment this out, since it's used for attributes and tag names, and we've only
    // switched one over.
    /*
    // - if the namespaceURI of this node is null,
    // - if the specified prefix is "xml" and the namespaceURI of this node is different from
    //   "http://www.w3.org/XML/1998/namespace",
    // - if this node is an attribute and the specified prefix is "xmlns" and
    //   the namespaceURI of this node is different from "http://www.w3.org/2000/xmlns/",
    // - or if this node is an attribute and the qualifiedName of this node is "xmlns" [Namespaces].
    if ((namespacePart(id()) == noNamespace && id() > ID_LAST_TAG) ||
        (_prefix == "xml" && String(document()->namespaceURI(id())) != "http://www.w3.org/XML/1998/namespace")) {
        ec = NAMESPACE_ERR;
        return;
    }*/
}

bool Node::canReplaceChild(Node* newChild, Node*)
{
    if (newChild->nodeType() != DOCUMENT_FRAGMENT_NODE) {
        if (!childTypeAllowed(newChild->nodeType()))
            return false;
    } else {
        for (Node *n = newChild->firstChild(); n; n = n->nextSibling()) {
            if (!childTypeAllowed(n->nodeType())) 
                return false;
        }
    }
    return true;
}

void Node::checkReplaceChild(Node* newChild, Node* oldChild, ExceptionCode& ec)
{
    // Perform error checking as required by spec for adding a new child. Used by
    // appendChild(), replaceChild() and insertBefore()
    
    // Not mentioned in spec: throw NOT_FOUND_ERR if newChild is null
    if (!newChild) {
        ec = NOT_FOUND_ERR;
        return;
    }
    
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    
    bool shouldAdoptChild = false;
    
    // WRONG_DOCUMENT_ERR: Raised if newChild was created from a different document than the one that
    // created this node.
    // We assume that if newChild is a DocumentFragment, all children are created from the same document
    // as the fragment itself (otherwise they could not have been added as children)
    if (newChild->document() != document()) {
        // but if the child is not in a document yet then loosen the
        // restriction, so that e.g. creating an element with the Option()
        // constructor and then adding it to a different document works,
        // as it does in Mozilla and Mac IE.
        if (!newChild->inDocument()) {
            shouldAdoptChild = true;
        } else {
            ec = WRONG_DOCUMENT_ERR;
            return;
        }
    }
    
    // HIERARCHY_REQUEST_ERR: Raised if this node is of a type that does not allow children of the type of the
    // newChild node, or if the node to append is one of this node's ancestors.
    
    // check for ancestor/same node
    if (newChild == this || isDescendantOf(newChild)) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }
    
    if (!canReplaceChild(newChild, oldChild)) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }
       
    // change the document pointer of newChild and all of its children to be the new document
    if (shouldAdoptChild)
        for (Node* node = newChild; node; node = node->traverseNextNode(newChild))
            node->setDocument(document());
}

void Node::checkAddChild(Node *newChild, ExceptionCode& ec)
{
    // Perform error checking as required by spec for adding a new child. Used by
    // appendChild(), replaceChild() and insertBefore()

    // Not mentioned in spec: throw NOT_FOUND_ERR if newChild is null
    if (!newChild) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    bool shouldAdoptChild = false;

    // WRONG_DOCUMENT_ERR: Raised if newChild was created from a different document than the one that
    // created this node.
    // We assume that if newChild is a DocumentFragment, all children are created from the same document
    // as the fragment itself (otherwise they could not have been added as children)
    if (newChild->document() != document()) {
        // but if the child is not in a document yet then loosen the
        // restriction, so that e.g. creating an element with the Option()
        // constructor and then adding it to a different document works,
        // as it does in Mozilla and Mac IE.
        if (!newChild->inDocument()) {
            shouldAdoptChild = true;
        } else {
            ec = WRONG_DOCUMENT_ERR;
            return;
        }
    }

    // HIERARCHY_REQUEST_ERR: Raised if this node is of a type that does not allow children of the type of the
    // newChild node, or if the node to append is one of this node's ancestors.

    // check for ancestor/same node
    if (newChild == this || isDescendantOf(newChild)) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }
    
    if (newChild->nodeType() != DOCUMENT_FRAGMENT_NODE) {
        if (!childTypeAllowed(newChild->nodeType())) {
            ec = HIERARCHY_REQUEST_ERR;
            return;
        }
    }
    else {
        for (Node *n = newChild->firstChild(); n; n = n->nextSibling()) {
            if (!childTypeAllowed(n->nodeType())) {
                ec = HIERARCHY_REQUEST_ERR;
                return;
            }
        }
    }
    
    // change the document pointer of newChild and all of its children to be the new document
    if (shouldAdoptChild)
        for (Node* node = newChild; node; node = node->traverseNextNode(newChild))
            node->setDocument(document());
}

bool Node::isDescendantOf(const Node *other) const
{
    // Return true if other is an ancestor of this, otherwise false
    if (!other)
        return false;
    for (const Node *n = parentNode(); n; n = n->parentNode()) {
        if (n == other)
            return true;
    }
    return false;
}

bool Node::contains(const Node* node) const
{
    if (!node)
        return false;
    return this == node || node->isDescendantOf(this);
}

bool Node::childAllowed(Node* newChild)
{
    return childTypeAllowed(newChild->nodeType());
}

void Node::attach()
{
    ASSERT(!attached());
    ASSERT(!renderer() || (renderer()->style() && renderer()->parent()));

    // If this node got a renderer it may be the previousRenderer() of sibling text nodes and thus affect the
    // result of Text::rendererIsNeeded() for those nodes.
    if (renderer()) {
        for (Node* next = nextSibling(); next; next = next->nextSibling()) {
            if (next->renderer())
                break;
            if (!next->attached())
                break;  // Assume this means none of the following siblings are attached.
            if (next->isTextNode())
                next->createRendererIfNeeded();
        }
    }

    m_attached = true;
}

void Node::willRemove()
{
}

void Node::detach()
{
    m_inDetach = true;

    if (renderer())
        renderer()->destroy();
    setRenderer(0);

    Document* doc = document();
    if (m_hovered)
        doc->hoveredNodeDetached(this);
    if (m_inActiveChain)
        doc->activeChainNodeDetached(this);

    m_active = false;
    m_hovered = false;
    m_inActiveChain = false;
    m_attached = false;
    m_inDetach = false;
}

Node *Node::previousEditable() const
{
    Node *node = previousLeafNode();
    while (node) {
        if (node->isContentEditable())
            return node;
        node = node->previousLeafNode();
    }
    return 0;
}

Node *Node::nextEditable() const
{
    Node *node = nextLeafNode();
    while (node) {
        if (node->isContentEditable())
            return node;
        node = node->nextLeafNode();
    }
    return 0;
}

RenderObject * Node::previousRenderer()
{
    for (Node *n = previousSibling(); n; n = n->previousSibling()) {
        if (n->renderer())
            return n->renderer();
    }
    return 0;
}

RenderObject * Node::nextRenderer()
{
    // Avoid an O(n^2) problem with this function by not checking for nextRenderer() when the parent element hasn't even 
    // been attached yet.
    if (parent() && !parent()->attached())
        return 0;

    for (Node *n = nextSibling(); n; n = n->nextSibling()) {
        if (n->renderer())
            return n->renderer();
    }
    return 0;
}

// FIXME: This code is used by editing.  Seems like it could move over there and not pollute Node.
Node *Node::previousNodeConsideringAtomicNodes() const
{
    if (previousSibling()) {
        Node *n = previousSibling();
        while (!isAtomicNode(n) && n->lastChild())
            n = n->lastChild();
        return n;
    }
    else if (parentNode()) {
        return parentNode();
    }
    else {
        return 0;
    }
}

Node *Node::nextNodeConsideringAtomicNodes() const
{
    if (!isAtomicNode(this) && firstChild())
        return firstChild();
    if (nextSibling())
        return nextSibling();
    const Node *n = this;
    while (n && !n->nextSibling())
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return 0;
}

Node *Node::previousLeafNode() const
{
    Node *node = previousNodeConsideringAtomicNodes();
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = node->previousNodeConsideringAtomicNodes();
    }
    return 0;
}

Node *Node::nextLeafNode() const
{
    Node *node = nextNodeConsideringAtomicNodes();
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = node->nextNodeConsideringAtomicNodes();
    }
    return 0;
}

void Node::createRendererIfNeeded()
{
    if (!document()->shouldCreateRenderers())
        return;

    ASSERT(!renderer());
    
    Node* parent = parentNode();    
    ASSERT(parent);
    
    RenderObject* parentRenderer = parent->renderer();
    if (parentRenderer && parentRenderer->canHaveChildren()
#if ENABLE(SVG) || ENABLE(XHTMLMP)
        && parent->childShouldCreateRenderer(this)
#endif
        ) {
        RefPtr<RenderStyle> style = styleForRenderer();
        if (rendererIsNeeded(style.get())) {
            if (RenderObject* r = createRenderer(document()->renderArena(), style.get())) {
                if (!parentRenderer->isChildAllowed(r, style.get()))
                    r->destroy();
                else {
                    setRenderer(r);
                    renderer()->setAnimatableStyle(style.release());
                    parentRenderer->addChild(renderer(), nextRenderer());
                }
            }
        }
    }
}

PassRefPtr<RenderStyle> Node::styleForRenderer()
{
    if (isElementNode()) {
        bool allowSharing = true;
#if ENABLE(XHTMLMP)
        // noscript needs the display property protected - it's a special case
        allowSharing = localName() != HTMLNames::noscriptTag.localName();
#endif
        return document()->styleSelector()->styleForElement(static_cast<Element*>(this), 0, allowSharing);
    }
    return parentNode() && parentNode()->renderer() ? parentNode()->renderer()->style() : 0;
}

bool Node::rendererIsNeeded(RenderStyle *style)
{
    return (document()->documentElement() == this) || (style->display() != NONE);
}

RenderObject* Node::createRenderer(RenderArena*, RenderStyle*)
{
    ASSERT(false);
    return 0;
}
    
RenderStyle* Node::nonRendererRenderStyle() const
{ 
    return 0; 
}   

void Node::setRenderStyle(PassRefPtr<RenderStyle> s)
{
    if (m_renderer)
        m_renderer->setAnimatableStyle(s); 
}

RenderStyle* Node::computedStyle()
{
    return parent() ? parent()->computedStyle() : 0;
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
    if (isContentEditable())
        return true;

    if (renderer()) {
        RenderStyle* style = renderer()->style();
        // We allow selections to begin within an element that has -webkit-user-select: none set,
        // but if the element is draggable then dragging should take priority over selection.
        if (style->userDrag() == DRAG_ELEMENT && style->userSelect() == SELECT_NONE)
            return false;
    }
    return parent() ? parent()->canStartSelection() : true;
}

Node* Node::shadowAncestorNode()
{
#if ENABLE(SVG)
    // SVG elements living in a shadow tree only occour when <use> created them.
    // For these cases we do NOT want to return the shadowParentNode() here
    // but the actual shadow tree element - as main difference to the HTML forms
    // shadow tree concept. (This function _could_ be made virtual - opinions?)
    if (isSVGElement())
        return this;
#endif

    Node* root = shadowTreeRootNode();
    if (root)
        return root->shadowParentNode();
    return this;
}

Node* Node::shadowTreeRootNode()
{
    Node* root = this;
    while (root) {
        if (root->isShadowNode())
            return root;
        root = root->parentNode();
    }
    return 0;
}

bool Node::isInShadowTree()
{
    for (Node* n = this; n; n = n->parentNode())
        if (n->isShadowNode())
            return true;
    return false;
}

bool Node::isBlockFlow() const
{
    return renderer() && renderer()->isBlockFlow();
}

bool Node::isBlockFlowOrBlockTable() const
{
    return renderer() && (renderer()->isBlockFlow() || (renderer()->isTable() && !renderer()->isInline()));
}

bool Node::isEditableBlock() const
{
    return isContentEditable() && isBlockFlow();
}

Element *Node::enclosingBlockFlowElement() const
{
    Node *n = const_cast<Node *>(this);
    if (isBlockFlow())
        return static_cast<Element *>(n);

    while (1) {
        n = n->parentNode();
        if (!n)
            break;
        if (n->isBlockFlow() || n->hasTagName(bodyTag))
            return static_cast<Element *>(n);
    }
    return 0;
}

Element *Node::enclosingInlineElement() const
{
    Node *n = const_cast<Node *>(this);
    Node *p;

    while (1) {
        p = n->parentNode();
        if (!p || p->isBlockFlow() || p->hasTagName(bodyTag))
            return static_cast<Element *>(n);
        // Also stop if any previous sibling is a block
        for (Node *sibling = n->previousSibling(); sibling; sibling = sibling->previousSibling()) {
            if (sibling->isBlockFlow())
                return static_cast<Element *>(n);
        }
        n = p;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

Element* Node::rootEditableElement() const
{
    Element* result = 0;
    for (Node* n = const_cast<Node*>(this); n && n->isContentEditable(); n = n->parentNode()) {
        if (n->isElementNode())
            result = static_cast<Element*>(n);
        if (n->hasTagName(bodyTag))
            break;
    }
    return result;
}

bool Node::inSameContainingBlockFlowElement(Node *n)
{
    return n ? enclosingBlockFlowElement() == n->enclosingBlockFlowElement() : false;
}

// FIXME: End of obviously misplaced HTML editing functions.  Try to move these out of Node.

PassRefPtr<NodeList> Node::getElementsByTagName(const String& name)
{
    return getElementsByTagNameNS(starAtom, name);
}
 
PassRefPtr<NodeList> Node::getElementsByTagNameNS(const AtomicString& namespaceURI, const String& localName)
{
    if (localName.isNull())
        return 0;
    
    NodeRareData* data = ensureRareData();
    if (!data->nodeLists()) {
        data->setNodeLists(NodeListsNodeData::create());
        document()->addNodeListCache();
    }

    String name = localName;
    if (document()->isHTMLDocument())
        name = localName.lower();
    
    AtomicString localNameAtom = name;
        
    pair<NodeListsNodeData::TagCacheMap::iterator, bool> result = data->nodeLists()->m_tagNodeListCaches.add(QualifiedName(nullAtom, localNameAtom, namespaceURI), 0);
    if (result.second)
        result.first->second = DynamicNodeList::Caches::create();
    
    return TagNodeList::create(this, namespaceURI.isEmpty() ? nullAtom : namespaceURI, localNameAtom, result.first->second.get());
}

PassRefPtr<NodeList> Node::getElementsByName(const String& elementName)
{
    NodeRareData* data = ensureRareData();
    if (!data->nodeLists()) {
        data->setNodeLists(NodeListsNodeData::create());
        document()->addNodeListCache();
    }

    pair<NodeListsNodeData::CacheMap::iterator, bool> result = data->nodeLists()->m_nameNodeListCaches.add(elementName, 0);
    if (result.second)
        result.first->second = DynamicNodeList::Caches::create();
    
    return NameNodeList::create(this, elementName, result.first->second.get());
}

PassRefPtr<NodeList> Node::getElementsByClassName(const String& classNames)
{
    NodeRareData* data = ensureRareData();
    if (!data->nodeLists()) {
        data->setNodeLists(NodeListsNodeData::create());
        document()->addNodeListCache();
    }

    pair<NodeListsNodeData::CacheMap::iterator, bool> result = data->nodeLists()->m_classNodeListCaches.add(classNames, 0);
    if (result.second)
        result.first->second = DynamicNodeList::Caches::create();
    
    return ClassNodeList::create(this, classNames, result.first->second.get());
}

template <typename Functor>
static bool forEachTagSelector(Functor& functor, CSSSelector* selector)
{
    ASSERT(selector);

    do {
        if (functor(selector))
            return true;
        if (CSSSelector* simpleSelector = selector->simpleSelector()) {
            if (forEachTagSelector(functor, simpleSelector))
                return true;
        }
    } while ((selector = selector->tagHistory()));

    return false;
}

template <typename Functor>
static bool forEachSelector(Functor& functor, const CSSSelectorList& selectorList)
{
    for (CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
        if (forEachTagSelector(functor, selector))
            return true;
    }

    return false;
}

class SelectorNeedsNamespaceResolutionFunctor {
public:
    bool operator()(CSSSelector* selector)
    {
        if (selector->hasTag() && selector->m_tag.prefix() != nullAtom && selector->m_tag.prefix() != starAtom)
            return true;
        if (selector->hasAttribute() && selector->attribute().prefix() != nullAtom && selector->attribute().prefix() != starAtom)
            return true;
        return false;
    }
};

static bool selectorNeedsNamespaceResolution(const CSSSelectorList& selectorList)
{
    SelectorNeedsNamespaceResolutionFunctor functor;
    return forEachSelector(functor, selectorList);
}

PassRefPtr<Element> Node::querySelector(const String& selectors, ExceptionCode& ec)
{
    if (selectors.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    bool strictParsing = !document()->inCompatMode();
    CSSParser p(strictParsing);

    CSSSelectorList querySelectorList;
    p.parseSelector(selectors, document(), querySelectorList);

    if (!querySelectorList.first()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    // throw a NAMESPACE_ERR if the selector includes any namespace prefixes.
    if (selectorNeedsNamespaceResolution(querySelectorList)) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    CSSStyleSelector::SelectorChecker selectorChecker(document(), strictParsing);

    // FIXME: we could also optimize for the the [id="foo"] case
    if (strictParsing && inDocument() && querySelectorList.hasOneSelector() && querySelectorList.first()->m_match == CSSSelector::Id) {
        ASSERT(querySelectorList.first()->attribute() == idAttr);
        Element* element = document()->getElementById(querySelectorList.first()->m_value);
        if (element && (isDocumentNode() || element->isDescendantOf(this)) && selectorChecker.checkSelector(querySelectorList.first(), element))
            return element;
        return 0;
    }

    // FIXME: We can speed this up by implementing caching similar to the one use by getElementById
    for (Node* n = firstChild(); n; n = n->traverseNextNode(this)) {
        if (n->isElementNode()) {
            Element* element = static_cast<Element*>(n);
            for (CSSSelector* selector = querySelectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
                if (selectorChecker.checkSelector(selector, element))
                    return element;
            }
        }
    }
    
    return 0;
}

PassRefPtr<NodeList> Node::querySelectorAll(const String& selectors, ExceptionCode& ec)
{
    if (selectors.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    bool strictParsing = !document()->inCompatMode();
    CSSParser p(strictParsing);

    CSSSelectorList querySelectorList;
    p.parseSelector(selectors, document(), querySelectorList);

    if (!querySelectorList.first()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    // Throw a NAMESPACE_ERR if the selector includes any namespace prefixes.
    if (selectorNeedsNamespaceResolution(querySelectorList)) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    return createSelectorNodeList(this, querySelectorList);
}

Document *Node::ownerDocument() const
{
    Document *doc = document();
    return doc == this ? 0 : doc;
}

KURL Node::baseURI() const
{
    return parentNode() ? parentNode()->baseURI() : KURL();
}

bool Node::isEqualNode(Node *other) const
{
    if (!other)
        return false;
    
    if (nodeType() != other->nodeType())
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
    
    NamedNodeMap *attrs = attributes();
    NamedNodeMap *otherAttrs = other->attributes();
    
    if (!attrs && otherAttrs)
        return false;
    
    if (attrs && !attrs->mapsEquivalent(otherAttrs))
        return false;
    
    Node *child = firstChild();
    Node *otherChild = other->firstChild();
    
    while (child) {
        if (!child->isEqualNode(otherChild))
            return false;
        
        child = child->nextSibling();
        otherChild = otherChild->nextSibling();
    }
    
    if (otherChild)
        return false;
    
    // FIXME: For DocumentType nodes we should check equality on
    // the entities and notations NamedNodeMaps as well.
    
    return true;
}

bool Node::isDefaultNamespace(const AtomicString &namespaceURI) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#isDefaultNamespaceAlgo
    
    switch (nodeType()) {
        case ELEMENT_NODE: {
            const Element *elem = static_cast<const Element *>(this);
            
            if (elem->prefix().isNull())
                return elem->namespaceURI() == namespaceURI;

            if (elem->hasAttributes()) {
                NamedNodeMap *attrs = elem->attributes();
                
                for (unsigned i = 0; i < attrs->length(); i++) {
                    Attribute *attr = attrs->attributeItem(i);
                    
                    if (attr->localName() == "xmlns")
                        return attr->value() == namespaceURI;
                }
            }

            if (Element* ancestor = ancestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);

            return false;
        }
        case DOCUMENT_NODE:
            if (Element* de = static_cast<const Document*>(this)->documentElement())
                return de->isDefaultNamespace(namespaceURI);
            return false;
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            return false;
        case ATTRIBUTE_NODE: {
            const Attr *attr = static_cast<const Attr *>(this);
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
            if (Element* de = static_cast<const Document*>(this)->documentElement())
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
                NamedNodeMap *attrs = elem->attributes();
                
                for (unsigned i = 0; i < attrs->length(); i++) {
                    Attribute *attr = attrs->attributeItem(i);
                    
                    if (attr->prefix() == "xmlns" && attr->localName() == prefix) {
                        if (!attr->value().isEmpty())
                            return attr->value();
                        
                        return String();
                    } else if (attr->localName() == "xmlns" && prefix.isNull()) {
                        if (!attr->value().isEmpty())
                            return attr->value();
                        
                        return String();
                    }
                }
            }
            if (Element* ancestor = ancestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            return String();
        }
        case DOCUMENT_NODE:
            if (Element* de = static_cast<const Document*>(this)->documentElement())
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
    
    if (hasAttributes()) {
        NamedNodeMap *attrs = attributes();
        
        for (unsigned i = 0; i < attrs->length(); i++) {
            Attribute *attr = attrs->attributeItem(i);
            
            if (attr->prefix() == "xmlns" &&
                attr->value() == _namespaceURI &&
                originalElement->lookupNamespaceURI(attr->localName()) == _namespaceURI)
                return attr->localName();
        }
    }
    
    if (Element* ancestor = ancestorElement())
        return ancestor->lookupNamespacePrefix(_namespaceURI, originalElement);
    return String();
}

void Node::appendTextContent(bool convertBRsToNewlines, StringBuilder& content) const
{
    switch (nodeType()) {
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case COMMENT_NODE:
            content.append(static_cast<const CharacterData*>(this)->data());
            break;

        case PROCESSING_INSTRUCTION_NODE:
            content.append(static_cast<const ProcessingInstruction*>(this)->data());
            break;
        
        case ELEMENT_NODE:
            if (hasTagName(brTag) && convertBRsToNewlines) {
                content.append('\n');
                break;
        }
        // Fall through.
        case ATTRIBUTE_NODE:
        case ENTITY_NODE:
        case ENTITY_REFERENCE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            content.setNonNull();

            for (Node *child = firstChild(); child; child = child->nextSibling()) {
                if (child->nodeType() == COMMENT_NODE || child->nodeType() == PROCESSING_INSTRUCTION_NODE)
                    continue;
            
                child->appendTextContent(convertBRsToNewlines, content);
            }
            break;

        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case NOTATION_NODE:
        case XPATH_NAMESPACE_NODE:
            break;
    }
}

String Node::textContent(bool convertBRsToNewlines) const
{
    StringBuilder content;
    appendTextContent(convertBRsToNewlines, content);
    return content.toString();
}

void Node::setTextContent(const String &text, ExceptionCode& ec)
{           
    switch (nodeType()) {
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
            setNodeValue(text, ec);
            break;
        case ELEMENT_NODE:
        case ATTRIBUTE_NODE:
        case ENTITY_NODE:
        case ENTITY_REFERENCE_NODE:
        case DOCUMENT_FRAGMENT_NODE: {
            ContainerNode *container = static_cast<ContainerNode *>(this);
            
            container->removeChildren();
            
            if (!text.isEmpty())
                appendChild(document()->createTextNode(text), ec);
            break;
        }
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case NOTATION_NODE:
        default:
            // Do nothing
            break;
    }
}

Element* Node::ancestorElement() const
{
    // In theory, there can be EntityReference nodes between elements, but this is currently not supported.
    for (Node* n = parentNode(); n; n = n->parentNode()) {
        if (n->isElementNode())
            return static_cast<Element*>(n);
    }
    return 0;
}

bool Node::offsetInCharacters() const
{
    return false;
}

unsigned short Node::compareDocumentPosition(Node* otherNode)
{
    // It is not clear what should be done if |otherNode| is 0.
    if (!otherNode)
        return DOCUMENT_POSITION_DISCONNECTED;

    if (otherNode == this)
        return DOCUMENT_POSITION_EQUIVALENT;
    
    Attr* attr1 = nodeType() == ATTRIBUTE_NODE ? static_cast<Attr*>(this) : 0;
    Attr* attr2 = otherNode->nodeType() == ATTRIBUTE_NODE ? static_cast<Attr*>(otherNode) : 0;
    
    Node* start1 = attr1 ? attr1->ownerElement() : this;
    Node* start2 = attr2 ? attr2->ownerElement() : otherNode;
    
    // If either of start1 or start2 is null, then we are disconnected, since one of the nodes is
    // an orphaned attribute node.
    if (!start1 || !start2)
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC;

    Vector<Node*, 16> chain1;
    Vector<Node*, 16> chain2;
    if (attr1)
        chain1.append(attr1);
    if (attr2)
        chain2.append(attr2);
    
    if (attr1 && attr2 && start1 == start2 && start1) {
        // We are comparing two attributes on the same node.  Crawl our attribute map
        // and see which one we hit first.
        NamedNodeMap* map = attr1->ownerElement()->attributes(true);
        unsigned length = map->length();
        for (unsigned i = 0; i < length; ++i) {
            // If neither of the two determining nodes is a child node and nodeType is the same for both determining nodes, then an 
            // implementation-dependent order between the determining nodes is returned. This order is stable as long as no nodes of
            // the same nodeType are inserted into or removed from the direct container. This would be the case, for example, 
            // when comparing two attributes of the same element, and inserting or removing additional attributes might change 
            // the order between existing attributes.
            Attribute* attr = map->attributeItem(i);
            if (attr1->attr() == attr)
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_FOLLOWING;
            if (attr2->attr() == attr)
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_PRECEDING;
        }
        
        ASSERT_NOT_REACHED();
        return DOCUMENT_POSITION_DISCONNECTED;
    }

    // If one node is in the document and the other is not, we must be disconnected.
    // If the nodes have different owning documents, they must be disconnected.  Note that we avoid
    // comparing Attr nodes here, since they return false from inDocument() all the time (which seems like a bug).
    if (start1->inDocument() != start2->inDocument() ||
        start1->document() != start2->document())
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC;

    // We need to find a common ancestor container, and then compare the indices of the two immediate children.
    Node* current;
    for (current = start1; current; current = current->parentNode())
        chain1.append(current);
    for (current = start2; current; current = current->parentNode())
        chain2.append(current);
   
    // Walk the two chains backwards and look for the first difference.
    unsigned index1 = chain1.size();
    unsigned index2 = chain2.size();
    for (unsigned i = min(index1, index2); i; --i) {
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
        return renderer()->localToAbsolute(p, false, true);
    
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
        return renderer()->absoluteToLocal(p, false, true);

    // Otherwise go up the tree looking for a renderer
    Element *parent = ancestorElement();
    if (parent)
        return parent->convertFromPage(p);

    // No parent - no conversion needed
    return p;
}

#ifndef NDEBUG

static void appendAttributeDesc(const Node* node, String& string, const QualifiedName& name, const char* attrDesc)
{
    if (node->isElementNode()) {
        String attr = static_cast<const Element*>(node)->getAttribute(name);
        if (!attr.isEmpty()) {
            string += attrDesc;
            string += attr;
        }
    }
}

void Node::showNode(const char* prefix) const
{
    if (!prefix)
        prefix = "";
    if (isTextNode()) {
        String value = nodeValue();
        value.replace('\\', "\\\\");
        value.replace('\n', "\\n");
        fprintf(stderr, "%s%s\t%p \"%s\"\n", prefix, nodeName().utf8().data(), this, value.utf8().data());
    } else {
        String attrs = "";
        appendAttributeDesc(this, attrs, classAttr, " CLASS=");
        appendAttributeDesc(this, attrs, styleAttr, " STYLE=");
        fprintf(stderr, "%s%s\t%p%s\n", prefix, nodeName().utf8().data(), this, attrs.utf8().data());
    }
}

void Node::showTreeForThis() const
{
    showTreeAndMark(this, "*");
}

void Node::showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char * markedLabel2) const
{
    const Node* rootNode;
    const Node* node = this;
    while (node->parentNode() && !node->hasTagName(bodyTag))
        node = node->parentNode();
    rootNode = node;
        
    for (node = rootNode; node; node = node->traverseNextNode()) {
        if (node == markedNode1)
            fprintf(stderr, "%s", markedLabel1);
        if (node == markedNode2)
            fprintf(stderr, "%s", markedLabel2);
                        
        for (const Node* tmpNode = node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentNode())
            fprintf(stderr, "\t");
        node->showNode();
    }
}

void Node::formatForDebugger(char* buffer, unsigned length) const
{
    String result;
    String s;
    
    s = nodeName();
    if (s.length() == 0)
        result += "<none>";
    else
        result += s;
          
    strncpy(buffer, result.utf8().data(), length - 1);
}

#endif

// --------

void NodeListsNodeData::invalidateCaches()
{
    m_childNodeListCaches->reset();
    TagCacheMap::const_iterator tagCachesEnd = m_tagNodeListCaches.end();
    for (TagCacheMap::const_iterator it = m_tagNodeListCaches.begin(); it != tagCachesEnd; ++it)
        it->second->reset();
    invalidateCachesThatDependOnAttributes();
}

void NodeListsNodeData::invalidateCachesThatDependOnAttributes()
{
    CacheMap::iterator classCachesEnd = m_classNodeListCaches.end();
    for (CacheMap::iterator it = m_classNodeListCaches.begin(); it != classCachesEnd; ++it)
        it->second->reset();

    CacheMap::iterator nameCachesEnd = m_nameNodeListCaches.end();
    for (CacheMap::iterator it = m_nameNodeListCaches.begin(); it != nameCachesEnd; ++it)
        it->second->reset();
}

bool NodeListsNodeData::isEmpty() const
{
    if (!m_listsWithCaches.isEmpty())
        return false;

    if (m_childNodeListCaches->refCount())
        return false;
    
    TagCacheMap::const_iterator tagCachesEnd = m_tagNodeListCaches.end();
    for (TagCacheMap::const_iterator it = m_tagNodeListCaches.begin(); it != tagCachesEnd; ++it) {
        if (it->second->refCount())
            return false;
    }

    CacheMap::const_iterator classCachesEnd = m_classNodeListCaches.end();
    for (CacheMap::const_iterator it = m_classNodeListCaches.begin(); it != classCachesEnd; ++it) {
        if (it->second->refCount())
            return false;
    }

    CacheMap::const_iterator nameCachesEnd = m_nameNodeListCaches.end();
    for (CacheMap::const_iterator it = m_nameNodeListCaches.begin(); it != nameCachesEnd; ++it) {
        if (it->second->refCount())
            return false;
    }

    return true;
}

void Node::getSubresourceURLs(ListHashSet<KURL>& urls) const
{
    addSubresourceAttributeURLs(urls);
}

ContainerNode* Node::eventParentNode()
{
    Node* parent = parentNode();
    ASSERT(!parent || parent->isContainerNode());
    return static_cast<ContainerNode*>(parent);
}

// --------

ScriptExecutionContext* Node::scriptExecutionContext() const
{
    return document();
}

const RegisteredEventListenerVector& Node::eventListeners() const
{
    if (hasRareData()) {
        if (RegisteredEventListenerVector* listeners = rareData()->listeners())
            return *listeners;
    }
    static const RegisteredEventListenerVector* emptyListenersVector = new RegisteredEventListenerVector;
    return *emptyListenersVector;
}

void Node::insertedIntoDocument()
{
    if (!eventListeners().isEmpty())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    setInDocument(true);
}

void Node::removedFromDocument()
{
    if (!eventListeners().isEmpty())
        document()->registerDisconnectedNodeWithEventListeners(this);

    setInDocument(false);
}

void Node::willMoveToNewOwnerDocument()
{
    if (!eventListeners().isEmpty())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    ASSERT(!willMoveToNewOwnerDocumentWasCalled);
    setWillMoveToNewOwnerDocumentWasCalled(true);
}

void Node::didMoveToNewOwnerDocument()
{
    if (!eventListeners().isEmpty())
        document()->registerDisconnectedNodeWithEventListeners(this);

    ASSERT(!didMoveToNewOwnerDocumentWasCalled);
    setDidMoveToNewOwnerDocumentWasCalled(true);
}

static inline void updateSVGElementInstancesAfterEventListenerChange(Node* referenceNode)
{
#if !ENABLE(SVG)
    UNUSED_PARAM(referenceNode);
#else
    ASSERT(referenceNode);
    if (!referenceNode->isSVGElement())
        return;

    // Elements living inside a <use> shadow tree, never cause any updates!
    if (referenceNode->shadowTreeRootNode())
        return;

    // We're possibly (a child of) an element that is referenced by a <use> client
    // If an event listeners changes on a referenced element, update all instances.
    for (Node* node = referenceNode; node; node = node->parentNode()) {
        if (!node->hasID() || !node->isSVGElement())
            continue;

        SVGElementInstance::invalidateAllInstancesOfElement(static_cast<SVGElement*>(node));
        break;
    }
#endif
}

void Node::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    Document* document = this->document();
    if (!document->attached())
        return;

    document->addListenerTypeIfNeeded(eventType);

    RegisteredEventListenerVector& listeners = ensureRareData()->ensureListeners();

    // Remove existing identical listener set with identical arguments.
    // The DOM2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(eventType, listener.get(), useCapture);

    // adding the first one
    if (listeners.isEmpty() && !inDocument())
        document->registerDisconnectedNodeWithEventListeners(this);

    listeners.append(RegisteredEventListener::create(eventType, listener, useCapture));
    updateSVGElementInstancesAfterEventListenerChange(this);
}

void Node::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!hasRareData())
        return;

    RegisteredEventListenerVector* listeners = rareData()->listeners();
    if (!listeners)
        return;

    size_t size = listeners->size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *listeners->at(i);
        if (r.eventType() == eventType && r.useCapture() == useCapture && *r.listener() == *listener) {
            r.setRemoved(true);
            listeners->remove(i);

            // removed last
            if (listeners->isEmpty() && !inDocument())
                document()->unregisterDisconnectedNodeWithEventListeners(this);

            updateSVGElementInstancesAfterEventListenerChange(this);
            return;
        }
    }
}

void Node::removeAllEventListenersSlowCase()
{
    ASSERT(hasRareData());

    RegisteredEventListenerVector* listeners = rareData()->listeners();
    if (!listeners)
        return;

    size_t size = listeners->size();
    for (size_t i = 0; i < size; ++i)
        listeners->at(i)->setRemoved(true);
    listeners->clear();
}

void Node::handleLocalEvents(Event* event, bool useCapture)
{
    if (disabled() && event->isMouseEvent())
        return;

    RegisteredEventListenerVector listenersCopy = eventListeners();
    size_t size = listenersCopy.size();
    for (size_t i = 0; i < size; ++i) {
        const RegisteredEventListener& r = *listenersCopy[i];
        if (r.eventType() == event->type() && r.useCapture() == useCapture && !r.removed())
            r.listener()->handleEvent(event, false);
    }
}

#if ENABLE(SVG)
static inline SVGElementInstance* eventTargetAsSVGElementInstance(Node* referenceNode)
{
    ASSERT(referenceNode);
    if (!referenceNode->isSVGElement())
        return 0;

    // Spec: The event handling for the non-exposed tree works as if the referenced element had been textually included
    // as a deeply cloned child of the 'use' element, except that events are dispatched to the SVGElementInstance objects
    for (Node* n = referenceNode; n; n = n->parentNode()) {
        if (!n->isShadowNode() || !n->isSVGElement())
            continue;

        Node* shadowTreeParentElement = n->shadowParentNode();
        ASSERT(shadowTreeParentElement->hasTagName(SVGNames::useTag));

        if (SVGElementInstance* instance = static_cast<SVGUseElement*>(shadowTreeParentElement)->instanceForShadowTreeElement(referenceNode))
            return instance;
    }

    return 0;
}
#endif

static inline EventTarget* eventTargetRespectingSVGTargetRules(Node* referenceNode)
{
    ASSERT(referenceNode);

#if ENABLE(SVG)
    if (SVGElementInstance* instance = eventTargetAsSVGElementInstance(referenceNode)) {
        ASSERT(instance->shadowTreeElement() == referenceNode);
        return instance;
    }
#endif

    return referenceNode;
}

bool Node::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec)
{
    RefPtr<Event> evt(e);
    ASSERT(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) { 
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }

    evt->setTarget(eventTargetRespectingSVGTargetRules(this));

    RefPtr<FrameView> view = document()->view();
    return dispatchGenericEvent(evt.release());
}

bool Node::dispatchGenericEvent(PassRefPtr<Event> prpEvent)
{
    RefPtr<Event> event(prpEvent);

    ASSERT(!eventDispatchForbidden());
    ASSERT(event->target());
    ASSERT(!event->type().isNull()); // JavaScript code can create an event with an empty name, but not null.

    InspectorTimelineAgent* timelineAgent = document()->inspectorTimelineAgent();
    if (timelineAgent)
        timelineAgent->willDispatchDOMEvent(*event);

    // Make a vector of ancestors to send the event to.
    // If the node is not in a document just send the event to it.
    // Be sure to ref all of nodes since event handlers could result in the last reference going away.
    RefPtr<Node> thisNode(this);
    Vector<RefPtr<ContainerNode> > ancestors;
    if (inDocument()) {
        for (ContainerNode* ancestor = eventParentNode(); ancestor; ancestor = ancestor->eventParentNode()) {
#if ENABLE(SVG)
            // Skip <use> shadow tree elements.
            if (ancestor->isSVGElement() && ancestor->isShadowNode())
                continue;
#endif
            ancestors.append(ancestor);
        }
    }

    // Set up a pointer to indicate whether / where to dispatch window events.
    // We don't dispatch load events to the window. That quirk was originally
    // added because Mozilla doesn't propagate load events to the window object.
    DOMWindow* targetForWindowEvents = 0;
    if (event->type() != eventNames().loadEvent) {
        Node* topLevelContainer = ancestors.isEmpty() ? this : ancestors.last().get();
        if (topLevelContainer->isDocumentNode())
            targetForWindowEvents = static_cast<Document*>(topLevelContainer)->domWindow();
    }

    // Give the target node a chance to do some work before DOM event handlers get a crack.
    void* data = preDispatchEventHandler(event.get());
    if (event->propagationStopped())
        goto doneDispatching;

    // Trigger capturing event handlers, starting at the top and working our way down.
    event->setEventPhase(Event::CAPTURING_PHASE);

    if (targetForWindowEvents) {
        event->setCurrentTarget(targetForWindowEvents);
        targetForWindowEvents->handleEvent(event.get(), true);
        if (event->propagationStopped())
            goto doneDispatching;
    }
    for (size_t i = ancestors.size(); i; --i) {
        ContainerNode* ancestor = ancestors[i - 1].get();
        event->setCurrentTarget(eventTargetRespectingSVGTargetRules(ancestor));
        ancestor->handleLocalEvents(event.get(), true);
        if (event->propagationStopped())
            goto doneDispatching;
    }

    event->setEventPhase(Event::AT_TARGET);

    // We do want capturing event listeners to be invoked here, even though
    // that violates some versions of the DOM specification; Mozilla does it.
    event->setCurrentTarget(eventTargetRespectingSVGTargetRules(this));
    handleLocalEvents(event.get(), true);
    if (event->propagationStopped())
        goto doneDispatching;
    handleLocalEvents(event.get(), false);
    if (event->propagationStopped())
        goto doneDispatching;

    if (event->bubbles() && !event->cancelBubble()) {
        // Trigger bubbling event handlers, starting at the bottom and working our way up.
        event->setEventPhase(Event::BUBBLING_PHASE);

        size_t size = ancestors.size();
        for (size_t i = 0; i < size; ++i) {
            ContainerNode* ancestor = ancestors[i].get();
            event->setCurrentTarget(eventTargetRespectingSVGTargetRules(ancestor));
            ancestor->handleLocalEvents(event.get(), false);
            if (event->propagationStopped() || event->cancelBubble())
                goto doneDispatching;
        }
        if (targetForWindowEvents) {
            event->setCurrentTarget(targetForWindowEvents);
            targetForWindowEvents->handleEvent(event.get(), false);
            if (event->propagationStopped() || event->cancelBubble())
                goto doneDispatching;
        }
    }

doneDispatching:
    event->setCurrentTarget(0);
    event->setEventPhase(0);

    // Pass the data from the preDispatchEventHandler to the postDispatchEventHandler.
    postDispatchEventHandler(event.get(), data);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!event->defaultPrevented() && !event->defaultHandled()) {
        // Non-bubbling events call only one default event handler, the one for the target.
        defaultEventHandler(event.get());
        ASSERT(!event->defaultPrevented());
        if (event->defaultHandled())
            goto doneWithDefault;
        // For bubbling events, call default event handlers on the same targets in the
        // same order as the bubbling phase.
        if (event->bubbles()) {
            size_t size = ancestors.size();
            for (size_t i = 0; i < size; ++i) {
                ContainerNode* ancestor = ancestors[i].get();
                ancestor->defaultEventHandler(event.get());
                ASSERT(!event->defaultPrevented());
                if (event->defaultHandled())
                    goto doneWithDefault;
            }
        }
    }

doneWithDefault:
    if (timelineAgent)
        timelineAgent->didDispatchDOMEvent();

    Document::updateStyleForAllDocuments();

    return !event->defaultPrevented();
}

void Node::dispatchSubtreeModifiedEvent()
{
    ASSERT(!eventDispatchForbidden());
    
    document()->incDOMTreeVersion();

    notifyNodeListsAttributeChanged(); // FIXME: Can do better some day. Really only care about the name attribute changing.
    
    if (!document()->hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return;

    ExceptionCode ec = 0;
    dispatchMutationEvent(eventNames().DOMSubtreeModifiedEvent, true, 0, String(), String(), ec); 
}

void Node::dispatchUIEvent(const AtomicString& eventType, int detail, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    ASSERT(eventType == eventNames().DOMFocusInEvent || eventType == eventNames().DOMFocusOutEvent || eventType == eventNames().DOMActivateEvent);
    
    bool cancelable = eventType == eventNames().DOMActivateEvent;
    
    ExceptionCode ec = 0;
    RefPtr<UIEvent> evt = UIEvent::create(eventType, true, cancelable, document()->defaultView(), detail);
    evt->setUnderlyingEvent(underlyingEvent);
    dispatchEvent(evt.release(), ec);
}

bool Node::dispatchKeyEvent(const PlatformKeyboardEvent& key)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    RefPtr<KeyboardEvent> keyboardEvent = KeyboardEvent::create(key, document()->defaultView());
    bool r = dispatchEvent(keyboardEvent, ec);
    
    // we want to return false if default is prevented (already taken care of)
    // or if the element is default-handled by the DOM. Otherwise we let it just
    // let it get handled by AppKit 
    if (keyboardEvent->defaultHandled())
        r = false;
    
    return r;
}

bool Node::dispatchMouseEvent(const PlatformMouseEvent& event, const AtomicString& eventType,
    int detail, Node* relatedTarget)
{
    ASSERT(!eventDispatchForbidden());
    
    IntPoint contentsPos;
    if (FrameView* view = document()->view())
        contentsPos = view->windowToContents(event.pos());

    short button = event.button();

    ASSERT(event.eventType() == MouseEventMoved || button != NoButton);
    
    return dispatchMouseEvent(eventType, button, detail,
        contentsPos.x(), contentsPos.y(), event.globalX(), event.globalY(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        false, relatedTarget, 0);
}

void Node::dispatchSimulatedMouseEvent(const AtomicString& eventType,
    PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());

    bool ctrlKey = false;
    bool altKey = false;
    bool shiftKey = false;
    bool metaKey = false;
    if (UIEventWithKeyState* keyStateEvent = findEventWithKeyState(underlyingEvent.get())) {
        ctrlKey = keyStateEvent->ctrlKey();
        altKey = keyStateEvent->altKey();
        shiftKey = keyStateEvent->shiftKey();
        metaKey = keyStateEvent->metaKey();
    }

    // Like Gecko, we just pass 0 for everything when we make a fake mouse event.
    // Internet Explorer instead gives the current mouse position and state.
    dispatchMouseEvent(eventType, 0, 0, 0, 0, 0, 0,
        ctrlKey, altKey, shiftKey, metaKey, true, 0, underlyingEvent);
}

void Node::dispatchSimulatedClick(PassRefPtr<Event> event, bool sendMouseEvents, bool showPressedLook)
{
    if (!gNodesDispatchingSimulatedClicks)
        gNodesDispatchingSimulatedClicks = new HashSet<Node*>;
    else if (gNodesDispatchingSimulatedClicks->contains(this))
        return;
    
    gNodesDispatchingSimulatedClicks->add(this);
    
    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(eventNames().mousedownEvent, event.get());
    setActive(true, showPressedLook);
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(eventNames().mouseupEvent, event.get());
    setActive(false);

    // always send click
    dispatchSimulatedMouseEvent(eventNames().clickEvent, event);
    
    gNodesDispatchingSimulatedClicks->remove(this);
}

bool Node::dispatchMouseEvent(const AtomicString& eventType, int button, int detail,
    int pageX, int pageY, int screenX, int screenY,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, 
    bool isSimulated, Node* relatedTargetArg, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    if (disabled()) // Don't even send DOM events for disabled controls..
        return true;
    
    if (eventType.isEmpty())
        return false; // Shouldn't happen.
    
    // Dispatching the first event can easily result in this node being destroyed.
    // Since we dispatch up to three events here, we need to make sure we're referenced
    // so the pointer will be good for the two subsequent ones.
    RefPtr<Node> protect(this);
    
    bool cancelable = eventType != eventNames().mousemoveEvent;
    
    ExceptionCode ec = 0;
    
    bool swallowEvent = false;
    
    // Attempting to dispatch with a non-EventTarget relatedTarget causes the relatedTarget to be silently ignored.
    RefPtr<Node> relatedTarget = relatedTargetArg;

    int adjustedPageX = pageX;
    int adjustedPageY = pageY;
    if (Frame* frame = document()->frame()) {
        float pageZoom = frame->pageZoomFactor();
        if (pageZoom != 1.0f) {
            // Adjust our pageX and pageY to account for the page zoom.
            adjustedPageX = lroundf(pageX / pageZoom);
            adjustedPageY = lroundf(pageY / pageZoom);
        }
    }

    RefPtr<MouseEvent> mouseEvent = MouseEvent::create(eventType,
        true, cancelable, document()->defaultView(),
        detail, screenX, screenY, adjustedPageX, adjustedPageY,
        ctrlKey, altKey, shiftKey, metaKey, button,
        relatedTarget, 0, isSimulated);
    mouseEvent->setUnderlyingEvent(underlyingEvent.get());
    mouseEvent->setAbsoluteLocation(IntPoint(pageX, pageY));
    
    dispatchEvent(mouseEvent, ec);
    bool defaultHandled = mouseEvent->defaultHandled();
    bool defaultPrevented = mouseEvent->defaultPrevented();
    if (defaultHandled || defaultPrevented)
        swallowEvent = true;
    
    // Special case: If it's a double click event, we also send the dblclick event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute.  This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    if (eventType == eventNames().clickEvent && detail == 2) {
        RefPtr<Event> doubleClickEvent = MouseEvent::create(eventNames().dblclickEvent,
            true, cancelable, document()->defaultView(),
            detail, screenX, screenY, pageX, pageY,
            ctrlKey, altKey, shiftKey, metaKey, button,
            relatedTarget, 0, isSimulated);
        doubleClickEvent->setUnderlyingEvent(underlyingEvent.get());
        if (defaultHandled)
            doubleClickEvent->setDefaultHandled();
        dispatchEvent(doubleClickEvent, ec);
        if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
            swallowEvent = true;
    }

    return swallowEvent;
}

void Node::dispatchWheelEvent(PlatformWheelEvent& e)
{
    ASSERT(!eventDispatchForbidden());
    if (e.deltaX() == 0 && e.deltaY() == 0)
        return;
    
    FrameView* view = document()->view();
    if (!view)
        return;
    
    IntPoint pos = view->windowToContents(e.pos());

    int adjustedPageX = pos.x();
    int adjustedPageY = pos.y();
    if (Frame* frame = document()->frame()) {
        float pageZoom = frame->pageZoomFactor();
        if (pageZoom != 1.0f) {
            // Adjust our pageX and pageY to account for the page zoom.
            adjustedPageX = lroundf(pos.x() / pageZoom);
            adjustedPageY = lroundf(pos.y() / pageZoom);
        }
    }
    
    RefPtr<WheelEvent> we = WheelEvent::create(e.wheelTicksX(), e.wheelTicksY(),
        document()->defaultView(), e.globalX(), e.globalY(), adjustedPageX, adjustedPageY,
        e.ctrlKey(), e.altKey(), e.shiftKey(), e.metaKey());

    we->setAbsoluteLocation(IntPoint(pos.x(), pos.y()));

    ExceptionCode ec = 0;
    if (!dispatchEvent(we.release(), ec))
        e.accept();
}

void Node::dispatchWebKitAnimationEvent(const AtomicString& eventType, const String& animationName, double elapsedTime)
{
    ASSERT(!eventDispatchForbidden());
    
    ExceptionCode ec = 0;
    dispatchEvent(WebKitAnimationEvent::create(eventType, animationName, elapsedTime), ec);
}

void Node::dispatchWebKitTransitionEvent(const AtomicString& eventType, const String& propertyName, double elapsedTime)
{
    ASSERT(!eventDispatchForbidden());
    
    ExceptionCode ec = 0;
    dispatchEvent(WebKitTransitionEvent::create(eventType, propertyName, elapsedTime), ec);
}

void Node::dispatchMutationEvent(const AtomicString& eventType, bool canBubble, PassRefPtr<Node> relatedNode, const String& prevValue, const String& newValue, ExceptionCode& ec)
{
    ASSERT(!eventDispatchForbidden());

    dispatchEvent(MutationEvent::create(eventType, canBubble, false, relatedNode, prevValue, newValue, String(), 0), ec);
}

void Node::dispatchFocusEvent()
{
    dispatchEvent(eventNames().focusEvent, false, false);
}

void Node::dispatchBlurEvent()
{
    dispatchEvent(eventNames().blurEvent, false, false);
}

bool Node::dispatchEvent(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(Event::create(eventType, canBubbleArg, cancelableArg), ec);
}

void Node::dispatchProgressEvent(const AtomicString &eventType, bool lengthComputableArg, unsigned loadedArg, unsigned totalArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    dispatchEvent(ProgressEvent::create(eventType, lengthComputableArg, loadedArg, totalArg), ec);
}

void Node::clearAttributeEventListener(const AtomicString& eventType)
{
    if (!hasRareData())
        return;

    RegisteredEventListenerVector* listeners = rareData()->listeners();
    if (!listeners)
        return;

    size_t size = listeners->size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *listeners->at(i);
        if (r.eventType() != eventType || !r.listener()->isAttribute())
            continue;

        r.setRemoved(true);
        listeners->remove(i);

        // removed last
        if (listeners->isEmpty() && !inDocument())
            document()->unregisterDisconnectedNodeWithEventListeners(this);

        updateSVGElementInstancesAfterEventListenerChange(this);
        return;
    }
}

void Node::setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    clearAttributeEventListener(eventType);
    if (listener)
        addEventListener(eventType, listener, false);
}

EventListener* Node::getAttributeEventListener(const AtomicString& eventType) const
{
    const RegisteredEventListenerVector& listeners = eventListeners();
    size_t size = listeners.size();
    for (size_t i = 0; i < size; ++i) {
        const RegisteredEventListener& r = *listeners[i];
        if (r.eventType() == eventType && r.listener()->isAttribute())
            return r.listener();
    }
    return 0;
}

bool Node::disabled() const
{
    return false;
}

void Node::defaultEventHandler(Event* event)
{
    if (event->target() != this)
        return;
    const AtomicString& eventType = event->type();
    if (eventType == eventNames().keydownEvent || eventType == eventNames().keypressEvent) {
        if (event->isKeyboardEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultKeyboardEventHandler(static_cast<KeyboardEvent*>(event));
    } else if (eventType == eventNames().clickEvent) {
        int detail = event->isUIEvent() ? static_cast<UIEvent*>(event)->detail() : 0;
        dispatchUIEvent(eventNames().DOMActivateEvent, detail, event);
    } else if (eventType == eventNames().contextmenuEvent) {
        if (Frame* frame = document()->frame())
            if (Page* page = frame->page())
                page->contextMenuController()->handleContextMenuEvent(event);
    } else if (eventType == eventNames().textInputEvent) {
        if (event->isTextEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultTextInputEventHandler(static_cast<TextEvent*>(event));
    }
}

EventListener* Node::onabort() const
{
    return getAttributeEventListener(eventNames().abortEvent);
}

void Node::setOnabort(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().abortEvent, eventListener);
}

EventListener* Node::onblur() const
{
    return getAttributeEventListener(eventNames().blurEvent);
}

void Node::setOnblur(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().blurEvent, eventListener);
}

EventListener* Node::onchange() const
{
    return getAttributeEventListener(eventNames().changeEvent);
}

void Node::setOnchange(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().changeEvent, eventListener);
}

EventListener* Node::onclick() const
{
    return getAttributeEventListener(eventNames().clickEvent);
}

void Node::setOnclick(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().clickEvent, eventListener);
}

EventListener* Node::oncontextmenu() const
{
    return getAttributeEventListener(eventNames().contextmenuEvent);
}

void Node::setOncontextmenu(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().contextmenuEvent, eventListener);
}

EventListener* Node::ondblclick() const
{
    return getAttributeEventListener(eventNames().dblclickEvent);
}

void Node::setOndblclick(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dblclickEvent, eventListener);
}

EventListener* Node::onerror() const
{
    return getAttributeEventListener(eventNames().errorEvent);
}

void Node::setOnerror(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().errorEvent, eventListener);
}

EventListener* Node::onfocus() const
{
    return getAttributeEventListener(eventNames().focusEvent);
}

void Node::setOnfocus(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().focusEvent, eventListener);
}

EventListener* Node::oninput() const
{
    return getAttributeEventListener(eventNames().inputEvent);
}

void Node::setOninput(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().inputEvent, eventListener);
}

EventListener* Node::oninvalid() const
{
    return getAttributeEventListener(eventNames().invalidEvent);
}

void Node::setOninvalid(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().invalidEvent, eventListener);
}

EventListener* Node::onkeydown() const
{
    return getAttributeEventListener(eventNames().keydownEvent);
}

void Node::setOnkeydown(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().keydownEvent, eventListener);
}

EventListener* Node::onkeypress() const
{
    return getAttributeEventListener(eventNames().keypressEvent);
}

void Node::setOnkeypress(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().keypressEvent, eventListener);
}

EventListener* Node::onkeyup() const
{
    return getAttributeEventListener(eventNames().keyupEvent);
}

void Node::setOnkeyup(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().keyupEvent, eventListener);
}

EventListener* Node::onload() const
{
    return getAttributeEventListener(eventNames().loadEvent);
}

void Node::setOnload(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().loadEvent, eventListener);
}

EventListener* Node::onmousedown() const
{
    return getAttributeEventListener(eventNames().mousedownEvent);
}

void Node::setOnmousedown(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mousedownEvent, eventListener);
}

EventListener* Node::onmousemove() const
{
    return getAttributeEventListener(eventNames().mousemoveEvent);
}

void Node::setOnmousemove(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mousemoveEvent, eventListener);
}

EventListener* Node::onmouseout() const
{
    return getAttributeEventListener(eventNames().mouseoutEvent);
}

void Node::setOnmouseout(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mouseoutEvent, eventListener);
}

EventListener* Node::onmouseover() const
{
    return getAttributeEventListener(eventNames().mouseoverEvent);
}

void Node::setOnmouseover(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mouseoverEvent, eventListener);
}

EventListener* Node::onmouseup() const
{
    return getAttributeEventListener(eventNames().mouseupEvent);
}

void Node::setOnmouseup(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mouseupEvent, eventListener);
}

EventListener* Node::onmousewheel() const
{
    return getAttributeEventListener(eventNames().mousewheelEvent);
}

void Node::setOnmousewheel(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mousewheelEvent, eventListener);
}

EventListener* Node::ondragenter() const
{
    return getAttributeEventListener(eventNames().dragenterEvent);
}

void Node::setOndragenter(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragenterEvent, eventListener);
}

EventListener* Node::ondragover() const
{
    return getAttributeEventListener(eventNames().dragoverEvent);
}

void Node::setOndragover(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragoverEvent, eventListener);
}

EventListener* Node::ondragleave() const
{
    return getAttributeEventListener(eventNames().dragleaveEvent);
}

void Node::setOndragleave(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragleaveEvent, eventListener);
}

EventListener* Node::ondrop() const
{
    return getAttributeEventListener(eventNames().dropEvent);
}

void Node::setOndrop(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dropEvent, eventListener);
}

EventListener* Node::ondragstart() const
{
    return getAttributeEventListener(eventNames().dragstartEvent);
}

void Node::setOndragstart(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragstartEvent, eventListener);
}

EventListener* Node::ondrag() const
{
    return getAttributeEventListener(eventNames().dragEvent);
}

void Node::setOndrag(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragEvent, eventListener);
}

EventListener* Node::ondragend() const
{
    return getAttributeEventListener(eventNames().dragendEvent);
}

void Node::setOndragend(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragendEvent, eventListener);
}

EventListener* Node::onscroll() const
{
    return getAttributeEventListener(eventNames().scrollEvent);
}

void Node::setOnscroll(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().scrollEvent, eventListener);
}

EventListener* Node::onselect() const
{
    return getAttributeEventListener(eventNames().selectEvent);
}

void Node::setOnselect(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().selectEvent, eventListener);
}

EventListener* Node::onsubmit() const
{
    return getAttributeEventListener(eventNames().submitEvent);
}

void Node::setOnsubmit(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().submitEvent, eventListener);
}

EventListener* Node::onbeforecut() const
{
    return getAttributeEventListener(eventNames().beforecutEvent);
}

void Node::setOnbeforecut(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().beforecutEvent, eventListener);
}

EventListener* Node::oncut() const
{
    return getAttributeEventListener(eventNames().cutEvent);
}

void Node::setOncut(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().cutEvent, eventListener);
}

EventListener* Node::onbeforecopy() const
{
    return getAttributeEventListener(eventNames().beforecopyEvent);
}

void Node::setOnbeforecopy(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().beforecopyEvent, eventListener);
}

EventListener* Node::oncopy() const
{
    return getAttributeEventListener(eventNames().copyEvent);
}

void Node::setOncopy(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().copyEvent, eventListener);
}

EventListener* Node::onbeforepaste() const
{
    return getAttributeEventListener(eventNames().beforepasteEvent);
}

void Node::setOnbeforepaste(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().beforepasteEvent, eventListener);
}

EventListener* Node::onpaste() const
{
    return getAttributeEventListener(eventNames().pasteEvent);
}

void Node::setOnpaste(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().pasteEvent, eventListener);
}

EventListener* Node::onreset() const
{
    return getAttributeEventListener(eventNames().resetEvent);
}

void Node::setOnreset(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().resetEvent, eventListener);
}

EventListener* Node::onsearch() const
{
    return getAttributeEventListener(eventNames().searchEvent);
}

void Node::setOnsearch(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().searchEvent, eventListener);
}

EventListener* Node::onselectstart() const
{
    return getAttributeEventListener(eventNames().selectstartEvent);
}

void Node::setOnselectstart(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().selectstartEvent, eventListener);
}

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::Node* node)
{
    if (node)
        node->showTreeForThis();
}

#endif

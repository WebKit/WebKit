/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "CSSParser.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
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
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "JSDOMBinding.h"
#include "Logging.h"
#include "NameNodeList.h"
#include "NamedAttrMap.h"
#include "NodeRareData.h"
#include "ProcessingInstruction.h"
#include "RenderObject.h"
#include "ScriptController.h"
#include "SelectorNodeList.h"
#include "StringBuilder.h"
#include "TagNodeList.h"
#include "Text.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include <runtime/ExecState.h>
#include <runtime/JSLock.h>
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

using namespace HTMLNames;

// --------

bool Node::isSupported(const String& feature, const String& version)
{
    return DOMImplementation::hasFeature(feature, version);
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

Node::StyleChange Node::diff( RenderStyle *s1, RenderStyle *s2 )
{
    // FIXME: The behavior of this function is just totally wrong.  It doesn't handle
    // explicit inheritance of non-inherited properties and so you end up not re-resolving
    // style in cases where you need to.
    StyleChange ch = NoInherit;
    EDisplay display1 = s1 ? s1->display() : NONE;
    bool fl1 = s1 && s1->hasPseudoStyle(RenderStyle::FIRST_LETTER);
    EDisplay display2 = s2 ? s2->display() : NONE;
    bool fl2 = s2 && s2->hasPseudoStyle(RenderStyle::FIRST_LETTER);
        
    if (display1 != display2 || fl1 != fl2 || (s1 && s2 && !s1->contentDataEquivalent(s2)))
        ch = Detach;
    else if (!s1 || !s2)
        ch = Inherit;
    else if (*s1 == *s2)
        ch = NoChange;
    else if (s1->inheritedNotEqual(s2))
        ch = Inherit;
    
    // If the pseudoStyles have changed, we want any StyleChange that is not NoChange
    // because setStyle will do the right thing with anything else.
    if (ch == NoChange && s1->hasPseudoStyle(RenderStyle::BEFORE)) {
        RenderStyle* ps2 = s2->getCachedPseudoStyle(RenderStyle::BEFORE);
        if (!ps2)
            ch = NoInherit;
        else {
            RenderStyle* ps1 = s1->getCachedPseudoStyle(RenderStyle::BEFORE);
            ch = ps1 && *ps1 == *ps2 ? NoChange : NoInherit;
        }
    }
    if (ch == NoChange && s1->hasPseudoStyle(RenderStyle::AFTER)) {
        RenderStyle* ps2 = s2->getCachedPseudoStyle(RenderStyle::AFTER);
        if (!ps2)
            ch = NoInherit;
        else {
            RenderStyle* ps1 = s1->getCachedPseudoStyle(RenderStyle::AFTER);
            ch = ps2 && *ps1 == *ps2 ? NoChange : NoInherit;
        }
    }
    
    return ch;
}

Node::Node(Document* doc, bool isElement, bool isContainer)
    : m_document(doc)
    , m_previous(0)
    , m_next(0)
    , m_renderer(0)
    , m_styleChange(NoStyleChange)
    , m_hasId(false)
    , m_hasClass(false)
    , m_attached(false)
    , m_hasChangedChild(false)
    , m_inDocument(false)
    , m_isLink(false)
    , m_active(false)
    , m_hovered(false)
    , m_inActiveChain(false)
    , m_inDetach(false)
    , m_inSubtreeMark(false)
    , m_hasRareData(false)
    , m_isElement(isElement)
    , m_isContainer(isContainer)
{
#ifndef NDEBUG
    if (shouldIgnoreLeaks)
        ignoreSet.add(this);
    else
        nodeCounter.increment();
#endif
}

void Node::setDocument(Document* doc)
{
    if (inDocument() || m_document == doc)
        return;

    willMoveToNewOwnerDocument();

    updateDOMNodeDocument(this, m_document.get(), doc);

    m_document = doc;

    didMoveToNewOwnerDocument();
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
}

inline NodeRareData* Node::rareData() const
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
        data->setNodeLists(std::auto_ptr<NodeListsNodeData>(new NodeListsNodeData));
        document()->addNodeListCache();
    }

    return ChildNodeList::create(this, &data->nodeLists()->m_childNodeListCaches);
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
    // any two adjacent text nodes are merged together.

    RefPtr<Node> node = this;
    while (Node* firstChild = node->firstChild())
        node = firstChild;
    for (; node; node = node->traverseNextNodePostOrder()) {
        NodeType type = node->nodeType();
        if (type == ELEMENT_NODE)
            static_cast<Element*>(node.get())->normalizeAttributes();

        Node* firstChild = node->firstChild();
        if (firstChild && !firstChild->nextSibling() && firstChild->isTextNode()) {
            Text* text = static_cast<Text*>(firstChild);
            if (!text->length()) {
                ExceptionCode ec;
                text->remove(ec);
            }
        }

        if (node == this)
            break;

        if (type == TEXT_NODE) {
            while (1) {
                Node* nextSibling = node->nextSibling();
                if (!nextSibling || !nextSibling->isTextNode())
                    break;
                // Current child and the next one are both text nodes. Merge them.
                Text* text = static_cast<Text*>(node.get());
                RefPtr<Text> nextText = static_cast<Text*>(nextSibling);
                unsigned offset = text->length();
                ExceptionCode ec;
                text->appendData(nextText->data(), ec);
                document()->textNodesMerged(nextText.get(), offset);
                nextText->remove(ec);
            }
        }
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

IntRect Node::getRect() const
{
    // FIXME: broken with transforms
    if (renderer()) {
        FloatPoint absPos = renderer()->localToAbsolute();
        return IntRect(roundedIntPoint(absPos),
                       IntSize(renderer()->width(), renderer()->height() + renderer()->borderTopExtra() + renderer()->borderBottomExtra()));
    }
    return IntRect();
}

void Node::setChanged(StyleChangeType changeType)
{
    if ((changeType != NoStyleChange) && !attached()) // changed compared to what?
        return;

    if (!(changeType == InlineStyleChange && (m_styleChange == FullStyleChange || m_styleChange == AnimationStyleChange)))
        m_styleChange = changeType;

    if (m_styleChange != NoStyleChange) {
        for (Node* p = parentNode(); p && !p->hasChangedChild(); p = p->parentNode())
            p->setHasChangedChild(true);
        document()->setDocumentChanged(true);
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
            n->setHasChangedChild(true);
        n->m_styleChange = FullStyleChange;
        n->m_attached = true;
    }

    if (mustDoFullAttach) {
        Node* lazyAttachedAncestor = outermostLazyAttachedAncestor(this);
        if (lazyAttachedAncestor->attached())
            lazyAttachedAncestor->detach();
        lazyAttachedAncestor->attach();
    } else {
        for (Node* p = parentNode(); p && !p->hasChangedChild(); p = p->parentNode())
            p->setHasChangedChild(true);
        document()->setDocumentChanged(true);
    }
}

bool Node::canLazyAttach()
{
    return shadowAncestorNode() == this;
}
    
void Node::setFocus(bool b)
{ 
    if (b || hasRareData())
        ensureRareData()->m_focused = b;
}

bool Node::rareDataFocused() const
{
    ASSERT(hasRareData());
    return rareData()->m_focused;
}
    
bool Node::isFocusable() const
{
    return hasRareData() && rareData()->tabIndexSetExplicitly();
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
    for( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
}

void Node::registerDynamicNodeList(DynamicNodeList* list)
{
    NodeRareData* data = ensureRareData();
    if (!data->nodeLists()) {
        data->setNodeLists(std::auto_ptr<NodeListsNodeData>(new NodeListsNodeData));
        document()->addNodeListCache();
    } else if (!m_document->hasNodeListCaches()) {
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

void Node::checkSetPrefix(const AtomicString &_prefix, ExceptionCode& ec)
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

bool Node::canReplaceChild(Node* newChild, Node* oldChild)
{
    if (newChild->nodeType() != DOCUMENT_FRAGMENT_NODE) {
        if (!childTypeAllowed(newChild->nodeType()))
            return false;
    }
    else {
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

void Node::insertedIntoDocument()
{
    setInDocument(true);
    insertedIntoTree(false);
}

void Node::removedFromDocument()
{
    if (m_document && m_document->getCSSTarget() == this)
        m_document->setCSSTarget(0);

    setInDocument(false);
    removedFromTree(false);
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
#if ENABLE(SVG)
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
    if (isElementNode())
        return document()->styleSelector()->styleForElement(static_cast<Element*>(this));
    return parentNode() && parentNode()->renderer() ? parentNode()->renderer()->style() : 0;
}

bool Node::rendererIsNeeded(RenderStyle *style)
{
    return (document()->documentElement() == this) || (style->display() != NONE);
}

RenderObject *Node::createRenderer(RenderArena *arena, RenderStyle *style)
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
    return renderer() && (renderer()->isBlockFlow() || renderer()->isTable() && !renderer()->isInline());
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
    return getElementsByTagNameNS("*", name);
}
 
PassRefPtr<NodeList> Node::getElementsByTagNameNS(const String& namespaceURI, const String& localName)
{
    if (localName.isNull())
        return 0;

    String name = localName;
    if (document()->isHTMLDocument())
        name = localName.lower();
    return TagNodeList::create(this, namespaceURI.isEmpty() ? nullAtom : AtomicString(namespaceURI), name);
}

PassRefPtr<NodeList> Node::getElementsByName(const String& elementName)
{
    NodeRareData* data = ensureRareData();
    if (!data->nodeLists()) {
        data->setNodeLists(std::auto_ptr<NodeListsNodeData>(new NodeListsNodeData));
        document()->addNodeListCache();
    }

    pair<NodeListsNodeData::CacheMap::iterator, bool> result = data->nodeLists()->m_nameNodeListCaches.add(elementName, 0);
    if (result.second)
        result.first->second = new DynamicNodeList::Caches;
    
    return NameNodeList::create(this, elementName, result.first->second);
}

PassRefPtr<NodeList> Node::getElementsByClassName(const String& classNames)
{
    NodeRareData* data = ensureRareData();
    if (!data->nodeLists()) {
        data->setNodeLists(std::auto_ptr<NodeListsNodeData>(new NodeListsNodeData));
        document()->addNodeListCache();
    }

    pair<NodeListsNodeData::CacheMap::iterator, bool> result = data->nodeLists()->m_classNodeListCaches.add(classNames, 0);
    if (result.second)
        result.first->second = new DynamicNodeList::Caches;
    
    return ClassNodeList::create(this, classNames, result.first->second);
}

template <typename Functor>
static bool forEachTagSelector(Functor& functor, CSSSelector* selector)
{
    ASSERT(selector);

    do {
        if (functor(selector))
            return true;
        if (CSSSelector* simpleSelector = selector->m_simpleSelector) {
            if (forEachTagSelector(functor, simpleSelector))
                return true;
        }
    } while ((selector = selector->m_tagHistory));

    return false;
}

template <typename Functor>
static bool forEachSelector(Functor& functor, CSSSelector* selector)
{
    for (; selector; selector = selector->next()) {
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
        if (selector->hasAttribute() && selector->m_attr.prefix() != nullAtom && selector->m_attr.prefix() != starAtom)
            return true;
        return false;
    }
};

static bool selectorNeedsNamespaceResolution(CSSSelector* currentSelector)
{
    SelectorNeedsNamespaceResolutionFunctor functor;
    return forEachSelector(functor, currentSelector);
}

PassRefPtr<Element> Node::querySelector(const String& selectors, ExceptionCode& ec)
{
    if (selectors.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    bool strictParsing = !document()->inCompatMode();
    CSSParser p(strictParsing);

    std::auto_ptr<CSSSelector> querySelector = p.parseSelector(selectors, document());
    if (!querySelector.get()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    // throw a NAMESPACE_ERR if the selector includes any namespace prefixes.
    if (selectorNeedsNamespaceResolution(querySelector.get())) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    CSSStyleSelector::SelectorChecker selectorChecker(document(), strictParsing);

    // FIXME: we could also optimize for the the [id="foo"] case
    if (strictParsing && querySelector->m_match == CSSSelector::Id && inDocument() && !querySelector->next()) {
        ASSERT(querySelector->m_attr == idAttr);
        Element* element = document()->getElementById(querySelector->m_value);
        if (element && (isDocumentNode() || element->isDescendantOf(this)) && selectorChecker.checkSelector(querySelector.get(), element))
            return element;
        return 0;
    }

    // FIXME: We can speed this up by implementing caching similar to the one use by getElementById
    for (Node* n = firstChild(); n; n = n->traverseNextNode(this)) {
        if (n->isElementNode()) {
            Element* element = static_cast<Element*>(n);
            for (CSSSelector* selector = querySelector.get(); selector; selector = selector->next()) {
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

    std::auto_ptr<CSSSelector> querySelector = p.parseSelector(selectors, document());

    if (!querySelector.get()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    // Throw a NAMESPACE_ERR if the selector includes any namespace prefixes.
    if (selectorNeedsNamespaceResolution(querySelector.get())) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    return createSelectorNodeList(this, querySelector.get());
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
    
    NamedAttrMap *attrs = attributes();
    NamedAttrMap *otherAttrs = other->attributes();
    
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

bool Node::isDefaultNamespace(const String &namespaceURI) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#isDefaultNamespaceAlgo
    
    switch (nodeType()) {
        case ELEMENT_NODE: {
            const Element *elem = static_cast<const Element *>(this);
            
            if (elem->prefix().isNull())
                return elem->namespaceURI() == namespaceURI;

            if (elem->hasAttributes()) {
                NamedAttrMap *attrs = elem->attributes();
                
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

String Node::lookupPrefix(const String &namespaceURI) const
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
                NamedAttrMap *attrs = elem->attributes();
                
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

String Node::lookupNamespacePrefix(const String &_namespaceURI, const Element *originalElement) const
{
    if (_namespaceURI.isNull())
        return String();
            
    if (originalElement->lookupNamespaceURI(prefix()) == _namespaceURI)
        return prefix();
    
    if (hasAttributes()) {
        NamedAttrMap *attrs = attributes();
        
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
            content.append(static_cast<const CharacterData*>(this)->CharacterData::nodeValue());
            break;

        case PROCESSING_INSTRUCTION_NODE:
            content.append(static_cast<const ProcessingInstruction*>(this)->ProcessingInstruction::nodeValue());
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
        NamedAttrMap* map = attr1->ownerElement()->attributes(true);
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
    m_childNodeListCaches.reset();
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

    if (m_childNodeListCaches.refCount)
        return false;

    CacheMap::const_iterator classCachesEnd = m_classNodeListCaches.end();
    for (CacheMap::const_iterator it = m_classNodeListCaches.begin(); it != classCachesEnd; ++it) {
        if (it->second->refCount)
            return false;
    }

    CacheMap::const_iterator nameCachesEnd = m_nameNodeListCaches.end();
    for (CacheMap::const_iterator it = m_nameNodeListCaches.begin(); it != nameCachesEnd; ++it) {
        if (it->second->refCount)
            return false;
    }

    return true;
}

void Node::getSubresourceURLs(Vector<KURL>& urls) const
{
    Vector<String> subresourceStrings;
    getSubresourceAttributeStrings(subresourceStrings);
    
    for (unsigned i = 0; i < subresourceStrings.size(); ++i) {
        String& subresourceString(subresourceStrings[i]);
        
        // FIXME: Is parseURL appropriate here?
        if (subresourceString.length())
            urls.append(document()->completeURL(parseURL(subresourceString)));
    }
}

// --------

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::Node* node)
{
    if (node)
        node->showTreeForThis();
}

#endif

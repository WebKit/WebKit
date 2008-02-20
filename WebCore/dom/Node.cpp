/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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
#include "HTMLNames.h"
#include "Logging.h"
#include "NameNodeList.h"
#include "NamedAttrMap.h"
#include "RenderObject.h"
#include "SelectorNodeList.h"
#include "TagNodeList.h"
#include "Text.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include "kjs_binding.h"

namespace WebCore {

using namespace HTMLNames;

typedef HashSet<DynamicNodeList*> NodeListSet;
struct NodeListsNodeData {
    NodeListSet m_listsToNotify;
    DynamicNodeList::Caches m_childNodeListCaches;
    
    typedef HashMap<String, DynamicNodeList::Caches*> CacheMap;
    CacheMap m_classNodeListCaches;
    CacheMap m_nameNodeListCaches;
    
    ~NodeListsNodeData()
    {
        deleteAllValues(m_classNodeListCaches);
        deleteAllValues(m_nameNodeListCaches);
    }
};

bool Node::isSupported(const String& feature, const String& version)
{
    return DOMImplementation::instance()->hasFeature(feature, version);
}

#ifndef NDEBUG
WTFLogChannel LogWebCoreNodeLeaks =  { 0x00000000, "", WTFLogChannelOn };

struct NodeCounter {
    static unsigned count; 

    ~NodeCounter() 
    { 
        if (count) 
            LOG(WebCoreNodeLeaks, "LEAK: %u Node\n", count); 
    }
};
unsigned NodeCounter::count = 0;
static NodeCounter nodeCounter;

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

Node::Node(Document *doc)
    : m_document(doc),
      m_previous(0),
      m_next(0),
      m_renderer(0),
      m_tabIndex(0),
      m_hasId(false),
      m_hasClass(false),
      m_attached(false),
      m_styleChange(NoStyleChange),
      m_hasChangedChild(false),
      m_inDocument(false),
      m_isLink(false),
      m_attrWasSpecifiedOrElementHasRareData(false),
      m_focused(false),
      m_active(false),
      m_hovered(false),
      m_inActiveChain(false),
      m_inDetach(false),
      m_dispatchingSimulatedEvent(false),
      m_inSubtreeMark(false)
{
#ifndef NDEBUG
    if (shouldIgnoreLeaks)
        ignoreSet.add(this);
    else
        ++NodeCounter::count;
#endif
}

void Node::setDocument(Document* doc)
{
    if (inDocument() || m_document == doc)
        return;

    willMoveToNewOwnerDocument();

    {
        KJS::JSLock lock;
        ScriptInterpreter::updateDOMNodeDocument(this, m_document.get(), doc);
    }    
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
        --NodeCounter::count;
#endif
    if (renderer())
        detach();

    if (m_previous)
        m_previous->setNextSibling(0);
    if (m_next)
        m_next->setPreviousSibling(0);
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

    // by default nodeValue is null, so setting it has no effect
}

PassRefPtr<NodeList> Node::childNodes()
{
    if (!m_nodeLists)
        m_nodeLists.set(new NodeListsNodeData);

    return new ChildNodeList(this, &m_nodeLists->m_childNodeListCaches);
}

Node* Node::virtualFirstChild() const
{
    return 0;
}

Node* Node::virtualLastChild() const
{
    return 0;
}

bool Node::virtualHasTagName(const QualifiedName&) const
{
    return false;
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

bool Node::insertBefore(PassRefPtr<Node>, Node*, ExceptionCode& ec)
{
    ec = HIERARCHY_REQUEST_ERR;
    return false;
}

bool Node::replaceChild(PassRefPtr<Node>, Node*, ExceptionCode& ec)
{
    ec = HIERARCHY_REQUEST_ERR;
    return false;
}

bool Node::removeChild(Node*, ExceptionCode& ec)
{
    ec = NOT_FOUND_ERR;
    return false;
}

bool Node::appendChild(PassRefPtr<Node>, ExceptionCode& ec)
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

bool Node::hasChildNodes(  ) const
{
  return false;
}

void Node::normalize ()
{
    ExceptionCode ec = 0;
    Node *child = firstChild();

    if (isElementNode()) {
        // Normalize any attribute children we might have 
        Element *element = static_cast<Element *>(this);
        NamedAttrMap *attrMap = element->attributes();
        
        if (attrMap) {
            unsigned numAttrs = attrMap->length();
            
            for (unsigned i = 0; i < numAttrs; i++) {
                Attribute *attribute = attrMap->attributeItem(i);
                Attr *attr = attribute->attr();
                
                if (attr)
                    attr->normalize();
            }
        }
    }
    
    // Recursively go through the subtree beneath us, normalizing all nodes. In the case
    // where there are two adjacent text nodes, they are merged together
    while (child) {
        Node *nextChild = child->nextSibling();

        if (nextChild && child->nodeType() == TEXT_NODE && nextChild->nodeType() == TEXT_NODE) {
            // Current child and the next one are both text nodes... merge them
            Text *currentText = static_cast<Text*>(child);
            Text *nextText = static_cast<Text*>(nextChild);

            currentText->appendData(nextText->data(),ec);
            if (ec)
                return;

            nextChild->remove(ec);
            if (ec)
                return;
        }
        else {
            child->normalize();
            child = nextChild;
        }
    }
    
    // Check if we have a single empty text node left and remove it if so
    child = firstChild();
    if (child && !child->nextSibling() && child->isTextNode()) {
        Text *text = static_cast<Text*>(child);
        if (text->data().isEmpty())
            child->remove(ec);
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
    int _x, _y;
    if (renderer() && renderer()->absolutePosition(_x, _y))
        return IntRect( _x, _y, renderer()->width(), renderer()->height() + renderer()->borderTopExtra() + renderer()->borderBottomExtra());

    return IntRect();
}

void Node::setChanged(StyleChangeType changeType)
{
    if ((changeType != NoStyleChange) && !attached()) // changed compared to what?
        return;

    if (!(changeType == InlineStyleChange && m_styleChange == FullStyleChange))
        m_styleChange = changeType;

    if (m_styleChange != NoStyleChange) {
        for (Node* p = parentNode(); p; p = p->parentNode())
            p->setHasChangedChild(true);
        document()->setDocumentChanged(true);
    }
}

bool Node::isFocusable() const
{
    return false;
}

bool Node::isKeyboardFocusable(KeyboardEvent*) const
{
    return isFocusable();
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
    if (!m_nodeLists)
        m_nodeLists.set(new NodeListsNodeData);
    else if (!m_document->hasNodeLists())
        // We haven't been receiving notifications while there were no registered lists, so the cache is invalid now.
        m_nodeLists->m_childNodeListCaches.reset();

    if (list->needsNotifications())
        m_nodeLists->m_listsToNotify.add(list);
    m_document->addNodeList();
}

void Node::unregisterDynamicNodeList(DynamicNodeList* list)
{
    ASSERT(m_nodeLists);
    m_document->removeNodeList();
    if (list->needsNotifications())
        m_nodeLists->m_listsToNotify.remove(list);
}

void Node::notifyLocalNodeListsAttributeChanged()
{
    if (!m_nodeLists)
        return;

    NodeListSet::iterator end = m_nodeLists->m_listsToNotify.end();
    for (NodeListSet::iterator i = m_nodeLists->m_listsToNotify.begin(); i != end; ++i)
        (*i)->rootNodeAttributeChanged();
}

void Node::notifyNodeListsAttributeChanged()
{
    for (Node *n = this; n; n = n->parentNode())
        n->notifyLocalNodeListsAttributeChanged();
}

void Node::notifyLocalNodeListsChildrenChanged()
{
    if (!m_nodeLists)
        return;

    m_nodeLists->m_childNodeListCaches.reset();

    NodeListSet::iterator end = m_nodeLists->m_listsToNotify.end();
    for (NodeListSet::iterator i = m_nodeLists->m_listsToNotify.begin(); i != end; ++i)
        (*i)->rootNodeChildrenChanged();
}

void Node::notifyNodeListsChildrenChanged()
{
    for (Node *n = this; n; n = n->parentNode())
        n->notifyLocalNodeListsChildrenChanged();
}

unsigned Node::childNodeCount() const
{
    return 0;
}

Node *Node::childNode(unsigned /*index*/) const
{
    return 0;
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

Node::StyleChange Node::diff( RenderStyle *s1, RenderStyle *s2 ) const
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
        RenderStyle* ps2 = s2->getPseudoStyle(RenderStyle::BEFORE);
        if (!ps2)
            ch = NoInherit;
        else {
            RenderStyle* ps1 = s1->getPseudoStyle(RenderStyle::BEFORE);
            ch = ps1 && *ps1 == *ps2 ? NoChange : NoInherit;
        }
    }
    if (ch == NoChange && s1->hasPseudoStyle(RenderStyle::AFTER)) {
        RenderStyle* ps2 = s2->getPseudoStyle(RenderStyle::AFTER);
        if (!ps2)
            ch = NoInherit;
        else {
            RenderStyle* ps1 = s1->getPseudoStyle(RenderStyle::AFTER);
            ch = ps2 && *ps1 == *ps2 ? NoChange : NoInherit;
        }
    }
    
    return ch;
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

bool Node::isReadOnlyNode()
{
    // Entity & Entity Reference nodes and their descendants are read-only
    Node *n = this;
    while (n) {
        if (n->nodeType() == ENTITY_NODE || n->nodeType() == ENTITY_REFERENCE_NODE)
            return true;
        n = n->parentNode();
    }
    return false;
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

// Offset specifies the child node to start at.  If it is past
// the last child, it specifies to start at next sibling.
Node *Node::nextEditable(int offset) const
{
    ASSERT(offset>=0);
    Node *node;
    if (hasChildNodes())
        node = (offset >= (int)childNodeCount()) ? nextSibling() : childNode(offset)->nextLeafNode();
    else
        node = nextLeafNode();
    while (node) {
        if (node->isContentEditable())
            return node;
        node = node->nextLeafNode();
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
    
    Node *parent = parentNode();    
    ASSERT(parent);
    
    RenderObject *parentRenderer = parent->renderer();
    if (parentRenderer && parentRenderer->canHaveChildren()
#if ENABLE(SVG)
        && parent->childShouldCreateRenderer(this)
#endif
        ) {
        RenderStyle* style = styleForRenderer(parentRenderer);
        if (rendererIsNeeded(style)) {
            if (RenderObject* r = createRenderer(document()->renderArena(), style)) {
                if (!parentRenderer->isChildAllowed(r, style))
                    r->destroy();
                else {
                    setRenderer(r);
                    renderer()->setAnimatableStyle(style);
                    parentRenderer->addChild(renderer(), nextRenderer());
                }
            }
        }
        style->deref(document()->renderArena());
    }
}

RenderStyle *Node::styleForRenderer(RenderObject *parent)
{
    RenderStyle *style = parent->style();
    style->ref();
    return style;
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

RenderStyle* Node::renderStyle() const
{ 
    return m_renderer ? m_renderer->style() : 0; 
}

void Node::setRenderStyle(RenderStyle* s)
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

    Node *n = this;    
    while (n) {
        if (n->isShadowNode())
            return n->shadowParentNode();
        n = n->parentNode();
    } 
    return this;
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

Element *Node::enclosingBlockFlowOrTableElement() const
{
    Node *n = const_cast<Node *>(this);
    if (isBlockFlowOrBlockTable())
        return static_cast<Element *>(n);

    while (1) {
        n = n->parentNode();
        if (!n)
            break;
        if (n->isBlockFlowOrBlockTable() || n->hasTagName(bodyTag))
            return static_cast<Element *>(n);
    }
    return 0;
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
    return new TagNodeList(this, namespaceURI.isEmpty() ? nullAtom : AtomicString(namespaceURI), name);
}

PassRefPtr<NodeList> Node::getElementsByName(const String& elementName)
{
    if (!m_nodeLists)
        m_nodeLists.set(new NodeListsNodeData);

    pair<NodeListsNodeData::CacheMap::iterator, bool> result = m_nodeLists->m_nameNodeListCaches.add(elementName, 0);
    if (result.second)
        result.first->second = new DynamicNodeList::Caches;
    
    return new NameNodeList(this, elementName, result.first->second);
}

PassRefPtr<NodeList> Node::getElementsByClassName(const String& classNames)
{
    if (!m_nodeLists)
        m_nodeLists.set(new NodeListsNodeData);

    pair<NodeListsNodeData::CacheMap::iterator, bool> result = m_nodeLists->m_classNodeListCaches.add(classNames, 0);
    if (result.second)
        result.first->second = new DynamicNodeList::Caches;
    
    return new ClassNodeList(this, classNames, result.first->second);
}

PassRefPtr<Element> Node::querySelector(const String& selectors, ExceptionCode& ec)
{
    if (selectors.isNull() || selectors.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    CSSStyleSheet tempStyleSheet(document());
    CSSParser p(true);
    RefPtr<CSSRule> rule = p.parseRule(&tempStyleSheet, selectors + "{}");
    if (!rule || !rule->isStyleRule()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    CSSStyleSelector* styleSelector = document()->styleSelector();
    CSSSelector* querySelector = static_cast<CSSStyleRule*>(rule.get())->selector();
    
    // FIXME: We can speed this up by implementing caching similar to the one use by getElementById
    for (Node* n = firstChild(); n; n = n->traverseNextNode(this)) {
        if (n->isElementNode()) {
            Element* element = static_cast<Element*>(n);
            styleSelector->initElementAndPseudoState(element);
            for (CSSSelector* selector = querySelector; selector; selector = selector->next()) {
                if (styleSelector->checkSelector(selector))
                    return element;
            }
        }
    }
    
    return 0;
}

PassRefPtr<NodeList> Node::querySelectorAll(const String& selectors, ExceptionCode& ec)
{
    if (selectors.isNull() || selectors.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    CSSStyleSheet tempStyleSheet(document());
    CSSParser p(true);
    RefPtr<CSSRule> rule = p.parseRule(&tempStyleSheet, selectors + "{}");
    if (!rule || !rule->isStyleRule()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    
    SelectorNodeList* resultList = new SelectorNodeList(this, static_cast<CSSStyleRule*>(rule.get())->selector());

    return resultList;
}

Document *Node::ownerDocument() const
{
    Document *doc = document();
    return doc == this ? 0 : doc;
}

bool Node::hasAttributes() const
{
    return false;
}

NamedAttrMap* Node::attributes() const
{
    return 0;
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
            return static_cast <const Document *>(this)->documentElement()->isDefaultNamespace(namespaceURI);
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
            return static_cast<const Document *>(this)->documentElement()->lookupPrefix(namespaceURI);
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
            return static_cast<const Document *>(this)->documentElement()->lookupNamespaceURI(prefix);
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

String Node::textContent(bool convertBRsToNewlines) const
{
    switch (nodeType()) {
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
            return nodeValue();
        
        case ELEMENT_NODE:
            if (hasTagName(brTag) && 
                convertBRsToNewlines)
                return "\n";
        case ATTRIBUTE_NODE:
        case ENTITY_NODE:
        case ENTITY_REFERENCE_NODE:
        case DOCUMENT_FRAGMENT_NODE: {
            String s = "";
        
            for (Node *child = firstChild(); child; child = child->nextSibling()) {
                if (child->nodeType() == COMMENT_NODE || child->nodeType() == PROCESSING_INSTRUCTION_NODE)
                    continue;
            
                s += child->textContent(convertBRsToNewlines);
            }
        
            return s;
        }
        
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case NOTATION_NODE:
        default:
            return String();
    }
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

}

#ifndef NDEBUG

void showTree(const WebCore::Node* node)
{
    if (node)
        node->showTreeForThis();
}

#endif

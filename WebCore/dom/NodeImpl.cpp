/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "NodeImpl.h"

#include "AtomicString.h"
#include "ChildNodeListImpl.h"
#include "DOMImplementationImpl.h"
#include "DocumentImpl.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameView.h"
#include "TextImpl.h"
#include "htmlediting.h"
#include "htmlnames.h"
#include "kjs_binding.h"
#include "render_object.h"
#include <kxmlcore/Assertions.h>
#include <qtextstream.h>

namespace WebCore {

using namespace HTMLNames;

/**
 * NodeList which lists all Nodes in a document with a given tag name
 */
class TagNodeListImpl : public NodeListImpl
{
public:
    TagNodeListImpl(NodeImpl *n, const AtomicString& namespaceURI, const AtomicString& localName);

    // DOM methods overridden from  parent classes
    virtual unsigned length() const;
    virtual NodeImpl *item (unsigned index) const;

    // Other methods (not part of DOM)

protected:
    virtual bool nodeMatches(NodeImpl *testNode) const;

    AtomicString m_namespaceURI;
    AtomicString m_localName;
};

TagNodeListImpl::TagNodeListImpl(NodeImpl *n, const AtomicString& namespaceURI, const AtomicString& localName)
    : NodeListImpl(n), 
      m_namespaceURI(namespaceURI), 
      m_localName(localName)
{
}

unsigned TagNodeListImpl::length() const
{
    return recursiveLength();
}

NodeImpl *TagNodeListImpl::item(unsigned index) const
{
    return recursiveItem(index);
}

bool TagNodeListImpl::nodeMatches(NodeImpl *testNode) const
{
    if (!testNode->isElementNode())
        return false;

    if (m_namespaceURI != starAtom && m_namespaceURI != testNode->namespaceURI())
        return false;
    
    return m_localName == starAtom || m_localName == testNode->localName();
}


#ifndef NDEBUG
struct NodeImplCounter { 
    static int count; 
    ~NodeImplCounter() { if (count != 0) fprintf(stderr, "LEAK: %d NodeImpl\n", count); }
};
int NodeImplCounter::count = 0;
static NodeImplCounter nodeImplCounter;

int gEventDispatchForbidden = 0;
#endif NDEBUG

NodeImpl::NodeImpl(DocumentImpl *doc)
    : document(doc),
      m_previous(0),
      m_next(0),
      m_renderer(0),
      m_nodeLists(0),
      m_tabIndex(0),
      m_hasId(false),
      m_hasClass(false),
      m_hasStyle(false),
      m_attached(false),
      m_changed(false),
      m_hasChangedChild(false),
      m_inDocument(false),
      m_isLink(false),
      m_specified(false),
      m_focused(false),
      m_active(false),
      m_hovered(false),
      m_inActiveChain(false),
      m_styleElement(false),
      m_implicit(false),
      m_inDetach(false)
{
#ifndef NDEBUG
    ++NodeImplCounter::count;
#endif
}

void NodeImpl::setDocument(DocumentImpl *doc)
{
    if (inDocument())
        return;
    
    document = doc;
}

NodeImpl::~NodeImpl()
{
#ifndef NDEBUG
    --NodeImplCounter::count;
#endif
    if (renderer())
        detach();
    delete m_nodeLists;
    if (m_previous)
        m_previous->setNextSibling(0);
    if (m_next)
        m_next->setPreviousSibling(0);
}

DOMString NodeImpl::nodeValue() const
{
  return DOMString();
}

void NodeImpl::setNodeValue( const DOMString &/*_nodeValue*/, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // be default nodeValue is null, so setting it has no effect
}

PassRefPtr<NodeListImpl> NodeImpl::childNodes()
{
    return new ChildNodeListImpl(this);
}

NodeImpl *NodeImpl::firstChild() const
{
  return 0;
}

NodeImpl *NodeImpl::lastChild() const
{
  return 0;
}

NodeImpl *NodeImpl::lastDescendant() const
{
    NodeImpl *n = const_cast<NodeImpl *>(this);
    while (n && n->lastChild())
        n = n->lastChild();
    return n;
}

bool NodeImpl::insertBefore(PassRefPtr<NodeImpl>, NodeImpl*, ExceptionCode& ec)
{
    ec = HIERARCHY_REQUEST_ERR;
    return false;
}

bool NodeImpl::replaceChild(PassRefPtr<NodeImpl>, NodeImpl*, ExceptionCode& ec)
{
    ec = HIERARCHY_REQUEST_ERR;
    return false;
}

bool NodeImpl::removeChild(NodeImpl*, ExceptionCode& ec)
{
    ec = NOT_FOUND_ERR;
    return false;
}

bool NodeImpl::appendChild(PassRefPtr<NodeImpl>, ExceptionCode& ec)
{
    ec = HIERARCHY_REQUEST_ERR;
    return false;
}

void NodeImpl::remove(ExceptionCode& ec)
{
    ref();
    if (NodeImpl *p = parentNode())
        p->removeChild(this, ec);
    else
        ec = HIERARCHY_REQUEST_ERR;
    deref();
}

bool NodeImpl::hasChildNodes(  ) const
{
  return false;
}

void NodeImpl::normalize ()
{
    ExceptionCode ec = 0;
    NodeImpl *child = firstChild();

    if (isElementNode()) {
        // Normalize any attribute children we might have 
        ElementImpl *element = static_cast<ElementImpl *>(this);
        NamedAttrMapImpl *attrMap = element->attributes();
        
        if (attrMap) {
            unsigned numAttrs = attrMap->length();
            
            for (unsigned i = 0; i < numAttrs; i++) {
                AttributeImpl *attribute = attrMap->attributeItem(i);
                AttrImpl *attr = attribute->attrImpl();
                
                if (attr)
                    attr->normalize();
            }
        }
    }
    
    // Recursively go through the subtree beneath us, normalizing all nodes. In the case
    // where there are two adjacent text nodes, they are merged together
    while (child) {
        NodeImpl *nextChild = child->nextSibling();

        if (nextChild && child->nodeType() == TEXT_NODE && nextChild->nodeType() == TEXT_NODE) {
            // Current child and the next one are both text nodes... merge them
            TextImpl *currentText = static_cast<TextImpl*>(child);
            TextImpl *nextText = static_cast<TextImpl*>(nextChild);

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
        TextImpl *text = static_cast<TextImpl*>(child);
        if (text->data().isEmpty())
            child->remove(ec);
    }
}

const AtomicString& NodeImpl::prefix() const
{
    // For nodes other than elements and attributes, the prefix is always null
    return nullAtom;
}

void NodeImpl::setPrefix(const AtomicString &/*_prefix*/, ExceptionCode& ec)
{
    // The spec says that for nodes other than elements and attributes, prefix is always null.
    // It does not say what to do when the user tries to set the prefix on another type of
    // node, however mozilla throws a NAMESPACE_ERR exception
    ec = NAMESPACE_ERR;
}

const AtomicString& NodeImpl::localName() const
{
    return emptyAtom;
}

const AtomicString& NodeImpl::namespaceURI() const
{
    return nullAtom;
}

ContainerNodeImpl* NodeImpl::addChild(PassRefPtr<NodeImpl>)
{
    return 0;
}

bool NodeImpl::isContentEditable() const
{
    return parent() && parent()->isContentEditable();
}

IntRect NodeImpl::getRect() const
{
    int _x, _y;
    if (renderer() && renderer()->absolutePosition(_x, _y))
        return IntRect( _x, _y, renderer()->width(), renderer()->height() + renderer()->borderTopExtra() + renderer()->borderBottomExtra());

    return IntRect();
}

void NodeImpl::setChanged(bool b)
{
    if (b && !attached()) // changed compared to what?
        return;

    m_changed = b;
    if ( b ) {
        NodeImpl *p = parentNode();
        while ( p ) {
            p->setHasChangedChild( true );
            p = p->parentNode();
        }
        getDocument()->setDocumentChanged(true);
    }
}

bool NodeImpl::isFocusable() const
{
    return false;
}

bool NodeImpl::isKeyboardFocusable() const
{
    return isFocusable();
}

bool NodeImpl::isMouseFocusable() const
{
    return isFocusable();
}

unsigned NodeImpl::nodeIndex() const
{
    NodeImpl *_tempNode = previousSibling();
    unsigned count=0;
    for( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
}

void NodeImpl::registerNodeList(NodeListImpl* list)
{
    if (!m_nodeLists)
        m_nodeLists = new NodeListSet;
    m_nodeLists->add(list);
}

void NodeImpl::unregisterNodeList(NodeListImpl* list)
{
    if (!m_nodeLists)
        return;
    m_nodeLists->remove(list);
}

void NodeImpl::notifyLocalNodeListsAttributeChanged()
{
    if (!m_nodeLists)
        return;

    NodeListSet::iterator end = m_nodeLists->end();
    for (NodeListSet::iterator i = m_nodeLists->begin(); i != end; ++i)
        (*i)->rootNodeAttributeChanged();
}

void NodeImpl::notifyNodeListsAttributeChanged()
{
    for (NodeImpl *n = this; n; n = n->parentNode())
        n->notifyLocalNodeListsAttributeChanged();
}

void NodeImpl::notifyLocalNodeListsChildrenChanged()
{
    if (!m_nodeLists)
        return;

    NodeListSet::iterator end = m_nodeLists->end();
    for (NodeListSet::iterator i = m_nodeLists->begin(); i != end; ++i)
        (*i)->rootNodeChildrenChanged();
}

void NodeImpl::notifyNodeListsChildrenChanged()
{
    for (NodeImpl *n = this; n; n = n->parentNode())
        n->notifyLocalNodeListsChildrenChanged();
}

unsigned NodeImpl::childNodeCount() const
{
    return 0;
}

NodeImpl *NodeImpl::childNode(unsigned /*index*/) const
{
    return 0;
}

NodeImpl *NodeImpl::traverseNextNode(const NodeImpl *stayWithin) const
{
    if (firstChild()) {
        assert(!stayWithin || firstChild()->isAncestor(stayWithin));
        return firstChild();
    }
    if (this == stayWithin)
        return 0;
    if (nextSibling()) {
        assert(!stayWithin || nextSibling()->isAncestor(stayWithin));
        return nextSibling();
    }
    const NodeImpl *n = this;
    while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n) {
        assert(!stayWithin || !n->nextSibling() || n->nextSibling()->isAncestor(stayWithin));
        return n->nextSibling();
    }
    return 0;
}

NodeImpl *NodeImpl::traverseNextSibling(const NodeImpl *stayWithin) const
{
    if (this == stayWithin)
        return 0;
    if (nextSibling()) {
        assert(!stayWithin || nextSibling()->isAncestor(stayWithin));
        return nextSibling();
    }
    const NodeImpl *n = this;
    while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n) {
        assert(!stayWithin || !n->nextSibling() || n->nextSibling()->isAncestor(stayWithin));
        return n->nextSibling();
    }
    return 0;
}

NodeImpl *NodeImpl::traversePreviousNode() const
{
    if (previousSibling()) {
        NodeImpl *n = previousSibling();
        while (n->lastChild())
            n = n->lastChild();
        return n;
    }
    
    return parentNode();
}

NodeImpl *NodeImpl::traversePreviousNodePostOrder(const NodeImpl *stayWithin) const
{
    if (lastChild()) {
        assert(!stayWithin || lastChild()->isAncestor(stayWithin));
        return lastChild();
    }
    if (this == stayWithin)
        return 0;
    if (previousSibling()) {
        assert(!stayWithin || previousSibling()->isAncestor(stayWithin));
        return previousSibling();
    }
    const NodeImpl *n = this;
    while (n && !n->previousSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n) {
        assert(!stayWithin || !n->previousSibling() || n->previousSibling()->isAncestor(stayWithin));
        return n->previousSibling();
    }
    return 0;
}

void NodeImpl::checkSetPrefix(const AtomicString &_prefix, ExceptionCode& ec)
{
    // Perform error checking as required by spec for setting Node.prefix. Used by
    // ElementImpl::setPrefix() and AttrImpl::setPrefix()

    // FIXME: Implement support for INVALID_CHARACTER_ERR: Raised if the specified prefix contains an illegal character.
    
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnly()) {
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
        (_prefix == "xml" && DOMString(getDocument()->namespaceURI(id())) != "http://www.w3.org/XML/1998/namespace")) {
        ec = NAMESPACE_ERR;
        return;
    }*/
}

void NodeImpl::checkAddChild(NodeImpl *newChild, ExceptionCode& ec)
{
    // Perform error checking as required by spec for adding a new child. Used by
    // appendChild(), replaceChild() and insertBefore()

    // Not mentioned in spec: throw NOT_FOUND_ERR if newChild is null
    if (!newChild) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    bool shouldAdoptChild = false;

    // WRONG_DOCUMENT_ERR: Raised if newChild was created from a different document than the one that
    // created this node.
    // We assume that if newChild is a DocumentFragment, all children are created from the same document
    // as the fragment itself (otherwise they could not have been added as children)
    if (newChild->getDocument() != getDocument()) {
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
    if (newChild == this || isAncestor(newChild)) {
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
        for (NodeImpl *n = newChild->firstChild(); n; n = n->nextSibling()) {
            if (!childTypeAllowed(n->nodeType())) {
                ec = HIERARCHY_REQUEST_ERR;
                return;
            }
        }
    }
    
    // change the document pointer of newChild and all of its children to be the new document
    if (shouldAdoptChild) {
        for (NodeImpl* node = newChild; node; node = node->traverseNextNode(newChild)) {
            KJS::ScriptInterpreter::updateDOMNodeDocument(node, node->getDocument(), getDocument());
            node->setDocument(getDocument());
        }
    }
}

bool NodeImpl::isAncestor(const NodeImpl *other) const
{
    // Return true if other is an ancestor of this, otherwise false
    if (!other)
        return false;
    for (const NodeImpl *n = parentNode(); n; n = n->parentNode()) {
        if (n == other)
            return true;
    }
    return false;
}

bool NodeImpl::childAllowed( NodeImpl *newChild )
{
    return childTypeAllowed(newChild->nodeType());
}

NodeImpl::StyleChange NodeImpl::diff( khtml::RenderStyle *s1, khtml::RenderStyle *s2 ) const
{
    // FIXME: The behavior of this function is just totally wrong.  It doesn't handle
    // explicit inheritance of non-inherited properties and so you end up not re-resolving
    // style in cases where you need to.
    StyleChange ch = NoInherit;
    EDisplay display1 = s1 ? s1->display() : NONE;
    bool fl1 = s1 ? s1->hasPseudoStyle(RenderStyle::FIRST_LETTER) : false;
    EDisplay display2 = s2 ? s2->display() : NONE;
    bool fl2 = s2 ? s2->hasPseudoStyle(RenderStyle::FIRST_LETTER) : false;
    if (display1 != display2 || fl1 != fl2)
        ch = Detach;
    else if ( !s1 || !s2 )
        ch = Inherit;
    else if ( *s1 == *s2 )
        ch = NoChange;
    else if ( s1->inheritedNotEqual( s2 ) )
        ch = Inherit;
    return ch;
}

#ifndef NDEBUG
void NodeImpl::dump(QTextStream *stream, QString ind) const
{
    // ### implement dump() for all appropriate subclasses

    if (m_hasId) { *stream << " hasId"; }
    if (m_hasClass) { *stream << " hasClass"; }
    if (m_hasStyle) { *stream << " hasStyle"; }
    if (m_specified) { *stream << " specified"; }
    if (m_focused) { *stream << " focused"; }
    if (m_active) { *stream << " active"; }
    if (m_styleElement) { *stream << " styleElement"; }
    if (m_implicit) { *stream << " implicit"; }

    *stream << " tabIndex=" << m_tabIndex;
    *stream << endl;

    NodeImpl *child = firstChild();
    while(child) {
        *stream << ind << child->nodeName().qstring().ascii() << ": ";
        child->dump(stream,ind+"  ");
        child = child->nextSibling();
    }
}
#endif

void NodeImpl::attach()
{
    assert(!attached());
    assert(!renderer() || (renderer()->style() && renderer()->parent()));
    getDocument()->incDOMTreeVersion();
    m_attached = true;
}

void NodeImpl::willRemove()
{
}

void NodeImpl::detach()
{
    m_inDetach = true;
//    assert(m_attached);

    if (renderer())
        renderer()->destroy();
    setRenderer(0);

    DocumentImpl* doc = getDocument();
    if (m_hovered)
        doc->hoveredNodeDetached(this);
    if (m_inActiveChain)
        doc->activeChainNodeDetached(this);
    doc->incDOMTreeVersion();

    m_active = false;
    m_hovered = false;
    m_inActiveChain = false;
    m_attached = false;
    m_inDetach = false;
}

bool NodeImpl::maintainsState()
{
    return false;
}

QString NodeImpl::state()
{
    return QString::null;
}

void NodeImpl::restoreState(QStringList &/*states*/)
{
}

void NodeImpl::insertedIntoDocument()
{
    setInDocument(true);
    insertedIntoTree(false);
}

void NodeImpl::removedFromDocument()
{
    setInDocument(false);
    removedFromTree(false);
}

void NodeImpl::childrenChanged()
{
}

bool NodeImpl::isReadOnly()
{
    // Entity & Entity Reference nodes and their descendants are read-only
    NodeImpl *n = this;
    while (n) {
        if (n->nodeType() == ENTITY_NODE || n->nodeType() == ENTITY_REFERENCE_NODE)
            return true;
        n = n->parentNode();
    }
    return false;
}

NodeImpl *NodeImpl::previousEditable() const
{
    NodeImpl *node = previousLeafNode();
    while (node) {
        if (node->isContentEditable())
            return node;
        node = node->previousLeafNode();
    }
    return 0;
}

// Offset specifies the child node to start at.  If it is past
// the last child, it specifies to start at next sibling.
NodeImpl *NodeImpl::nextEditable(int offset) const
{
    assert(offset>=0);
    NodeImpl *node;
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

NodeImpl *NodeImpl::nextEditable() const
{
    NodeImpl *node = nextLeafNode();
    while (node) {
        if (node->isContentEditable())
            return node;
        node = node->nextLeafNode();
    }
    return 0;
}

RenderObject * NodeImpl::previousRenderer()
{
    for (NodeImpl *n = previousSibling(); n; n = n->previousSibling()) {
        if (n->renderer())
            return n->renderer();
    }
    return 0;
}

RenderObject * NodeImpl::nextRenderer()
{
    // Avoid an O(n^2) problem with this function by not checking for nextRenderer() when the parent element hasn't even 
    // been attached yet.
    if (parent() && !parent()->attached())
        return 0;

    for (NodeImpl *n = nextSibling(); n; n = n->nextSibling()) {
        if (n->renderer())
            return n->renderer();
    }
    return 0;
}

// FIXME: This code is used by editing.  Seems like it could move over there and not pollute NodeImpl.
NodeImpl *NodeImpl::previousNodeConsideringAtomicNodes() const
{
    if (previousSibling()) {
        NodeImpl *n = previousSibling();
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

NodeImpl *NodeImpl::nextNodeConsideringAtomicNodes() const
{
    if (!isAtomicNode(this) && firstChild())
        return firstChild();
    if (nextSibling())
        return nextSibling();
    const NodeImpl *n = this;
    while (n && !n->nextSibling())
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return 0;
}

NodeImpl *NodeImpl::previousLeafNode() const
{
    NodeImpl *node = previousNodeConsideringAtomicNodes();
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = node->previousNodeConsideringAtomicNodes();
    }
    return 0;
}

NodeImpl *NodeImpl::nextLeafNode() const
{
    NodeImpl *node = nextNodeConsideringAtomicNodes();
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = node->nextNodeConsideringAtomicNodes();
    }
    return 0;
}

void NodeImpl::createRendererIfNeeded()
{
    if (!getDocument()->shouldCreateRenderers())
        return;
    
    assert(!attached());
    assert(!renderer());
    
    NodeImpl *parent = parentNode();    
    assert(parent);
    
    RenderObject *parentRenderer = parent->renderer();
    if (parentRenderer && parentRenderer->canHaveChildren()
#if SVG_SUPPORT
        && parent->childShouldCreateRenderer(this)
#endif
        ) {
        RenderStyle* style = styleForRenderer(parentRenderer);
        style->ref();
#ifndef KHTML_NO_XBL
        bool resolveStyle = false;
        if (getDocument()->bindingManager()->loadBindings(this, style->bindingURIs(), true, &resolveStyle) && 
            rendererIsNeeded(style)) {
            if (resolveStyle) {
                style->deref();
                style = styleForRenderer(parentRenderer);
            }
#else
        if (rendererIsNeeded(style)) {
#endif
            setRenderer(createRenderer(getDocument()->renderArena(), style));
            if (renderer()) {
                renderer()->setStyle(style);
                parentRenderer->addChild(renderer(), nextRenderer());
            }
#ifndef KHTML_NO_XBL
        } // avoid confusing the change log code parser by having two close braces to match the two open braces above
#else
        }
#endif
        style->deref(getDocument()->renderArena());
    }
}

RenderStyle *NodeImpl::styleForRenderer(RenderObject *parent)
{
    return parent->style();
}

bool NodeImpl::rendererIsNeeded(RenderStyle *style)
{
    return (getDocument()->documentElement() == this) || (style->display() != NONE);
}

RenderObject *NodeImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    assert(false);
    return 0;
}

int NodeImpl::maxOffset() const
{
    return 1;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks questions about HTML in the core DOM class
// is obviously misplaced.
int NodeImpl::caretMinOffset() const
{
    return renderer() ? renderer()->caretMinOffset() : 0;
}

int NodeImpl::caretMaxOffset() const
{
    return renderer() ? renderer()->caretMaxOffset() : 1;
}

unsigned NodeImpl::caretMaxRenderedOffset() const
{
    return renderer() ? renderer()->caretMaxRenderedOffset() : 1;
}

int NodeImpl::previousOffset (int current) const
{
    return renderer() ? renderer()->previousOffset(current) : current - 1;
}

int NodeImpl::nextOffset (int current) const
{
    return renderer() ? renderer()->nextOffset(current) : current + 1;
}

bool NodeImpl::isBlockFlow() const
{
    return renderer() && renderer()->isBlockFlow();
}

bool NodeImpl::isBlockFlowOrBlockTable() const
{
    return renderer() && (renderer()->isBlockFlow() || renderer()->isTable() && !renderer()->isInline());
}

bool NodeImpl::isEditableBlock() const
{
    return isContentEditable() && isBlockFlow();
}

ElementImpl *NodeImpl::enclosingBlockFlowOrTableElement() const
{
    NodeImpl *n = const_cast<NodeImpl *>(this);
    if (isBlockFlowOrBlockTable())
        return static_cast<ElementImpl *>(n);

    while (1) {
        n = n->parentNode();
        if (!n)
            break;
        if (n->isBlockFlowOrBlockTable() || n->hasTagName(bodyTag))
            return static_cast<ElementImpl *>(n);
    }
    return 0;
}

ElementImpl *NodeImpl::enclosingBlockFlowElement() const
{
    NodeImpl *n = const_cast<NodeImpl *>(this);
    if (isBlockFlow())
        return static_cast<ElementImpl *>(n);

    while (1) {
        n = n->parentNode();
        if (!n)
            break;
        if (n->isBlockFlow() || n->hasTagName(bodyTag))
            return static_cast<ElementImpl *>(n);
    }
    return 0;
}

ElementImpl *NodeImpl::enclosingInlineElement() const
{
    NodeImpl *n = const_cast<NodeImpl *>(this);
    NodeImpl *p;

    while (1) {
        p = n->parentNode();
        if (!p || p->isBlockFlow() || p->hasTagName(bodyTag))
            return static_cast<ElementImpl *>(n);
        // Also stop if any previous sibling is a block
        for (NodeImpl *sibling = n->previousSibling(); sibling; sibling = sibling->previousSibling()) {
            if (sibling->isBlockFlow())
                return static_cast<ElementImpl *>(n);
        }
        n = p;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

ElementImpl *NodeImpl::rootEditableElement() const
{
    if (!isContentEditable())
        return 0;

    NodeImpl *n = const_cast<NodeImpl *>(this);
    if (n->hasTagName(bodyTag))
        return static_cast<ElementImpl *>(n);

    NodeImpl *result = n->isEditableBlock() ? n : 0;
    while (1) {
        n = n->parentNode();
        if (!n || !n->isContentEditable())
            break;
        if (n->hasTagName(bodyTag)) {
            result = n;
            break;
        }
        if (n->isBlockFlow())
            result = n;
    }
    return static_cast<ElementImpl *>(result);
}

bool NodeImpl::inSameRootEditableElement(NodeImpl *n)
{
    return n ? rootEditableElement() == n->rootEditableElement() : false;
}

bool NodeImpl::inSameContainingBlockFlowElement(NodeImpl *n)
{
    return n ? enclosingBlockFlowElement() == n->enclosingBlockFlowElement() : false;
}

// FIXME: End of obviously misplaced HTML editing functions.  Try to move these out of NodeImpl.

PassRefPtr<NodeListImpl> NodeImpl::getElementsByTagName(const DOMString& name)
{
    return getElementsByTagNameNS("*", name);
}
 
PassRefPtr<NodeListImpl> NodeImpl::getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName)
{
    if (namespaceURI.isNull() || localName.isNull())
        return 0; // FIXME: Who relies on getting 0 instead of a node list in this case?
    
    DOMString name = localName;
    if (getDocument()->isHTMLDocument())
        name = localName.lower();
    return new TagNodeListImpl(this, AtomicString(namespaceURI), AtomicString(name));
}

bool NodeImpl::isSupported(const DOMString &feature, const DOMString &version)
{
    return DOMImplementationImpl::instance()->hasFeature(feature, version);
}

DocumentImpl *NodeImpl::ownerDocument() const
{
    DocumentImpl *doc = getDocument();
    return doc == this ? 0 : doc;
}

bool NodeImpl::hasAttributes() const
{
    return false;
}

NamedAttrMapImpl *NodeImpl::attributes() const
{
    return 0;
}

bool NodeImpl::isEqualNode(NodeImpl *other) const
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
    
    NamedAttrMapImpl *attrs = attributes();
    NamedAttrMapImpl *otherAttrs = other->attributes();
    
    if (!attrs && otherAttrs)
        return false;
    
    if (attrs && !attrs->mapsEquivalent(otherAttrs))
        return false;
    
    NodeImpl *child = firstChild();
    NodeImpl *otherChild = other->firstChild();
    
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

bool NodeImpl::isDefaultNamespace(const DOMString &namespaceURI) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#isDefaultNamespaceAlgo
    
    switch (nodeType()) {
        case ELEMENT_NODE: {
            const ElementImpl *elem = static_cast<const ElementImpl *>(this);
            
            if (elem->prefix().isNull())
                return elem->namespaceURI() == namespaceURI;

            if (elem->hasAttributes()) {
                NamedAttrMapImpl *attrs = elem->attributes();
                
                for (unsigned i = 0; i < attrs->length(); i++) {
                    AttributeImpl *attr = attrs->attributeItem(i);
                    
                    if (attr->localName() == "xmlns")
                        return attr->value() == namespaceURI;
                }
            }

            if (ElementImpl* ancestor = ancestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);

            return false;
        }
        case DOCUMENT_NODE:
            return static_cast <const DocumentImpl *>(this)->documentElement()->isDefaultNamespace(namespaceURI);
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            return false;
        case ATTRIBUTE_NODE: {
            const AttrImpl *attr = static_cast<const AttrImpl *>(this);
            if (attr->ownerElement())
                return attr->ownerElement()->isDefaultNamespace(namespaceURI);
            return false;
        }
        default:
            if (ElementImpl* ancestor = ancestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);
            return false;
    }
}

DOMString NodeImpl::lookupPrefix(const DOMString &namespaceURI) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespacePrefixAlgo
    
    if (namespaceURI.isEmpty())
        return DOMString();
    
    switch (nodeType()) {
        case ELEMENT_NODE:
            return lookupNamespacePrefix(namespaceURI, static_cast<const ElementImpl *>(this));
        case DOCUMENT_NODE:
            return static_cast<const DocumentImpl *>(this)->documentElement()->lookupPrefix(namespaceURI);
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_FRAGMENT_NODE:
        case DOCUMENT_TYPE_NODE:
            return DOMString();
        case ATTRIBUTE_NODE: {
            const AttrImpl *attr = static_cast<const AttrImpl *>(this);
            if (attr->ownerElement())
                return attr->ownerElement()->lookupPrefix(namespaceURI);
            return DOMString();
        }
        default:
            if (ElementImpl* ancestor = ancestorElement())
                return ancestor->lookupPrefix(namespaceURI);
            return DOMString();
    }
}

DOMString NodeImpl::lookupNamespaceURI(const DOMString &prefix) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespaceURIAlgo
    
    if (!prefix.isNull() && prefix.isEmpty())
        return DOMString();
    
    switch (nodeType()) {
        case ELEMENT_NODE: {
            const ElementImpl *elem = static_cast<const ElementImpl *>(this);
            
            if (!elem->namespaceURI().isNull() && elem->prefix() == prefix)
                return elem->namespaceURI();
            
            if (elem->hasAttributes()) {
                NamedAttrMapImpl *attrs = elem->attributes();
                
                for (unsigned i = 0; i < attrs->length(); i++) {
                    AttributeImpl *attr = attrs->attributeItem(i);
                    
                    if (attr->prefix() == "xmlns" && attr->localName() == prefix) {
                        if (!attr->value().isEmpty())
                            return attr->value();
                        
                        return DOMString();
                    } else if (attr->localName() == "xmlns" && prefix.isNull()) {
                        if (!attr->value().isEmpty())
                            return attr->value();
                        
                        return DOMString();
                    }
                }
            }
            if (ElementImpl* ancestor = ancestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            return DOMString();
        }
        case DOCUMENT_NODE:
            return static_cast<const DocumentImpl *>(this)->documentElement()->lookupNamespaceURI(prefix);
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            return DOMString();
        case ATTRIBUTE_NODE: {
            const AttrImpl *attr = static_cast<const AttrImpl *>(this);
            
            if (attr->ownerElement())
                return attr->ownerElement()->lookupNamespaceURI(prefix);
            else
                return DOMString();
        }
        default:
            if (ElementImpl* ancestor = ancestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            return DOMString();
    }
}

DOMString NodeImpl::lookupNamespacePrefix(const DOMString &_namespaceURI, const ElementImpl *originalElement) const
{
    if (_namespaceURI.isNull())
        return DOMString();
            
    if (originalElement->lookupNamespaceURI(prefix()) == _namespaceURI)
        return prefix();
    
    if (hasAttributes()) {
        NamedAttrMapImpl *attrs = attributes();
        
        for (unsigned i = 0; i < attrs->length(); i++) {
            AttributeImpl *attr = attrs->attributeItem(i);
            
            if (attr->prefix() == "xmlns" &&
                attr->value() == _namespaceURI &&
                originalElement->lookupNamespaceURI(attr->localName()) == _namespaceURI)
                return attr->localName();
        }
    }
    
    if (ElementImpl* ancestor = ancestorElement())
        return ancestor->lookupNamespacePrefix(_namespaceURI, originalElement);
    return DOMString();
}

DOMString NodeImpl::textContent() const
{
    switch (nodeType()) {
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
            return nodeValue();
        
        case ELEMENT_NODE:
        case ATTRIBUTE_NODE:
        case ENTITY_NODE:
        case ENTITY_REFERENCE_NODE:
        case DOCUMENT_FRAGMENT_NODE: {
            DOMString s = "";
        
            for (NodeImpl *child = firstChild(); child; child = child->nextSibling()) {
                if (child->nodeType() == COMMENT_NODE || child->nodeType() == PROCESSING_INSTRUCTION_NODE)
                    continue;
            
                s += child->textContent();
            }
        
            return s;
        }
        
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case NOTATION_NODE:
        default:
            return DOMString();            
    }
}

void NodeImpl::setTextContent(const DOMString &text, ExceptionCode& ec)
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
            ContainerNodeImpl *container = static_cast<ContainerNodeImpl *>(this);
            
            container->removeChildren();
            
            if (!text.isEmpty())
                appendChild(getDocument()->createTextNode(text), ec);
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

ElementImpl* NodeImpl::ancestorElement() const
{
    // In theory, there can be EntityReference nodes between elements, but this is currently not supported.
    for (NodeImpl* n = parentNode(); n; n = n->parentNode()) {
        if (n->isElementNode())
            return static_cast<ElementImpl*>(n);
    }
    return 0;
}

bool NodeImpl::offsetInCharacters() const
{
    return false;
}

#ifndef NDEBUG

static void appendAttributeDesc(const NodeImpl *node, DOMString &string, const QualifiedName& name, QString attrDesc)
{
    if (node->isElementNode()) {
        DOMString attr = static_cast<const ElementImpl *>(node)->getAttribute(name);
        if (!attr.isEmpty()) {
            string += attrDesc;
            string += attr;
        }
    }
}

void NodeImpl::showNode(const char *prefix) const
{
    if (!prefix)
        prefix = "";
    if (isTextNode()) {
        QString value = nodeValue().qstring();
        value.replace('\\', "\\\\");
        value.replace('\n', "\\n");
        fprintf(stderr, "%s%s\t%p \"%s\"\n", prefix, nodeName().qstring().utf8().data(), this, value.utf8().data());
    } else {
        DOMString attrs = "";
        appendAttributeDesc(this, attrs, classAttr, " CLASS=");
        appendAttributeDesc(this, attrs, styleAttr, " STYLE=");
        fprintf(stderr, "%s%s\t%p%s\n", prefix, nodeName().qstring().utf8().data(), this, attrs.qstring().ascii());
    }
}

void NodeImpl::showTree() const
{
    showTreeAndMark((NodeImpl *)this, "*", NULL, NULL);
}

void showTree(const NodeImpl *node)
{
    if (node)
        node->showTree();
}

void NodeImpl::showTreeAndMark(NodeImpl * markedNode1, const char * markedLabel1, NodeImpl * markedNode2, const char * markedLabel2) const
{
    NodeImpl *rootNode;
    NodeImpl *node = (NodeImpl *)this;
    while (node->parentNode() && !node->hasTagName(bodyTag))
        node = node->parentNode();
    rootNode = node;
        
    for (node = rootNode; node; node = node->traverseNextNode()) {
        NodeImpl *tmpNode;
                
        if (node == markedNode1)
            fprintf(stderr, "%s", markedLabel1);
        if (node == markedNode2)
            fprintf(stderr, "%s", markedLabel2);
                        
        for (tmpNode = node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentNode())
            fprintf(stderr, "\t");
        node->showNode(0);
    }
}

void NodeImpl::formatForDebugger(char *buffer, unsigned length) const
{
    DOMString result;
    DOMString s;
    
    s = nodeName();
    if (s.length() == 0)
        result += "<none>";
    else
        result += s;
          
    strncpy(buffer, result.qstring().latin1(), length - 1);
}

#endif

}

/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis           <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <kdom/Helper.h>

#include "NodeImpl.h"
#include "TextImpl.h"
#include "kdomevents.h"
#include "RenderStyle.h"
#include "ElementImpl.h"
#include "DocumentImpl.h"
#include "NodeListImpl.h"
#include "DOMExceptionImpl.h"
#include "DocumentTypeImpl.h"
#include "NamedAttrMapImpl.h"
#include "MutationEventImpl.h"
#include "DOMImplementationImpl.h"

using namespace KDOM;

NodeImpl::NodeImpl(DocumentPtr *doc) : EventTargetImpl(), next(0), prev(0), document(doc), m_render(0),
                                      m_hasId(false), m_hasStyle(false), m_attached(false),
                                      m_closed(false), m_changed(false), m_hasChangedChild(false),
                                      m_inDocument(false), m_hasAnchor(false), m_specified(false),
                                      m_focused(false), m_active(false), m_dom2(false), m_nullNSSpecified(false)
{
    if(document)
        document->ref();
}

NodeImpl::~NodeImpl()
{
    if(m_render)
        detach();

    if(document)
        document->deref();

    if(prev)
        prev->next = 0;

    if(next)
        next->prev = 0;
}

void NodeImpl::setPreviousSibling(NodeImpl *prev)
{
    this->prev = prev;
}

void NodeImpl::setNextSibling(NodeImpl *next)
{
    this->next = next;
}

NodeImpl *NodeImpl::insertBefore(NodeImpl *, NodeImpl *)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if(isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    // HIERARCHY_REQUEST_ERR: Raised if this node is of a type that does not allow
    // children of the type of the newChild node, or if the node to append is one of
    // this node's ancestors or this node itself, or if this node is of type Document
    // and the DOM application attempts to append a second DocumentType or Element node.
    throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
}

NodeImpl *NodeImpl::replaceChild(NodeImpl *, NodeImpl *)
{
    // HIERARCHY_REQUEST_ERR: Raised if this node is of a type that does not allow
    // children of the type of the newChild node, or if the node to append is one of
    // this node's ancestors or this node itself, or if this node is of type Document
    // and the DOM application attempts to append a second DocumentType or Element node.
    throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
}

NodeImpl *NodeImpl::appendChild(NodeImpl *)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if(isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    // HIERARCHY_REQUEST_ERR: Raised if this node is of a type that does not allow
    // children of the type of the newChild node, or if the node to append is one of
    // this node's ancestors or this node itself, or if this node is of type Document
    // and the DOM application attempts to append a second DocumentType or Element node.
    throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
}

NodeImpl *NodeImpl::removeChild(NodeImpl *oldChild)
{
    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if(!oldChild || oldChild->parentNode() != this)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if(isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    return 0;
}

bool NodeImpl::hasChildNodes() const
{
    return false;
}

bool NodeImpl::isAncestor(NodeImpl *current, NodeImpl *other)
{
    if(!current || !other)
        return false;

    // Return true if other is the same as this node or an ancestor of it, otherwise false
    NodeImpl *n;
    for(n = this; n; n = n->parentNode())
    {
        if(n == other)
            return true;
    }

    return false;
}

void NodeImpl::attach()
{
    m_attached = true;
}

void NodeImpl::detach()
{
    m_attached = false;
}

DOMStringImpl *NodeImpl::localName() const
{
    return 0;
}

DOMStringImpl *NodeImpl::nodeName() const
{
    return 0;
}

DOMStringImpl *NodeImpl::nodeValue() const
{
    return 0;
}

void NodeImpl::setNodeValue(DOMStringImpl *)
{
}    

unsigned short NodeImpl::nodeType() const
{
    return 0;
}

NamedAttrMapImpl *NodeImpl::attributes(bool) const
{
    return 0;
}

NodeImpl *NodeImpl::parentNode() const
{
    return static_cast<NodeImpl *>(m_parent); // defined in TreeShared...
}

NodeImpl *NodeImpl::previousSibling() const
{
    return prev;
}

NodeImpl *NodeImpl::nextSibling() const
{
    return next;
}

NodeImpl *NodeImpl::firstChild() const
{
    return 0;
}

NodeImpl *NodeImpl::lastChild() const
{
    return 0;
}

DOMStringImpl *NodeImpl::namespaceURI() const
{
    return 0;
}

DOMStringImpl *NodeImpl::prefix() const
{
    // For nodes other than elements and attributes, the prefix is always null
    return 0;
}

void NodeImpl::setPrefix(DOMStringImpl *)
{
    // The spec says that for nodes other than elements and attributes, prefix is always null.
    // It does not say what to do when the user tries to set the prefix on another type of
    // node, however mozilla throws a NAMESPACE_ERR exception
    throw new DOMExceptionImpl(NAMESPACE_ERR);
}

bool NodeImpl::hasAttributes() const
{
    return false;
}

bool NodeImpl::isSupported(DOMStringImpl *feature, DOMStringImpl *version) const
{
    return DOMImplementationImpl::self()->hasFeature(feature, version);
}

DOMStringImpl *NodeImpl::textContent() const
{
    DOMStringImpl *ret = new DOMStringImpl();

    NodeImpl *child = firstChild();
    while(child != 0)
    {
        ret->append(DOMString(child->textContent()).string());
        child = child->nextSibling();
    }

    return ret;
}

void NodeImpl::setTextContent(DOMStringImpl *text)
{
    switch(nodeType())
    {
        case ELEMENT_NODE:  
        case ENTITY_NODE:  
        case ENTITY_REFERENCE_NODE:
        case DOCUMENT_FRAGMENT_NODE:  
        {
            // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
            if(isReadOnly())
                throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

            // Remove all childs
            NodeImpl *current = firstChild();
            while(current)
            {
                removeChild(current);
                current = firstChild();
            }

            // Add textnode containing data
            if(text && !text->isEmpty())
                appendChild(ownerDocument()->createTextNode(text));

            break;
        }
        case ATTRIBUTE_NODE:
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        {
            setNodeValue(text);
            break;
        }
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case NOTATION_NODE:
            break;
        default:
            throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);
    }
}

bool NodeImpl::isReadOnly() const
{
    const NodeImpl *n = this;
    while(n != 0)
    {
        unsigned short type = n->nodeType();
        if((type == ENTITY_NODE || type == ENTITY_REFERENCE_NODE ||
            type == NOTATION_NODE || type == DOCUMENT_TYPE_NODE)
            && !docPtr()->document()->parsing())
            return true;

        n = n->parentNode();
    }

    return false;
}

RenderObject *NodeImpl::previousRenderer()
{
    for(NodeImpl *n = previousSibling(); n; n = n->previousSibling())
    {
        if(n->renderer())
            return n->renderer();
    }

    return 0;
}

RenderObject *NodeImpl::nextRenderer()
{
    for(NodeImpl *n = nextSibling(); n; n = n->nextSibling())
    {
        if(n->renderer())
            return n->renderer();
    }

    return 0;
}

bool NodeImpl::childAllowed(NodeImpl *newChild)
{
    return childTypeAllowed(newChild->nodeType());
}

bool NodeImpl::childTypeAllowed(unsigned short) const
{
    return false;
}

NodeImpl *NodeImpl::cloneNode(bool deep) const
{
    return cloneNode(deep, docPtr());
}

NodeImpl *NodeImpl::cloneNode(bool, DocumentPtr *) const
{
    return 0;
}

DocumentImpl *NodeImpl::ownerDocument() const
{
    DocumentImpl *doc = document->document();
    return doc == this ? 0 : doc;
}

NodeListImpl *NodeImpl::childNodes() const
{
    return new NodeListImpl(const_cast<NodeImpl *>(this));
}

void NodeImpl::normalize()
{
    NodeImpl *child = firstChild();

    // Recursively go through the subtree beneath us, normalizing all nodes. In the case
    // where there are two adjacent text nodes, they are merged together
    while(child)
    {
        // The simple case: an empty text node
        DOMStringImpl *nodeValue = child->nodeValue();
        if(child->nodeType() == TEXT_NODE && (!nodeValue || nodeValue->isEmpty()))
            removeChild(child);

        NodeImpl *nextChild = child->nextSibling();
        if(nextChild != 0 && child->nodeType() == TEXT_NODE && nextChild->nodeType() == TEXT_NODE)
        {
            // Current child and the next one are both text nodes... merge them
            TextImpl *currentText = static_cast<TextImpl *>(child);
            TextImpl *nextText = static_cast<TextImpl *>(nextChild);

            currentText->appendData(nextText->data());
            removeChild(nextChild);
            currentText->normalize();
        }
        else
        {
            child->normalize();
            child = nextChild;
        }
    }

    if(nodeType() == ELEMENT_NODE)
    {
        unsigned long length = attributes()->length();
        for(unsigned long i = 0; i < length; i++)
            attributes()->item(i)->normalize();
    }
}

DOMStringImpl *NodeImpl::toString() const
{
    QString str;
    QTextOStream subset(&str);
    Helper::PrintNode(subset, const_cast<NodeImpl *>(this));
    return new DOMStringImpl(str);
}

void NodeImpl::insertedIntoDocument()
{
    setInDocument(true);
}

void NodeImpl::removedFromDocument()
{
    setInDocument(false);
}

void NodeImpl::childrenChanged()
{
    if(parentNode())
        parentNode()->childrenChanged();
}

void NodeImpl::setOwnerDocument(DocumentImpl *doc)
{
    if(document)
        document->deref();

    document = new DocumentPtr();
    document->doc = doc;

    if(document)
        document->ref();
}

NodeImpl *NodeImpl::addChild(NodeImpl *)
{
    return 0;
}

NodeBaseImpl::NodeBaseImpl(DocumentPtr *doc) : NodeImpl(doc), first(0), last(0)
{
}

NodeBaseImpl::~NodeBaseImpl()
{
    // we have to tell all children, that the parent has died...
    NodeImpl *n;
    NodeImpl *next;

    for(n = first; n != 0; n = next)
    {
        next = n->nextSibling();

        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);

        if(!n->refCount())
            delete n;
    }
}

NodeImpl *NodeBaseImpl::addChild(NodeImpl *newChild)
{
    // short check for consistency with DTD
    if(nodeType() != ELEMENT_NODE && newChild->nodeType() != ELEMENT_NODE && !childAllowed(newChild))
    {
        // kdDebug(6020) << "AddChild failed! id=" << id() << ", child->id=" << newChild->id() << endl;
        return 0;
    }

    newChild->setParent(this);

    if(last)
    {
        newChild->setPreviousSibling(last);
        last->setNextSibling(newChild);
        last = newChild;
    }
    else
        first = last = newChild;

    newChild->insertedIntoDocument();
    childrenChanged();

    if(newChild->nodeType() == ELEMENT_NODE)
        return newChild;

    return this;
}

NodeImpl *NodeBaseImpl::insertBefore(NodeImpl *newChild, NodeImpl *refChild)
{
    // Make sure adding the new child is ok
    checkAddChild(this, newChild);
    checkDocumentAddChild(this, newChild);

    bool append = (refChild == 0);

    // NOT_FOUND_ERR: Raised if refChild is not a child of this node
    if(!append && refChild->parentNode() != this)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    bool isFragment = (newChild->nodeType() == DOCUMENT_FRAGMENT_NODE);

    // If newChild is a DocumentFragment with no children.... there's nothing to do.
    // Just return the document fragment
    if(isFragment && !newChild->firstChild())
        return newChild;

    NodeImpl *nextChild = 0;
    NodeImpl *child = isFragment ? newChild->firstChild() : newChild;

    NodeImpl *prev = append ? last : refChild->previousSibling();
    if((prev == newChild) || (refChild == newChild)) // nothing to do
        return newChild;

    while(child != 0)
    {
        nextChild = isFragment ? child->nextSibling() : 0;

        // If child is already present in the tree, first remove it
        NodeImpl *newParent = child->parentNode();
        if(newParent != 0)
            newParent->removeChild(child);

        // Add child in the correct position
        if(prev != 0)
            prev->setNextSibling(child);
        else
            first = child;

        child->setParent(this);
        child->setPreviousSibling(prev);

        if(append)
        {
            child->setNextSibling(0);
            last = child;
        }
        else
        {
            refChild->setPreviousSibling(child);
            child->setNextSibling(refChild);
        }

        // Add child to the rendering tree
        // ### should we detach() it first if it's already attached?
        if(attached() && !child->attached())
            child->attach();

        dispatchChildInsertedEvents(child);

        prev = child;
        child = nextChild;
    }

    setChanged(true);
    dispatchSubtreeModifiedEvent();
    return newChild;
}

NodeImpl *NodeBaseImpl::replaceChild(NodeImpl *newChild, NodeImpl *oldChild)
{
    if(oldChild == newChild) // nothing to do
        return oldChild;

    // Make sure adding the new child is ok
    checkAddChild(this, newChild);

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node
    if(!oldChild || oldChild->parentNode() != this)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    bool isFragment = (newChild->nodeType() == DOCUMENT_FRAGMENT_NODE);

    NodeImpl *nextChild = 0;
    NodeImpl *child = isFragment ? newChild->firstChild() : newChild;

    // Remove the old child
    NodeImpl *prev = oldChild->previousSibling();
    NodeImpl *next = oldChild->nextSibling();

    checkDocumentAddChild(this, newChild, oldChild);
    removeChild(oldChild);

    // Now actually add the child(ren)
    while(child != 0)
    {
        nextChild = isFragment ? child->nextSibling() : 0;

        // If child is already present in the tree, first remove it
        NodeImpl *newParent = child->parentNode();
        if(newParent != 0)
            newParent->removeChild(child);

        // Add child in the correct position
        if(prev != 0)
            prev->setNextSibling(child);
        else
            first = child;

        if(next != 0)
            next->setPreviousSibling(child);
        else
            last = child;

        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(next);

        // Add child to the rendering tree
        // ### should we detach() it first if it's already attached?
        if(attached() && !child->attached())
            child->attach();

        dispatchChildInsertedEvents(child);

        prev = child;
        child = nextChild;
    }

    setChanged(true);
    dispatchSubtreeModifiedEvent();
    return oldChild;
}

NodeImpl *NodeBaseImpl::removeChild(NodeImpl *oldChild)
{
    NodeImpl::removeChild(oldChild);

    dispatchChildRemovalEvents(oldChild);

    // Remove from rendering tree
    if(oldChild->attached())
        oldChild->detach();

    // Remove the child
    NodeImpl *prev = oldChild->previousSibling();
    NodeImpl *next = oldChild->nextSibling();

    if(next != 0)
        next->setPreviousSibling(prev);
    if(prev != 0)
        prev->setNextSibling(next);

    if(first == oldChild)
        first = next;
    if(last == oldChild)
        last = prev;

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    setChanged(true);
    
    // Dispatch post-removal mutation events
    dispatchSubtreeModifiedEvent();
    return oldChild;
}

NodeImpl *NodeBaseImpl::appendChild(NodeImpl *newChild)
{
    return insertBefore(newChild, 0);
}

void NodeBaseImpl::checkAddChild(NodeImpl *current, NodeImpl *newChild)
{
    if(!current) // Should never happens, but who knows? :)
        return;

    // Perform error checking as required by spec for adding a new child.
    // Used by appendChild(), replaceChild() and insertBefore()
    // NOT_FOUND_ERR: Raised if newChild is null (not mentioned in the spec!)
    if(!newChild)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if(current->isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    // WRONG_DOCUMENT_ERR: Raised if newChild was created from a different
    //                       document than the one that created this node.
    // We assume that if newChild is a DocumentFragment, all children are created from the same document
    // as the fragment itself (otherwise they could not have been added as children)
    // Exception: ownerDocument() of #document is null!

    bool isDoc = current->nodeType() == DOCUMENT_NODE;
    if((current->ownerDocument() != newChild->ownerDocument() && !isDoc) ||
        (isDoc && newChild->ownerDocument() && newChild->ownerDocument() != current))
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    // HIERARCHY_REQUEST_ERR: Raised if this node is of a type that does not allow children of the type
    //                          of the newChild node, or if the node to append is one of this node's ancestors.
    // check for ancestor/same node
    if(isAncestor(current, newChild))
        throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);

    // check node allowed
    if(newChild->nodeType() == DOCUMENT_FRAGMENT_NODE)
    {
        // newChild is a DocumentFragment... check all its children instead of newChild itself
        for(NodeImpl *child = newChild->firstChild(); child != 0; child = child->nextSibling())
        {
            if(!current->childAllowed(child))
                throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
        }
    }
    else
    {
        // newChild is not a DocumentFragment... check if it's allowed directly
        if(!current->childAllowed(newChild))
            throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
    }
}

void NodeBaseImpl::checkDocumentAddChild(NodeImpl *current, NodeImpl *newChild, NodeImpl *oldChild)
{
    // if this node is of type Document and the DOM application attempts to append
    // a second DocumentType or Element node.
    if(current->nodeType() == DOCUMENT_NODE)
    {
        ElementImpl *docElement = 0;
        for(NodeImpl *n = current->firstChild(); n != 0; n = n->nextSibling())
        {
            if(n->nodeType() == ELEMENT_NODE && n != oldChild)
            {
                docElement = static_cast<ElementImpl *>(n);
                break;
            }
        }

        DocumentTypeImpl *docType = 0;
        for(NodeImpl *n = current->firstChild(); n != 0; n = n->nextSibling())
        {
            if(n->nodeType() == DOCUMENT_TYPE_NODE && n != oldChild)
            {
                docType = static_cast<DocumentTypeImpl *>(n);
                break;
            }
        }

        if((newChild->nodeType() == ELEMENT_NODE && docElement != 0) ||
           (newChild->nodeType() == DOCUMENT_TYPE_NODE && docType != 0))
            throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
    }
}

NodeImpl *NodeBaseImpl::firstChild() const
{
    return first;
}

NodeImpl *NodeBaseImpl::lastChild() const
{
    return last;
}

void NodeBaseImpl::cloneChildNodes(NodeImpl *clone, DocumentPtr *doc) const
{
    for(NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
    {
        NodeImpl *c = n->cloneNode(true, doc);
        if(c != 0)
            clone->appendChild(c); // appendChild...
    }
}

bool NodeBaseImpl::hasChildNodes() const
{
    return first != 0;
}

void NodeBaseImpl::removeChildren()
{
    NodeImpl *n, *next;
    for(n = first, first = 0; n != 0; n = next)
    {
        next = n->nextSibling();
        
        if(n->attached())
            n->detach();
        
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        
        if(!n->refCount())
            delete n;
        else
        {
            for(NodeImpl *c = n; c; c = c->traverseNextNode(n))
                c->removedFromDocument();
        }
    }
    
    last = 0;
}

void NodeBaseImpl::attach()
{
    NodeImpl *child = first;
    while(child != 0)
    {
        child->attach();
        child = child->nextSibling();
    }

    NodeImpl::attach();
}

void NodeBaseImpl::detach()
{
    NodeImpl *child = first;
    while(child != 0)
    {
        NodeImpl *prev = child;
        child = child->nextSibling();
        prev->detach();
    }

    NodeImpl::detach();
}

NodeImpl::Id NodeImpl::localId() const
{
    return id() & NodeImpl_IdLocalMask;
}

bool NodeImpl::isDefaultNamespace(DOMStringImpl *_namespaceURI) const
{
    switch(nodeType())
    {
        case ELEMENT_NODE:  
        {
            if((!prefix() || prefix()->isEmpty()))
                return namespaceURI() == _namespaceURI;

            // EntityReferences may have to be skipped to get to it
            if(parentNode())
                return parentNode()->isDefaultNamespace(_namespaceURI);

            return false;
        }
        case DOCUMENT_NODE:
            return static_cast<const DocumentImpl *>(this)->documentElement()->isDefaultNamespace(_namespaceURI);
        case ATTRIBUTE_NODE:
        {
            const AttrImpl *attrPtr = static_cast<const AttrImpl *>(this);
            if(attrPtr->ownerElement())
                      return attrPtr->ownerElement()->isDefaultNamespace(_namespaceURI);
        }
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            return false;
        case ENTITY_REFERENCE_NODE:
            return parentNode()->isDefaultNamespace(_namespaceURI);
        default:
            // EntityReferences may have to be skipped to get to it
            if(parentNode())
                return parentNode()->isDefaultNamespace(_namespaceURI);
    };

    return false;
}

bool NodeImpl::isSameNode(NodeImpl *other) const
{
    return (this == other);
}

bool NodeImpl::isEqualNode(NodeImpl *arg) const
{
    if(!arg)
        return false;

    if(isSameNode(arg))
        return true;

    if(nodeType() != arg->nodeType())
        return false;

    if(nodeName() != arg->nodeName())
        return false;

    if(localName() != arg->localName())
        return false;

    if(namespaceURI() != arg->namespaceURI())
        return false;

    if(prefix() != arg->prefix())
        return false;

    if(nodeValue() != arg->nodeValue())
        return false;

    unsigned long length = attributes() ? attributes()->length() : 0;
    unsigned long length2 = arg->attributes() ? arg->attributes()->length() : 0;
    if(length != length2)
        return false;

    for(unsigned long i = 0; i < length; i++)
    {
        if(!attributes()->item(i)->isEqualNode(arg->attributes()->item(i)))
            return false;
    }

    NodeListImpl *list = childNodes();
    NodeListImpl *list2 = arg->childNodes();
    length = list->length(); length2 = list2->length();

    list->ref(); list2->ref();

    if(length != length2)
        return false;

    for(unsigned long i = 0; i < length; i++)
    {
        if(!list->item(i)->isEqualNode(list2->item(i)))
            return false;
    }
    
    list->deref(); list2->deref();
    return true;
}

DOMStringImpl *NodeImpl::lookupNamespaceURI(DOMStringImpl *_prefix) const
{
    if(!_prefix || _prefix->isEmpty())
        return 0;

    switch(nodeType())
    {
        case ELEMENT_NODE:
        {
            // Note: prefix could be "null" in this case we are looking for default namespace
            DOMStringImpl *ns = namespaceURI();
            if((ns && !ns->isEmpty()) && DOMString(prefix()) == DOMString(_prefix))
                return ns;

            if(hasAttributes())
            {
                unsigned long len = attributes()->length();
                for(unsigned long i = 0; i < len; i++)
                {
                    NodeImpl *node = attributes()->item(i);
                    AttrImpl *attr = static_cast<AttrImpl *>(node);

                    if(DOMString(attr->prefix()) == DOMString("xmlns") &&
                       DOMString(attr->localName()) == DOMString(_prefix))
                    { // non default namespace
                        DOMStringImpl *value = attr->value();
                        if(value && !value->isEmpty())
                            return value;

                        return 0;
                    }
                    else if(DOMString(attr->localName()) == DOMString("xmlns") &&
                            (!_prefix || _prefix->isEmpty()))
                    { // default namespace
                        DOMStringImpl *value = attr->value();
                        if(value && !value->isEmpty())
                            return value;

                        return 0;
                    }
                }
            }

            // EntityReferences may have to be skipped to get to it 
            if(parentNode())
                return parentNode()->lookupNamespaceURI(_prefix);

            return 0; 
        }
        case DOCUMENT_NODE: 
            return static_cast<const DocumentImpl *>(this)->documentElement()->lookupNamespaceURI(_prefix);
        case ENTITY_NODE: 
        case NOTATION_NODE: 
        case DOCUMENT_TYPE_NODE: 
        case DOCUMENT_FRAGMENT_NODE: 
            return 0;
        case ENTITY_REFERENCE_NODE:
            return parentNode()->lookupNamespaceURI(_prefix);
        case ATTRIBUTE_NODE: 
        {
            const AttrImpl *attrPtr = static_cast<const AttrImpl *>(this);

            if(attrPtr->ownerElement())
                return attrPtr->ownerElement()->lookupNamespaceURI(_prefix);

            return 0;
        }
        default: 
            // EntityReferences may have to be skipped to get to it 
            if(parentNode())
                return parentNode()->lookupNamespaceURI(_prefix);
    }

    return 0;
}

DOMStringImpl *NodeImpl::lookupNamespacePrefix(DOMStringImpl *_namespaceURI, const ElementImpl *originalElement) const
{ 
    DOMString ns(_namespaceURI);

    if(!ns.isEmpty() && DOMString(namespaceURI()) == ns && (prefix() && !prefix()->isEmpty()) &&
       DOMString(originalElement->lookupNamespaceURI(prefix())) == ns)
    {
        return prefix();
    }

    if(hasAttributes())
    {
        unsigned long len = attributes()->length();
        for(unsigned long i = 0;i < len;i++)
        {
            NodeImpl *node = attributes()->item(i);
            AttrImpl *attr = static_cast<AttrImpl *>(node);

            DOMStringImpl *local = attr->localName();
            if(DOMString(attr->prefix()) == DOMString("xmlns") && DOMString(attr->value()) == ns &&
               DOMString(originalElement->lookupNamespaceURI(local)) == ns)
            {
                return local;
            }
        }
    }

    // EntityReferences may have to be skipped to get to it 
    if(parentNode())
        return parentNode()->lookupNamespacePrefix(_namespaceURI, originalElement);

    return 0;
}

DOMStringImpl *NodeImpl::lookupPrefix(DOMStringImpl *_namespaceURI) const
{
    if(!_namespaceURI || _namespaceURI->isEmpty())
        return 0;

    switch(nodeType())
    {
        case ELEMENT_NODE:
            return lookupNamespacePrefix(_namespaceURI, static_cast<const ElementImpl *>(this));
        case DOCUMENT_NODE:
            return static_cast<const DocumentImpl *>(this)->documentElement()->lookupPrefix(_namespaceURI); 
        case ENTITY_NODE: 
        case NOTATION_NODE: 
        case DOCUMENT_FRAGMENT_NODE: 
        case DOCUMENT_TYPE_NODE: 
            return 0;
        case ATTRIBUTE_NODE: 
        {
            const AttrImpl *attrPtr = static_cast<const AttrImpl *>(this);

            if(attrPtr->ownerElement())
                return attrPtr->ownerElement()->lookupPrefix(_namespaceURI);

            return 0;
        }
        default: 
        {
            // EntityReferences may have to be skipped to get to it 
            if(parentNode())
                return parentNode()->lookupPrefix(_namespaceURI);
        }
    }

    return 0;
}

void NodeImpl::dispatchSubtreeModifiedEvent()
{
    childrenChanged();
    if(!ownerDocument() || !ownerDocument()->hasListenerType(DOMSUBTREEMODIFIED_EVENT))
        return;

    DOMStringImpl *eventType = new DOMStringImpl("MutationEvents");
    eventType->ref();

    MutationEventImpl *event = static_cast<MutationEventImpl *>(ownerDocument()->createEvent(eventType));
    eventType->deref();

    event->ref();

    event->initMutationEvent(new DOMStringImpl("DOMSubtreeModified"), true, false, 0, 0, 0, 0, 0);
    dispatchEvent(event);

    event->deref();
}

void NodeImpl::dispatchChildRemovalEvents(NodeImpl *child)
{
    if(!ownerDocument() || !ownerDocument()->hasListenerType(DOMNODEREMOVED_EVENT))
        return;

    DOMStringImpl *eventType = new DOMStringImpl("MutationEvents");
    eventType->ref();

    MutationEventImpl *event = static_cast<MutationEventImpl *>(ownerDocument()->createEvent(eventType));
    event->ref();

    event->initMutationEvent(new DOMStringImpl("DOMNodeRemoved"), true, false, this, 0, 0, 0, 0);
    dispatchEvent(event);

    event->deref();

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if(ownerDocument()->hasListenerType(DOMNODEREMOVEDFROMDOCUMENT_EVENT))
    {
        MutationEventImpl *event = static_cast<MutationEventImpl *>(ownerDocument()->createEvent(eventType));
        event->ref();

        event->initMutationEvent(new DOMStringImpl("DOMNodeRemovedFromDocument"), false, false, 0, 0, 0, 0, 0);
        child->dispatchEvent(event);
        dispatchEventToSubTree(child, event);

        event->deref();
    }

    eventType->deref();
}

void NodeImpl::dispatchChildInsertedEvents(NodeImpl *child)
{
    if(!ownerDocument() || !ownerDocument()->hasListenerType(DOMNODEINSERTED_EVENT))
        return;

    DOMStringImpl *eventType = new DOMStringImpl("MutationEvents");
    eventType->ref();

    MutationEventImpl *event = static_cast<MutationEventImpl *>(ownerDocument()->createEvent(eventType));
    event->ref();

    event->initMutationEvent(new DOMStringImpl("DOMNodeInserted"), true, false, this, 0, 0, 0, 0);
    child->dispatchEvent(event);

    event->deref();

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if(ownerDocument()->hasListenerType(DOMNODEINSERTEDINTODOCUMENT_EVENT))
    {
        MutationEventImpl *event = static_cast<MutationEventImpl *>(ownerDocument()->createEvent(eventType));
        event->ref();

        event->initMutationEvent(new DOMStringImpl("DOMNodeInsertedIntoDocument"), false, false, 0, 0, 0, 0, 0);
        child->dispatchEvent(event);
        dispatchEventToSubTree(child, event);

        event->deref();
    }

    eventType->deref();
}

void NodeImpl::dispatchEventToSubTree(NodeImpl *node, EventImpl *event)
{
    NodeImpl *child = node->firstChild();
    while(child != 0)
    {
        child->dispatchEvent(event);
        dispatchEventToSubTree(child, event);

        child = child->nextSibling();
    }
}

NodeImpl *NodeImpl::traverseNextNode(NodeImpl *stayWithin) const
{
    if(firstChild() || stayWithin == this)
        return firstChild();
    else if(nextSibling())
        return nextSibling();
    else
    {
        const NodeImpl *n = this;
        while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
            n = n->parentNode();

        if(n)
            return n->nextSibling();
    }

    return 0;
}

void NodeImpl::close()
{
    m_closed = true;
}

NodeImpl::StyleChange NodeImpl::diff(RenderStyle *s1, RenderStyle *s2) const
{
    StyleChange ch = NoInherit;
    if(!s1 || !s2)
        ch = Inherit;
    else if(s1->equals(s2))
        ch = NoChange;
    else if(s1->inheritedNotEqual(s2))
        ch = Inherit;

    return ch;
}

void NodeImpl::setChanged(bool b, bool deep)
{
    if(b && !attached()) // changed compared to what?
        return;

    m_changed = b;
    if(deep)
        for(NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
            n->setChanged(b, deep);
    else if(b)
    {
        NodeImpl *p = parentNode();
        while(p)
        {
            p->setHasChangedChild(true);
            p = p->parentNode();
        }
        //ownerDocument()->setDocumentChanged();
    }
}

unsigned long NodeImpl::nodeIndex() const
{
    NodeImpl *_tempNode = previousSibling();
    unsigned long count = 0;
    for(count = 0; _tempNode; count++)
        _tempNode = _tempNode->previousSibling();

    return count;
}

DOMStringImpl *NodeImpl::baseURI() const
{
    return new DOMStringImpl(baseKURI().url());
}

KURL NodeImpl::baseKURI() const
{
    QString myBase;

    switch(nodeType())
    {
        case ELEMENT_NODE:
        {
            const ElementImpl *me = static_cast<const ElementImpl *>(this);
            DOMString mine(me->getAttribute(new DOMStringImpl("xml:base")));
            myBase = mine.string();
#ifndef APPLE_COMPILE_HACK
            if(!KURL::isRelativeURL(myBase))
                return KURL(mine.string());
#endif
            break;
        }
        case DOCUMENT_TYPE_NODE:
            return KURL();
        case DOCUMENT_NODE:
            return static_cast<const DocumentImpl *>(this)->documentKURI();
        default:
            /* ATTRIBUTE_NODE, TEXT_NODE, CDATA_SECTION_NODE, ENTITY_REFERENCE_NODE,
             * ENTITY_NODE, PROCESSING_INSTRUCTION_NODE, COMMENT_NODE, DOCUMENT_TYPE_NODE,
             * DOCUMENT_FRAGMENT_NODE, and NOTATION_NODE. :) */
            /* Their base will be the parent's base */
            break;
    }

    NodeImpl *parent = parentNode();
    KURL parentBase;

    if(parent)
        parentBase = parent->baseKURI();

    return KURL(parentBase, myBase);
}

unsigned short NodeImpl::compareDocumentPosition(NodeImpl *other) const
{
    if(other == this)
        return 0;

    if(other->nodeType() == ENTITY_NODE || other->nodeType() == NOTATION_NODE ||
       nodeType() == ENTITY_NODE || nodeType() == NOTATION_NODE)
        return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC;

    Q3PtrList<NodeImpl> containers;
    NodeImpl *container = const_cast<NodeImpl *>(this);
    while(container)
    {
        containers.append(container);
        if(container->nodeType() == ATTRIBUTE_NODE)
            container = static_cast<AttrImpl *>(container)->ownerElement();
        else
            container = container->parentNode();
    }
    container = other;
    NodeImpl *other_determining = other;
    NodeImpl *self_determining = 0;
    while(container)
    {
        if(containers.containsRef(container))
        {
            int index = containers.findRef(container);
            if(index < 1) index = 1;
                self_determining = containers.at(index - 1);
            break;
        }
        other_determining = container;
        if(container->nodeType() == ATTRIBUTE_NODE)
            container = static_cast<AttrImpl *>(container)->ownerElement();
        else
            container = container->parentNode();
    }
    if(!self_determining)
    {
        if(other > this)
            return DOCUMENT_POSITION_DISCONNECTED |
                   DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC |
                   DOCUMENT_POSITION_FOLLOWING;

        return DOCUMENT_POSITION_DISCONNECTED |
               DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC |
               DOCUMENT_POSITION_PRECEDING;
    }
    if(container == other)
        return DOCUMENT_POSITION_CONTAINS | DOCUMENT_POSITION_PRECEDING;

    if(container == this)
        return DOCUMENT_POSITION_CONTAINED_BY | DOCUMENT_POSITION_FOLLOWING;

    int index1 = container->childNodes()->index(other_determining);
    if(index1 > -1)
    {
        int index2 = container->childNodes()->index(self_determining);
        if(index2 > -1)
        {
            if(index1 > index2)
                return DOCUMENT_POSITION_FOLLOWING;

            return DOCUMENT_POSITION_PRECEDING;
        }

        return DOCUMENT_POSITION_FOLLOWING;
    }
    if(container->childNodes()->index(self_determining) > -1)
        return DOCUMENT_POSITION_PRECEDING;
    if(other_determining->nodeType() != self_determining->nodeType())
    {
        if(other_determining->nodeType() > self_determining->nodeType())
            return DOCUMENT_POSITION_FOLLOWING;

        return DOCUMENT_POSITION_PRECEDING;
    }

    if(self_determining->nodeType() == ATTRIBUTE_NODE)
    {
        NamedAttrMapImpl *attrs = container->attributes();
        if(attrs->index(other_determining) > attrs->index(other_determining))
            return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_FOLLOWING;

        return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_PRECEDING;
    }

    if(other_determining > self_determining)
        return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_FOLLOWING;

    return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_PRECEDING;
}

// vim:ts=4:noet

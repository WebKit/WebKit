/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
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
#include "Element.h"

#include "cssstyleselector.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "NamedAttrMap.h"
#include "RenderBlock.h"
#include "KWQTextStream.h"

namespace WebCore {

using namespace HTMLNames;

Element::Element(const QualifiedName& qName, Document *doc)
    : ContainerNode(doc), m_tagName(qName)
{
}

Element::~Element()
{
    if (namedAttrMap)
        namedAttrMap->detachFromElement();
}

PassRefPtr<Node> Element::cloneNode(bool deep)
{
    ExceptionCode ec = 0;
    RefPtr<Element> clone = document()->createElementNS(namespaceURI(), nodeName(), ec);
    assert(!ec);
    
    // clone attributes
    if (namedAttrMap)
        *clone->attributes() = *namedAttrMap;

    clone->copyNonAttributeProperties(this);
    
    if (deep)
        cloneChildNodes(clone.get());

    return clone.release();
}

void Element::removeAttribute(const QualifiedName& name, ExceptionCode& ec)
{
    if (namedAttrMap) {
        namedAttrMap->removeNamedItem(name, ec);
        if (ec == NOT_FOUND_ERR)
            ec = 0;
    }
}

void Element::setAttribute(const QualifiedName& name, const String &value)
{
    ExceptionCode ec = 0;
    setAttribute(name, value.impl(), ec);
}

// Virtual function, defined in base class.
NamedAttrMap *Element::attributes() const
{
    return attributes(false);
}

NamedAttrMap* Element::attributes(bool readonly) const
{
    updateStyleAttributeIfNeeded();
    if (!readonly && !namedAttrMap)
        createAttributeMap();
    return namedAttrMap.get();
}

Node::NodeType Element::nodeType() const
{
    return ELEMENT_NODE;
}

const AtomicStringList* Element::getClassList() const
{
    return 0;
}

const AtomicString& Element::getIDAttribute() const
{
    return namedAttrMap ? namedAttrMap->id() : nullAtom;
}

bool Element::hasAttribute(const QualifiedName& name) const
{
    return hasAttributeNS(name.namespaceURI(), name.localName());
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const
{
    if (name == styleAttr)
        updateStyleAttributeIfNeeded();

    if (namedAttrMap)
        if (Attribute* a = namedAttrMap->getAttributeItem(name))
            return a->value();

    return nullAtom;
}

void Element::scrollIntoView(bool alignToTop) 
{
    document()->updateLayoutIgnorePendingStylesheets();
    IntRect bounds = getRect();    
    if (renderer()) {
        // Align to the top / bottom and to the closest edge.
        if (alignToTop)
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignTopAlways);
        else
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignBottomAlways);
    }
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document()->updateLayoutIgnorePendingStylesheets();
    IntRect bounds = getRect();    
    if (renderer()) {
        if (centerIfNeeded)
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, RenderLayer::gAlignCenterIfNeeded, RenderLayer::gAlignCenterIfNeeded);
        else
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignToEdgeIfNeeded);
    }
}

void Element::scrollByUnits(int units, KWQScrollGranularity granularity)
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject *rend = renderer()) {
        if (rend->hasOverflowClip()) {
            KWQScrollDirection direction = KWQScrollDown;
            if (units < 0) {
                direction = KWQScrollUp;
                units = -units;
            }
            rend->layer()->scroll(direction, granularity, units);
        }
    }
}

void Element::scrollByLines(int lines)
{
    scrollByUnits(lines, KWQScrollLine);
}

void Element::scrollByPages(int pages)
{
    scrollByUnits(pages, KWQScrollPage);
}

int Element::offsetLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->offsetLeft();
    return 0;
}

int Element::offsetTop()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->offsetTop();
    return 0;
}

int Element::offsetWidth()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->offsetWidth();
    return 0;
}

int Element::offsetHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->offsetHeight();
    return 0;
}

Element* Element::offsetParent()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        if (RenderObject* offsetParent = rend->offsetParent())
            return static_cast<Element*>(offsetParent->element());
    return 0;
}

int Element::clientWidth()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->clientWidth();
    return 0;
}

int Element::clientHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->clientHeight();
    return 0;
}

int Element::scrollLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();
    RenderObject* rend = renderer();
    if (rend && rend->layer())
        return rend->layer()->scrollXOffset();
    return 0;
}

int Element::scrollTop()
{
    document()->updateLayoutIgnorePendingStylesheets();
    RenderObject* rend = renderer();
    if (rend && rend->layer())
        return rend->layer()->scrollYOffset();
    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    document()->updateLayoutIgnorePendingStylesheets();
    RenderObject *rend = renderer();
    if (rend && rend->hasOverflowClip())
        rend->layer()->scrollToXOffset(newLeft);
}

void Element::setScrollTop(int newTop)
{
    document()->updateLayoutIgnorePendingStylesheets();
    RenderObject *rend = renderer();
    if (rend && rend->hasOverflowClip())
        rend->layer()->scrollToYOffset(newTop);
}

int Element::scrollWidth()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->scrollWidth();
    return 0;
}

int Element::scrollHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->scrollHeight();
    return 0;
}

static inline bool inHTMLDocument(const Element* e)
{
    return e && e->document()->isHTMLDocument();
}

const AtomicString& Element::getAttribute(const String& name) const
{
    String localName = inHTMLDocument(this) ? name.lower() : name;
    if (localName == styleAttr.localName())
        updateStyleAttributeIfNeeded();
    
    if (namedAttrMap)
        if (Attribute* a = namedAttrMap->getAttributeItem(localName))
            return a->value();
    
    return nullAtom;
}

const AtomicString& Element::getAttributeNS(const String& namespaceURI, const String& localName) const
{
    return getAttribute(QualifiedName(nullAtom, localName.impl(), namespaceURI.impl()));
}

void Element::setAttribute(const String& name, const String& value, ExceptionCode& ec)
{
    if (!Document::isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }

    // allocate attributemap if necessary
    Attribute* old = attributes(false)->getAttributeItem(name);

    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (namedAttrMap->isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    
    if (inDocument())
        document()->incDOMTreeVersion();

    String localName = inHTMLDocument(this) ? name.lower() : name;

    if (localName == idAttr.localName())
        updateId(old ? old->value() : nullAtom, value);
    
    if (old && value.isNull())
        namedAttrMap->removeAttribute(old->name());
    else if (!old && !value.isNull())
        namedAttrMap->addAttribute(createAttribute(QualifiedName(nullAtom, localName.impl(), nullAtom), value.impl()));
    else if (old && !value.isNull()) {
        old->setValue(value);
        attributeChanged(old);
    }
}

void Element::setAttribute(const QualifiedName& name, StringImpl* value, ExceptionCode& ec)
{
    if (inDocument())
        document()->incDOMTreeVersion();

    // allocate attributemap if necessary
    Attribute* old = attributes(false)->getAttributeItem(name);

    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (namedAttrMap->isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    if (name == idAttr)
        updateId(old ? old->value() : nullAtom, value);
    
    if (old && !value)
        namedAttrMap->removeAttribute(name);
    else if (!old && value)
        namedAttrMap->addAttribute(createAttribute(name, value));
    else if (old && value) {
        old->setValue(value);
        attributeChanged(old);
    }
}

Attribute* Element::createAttribute(const QualifiedName& name, StringImpl* value)
{
    return new Attribute(name, value);
}

void Element::setAttributeMap(NamedAttrMap* list)
{
    if (inDocument())
        document()->incDOMTreeVersion();

    // If setting the whole map changes the id attribute, we need to call updateId.

    Attribute *oldId = namedAttrMap ? namedAttrMap->getAttributeItem(idAttr) : 0;
    Attribute *newId = list ? list->getAttributeItem(idAttr) : 0;

    if (oldId || newId)
        updateId(oldId ? oldId->value() : nullAtom, newId ? newId->value() : nullAtom);

    if (namedAttrMap)
        namedAttrMap->element = 0;

    namedAttrMap = list;

    if (namedAttrMap) {
        namedAttrMap->element = this;
        unsigned len = namedAttrMap->length();
        for (unsigned i = 0; i < len; i++)
            attributeChanged(namedAttrMap->attrs[i]);
        // FIXME: What about attributes that were in the old map that are not in the new map?
    }
}

bool Element::hasAttributes() const
{
    updateStyleAttributeIfNeeded();
    return namedAttrMap && namedAttrMap->length() > 0;
}

String Element::nodeName() const
{
    return m_tagName.toString();
}

void Element::setPrefix(const AtomicString &_prefix, ExceptionCode& ec)
{
    checkSetPrefix(_prefix, ec);
    if (ec)
        return;

    m_tagName.setPrefix(_prefix);
}

Node* Element::insertAdjacentElement(const String& where, Node* newChild, int& exception)
{
    if (!newChild) {
        // IE throws COM Exception E_INVALIDARG; this is the best DOM exception alternative
        exception = TYPE_MISMATCH_ERR;
        return 0;
    }
    
    // In Internet Explorer if the element has no parent and where is "beforeBegin" or "afterEnd",
    // a document fragment is created and the elements appended in the correct order. This document
    // fragment isn't returned anywhere.
    //
    // This is impossible for us to implement as the DOM tree does not allow for such structures,
    // Opera also appears to disallow such usage.
    
    if (equalIgnoringCase(where, "beforeBegin")) {
        if (Node* p = parent())
            return p->insertBefore(newChild, this, exception) ? newChild : 0;
    } else if (equalIgnoringCase(where, "afterBegin")) {
        return insertBefore(newChild, firstChild(), exception) ? newChild : 0;
    } else if (equalIgnoringCase(where, "beforeEnd")) {
        return appendChild(newChild, exception) ? newChild : 0;
    } else if (equalIgnoringCase(where, "afterEnd")) {
        if (Node* p = parent())
            return p->insertBefore(newChild, nextSibling(), exception) ? newChild : 0;
    } else {
        // IE throws COM Exception E_INVALIDARG; this is the best DOM exception alternative
        exception = NOT_SUPPORTED_ERR;
    }
    return 0;
}

bool Element::contains(const Element* element) const
{
    if (!element)
        return false;
    return this == element || element->isAncestor(this);
}

void Element::createAttributeMap() const
{
    namedAttrMap = new NamedAttrMap(const_cast<Element*>(this));
}

bool Element::isURLAttribute(Attribute *attr) const
{
    return false;
}

RenderStyle *Element::styleForRenderer(RenderObject *parentRenderer)
{
    return document()->styleSelector()->styleForElement(this);
}

RenderObject *Element::createRenderer(RenderArena *arena, RenderStyle *style)
{
    if (document()->documentElement() == this && style->display() == NONE) {
        // Ignore display: none on root elements.  Force a display of block in that case.
        RenderBlock* result = new (arena) RenderBlock(this);
        if (result) result->setStyle(style);
        return result;
    }
    return RenderObject::createObject(this, style);
}


void Element::insertedIntoDocument()
{
    // need to do superclass processing first so inDocument() is true
    // by the time we reach updateId
    ContainerNode::insertedIntoDocument();

    if (hasID()) {
        NamedAttrMap* attrs = attributes(true);
        if (attrs) {
            Attribute* idItem = attrs->getAttributeItem(idAttr);
            if (idItem && !idItem->isNull())
                updateId(nullAtom, idItem->value());
        }
    }
}

void Element::removedFromDocument()
{
    if (hasID()) {
        NamedAttrMap* attrs = attributes(true);
        if (attrs) {
            Attribute* idItem = attrs->getAttributeItem(idAttr);
            if (idItem && !idItem->isNull())
                updateId(idItem->value(), nullAtom);
        }
    }

    ContainerNode::removedFromDocument();
}

void Element::attach()
{
#if SPEED_DEBUG < 1
    createRendererIfNeeded();
#endif
    ContainerNode::attach();
}

void Element::recalcStyle( StyleChange change )
{
    // ### should go away and be done in renderobject
    RenderStyle* _style = renderer() ? renderer()->style() : 0;
    bool hasParentRenderer = parent() ? parent()->renderer() : false;
    
    if ( hasParentRenderer && (change >= Inherit || changed()) ) {
        RenderStyle *newStyle = document()->styleSelector()->styleForElement(this);
        StyleChange ch = diff( _style, newStyle );
        if (ch == Detach) {
            if (attached())
                detach();
            // ### Suboptimal. Style gets calculated again.
            attach();
            // attach recalulates the style for all children. No need to do it twice.
            setChanged( false );
            setHasChangedChild( false );
            newStyle->deref(document()->renderArena());
            return;
        }
        else if (ch != NoChange) {
            if (renderer() && newStyle)
                renderer()->setStyle(newStyle);
        }
        else if (changed() && renderer() && newStyle && (document()->usesSiblingRules() || document()->usesDescendantRules())) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.  This is only necessary if the document actually uses
            // sibling/descendant rules, since otherwise it isn't possible for ancestor styles to affect sharing of
            // descendants.
            renderer()->setStyleInternal(newStyle);
        }

        newStyle->deref(document()->renderArena());

        if ( change != Force) {
            if (document()->usesDescendantRules())
                change = Force;
            else
                change = ch;
        }
    }

    for (Node *n = fastFirstChild(); n; n = n->nextSibling()) {
        if (change >= Inherit || n->isTextNode() || n->hasChangedChild() || n->changed())
            n->recalcStyle(change);
    }

    setChanged( false );
    setHasChangedChild( false );
}

bool Element::childTypeAllowed(NodeType type)
{
    switch (type) {
        case ELEMENT_NODE:
        case TEXT_NODE:
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        case CDATA_SECTION_NODE:
        case ENTITY_REFERENCE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

void Element::dispatchAttrRemovalEvent(Attribute*)
{
    assert(!eventDispatchForbidden());
#if 0
    if (!document()->hasListenerType(Document::DOMATTRMODIFIED_LISTENER))
        return;
    ExceptionCode ec = 0;
    dispatchEvent(new MutationEvent(DOMAttrModifiedEvent, true, false, attr, attr->value(),
        attr->value(), document()->attrName(attr->id()), MutationEvent::REMOVAL), ec);
#endif
}

void Element::dispatchAttrAdditionEvent(Attribute *attr)
{
    assert(!eventDispatchForbidden());
#if 0
    if (!document()->hasListenerType(Document::DOMATTRMODIFIED_LISTENER))
        return;
    ExceptionCode ec = 0;
    dispatchEvent(new MutationEvent(DOMAttrModifiedEvent, true, false, attr, attr->value(),
        attr->value(),document()->attrName(attr->id()), MutationEvent::ADDITION), ec);
#endif
}

String Element::openTagStartToString() const
{
    String result = "<" + nodeName();

    NamedAttrMap *attrMap = attributes(true);

    if (attrMap) {
        unsigned numAttrs = attrMap->length();
        for (unsigned i = 0; i < numAttrs; i++) {
            result += " ";

            Attribute *attribute = attrMap->attributeItem(i);
            result += attribute->name().toString();
            if (!attribute->value().isNull()) {
                result += "=\"";
                // FIXME: substitute entities for any instances of " or '
                result += attribute->value();
                result += "\"";
            }
        }
    }

    return result;
}

String Element::toString() const
{
    String result = openTagStartToString();

    if (hasChildNodes()) {
        result += ">";

        for (Node *child = firstChild(); child != NULL; child = child->nextSibling()) {
            result += child->toString();
        }

        result += "</";
        result += nodeName();
        result += ">";
    } else {
        result += " />";
    }

    return result;
}

void Element::updateId(const AtomicString& oldId, const AtomicString& newId)
{
    if (!inDocument())
        return;

    if (oldId == newId)
        return;

    Document* doc = document();
    if (!oldId.isEmpty())
        doc->removeElementById(oldId, this);
    if (!newId.isEmpty())
        doc->addElementById(newId, this);
}

#ifndef NDEBUG
void Element::dump(QTextStream *stream, DeprecatedString ind) const
{
    updateStyleAttributeIfNeeded();
    if (namedAttrMap) {
        for (unsigned i = 0; i < namedAttrMap->length(); i++) {
            Attribute *attr = namedAttrMap->attributeItem(i);
            *stream << " " << attr->name().localName().deprecatedString().ascii()
                    << "=\"" << attr->value().deprecatedString().ascii() << "\"";
        }
    }

    ContainerNode::dump(stream,ind);
}
#endif

#ifndef NDEBUG
void Element::formatForDebugger(char *buffer, unsigned length) const
{
    String result;
    String s;
    
    s = nodeName();
    if (s.length() > 0) {
        result += s;
    }
          
    s = getAttribute(idAttr);
    if (s.length() > 0) {
        if (result.length() > 0)
            result += "; ";
        result += "id=";
        result += s;
    }
          
    s = getAttribute(classAttr);
    if (s.length() > 0) {
        if (result.length() > 0)
            result += "; ";
        result += "class=";
        result += s;
    }
          
    strncpy(buffer, result.deprecatedString().latin1(), length - 1);
}
#endif

PassRefPtr<Attr> Element::setAttributeNode(Attr *attr, ExceptionCode& ec)
{
    ASSERT(attr);
    return static_pointer_cast<Attr>(attributes(false)->setNamedItem(attr, ec));
}

PassRefPtr<Attr> Element::removeAttributeNode(Attr *attr, ExceptionCode& ec)
{
    if (!attr || attr->ownerElement() != this) {
        ec = NOT_FOUND_ERR;
        return 0;
    }
    if (document() != attr->document()) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    NamedAttrMap *attrs = attributes(true);
    if (!attrs)
        return 0;

    return static_pointer_cast<Attr>(attrs->removeNamedItem(attr->qualifiedName(), ec));
}

void Element::setAttributeNS(const String& namespaceURI, const String& qualifiedName, const String& value, ExceptionCode& ec)
{
    String prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }
    setAttribute(QualifiedName(prefix.impl(), localName.impl(), namespaceURI.impl()), value.impl(), ec);
}

void Element::removeAttribute(const String& name, ExceptionCode& ec)
{
    String localName = inHTMLDocument(this) ? name.lower() : name;

    if (namedAttrMap) {
        namedAttrMap->removeNamedItem(localName, ec);
        if (ec == NOT_FOUND_ERR)
            ec = 0;
    }
}

void Element::removeAttributeNS(const String& namespaceURI, const String& localName, ExceptionCode& ec)
{
    removeAttribute(QualifiedName(nullAtom, localName.impl(), namespaceURI.impl()), ec);
}

PassRefPtr<Attr> Element::getAttributeNode(const String& name)
{
    NamedAttrMap *attrs = attributes(true);
    if (!attrs)
        return 0;
    String localName = inHTMLDocument(this) ? name.lower() : name;
    return static_pointer_cast<Attr>(attrs->getNamedItem(localName));
}

PassRefPtr<Attr> Element::getAttributeNodeNS(const String& namespaceURI, const String& localName)
{
    NamedAttrMap *attrs = attributes(true);
    if (!attrs)
        return 0;
    return static_pointer_cast<Attr>(attrs->getNamedItem(QualifiedName(nullAtom, localName.impl(), namespaceURI.impl())));
}

bool Element::hasAttribute(const String& name) const
{
    NamedAttrMap *attrs = attributes(true);
    if (!attrs)
        return false;
    String localName = inHTMLDocument(this) ? name.lower() : name;
    return attrs->getAttributeItem(localName);
}

bool Element::hasAttributeNS(const String& namespaceURI, const String& localName) const
{
    NamedAttrMap *attrs = attributes(true);
    if (!attrs)
        return false;
    return attrs->getAttributeItem(QualifiedName(nullAtom, localName.impl(), namespaceURI.impl()));
}

CSSStyleDeclaration *Element::style()
{
    return 0;
}

void Element::focus()
{
    Document* doc = document();
    doc->updateLayout();
    if (isFocusable()) {
        doc->setFocusNode(this);
        if (rootEditableElement() == this) {
            // FIXME: we should restore the previous selection if there is one, instead of always selecting all.
            if (doc->frame()->selectContentsOfNode(this))
                doc->frame()->revealSelection();
        } else if (renderer() && !renderer()->isWidget())
            renderer()->enclosingLayer()->scrollRectToVisible(getRect());
    }
}

void Element::blur()
{
    Document* doc = document();
    if (doc->focusNode() == this)
        doc->setFocusNode(0);
}

}

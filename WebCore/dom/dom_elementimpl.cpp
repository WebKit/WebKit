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
#include "dom_elementimpl.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "Text.h"
#include "css_stylesheetimpl.h"
#include "css_valueimpl.h"
#include "cssstyleselector.h"
#include "CSSValueKeywords.h"
#include "dom2_eventsimpl.h"
#include "dom_xmlimpl.h"
#include "HTMLNames.h"
#include "HTMLParser.h"
#include "RenderCanvas.h"
#include <qtextstream.h>

namespace WebCore {

using namespace HTMLNames;

struct MappedAttributeKey {
    uint16_t type;
    StringImpl* name;
    StringImpl* value;
    MappedAttributeKey(MappedAttributeEntry t, StringImpl* n, StringImpl* v)
        : type(t), name(n), value(v) { }
};

static inline bool operator==(const MappedAttributeKey& a, const MappedAttributeKey& b)
    { return a.type == b.type && a.name == b.name && a.value == b.value; } 

struct MappedAttributeKeyTraits {
    typedef MappedAttributeKey TraitType;
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;
    static MappedAttributeKey emptyValue() { return MappedAttributeKey(eNone, 0, 0); }
    static MappedAttributeKey deletedValue() { return MappedAttributeKey(eLastEntry, 0, 0); }
};

struct MappedAttributeHash {
    static unsigned hash(const MappedAttributeKey&);
    static bool equal(const MappedAttributeKey& a, const MappedAttributeKey& b) { return a == b; }
};

typedef HashMap<MappedAttributeKey, CSSMappedAttributeDeclaration*, MappedAttributeHash, MappedAttributeKeyTraits> MappedAttributeDecls;

static MappedAttributeDecls* mappedAttributeDecls = 0;

Attribute* Attribute::clone(bool) const
{
    return new Attribute(m_name, m_value);
}

PassRefPtr<Attr> Attribute::createAttrIfNeeded(Element* e)
{
    RefPtr<Attr> r(m_impl);
    if (!r) {
        r = new Attr(e, e->getDocument(), this);
        r->createTextChild();
    }
    return r.release();
}

Attr::Attr(Element* element, Document* docPtr, Attribute* a)
    : ContainerNode(docPtr),
      m_element(element),
      m_attribute(a),
      m_ignoreChildrenChanged(0)
{
    assert(!m_attribute->m_impl);
    m_attribute->m_impl = this;
    m_specified = true;
}

Attr::~Attr()
{
    assert(m_attribute->m_impl == this);
    m_attribute->m_impl = 0;
}

void Attr::createTextChild()
{
    assert(refCount());
    if (!m_attribute->value().isEmpty()) {
        ExceptionCode ec = 0;
        m_ignoreChildrenChanged++;
        appendChild(getDocument()->createTextNode(m_attribute->value().impl()), ec);
        m_ignoreChildrenChanged--;
    }
}

String Attr::nodeName() const
{
    return name();
}

Node::NodeType Attr::nodeType() const
{
    return ATTRIBUTE_NODE;
}

const AtomicString& Attr::localName() const
{
    return m_attribute->localName();
}

const AtomicString& Attr::namespaceURI() const
{
    return m_attribute->namespaceURI();
}

const AtomicString& Attr::prefix() const
{
    return m_attribute->prefix();
}

void Attr::setPrefix(const AtomicString &_prefix, ExceptionCode& ec)
{
    checkSetPrefix(_prefix, ec);
    if (ec)
        return;

    m_attribute->setPrefix(_prefix);
}

String Attr::nodeValue() const
{
    return value();
}

void Attr::setValue( const String& v, ExceptionCode& ec)
{
    ec = 0;

    // do not interprete entities in the string, its literal!

    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // ### what to do on 0 ?
    if (v.isNull()) {
        ec = DOMSTRING_SIZE_ERR;
        return;
    }

    int e = 0;
    m_ignoreChildrenChanged++;
    removeChildren();
    appendChild(getDocument()->createTextNode(v.impl()), e);
    m_ignoreChildrenChanged--;
    
    m_attribute->setValue(v.impl());
    if (m_element)
        m_element->attributeChanged(m_attribute.get());
}

void Attr::setNodeValue(const String& v, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setValue()
    setValue(v, ec);
}

PassRefPtr<Node> Attr::cloneNode(bool /*deep*/)
{
    RefPtr<Attr> clone = new Attr(0, getDocument(), m_attribute->clone());
    cloneChildNodes(clone.get());
    return clone.release();
}

// DOM Section 1.1.1
bool Attr::childTypeAllowed(NodeType type)
{
    switch (type) {
        case TEXT_NODE:
        case ENTITY_REFERENCE_NODE:
            return true;
        default:
            return false;
    }
}

void Attr::childrenChanged()
{
    Node::childrenChanged();
    
    if (m_ignoreChildrenChanged > 0)
        return;
    
    // FIXME: We should include entity references in the value
    
    String val = "";
    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            val += static_cast<Text *>(n)->data();
    }
    
    m_attribute->setValue(val.impl());
    if (m_element)
        m_element->attributeChanged(m_attribute.get());
}

String Attr::toString() const
{
    String result;

    result += nodeName();

    // FIXME: substitute entities for any instances of " or ' --
    // maybe easier to just use text value and ignore existing
    // entity refs?

    if (firstChild() != NULL) {
        result += "=\"";

        for (Node *child = firstChild(); child != NULL; child = child->nextSibling()) {
            result += child->toString();
        }
        
        result += "\"";
    }

    return result;
}

// -------------------------------------------------------------------------

#ifndef NDEBUG
struct ElementImplCounter 
{ 
    static int count; 
    ~ElementImplCounter() { /* if (count != 0) fprintf(stderr, "LEAK: %d Element\n", count); */ } 
};
int ElementImplCounter::count;
static ElementImplCounter elementImplCounter;
#endif NDEBUG

Element::Element(const QualifiedName& qName, Document *doc)
    : ContainerNode(doc), m_tagName(qName)
{
#ifndef NDEBUG
    ++ElementImplCounter::count;
#endif
}

Element::~Element()
{
#ifndef NDEBUG
    --ElementImplCounter::count;
#endif
    if (namedAttrMap)
        namedAttrMap->detachFromElement();
}

PassRefPtr<Node> Element::cloneNode(bool deep)
{
    ExceptionCode ec = 0;
    RefPtr<Element> clone = getDocument()->createElementNS(namespaceURI(), nodeName(), ec);
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
    IntRect bounds = this->getRect();    
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
    IntRect bounds = this->getRect();    
    if (renderer()) {
        if (centerIfNeeded)
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, RenderLayer::gAlignCenterIfNeeded, RenderLayer::gAlignCenterIfNeeded);
        else
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignToEdgeIfNeeded);
    }
}

static inline bool inHTMLDocument(const Element* e)
{
    return e && e->getDocument()->isHTMLDocument();
}

const AtomicString& Element::getAttribute(const String& name) const
{
    String localName = inHTMLDocument(this) ? name.lower() : name;
    return getAttribute(QualifiedName(nullAtom, localName.impl(), nullAtom));
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
    String localName = inHTMLDocument(this) ? name.lower() : name;
    setAttribute(QualifiedName(nullAtom, localName.impl(), nullAtom), value.impl(), ec);
}

void Element::setAttribute(const QualifiedName& name, StringImpl* value, ExceptionCode& ec)
{
    if (inDocument())
        getDocument()->incDOMTreeVersion();

    // allocate attributemap if necessary
    Attribute* old = attributes(false)->getAttributeItem(name);

    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (namedAttrMap->isReadOnly()) {
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
        getDocument()->incDOMTreeVersion();

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
    return getDocument()->styleSelector()->styleForElement(this);
}

RenderObject *Element::createRenderer(RenderArena *arena, RenderStyle *style)
{
    if (getDocument()->documentElement() == this && style->display() == NONE) {
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
        RenderStyle *newStyle = getDocument()->styleSelector()->styleForElement(this);
        newStyle->ref();
        StyleChange ch = diff( _style, newStyle );
        if (ch == Detach) {
            if (attached()) detach();
            // ### Suboptimal. Style gets calculated again.
            attach();
            // attach recalulates the style for all children. No need to do it twice.
            setChanged( false );
            setHasChangedChild( false );
            newStyle->deref(getDocument()->renderArena());
            return;
        }
        else if (ch != NoChange) {
            if (renderer() && newStyle)
                renderer()->setStyle(newStyle);
        }
        else if (changed() && renderer() && newStyle && (getDocument()->usesSiblingRules() || getDocument()->usesDescendantRules())) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.  This is only necessary if the document actually uses
            // sibling/descendant rules, since otherwise it isn't possible for ancestor styles to affect sharing of
            // descendants.
            renderer()->setStyleInternal(newStyle);
        }

        newStyle->deref(getDocument()->renderArena());

        if ( change != Force) {
            if (getDocument()->usesDescendantRules())
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
    if (!getDocument()->hasListenerType(Document::DOMATTRMODIFIED_LISTENER))
        return;
    ExceptionCode ec = 0;
    dispatchEvent(new MutationEvent(DOMAttrModifiedEvent, true, false, attr, attr->value(),
        attr->value(), getDocument()->attrName(attr->id()), MutationEvent::REMOVAL), ec);
#endif
}

void Element::dispatchAttrAdditionEvent(Attribute *attr)
{
    assert(!eventDispatchForbidden());
#if 0
    if (!getDocument()->hasListenerType(Document::DOMATTRMODIFIED_LISTENER))
        return;
    ExceptionCode ec = 0;
    dispatchEvent(new MutationEvent(DOMAttrModifiedEvent, true, false, attr, attr->value(),
        attr->value(),getDocument()->attrName(attr->id()), MutationEvent::ADDITION), ec);
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

    Document* doc = getDocument();
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
    if (getDocument() != attr->getDocument()) {
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
    removeAttribute(QualifiedName(nullAtom, localName.impl(), nullAtom), ec);
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
    return static_pointer_cast<Attr>(attrs->getNamedItem(QualifiedName(nullAtom, localName.impl(), nullAtom)));
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
    return attrs->getAttributeItem(QualifiedName(nullAtom, localName.impl(), nullAtom));
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
    Document* doc = getDocument();
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
    Document* doc = getDocument();
    if (doc->focusNode() == this)
        doc->setFocusNode(0);
}

// -------------------------------------------------------------------------

NamedAttrMap::NamedAttrMap(Element *e)
    : element(e)
    , attrs(0)
    , len(0)
{
}

NamedAttrMap::~NamedAttrMap()
{
    NamedAttrMap::clearAttributes(); // virtual method, so qualify just to be explicit
}

bool NamedAttrMap::isMappedAttributeMap() const
{
    return false;
}

PassRefPtr<Node> NamedAttrMap::getNamedItem(const String& name) const
{
    String localName = inHTMLDocument(element) ? name.lower() : name;
    return getNamedItem(QualifiedName(nullAtom, localName.impl(), nullAtom));
}

PassRefPtr<Node> NamedAttrMap::getNamedItemNS(const String& namespaceURI, const String& localName) const
{
    return getNamedItem(QualifiedName(nullAtom, localName.impl(), namespaceURI.impl()));
}

PassRefPtr<Node> NamedAttrMap::removeNamedItem(const String& name, ExceptionCode& ec)
{
    String localName = inHTMLDocument(element) ? name.lower() : name;
    return removeNamedItem(QualifiedName(nullAtom, localName.impl(), nullAtom), ec);
}

PassRefPtr<Node> NamedAttrMap::removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode& ec)
{
    return removeNamedItem(QualifiedName(nullAtom, localName.impl(), namespaceURI.impl()), ec);
}

PassRefPtr<Node> NamedAttrMap::getNamedItem(const QualifiedName& name) const
{
    Attribute* a = getAttributeItem(name);
    if (!a) return 0;

    return a->createAttrIfNeeded(element);
}

PassRefPtr<Node> NamedAttrMap::setNamedItem(Node* arg, ExceptionCode& ec)
{
    if (!element) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this map is readonly.
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    // WRONG_DOCUMENT_ERR: Raised if arg was created from a different document than the one that created this map.
    if (arg->getDocument() != element->getDocument()) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    // Not mentioned in spec: throw a HIERARCHY_REQUEST_ERROR if the user passes in a non-attribute node
    if (!arg->isAttributeNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }
    Attr *attr = static_cast<Attr*>(arg);

    Attribute* a = attr->attr();
    Attribute* old = getAttributeItem(a->name());
    if (old == a)
        return RefPtr<Node>(arg); // we know about it already

    // INUSE_ATTRIBUTE_ERR: Raised if arg is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attr->ownerElement()) {
        ec = INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    if (a->name() == idAttr)
        element->updateId(old ? old->value() : nullAtom, a->value());

    // ### slightly inefficient - resizes attribute array twice.
    RefPtr<Node> r;
    if (old) {
        r = old->createAttrIfNeeded(element);
        removeAttribute(a->name());
    }

    addAttribute(a);
    return r.release();
}

// The DOM2 spec doesn't say that removeAttribute[NS] throws NOT_FOUND_ERR
// if the attribute is not found, but at this level we have to throw NOT_FOUND_ERR
// because of removeNamedItem, removeNamedItemNS, and removeAttributeNode.
PassRefPtr<Node> NamedAttrMap::removeNamedItem(const QualifiedName& name, ExceptionCode& ec)
{
    // ### should this really be raised when the attribute to remove isn't there at all?
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    Attribute* a = getAttributeItem(name);
    if (!a) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    RefPtr<Node> r = a->createAttrIfNeeded(element);

    if (name == idAttr)
        element->updateId(a->value(), nullAtom);

    removeAttribute(name);
    return r.release();
}

PassRefPtr<Node> NamedAttrMap::item ( unsigned index ) const
{
    if (index >= len)
        return 0;

    return attrs[index]->createAttrIfNeeded(element);
}

Attribute* NamedAttrMap::getAttributeItem(const QualifiedName& name) const
{
    for (unsigned i = 0; i < len; ++i) {
        if (attrs[i]->name().matches(name))
            return attrs[i];
    }
    return 0;
}

void NamedAttrMap::clearAttributes()
{
    if (attrs) {
        for (unsigned i = 0; i < len; i++) {
            if (attrs[i]->m_impl)
                attrs[i]->m_impl->m_element = 0;
            attrs[i]->deref();
        }
        fastFree(attrs);
        attrs = 0;
    }
    len = 0;
}

void NamedAttrMap::detachFromElement()
{
    // we allow a NamedAttrMap w/o an element in case someone still has a reference
    // to if after the element gets deleted - but the map is now invalid
    element = 0;
    clearAttributes();
}

NamedAttrMap& NamedAttrMap::operator=(const NamedAttrMap& other)
{
    // clone all attributes in the other map, but attach to our element
    if (!element)
        return *this;

    // If assigning the map changes the id attribute, we need to call
    // updateId.

    Attribute *oldId = getAttributeItem(idAttr);
    Attribute *newId = other.getAttributeItem(idAttr);

    if (oldId || newId)
        element->updateId(oldId ? oldId->value() : nullAtom, newId ? newId->value() : nullAtom);

    clearAttributes();
    len = other.len;
    attrs = static_cast<Attribute **>(fastMalloc(len * sizeof(Attribute *)));

    // first initialize attrs vector, then call attributeChanged on it
    // this allows attributeChanged to use getAttribute
    for (unsigned i = 0; i < len; i++) {
        attrs[i] = other.attrs[i]->clone();
        attrs[i]->ref();
    }

    // FIXME: This is wasteful.  The class list could be preserved on a copy, and we
    // wouldn't have to waste time reparsing the attribute.
    // The derived class, HTMLNamedAttrMap, which manages a parsed class list for the CLASS attribute,
    // will update its member variable when parse attribute is called.
    for(unsigned i = 0; i < len; i++)
        element->attributeChanged(attrs[i], true);

    return *this;
}

void NamedAttrMap::addAttribute(Attribute *attribute)
{
    // Add the attribute to the list
    Attribute **newAttrs = static_cast<Attribute **>(fastMalloc((len + 1) * sizeof(Attribute *)));
    if (attrs) {
      for (unsigned i = 0; i < len; i++)
        newAttrs[i] = attrs[i];
      fastFree(attrs);
    }
    attrs = newAttrs;
    attrs[len++] = attribute;
    attribute->ref();

    Attr * const attr = attribute->m_impl;
    if (attr)
        attr->m_element = element;

    // Notify the element that the attribute has been added, and dispatch appropriate mutation events
    // Note that element may be null here if we are called from insertAttr() during parsing
    if (element) {
        RefPtr<Attribute> a = attribute;
        element->attributeChanged(a.get());
        // Because of our updateStyleAttributeIfNeeded() style modification events are never sent at the right time, so don't bother sending them.
        if (a->name() != styleAttr) {
            element->dispatchAttrAdditionEvent(a.get());
            element->dispatchSubtreeModifiedEvent(false);
        }
    }
}

void NamedAttrMap::removeAttribute(const QualifiedName& name)
{
    unsigned index = len+1;
    for (unsigned i = 0; i < len; ++i)
        if (attrs[i]->name().matches(name)) {
            index = i;
            break;
        }

    if (index >= len) return;

    // Remove the attribute from the list
    Attribute* attr = attrs[index];
    if (attrs[index]->m_impl)
        attrs[index]->m_impl->m_element = 0;
    if (len == 1) {
        fastFree(attrs);
        attrs = 0;
        len = 0;
    } else {
        Attribute **newAttrs = static_cast<Attribute **>(fastMalloc((len - 1) * sizeof(Attribute *)));
        unsigned i;
        for (i = 0; i < unsigned(index); i++)
            newAttrs[i] = attrs[i];
        len--;
        for (; i < len; i++)
            newAttrs[i] = attrs[i+1];
        fastFree(attrs);
        attrs = newAttrs;
    }

    // Notify the element that the attribute has been removed
    // dispatch appropriate mutation events
    if (element && !attr->m_value.isNull()) {
        AtomicString value = attr->m_value;
        attr->m_value = nullAtom;
        element->attributeChanged(attr);
        attr->m_value = value;
    }
    if (element) {
        element->dispatchAttrRemovalEvent(attr);
        element->dispatchSubtreeModifiedEvent(false);
    }
    attr->deref();
}

bool NamedAttrMap::mapsEquivalent(const NamedAttrMap* otherMap) const
{
    if (!otherMap)
        return false;
    
    if (length() != otherMap->length())
        return false;
    
    for (unsigned i = 0; i < length(); i++) {
        Attribute *attr = attributeItem(i);
        Attribute *otherAttr = otherMap->getAttributeItem(attr->name());
            
        if (!otherAttr || attr->value() != otherAttr->value())
            return false;
    }
    
    return true;
}

// ------------------------------- Styled Element and Mapped Attribute Implementation

CSSMappedAttributeDeclaration::~CSSMappedAttributeDeclaration()
{
    if (m_entryType != ePersistent)
        StyledElement::removeMappedAttributeDecl(m_entryType, m_attrName, m_attrValue);
}

CSSMappedAttributeDeclaration* StyledElement::getMappedAttributeDecl(MappedAttributeEntry entryType, Attribute* attr)
{
    if (!mappedAttributeDecls)
        return 0;
    return mappedAttributeDecls->get(MappedAttributeKey(entryType, attr->name().localName().impl(), attr->value().impl()));
}

void StyledElement::setMappedAttributeDecl(MappedAttributeEntry entryType, Attribute* attr, CSSMappedAttributeDeclaration* decl)
{
    if (!mappedAttributeDecls)
        mappedAttributeDecls = new MappedAttributeDecls;
    mappedAttributeDecls->set(MappedAttributeKey(entryType, attr->name().localName().impl(), attr->value().impl()), decl);
}

void StyledElement::removeMappedAttributeDecl(MappedAttributeEntry entryType,
                                                  const QualifiedName& attrName, const AtomicString& attrValue)
{
    if (!mappedAttributeDecls)
        return;
    mappedAttributeDecls->remove(MappedAttributeKey(entryType, attrName.localName().impl(), attrValue.impl()));
}

void StyledElement::invalidateStyleAttribute()
{
    m_isStyleAttributeValid = false;
}

void StyledElement::updateStyleAttributeIfNeeded() const
{
    if (!m_isStyleAttributeValid) {
        m_isStyleAttributeValid = true;
        m_synchronizingStyleAttribute = true;
        if (m_inlineStyleDecl)
            const_cast<StyledElement*>(this)->setAttribute(styleAttr, m_inlineStyleDecl->cssText());
        m_synchronizingStyleAttribute = false;
    }
}

Attribute* MappedAttribute::clone(bool preserveDecl) const
{
    return new MappedAttribute(name(), value(), preserveDecl ? m_styleDecl.get() : 0);
}

NamedMappedAttrMap::NamedMappedAttrMap(Element *e)
:NamedAttrMap(e), m_mappedAttributeCount(0)
{}

void NamedMappedAttrMap::clearAttributes()
{
    m_classList.clear();
    m_mappedAttributeCount = 0;
    NamedAttrMap::clearAttributes();
}

bool NamedMappedAttrMap::isMappedAttributeMap() const
{
    return true;
}

int NamedMappedAttrMap::declCount() const
{
    int result = 0;
    for (unsigned i = 0; i < length(); i++) {
        MappedAttribute* attr = attributeItem(i);
        if (attr->decl())
            result++;
    }
    return result;
}

bool NamedMappedAttrMap::mapsEquivalent(const NamedMappedAttrMap* otherMap) const
{
    // The # of decls must match.
    if (declCount() != otherMap->declCount())
        return false;
    
    // The values for each decl must match.
    for (unsigned i = 0; i < length(); i++) {
        MappedAttribute* attr = attributeItem(i);
        if (attr->decl()) {
            Attribute* otherAttr = otherMap->getAttributeItem(attr->name());
            if (!otherAttr || (attr->value() != otherAttr->value()))
                return false;
        }
    }
    return true;
}

inline static bool isClassWhitespace(QChar c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

void NamedMappedAttrMap::parseClassAttribute(const String& classStr)
{
    m_classList.clear();
    if (!element->hasClass())
        return;
    
    String classAttr = element->getDocument()->inCompatMode() ? 
        (classStr.impl()->isLower() ? classStr : String(classStr.impl()->lower())) :
        classStr;
    
    AtomicStringList* curr = 0;
    
    const QChar* str = classAttr.unicode();
    int length = classAttr.length();
    int sPos = 0;

    while (true) {
        while (sPos < length && isClassWhitespace(str[sPos]))
            ++sPos;
        if (sPos >= length)
            break;
        int ePos = sPos + 1;
        while (ePos < length && !isClassWhitespace(str[ePos]))
            ++ePos;
        if (curr) {
            curr->setNext(new AtomicStringList(AtomicString(str + sPos, ePos - sPos)));
            curr = curr->next();
        } else {
            if (sPos == 0 && ePos == length) {
                m_classList.setString(AtomicString(classAttr));
                break;
            }
            m_classList.setString(AtomicString(str + sPos, ePos - sPos));
            curr = &m_classList;
        }
        sPos = ePos + 1;
    }
}

StyledElement::StyledElement(const QualifiedName& name, Document *doc)
    : Element(name, doc)
{
    m_isStyleAttributeValid = true;
    m_synchronizingStyleAttribute = false;
}

StyledElement::~StyledElement()
{
    destroyInlineStyleDecl();
}

Attribute* StyledElement::createAttribute(const QualifiedName& name, StringImpl* value)
{
    return new MappedAttribute(name, value);
}

void StyledElement::createInlineStyleDecl()
{
    m_inlineStyleDecl = new CSSMutableStyleDeclaration;
    m_inlineStyleDecl->setParent(getDocument()->elementSheet());
    m_inlineStyleDecl->setNode(this);
    m_inlineStyleDecl->setStrictParsing(!getDocument()->inCompatMode());
}

void StyledElement::destroyInlineStyleDecl()
{
    if (m_inlineStyleDecl) {
        m_inlineStyleDecl->setNode(0);
        m_inlineStyleDecl->setParent(0);
        m_inlineStyleDecl = 0;
    }
}

void StyledElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    MappedAttribute* mappedAttr = static_cast<MappedAttribute*>(attr);
    if (mappedAttr->decl() && !preserveDecls) {
        mappedAttr->setDecl(0);
        setChanged();
        if (namedAttrMap)
            mappedAttributes()->declRemoved();
    }

    bool checkDecl = true;
    MappedAttributeEntry entry;
    bool needToParse = mapToEntry(attr->name(), entry);
    if (preserveDecls) {
        if (mappedAttr->decl()) {
            setChanged();
            if (namedAttrMap)
                mappedAttributes()->declAdded();
            checkDecl = false;
        }
    }
    else if (!attr->isNull() && entry != eNone) {
        CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(entry, attr);
        if (decl) {
            mappedAttr->setDecl(decl);
            setChanged();
            if (namedAttrMap)
                mappedAttributes()->declAdded();
            checkDecl = false;
        } else
            needToParse = true;
    }

    if (needToParse)
        parseMappedAttribute(mappedAttr);
    
    if (checkDecl && mappedAttr->decl()) {
        // Add the decl to the table in the appropriate spot.
        setMappedAttributeDecl(entry, attr, mappedAttr->decl());
        mappedAttr->decl()->setMappedState(entry, attr->name(), attr->value());
        mappedAttr->decl()->setParent(0);
        mappedAttr->decl()->setNode(0);
        if (namedAttrMap)
            mappedAttributes()->declAdded();
    }
}

bool StyledElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    result = eNone;
    if (attrName == styleAttr)
        return !m_synchronizingStyleAttribute;
    return true;
}

void StyledElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == idAttr) {
        // unique id
        setHasID(!attr->isNull());
        if (namedAttrMap) {
            if (attr->isNull())
                namedAttrMap->setID(nullAtom);
            else if (getDocument()->inCompatMode() && !attr->value().impl()->isLower())
                namedAttrMap->setID(AtomicString(attr->value().domString().lower()));
            else
                namedAttrMap->setID(attr->value());
        }
        setChanged();
    } else if (attr->name() == classAttr) {
        // class
        setHasClass(!attr->isNull());
        if (namedAttrMap)
            mappedAttributes()->parseClassAttribute(attr->value());
        setChanged();
    } else if (attr->name() == styleAttr) {
        setHasStyle(!attr->isNull());
        if (attr->isNull())
            destroyInlineStyleDecl();
        else
            getInlineStyleDecl()->parseDeclaration(attr->value());
        m_isStyleAttributeValid = true;
        setChanged();
    }
}

void StyledElement::createAttributeMap() const
{
    namedAttrMap = new NamedMappedAttrMap(const_cast<StyledElement*>(this));
}

CSSMutableStyleDeclaration* StyledElement::getInlineStyleDecl()
{
    if (!m_inlineStyleDecl)
        createInlineStyleDecl();
    return m_inlineStyleDecl.get();
}

CSSStyleDeclaration* StyledElement::style()
{
    return getInlineStyleDecl();
}

CSSMutableStyleDeclaration* StyledElement::additionalAttributeStyleDecl()
{
    return 0;
}

const AtomicStringList* StyledElement::getClassList() const
{
    return namedAttrMap ? mappedAttributes()->getClassList() : 0;
}

static inline bool isHexDigit( const QChar &c ) {
    return ( c >= '0' && c <= '9' ) ||
           ( c >= 'a' && c <= 'f' ) ||
           ( c >= 'A' && c <= 'F' );
}

static inline int toHex( const QChar &c ) {
    return ( (c >= '0' && c <= '9')
             ? (c.unicode() - '0')
             : ( ( c >= 'a' && c <= 'f' )
                 ? (c.unicode() - 'a' + 10)
                 : ( ( c >= 'A' && c <= 'F' )
                     ? (c.unicode() - 'A' + 10)
                     : -1 ) ) );
}

void StyledElement::addCSSProperty(MappedAttribute* attr, int id, const String &value)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setProperty(id, value, false);
}

void StyledElement::addCSSProperty(MappedAttribute* attr, int id, int value)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setProperty(id, value, false);
}

void StyledElement::addCSSStringProperty(MappedAttribute* attr, int id, const String &value, CSSPrimitiveValue::UnitTypes type)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setStringProperty(id, value, type, false);
}

void StyledElement::addCSSImageProperty(MappedAttribute* attr, int id, const String &URL)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setImageProperty(id, URL, false);
}

void StyledElement::addCSSLength(MappedAttribute* attr, int id, const String &value)
{
    // FIXME: This function should not spin up the CSS parser, but should instead just figure out the correct
    // length unit and make the appropriate parsed value.
    if (!attr->decl()) createMappedDecl(attr);

    // strip attribute garbage..
    StringImpl* v = value.impl();
    if (v) {
        unsigned int l = 0;
        
        while (l < v->length() && (*v)[l].unicode() <= ' ')
            l++;
        
        for (; l < v->length(); l++) {
            char cc = (*v)[l].latin1();
            if (cc > '9' || (cc < '0' && cc != '*' && cc != '%' && cc != '.'))
                break;
        }

        if (l != v->length()) {
            attr->decl()->setLengthProperty(id, v->substring(0, l), false);
            return;
        }
    }
    
    attr->decl()->setLengthProperty(id, value, false);
}

/* color parsing that tries to match as close as possible IE 6. */
void StyledElement::addCSSColor(MappedAttribute* attr, int id, const String &c)
{
    // this is the only case no color gets applied in IE.
    if ( !c.length() )
        return;

    if (!attr->decl()) createMappedDecl(attr);
    
    if (attr->decl()->setProperty(id, c, false) )
        return;
    
    DeprecatedString color = c.deprecatedString();
    // not something that fits the specs.
    
    // we're emulating IEs color parser here. It maps transparent to black, otherwise it tries to build a rgb value
    // out of everyhting you put in. The algorithm is experimentally determined, but seems to work for all test cases I have.
    
    // the length of the color value is rounded up to the next
    // multiple of 3. each part of the rgb triple then gets one third
    // of the length.
    //
    // Each triplet is parsed byte by byte, mapping
    // each number to a hex value (0-9a-fA-F to their values
    // everything else to 0).
    //
    // The highest non zero digit in all triplets is remembered, and
    // used as a normalization point to normalize to values between 0
    // and 255.
    
    if ( color.lower() != "transparent" ) {
        if ( color[0] == '#' )
            color.remove( 0,  1 );
        int basicLength = (color.length() + 2) / 3;
        if ( basicLength > 1 ) {
            // IE ignores colors with three digits or less
            int colors[3] = { 0, 0, 0 };
            int component = 0;
            int pos = 0;
            int maxDigit = basicLength-1;
            while ( component < 3 ) {
                // search forward for digits in the string
                int numDigits = 0;
                while ( pos < (int)color.length() && numDigits < basicLength ) {
                    int hex = toHex( color[pos] );
                    colors[component] = (colors[component] << 4);
                    if ( hex > 0 ) {
                        colors[component] += hex;
                        maxDigit = kMin( maxDigit, numDigits );
                    }
                    numDigits++;
                    pos++;
                }
                while ( numDigits++ < basicLength )
                    colors[component] <<= 4;
                component++;
            }
            maxDigit = basicLength - maxDigit;
            
            // normalize to 00-ff. The highest filled digit counts, minimum is 2 digits
            maxDigit -= 2;
            colors[0] >>= 4*maxDigit;
            colors[1] >>= 4*maxDigit;
            colors[2] >>= 4*maxDigit;
            // assert( colors[0] < 0x100 && colors[1] < 0x100 && colors[2] < 0x100 );
            
            color.sprintf("#%02x%02x%02x", colors[0], colors[1], colors[2] );
            if ( attr->decl()->setProperty(id, String(color), false) )
                return;
        }
    }
    attr->decl()->setProperty(id, CSS_VAL_BLACK, false);
}

void StyledElement::createMappedDecl(MappedAttribute* attr)
{
    CSSMappedAttributeDeclaration* decl = new CSSMappedAttributeDeclaration(0);
    attr->setDecl(decl);
    decl->setParent(getDocument()->elementSheet());
    decl->setNode(this);
    decl->setStrictParsing(false); // Mapped attributes are just always quirky.
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned MappedAttributeHash::hash(const MappedAttributeKey& key)
{
    uint32_t hash = PHI;
    uint32_t tmp;

    const uint16_t* p;

    p = reinterpret_cast<const uint16_t*>(&key.name);
    hash += p[0];
    tmp = (p[1] << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    hash += hash >> 11;
    assert(sizeof(key.name) == 4 || sizeof(key.name) == 8);
    if (sizeof(key.name) == 8) {
        p += 2;
        hash += p[0];
        tmp = (p[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        hash += hash >> 11;
    }

    p = reinterpret_cast<const uint16_t*>(&key.value);
    hash += p[0];
    tmp = (p[1] << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    hash += hash >> 11;
    assert(sizeof(key.value) == 4 || sizeof(key.value) == 8);
    if (sizeof(key.value) == 8) {
        p += 2;
        hash += p[0];
        tmp = (p[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        hash += hash >> 11;
    }

    // Handle end case
    hash += key.type;
    hash ^= hash << 11;
    hash += hash >> 17;

    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // This avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked
    if (hash == 0)
        hash = 0x80000000;

    return hash;
}

}

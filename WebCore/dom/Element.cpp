/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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
#include "Element.h"

#include "CSSStyleSelector.h"
#include "ClassNames.h"
#include "ClassNodeList.h"
#include "Document.h"
#include "Editor.h"
#include "ExceptionCode.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "NamedAttrMap.h"
#include "NodeList.h"
#include "Page.h"
#include "RenderBlock.h"
#include "SelectionController.h"
#include "TextIterator.h"
#include "TextStream.h"
#include "XMLNames.h"

namespace WebCore {

using namespace HTMLNames;
using namespace XMLNames;

class ElementRareData {
public:
    ElementRareData(Element*);
    void resetComputedStyle(Element*);

    IntSize m_minimumSizeForResizing;
    RenderStyle* m_computedStyle;
    bool m_needsFocusAppearanceUpdateSoonAfterAttach;
};

typedef HashMap<const Element*, ElementRareData*> ElementRareDataMap;

static ElementRareDataMap& rareDataMap()
{
    static ElementRareDataMap* dataMap = new ElementRareDataMap;
    return *dataMap;
}

static ElementRareData* rareDataFromMap(const Element* element)
{
    return rareDataMap().get(element);
}

static inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(INT_MAX, INT_MAX);
}

inline ElementRareData::ElementRareData(Element* element)
    : m_minimumSizeForResizing(defaultMinimumSizeForResizing())
    , m_computedStyle(0)
    , m_needsFocusAppearanceUpdateSoonAfterAttach(false)
{
}

void ElementRareData::resetComputedStyle(Element* element)
{
    if (!m_computedStyle)
        return;
    m_computedStyle->deref(element->document()->renderArena());
    m_computedStyle = 0;
}

Element::Element(const QualifiedName& qName, Document *doc)
    : ContainerNode(doc)
    , m_tagName(qName)
    , m_isStyleAttributeValid(true)
    , m_synchronizingStyleAttribute(false)
    , m_parsingChildrenFinished(true)
{
}

Element::~Element()
{
    if (namedAttrMap)
        namedAttrMap->detachFromElement();

    if (!m_attrWasSpecifiedOrElementHasRareData)
        ASSERT(!rareDataMap().contains(this));
    else {
        ElementRareDataMap& dataMap = rareDataMap();
        ElementRareDataMap::iterator it = dataMap.find(this);
        ASSERT(it != dataMap.end());
        delete it->second;
        dataMap.remove(it);
    }
}

inline ElementRareData* Element::rareData()
{
    return m_attrWasSpecifiedOrElementHasRareData ? rareDataFromMap(this) : 0;
}

inline const ElementRareData* Element::rareData() const
{
    return m_attrWasSpecifiedOrElementHasRareData ? rareDataFromMap(this) : 0;
}

ElementRareData* Element::createRareData()
{
    if (m_attrWasSpecifiedOrElementHasRareData)
        return rareDataMap().get(this);
    ASSERT(!rareDataMap().contains(this));
    ElementRareData* data = new ElementRareData(this);
    rareDataMap().set(this, data);
    m_attrWasSpecifiedOrElementHasRareData = true;
    return data;
}

PassRefPtr<Node> Element::cloneNode(bool deep)
{
    ExceptionCode ec = 0;
    RefPtr<Element> clone = document()->createElementNS(namespaceURI(), nodeName(), ec);
    ASSERT(!ec);
    
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

void Element::setBooleanAttribute(const QualifiedName& name, bool b)
{
    if (b)
        setAttribute(name, name.localName());
    else {
        ExceptionCode ex;
        removeAttribute(name, ex);
    }
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

const ClassNames* Element::getClassNames() const
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

void Element::scrollByUnits(int units, ScrollGranularity granularity)
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject *rend = renderer()) {
        if (rend->hasOverflowClip()) {
            ScrollDirection direction = ScrollDown;
            if (units < 0) {
                direction = ScrollUp;
                units = -units;
            }
            rend->layer()->scroll(direction, granularity, units);
        }
    }
}

void Element::scrollByLines(int lines)
{
    scrollByUnits(lines, ScrollByLine);
}

void Element::scrollByPages(int pages)
{
    scrollByUnits(pages, ScrollByPage);
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

int Element::clientLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (RenderObject* rend = renderer())
        return rend->clientLeft();
    return 0;
}

int Element::clientTop()
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (RenderObject* rend = renderer())
        return rend->clientTop();
    return 0;
}

int Element::clientWidth()
{
    document()->updateLayoutIgnorePendingStylesheets();

    // When in strict mode, clientWidth for the document element should return the width of the containing frame.
    // When in quirks mode, clientWidth for the body element should return the width of the containing frame.
    bool inCompatMode = document()->inCompatMode();
    if ((!inCompatMode && document()->documentElement() == this) ||
        (inCompatMode && isHTMLElement() && document()->body() == this)) {
        if (FrameView* view = document()->view())
            return view->visibleWidth();
    }
    

    if (RenderObject* rend = renderer())
        return rend->clientWidth();
    return 0;
}

int Element::clientHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();

    // When in strict mode, clientHeight for the document element should return the height of the containing frame.
    // When in quirks mode, clientHeight for the body element should return the height of the containing frame.
    bool inCompatMode = document()->inCompatMode();     

    if ((!inCompatMode && document()->documentElement() == this) ||
        (inCompatMode && isHTMLElement() && document()->body() == this)) {
        if (FrameView* view = document()->view())
            return view->visibleHeight();
    }
    
    if (RenderObject* rend = renderer())
        return rend->clientHeight();
    return 0;
}

int Element::scrollLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->scrollLeft();
    return 0;
}

int Element::scrollTop()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        return rend->scrollTop();
    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject *rend = renderer())
        rend->setScrollLeft(newLeft);
}

void Element::setScrollTop(int newTop)
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject *rend = renderer())
        rend->setScrollTop(newTop);
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

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
}

const AtomicString& Element::getAttribute(const String& name) const
{
    String localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    if (localName == styleAttr.localName())
        updateStyleAttributeIfNeeded();
    
    if (namedAttrMap)
        if (Attribute* a = namedAttrMap->getAttributeItem(localName))
            return a->value();
    
    return nullAtom;
}

const AtomicString& Element::getAttributeNS(const String& namespaceURI, const String& localName) const
{
    return getAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

void Element::setAttribute(const String& name, const String& value, ExceptionCode& ec)
{
    if (!Document::isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }

    String localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;

    // allocate attributemap if necessary
    Attribute* old = attributes(false)->getAttributeItem(localName);

    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (namedAttrMap->isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    
    document()->incDOMTreeVersion();

    if (localName == idAttr.localName())
        updateId(old ? old->value() : nullAtom, value);
    
    if (old && value.isNull())
        namedAttrMap->removeAttribute(old->name());
    else if (!old && !value.isNull())
        namedAttrMap->addAttribute(createAttribute(QualifiedName(nullAtom, localName, nullAtom), value.impl()));
    else if (old && !value.isNull()) {
        old->setValue(value);
        attributeChanged(old);
    }
}

void Element::setAttribute(const QualifiedName& name, StringImpl* value, ExceptionCode& ec)
{
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

String Element::nodeNamePreservingCase() const
{
    return m_tagName.toString();
}

void Element::setPrefix(const AtomicString &_prefix, ExceptionCode& ec)
{
    ec = 0;
    checkSetPrefix(_prefix, ec);
    if (ec)
        return;

    m_tagName.setPrefix(_prefix);
}

String Element::baseURI() const
{
    KURL xmlbase(getAttribute(baseAttr).deprecatedString());

    if (!xmlbase.protocol().isEmpty())
        return xmlbase.string();

    Node* parent = parentNode();
    if (parent)
        return KURL(parent->baseURI().deprecatedString(), xmlbase.deprecatedString()).string();

    return xmlbase.string();
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

bool Element::contains(const Node* node) const
{
    if (!node)
        return false;
    return this == node || node->isDescendantOf(this);
}

void Element::createAttributeMap() const
{
    namedAttrMap = new NamedAttrMap(const_cast<Element*>(this));
}

bool Element::isURLAttribute(Attribute *attr) const
{
    return false;
}

const QualifiedName& Element::imageSourceAttributeName() const
{
    return srcAttr;
}

RenderStyle* Element::styleForRenderer(RenderObject* parentRenderer)
{
    return document()->styleSelector()->styleForElement(this);
}

RenderObject* Element::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (document()->documentElement() == this && style->display() == NONE) {
        // Ignore display: none on root elements.  Force a display of block in that case.
        RenderBlock* result = new (arena) RenderBlock(this);
        if (result)
            result->setAnimatableStyle(style);
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
    createRendererIfNeeded();
    ContainerNode::attach();
    if (ElementRareData* rd = rareData()) {
        if (rd->m_needsFocusAppearanceUpdateSoonAfterAttach) {
            if (isFocusable() && document()->focusedNode() == this)
                document()->updateFocusAppearanceSoon();
            rd->m_needsFocusAppearanceUpdateSoonAfterAttach = false;
        }
    }
}

void Element::detach()
{
    cancelFocusAppearanceUpdate();
    if (ElementRareData* rd = rareData())
        rd->resetComputedStyle(this);
    ContainerNode::detach();
}

void Element::recalcStyle(StyleChange change)
{
    RenderStyle* currentStyle = renderStyle();
    bool hasParentStyle = parentNode() ? parentNode()->renderStyle() : false;
    bool hasPositionalChildren = currentStyle && (currentStyle->childrenAffectedByFirstChildRules() || currentStyle->childrenAffectedByLastChildRules() ||
                                                  currentStyle->childrenAffectedByForwardPositionalRules() || currentStyle->childrenAffectedByBackwardPositionalRules());

#if ENABLE(SVG)
    if (!hasParentStyle && isShadowNode() && isSVGElement())
        hasParentStyle = true;
#endif

    if ((change > NoChange || changed())) {
        if (ElementRareData* rd = rareData())
            rd->resetComputedStyle(this);
    }
    if (hasParentStyle && (change >= Inherit || changed())) {
        RenderStyle *newStyle = document()->styleSelector()->styleForElement(this);
        StyleChange ch = diff(currentStyle, newStyle);
        if (ch == Detach) {
            if (attached())
                detach();
            // ### Suboptimal. Style gets calculated again.
            attach();
            // attach recalulates the style for all children. No need to do it twice.
            setChanged(NoStyleChange);
            setHasChangedChild(false);
            newStyle->deref(document()->renderArena());
            return;
        }

        if (currentStyle && newStyle) {
            // Preserve "affected by" bits that were propagated to us from descendants in the case where we didn't do a full
            // style change (e.g., only inline style changed).
            if (currentStyle->affectedByHoverRules())
                newStyle->setAffectedByHoverRules(true);
            if (currentStyle->affectedByActiveRules())
                newStyle->setAffectedByActiveRules(true);
            if (currentStyle->affectedByDragRules())
                newStyle->setAffectedByDragRules(true);
            if (currentStyle->childrenAffectedByForwardPositionalRules())
                newStyle->setChildrenAffectedByForwardPositionalRules();
            if (currentStyle->childrenAffectedByBackwardPositionalRules())
                newStyle->setChildrenAffectedByBackwardPositionalRules();
            if (currentStyle->childrenAffectedByFirstChildRules())
                newStyle->setChildrenAffectedByFirstChildRules();
            if (currentStyle->childrenAffectedByLastChildRules())
                newStyle->setChildrenAffectedByLastChildRules();
        }

        if (ch != NoChange) {
            if (newStyle)
                setRenderStyle(newStyle);
        } else if (changed() && newStyle && (document()->usesSiblingRules() || document()->usesDescendantRules())) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.  This is only necessary if the document actually uses
            // sibling/descendant rules, since otherwise it isn't possible for ancestor styles to affect sharing of
            // descendants.
            if (renderer())
                renderer()->setStyleInternal(newStyle);
            else
                setRenderStyle(newStyle);
        }

        newStyle->deref(document()->renderArena());

        if (change != Force) {
            if ((document()->usesDescendantRules() || hasPositionalChildren) && styleChangeType() == FullStyleChange)
                change = Force;
            else
                change = ch;
        }
    }

    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (change >= Inherit || n->isTextNode() || n->hasChangedChild() || n->changed())
            n->recalcStyle(change);
    }

    setChanged(NoStyleChange);
    setHasChangedChild(false);
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

static void checkFirstChildRules(Element* e, RenderStyle* style)
{
    if (style->childrenAffectedByFirstChildRules()) {
        // Check our first two children.  They need to be true and false respectively.
        bool checkingFirstChild = true;
        for (Node* n = e->firstChild(); n; n = n->nextSibling()) {
            if (n->isElementNode()) {
                if (checkingFirstChild) {
                    if (n->attached() && n->renderStyle() && !n->renderStyle()->firstChildState())
                        n->setChanged();
                    checkingFirstChild = false;
                } else {
                    if (n->attached() && n->renderStyle() && n->renderStyle()->firstChildState())
                        n->setChanged();
                    break;
                }
            }
        }
    } 
}

static void checkLastChildRules(Element* e, RenderStyle* style)
{
    if (style->childrenAffectedByLastChildRules()) {
        // Check our last two children.  They need to be true and false respectively.
        bool checkingLastChild = true;
        for (Node* n = e->lastChild(); n; n = n->previousSibling()) {
            if (n->isElementNode()) {
                if (checkingLastChild) {
                    if (n->attached() && n->renderStyle() && !n->renderStyle()->lastChildState())
                        n->setChanged();
                    checkingLastChild = false;
                } else {
                    if (n->attached() && n->renderStyle() && n->renderStyle()->lastChildState())
                        n->setChanged();
                    break;
                }
            }
        }
    }
}

static bool checkEmptyRules(Element* e, RenderStyle* style)
{
    return style->affectedByEmpty() && (!style->emptyState() || e->hasChildNodes());
}

static void checkStyleRules(Element* e, RenderStyle* style, bool changedByParser)
{
    if (e->changed() || !style)
        return;

    if (style->childrenAffectedByBackwardPositionalRules() || 
        (!changedByParser && style->childrenAffectedByForwardPositionalRules()) ||
        checkEmptyRules(e, style)) {
        e->setChanged();
        return;
    }

    checkFirstChildRules(e, style);
    checkLastChildRules(e, style);
}

void Element::childrenChanged(bool changedByParser)
{
    ContainerNode::childrenChanged(changedByParser);
    if (!changedByParser)
        checkStyleRules(this, renderStyle(), false);
}

void Element::finishParsingChildren()
{
    ContainerNode::finishParsingChildren();
    m_parsingChildrenFinished = true;
    checkStyleRules(this, renderStyle(), true);
}

void Element::dispatchAttrRemovalEvent(Attribute*)
{
    ASSERT(!eventDispatchForbidden());
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
    ASSERT(!eventDispatchForbidden());
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
void Element::dump(TextStream *stream, DeprecatedString ind) const
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

PassRefPtr<Attr> Element::setAttributeNodeNS(Attr* attr, ExceptionCode& ec)
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
    setAttribute(QualifiedName(prefix, localName, namespaceURI), value.impl(), ec);
}

void Element::removeAttribute(const String& name, ExceptionCode& ec)
{
    String localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;

    if (namedAttrMap) {
        namedAttrMap->removeNamedItem(localName, ec);
        if (ec == NOT_FOUND_ERR)
            ec = 0;
    }
}

void Element::removeAttributeNS(const String& namespaceURI, const String& localName, ExceptionCode& ec)
{
    removeAttribute(QualifiedName(nullAtom, localName, namespaceURI), ec);
}

PassRefPtr<Attr> Element::getAttributeNode(const String& name)
{
    NamedAttrMap* attrs = attributes(true);
    if (!attrs)
        return 0;
    String localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    return static_pointer_cast<Attr>(attrs->getNamedItem(localName));
}

PassRefPtr<Attr> Element::getAttributeNodeNS(const String& namespaceURI, const String& localName)
{
    NamedAttrMap* attrs = attributes(true);
    if (!attrs)
        return 0;
    return static_pointer_cast<Attr>(attrs->getNamedItem(QualifiedName(nullAtom, localName, namespaceURI)));
}

bool Element::hasAttribute(const String& name) const
{
    NamedAttrMap* attrs = attributes(true);
    if (!attrs)
        return false;
    String localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    return attrs->getAttributeItem(localName);
}

bool Element::hasAttributeNS(const String& namespaceURI, const String& localName) const
{
    NamedAttrMap* attrs = attributes(true);
    if (!attrs)
        return false;
    return attrs->getAttributeItem(QualifiedName(nullAtom, localName, namespaceURI));
}

CSSStyleDeclaration *Element::style()
{
    return 0;
}

void Element::focus(bool restorePreviousSelection)
{
    Document* doc = document();
    if (doc->focusedNode() == this)
        return;

    doc->updateLayoutIgnorePendingStylesheets();
    
    if (!supportsFocus())
        return;
    
    if (Page* page = doc->page())
        page->focusController()->setFocusedNode(this, doc->frame());

    if (!isFocusable()) {
        createRareData()->m_needsFocusAppearanceUpdateSoonAfterAttach = true;
        return;
    }
        
    cancelFocusAppearanceUpdate();
    updateFocusAppearance(restorePreviousSelection);
}

void Element::updateFocusAppearance(bool restorePreviousSelection)
{
    if (this == rootEditableElement()) { 
        Frame* frame = document()->frame();
        if (!frame)
            return;

        // FIXME: We should restore the previous selection if there is one.
        Selection newSelection = hasTagName(htmlTag) || hasTagName(bodyTag) ? Selection(Position(this, 0), DOWNSTREAM) : Selection::selectionFromContentsOfNode(this);
        
        if (frame->shouldChangeSelection(newSelection)) {
            frame->selectionController()->setSelection(newSelection);
            frame->revealSelection();
        }
    } else if (renderer() && !renderer()->isWidget())
        renderer()->enclosingLayer()->scrollRectToVisible(getRect());
}

void Element::blur()
{
    cancelFocusAppearanceUpdate();
    Document* doc = document();
    if (doc->focusedNode() == this) {
        if (doc->frame())
            doc->frame()->page()->focusController()->setFocusedNode(0, doc->frame());
        else
            doc->setFocusedNode(0);
    }
}

String Element::innerText() const
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    document()->updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return textContent(true);

    return plainText(rangeOfContents(const_cast<Element*>(this)).get());
}

String Element::outerText() const
{
    // Getting outerText is the same as getting innerText, only
    // setting is different. You would think this should get the plain
    // text for the outer range, but this is wrong, <br> for instance
    // would return different values for inner and outer text by such
    // a rule, but it doesn't in WinIE, and we want to match that.
    return innerText();
}

String Element::title() const
{
    return String();
}

IntSize Element::minimumSizeForResizing() const
{
    const ElementRareData* rd = rareData();
    return rd ? rd->m_minimumSizeForResizing : defaultMinimumSizeForResizing();
}

void Element::setMinimumSizeForResizing(const IntSize& size)
{
    if (size == defaultMinimumSizeForResizing() && !rareData())
        return;
    createRareData()->m_minimumSizeForResizing = size;
}

RenderStyle* Element::computedStyle()
{
    if (RenderStyle* usedStyle = renderStyle())
        return usedStyle;

    if (!attached())
        // FIXME: Try to do better than this. Ensure that styleForElement() works for elements that are not in the
        // document tree and figure out when to destroy the computed style for such elements.
        return 0;

    ElementRareData* rd = createRareData();
    if (!rd->m_computedStyle)
        rd->m_computedStyle = document()->styleSelector()->styleForElement(this, parent() ? parent()->computedStyle() : 0);
    return rd->m_computedStyle;
}

void Element::cancelFocusAppearanceUpdate()
{
    if (ElementRareData* rd = rareData())
        rd->m_needsFocusAppearanceUpdateSoonAfterAttach = false;
    if (document()->focusedNode() == this)
        document()->cancelFocusAppearanceUpdate();
}

bool Element::virtualHasTagName(const QualifiedName& name) const
{
    return hasTagName(name);
}

}

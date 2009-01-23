/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "CSSStyleSelector.h"
#include "CString.h"
#include "Document.h"
#include "Editor.h"
#include "ElementRareData.h"
#include "ExceptionCode.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "NamedAttrMap.h"
#include "NodeList.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformString.h"
#include "RenderBlock.h"
#include "SelectionController.h"
#include "TextIterator.h"
#include "XMLNames.h"

namespace WebCore {

using namespace HTMLNames;
using namespace XMLNames;
    
Element::Element(const QualifiedName& tagName, Document* doc)
    : ContainerNode(doc, true)
    , m_tagName(tagName)
{
}

Element::~Element()
{
    if (namedAttrMap)
        namedAttrMap->detachFromElement();
}

inline ElementRareData* Element::rareData() const
{
    ASSERT(hasRareData());
    return static_cast<ElementRareData*>(NodeRareData::rareDataFromMap(this));
}
    
inline ElementRareData* Element::ensureRareData()
{
    return static_cast<ElementRareData*>(Node::ensureRareData());
}
    
NodeRareData* Element::createRareData()
{
    return new ElementRareData;
}
    
PassRefPtr<Node> Element::cloneNode(bool deep)
{
    ExceptionCode ec = 0;
    RefPtr<Element> clone = document()->createElementNS(namespaceURI(), nodeName(), ec);
    ASSERT(!ec);
    
    // clone attributes
    if (namedAttrMap)
        clone->attributes()->setAttributes(*namedAttrMap);

    clone->copyNonAttributeProperties(this);
    
    if (deep)
        cloneChildNodes(clone.get());

    return clone.release();
}

PassRefPtr<Element> Element::cloneElement()
{
    return static_pointer_cast<Element>(cloneNode(false));
}

void Element::removeAttribute(const QualifiedName& name, ExceptionCode& ec)
{
    if (namedAttrMap) {
        namedAttrMap->removeNamedItem(name, ec);
        if (ec == NOT_FOUND_ERR)
            ec = 0;
    }
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value)
{
    ExceptionCode ec;
    setAttribute(name, value, ec);
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
NamedAttrMap* Element::attributes() const
{
    return attributes(false);
}

NamedAttrMap* Element::attributes(bool readonly) const
{
    if (!m_isStyleAttributeValid)
        updateStyleAttribute();

#if ENABLE(SVG)
    if (!m_areSVGAttributesValid)
        updateAnimatedSVGAttribute(String());
#endif

    if (!readonly && !namedAttrMap)
        createAttributeMap();
    return namedAttrMap.get();
}

Node::NodeType Element::nodeType() const
{
    return ELEMENT_NODE;
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
    if (name == styleAttr && !m_isStyleAttributeValid)
        updateStyleAttribute();

#if ENABLE(SVG)
    if (!m_areSVGAttributesValid)
        updateAnimatedSVGAttribute(name.localName());
#endif

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
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, false, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignTopAlways);
        else
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, false, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignBottomAlways);
    }
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document()->updateLayoutIgnorePendingStylesheets();
    IntRect bounds = getRect();    
    if (renderer()) {
        if (centerIfNeeded)
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, false, RenderLayer::gAlignCenterIfNeeded, RenderLayer::gAlignCenterIfNeeded);
        else
            renderer()->enclosingLayer()->scrollRectToVisible(bounds, false, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignToEdgeIfNeeded);
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
            toRenderBox(rend)->layer()->scroll(direction, granularity, units);
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

static float localZoomForRenderer(RenderObject* renderer)
{
    // FIXME: This does the wrong thing if two opposing zooms are in effect and canceled each
    // other out, but the alternative is that we'd have to crawl up the whole render tree every
    // time (or store an additional bit in the RenderStyle to indicate that a zoom was specified).
    float zoomFactor = 1.0f;
    if (renderer->style()->effectiveZoom() != 1.0f) {
        // Need to find the nearest enclosing RenderObject that set up
        // a differing zoom, and then we divide our result by it to eliminate the zoom.
        RenderObject* prev = renderer;
        for (RenderObject* curr = prev->parent(); curr; curr = curr->parent()) {
            if (curr->style()->effectiveZoom() != prev->style()->effectiveZoom()) {
                zoomFactor = prev->style()->zoom();
                break;
            }
            prev = curr;
        }
        if (prev->isRenderView())
            zoomFactor = prev->style()->zoom();
    }
    return zoomFactor;
}

static int adjustForLocalZoom(int value, RenderObject* renderer)
{
    float zoomFactor = localZoomForRenderer(renderer);
    if (zoomFactor == 1.0f)
        return value;
    return static_cast<int>(value / zoomFactor);
}

static int adjustForAbsoluteZoom(int value, RenderObject* renderer)
{
    float zoomFactor = renderer->style()->effectiveZoom();
    if (zoomFactor == 1.0f)
        return value;
    return static_cast<int>(value / zoomFactor);
}

int Element::offsetLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForLocalZoom(rend->offsetLeft(), rend);
    return 0;
}

int Element::offsetTop()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForLocalZoom(rend->offsetTop(), rend);
    return 0;
}

int Element::offsetWidth()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->offsetWidth(), rend);
    return 0;
}

int Element::offsetHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->offsetHeight(), rend);
    return 0;
}

Element* Element::offsetParent()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        if (RenderObject* offsetParent = rend->offsetParent())
            return static_cast<Element*>(offsetParent->element());
    return 0;
}

int Element::clientLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->clientLeft(), rend);
    return 0;
}

int Element::clientTop()
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->clientTop(), rend);
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
            return view->layoutWidth();
    }
    

    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->clientWidth(), rend);
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
            return view->layoutHeight();
    }
    
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->clientHeight(), rend);
    return 0;
}

int Element::scrollLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollLeft(), rend);
    return 0;
}

int Element::scrollTop()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollTop(), rend);
    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        rend->setScrollLeft(static_cast<int>(newLeft * rend->style()->effectiveZoom()));
}

void Element::setScrollTop(int newTop)
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        rend->setScrollTop(static_cast<int>(newTop * rend->style()->effectiveZoom()));
}

int Element::scrollWidth()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollWidth(), rend);
    return 0;
}

int Element::scrollHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollHeight(), rend);
    return 0;
}

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
}

const AtomicString& Element::getAttribute(const String& name) const
{
    String localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    if (localName == styleAttr.localName() && !m_isStyleAttributeValid)
        updateStyleAttribute();

#if ENABLE(SVG)
    if (!m_areSVGAttributesValid)
        updateAnimatedSVGAttribute(name);
#endif

    if (namedAttrMap)
        if (Attribute* a = namedAttrMap->getAttributeItem(name, shouldIgnoreAttributeCase(this)))
            return a->value();
    
    return nullAtom;
}

const AtomicString& Element::getAttributeNS(const String& namespaceURI, const String& localName) const
{
    return getAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

void Element::setAttribute(const AtomicString& name, const AtomicString& value, ExceptionCode& ec)
{
    if (!Document::isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }

    const AtomicString& localName = (shouldIgnoreAttributeCase(this) && !name.string().impl()->isLower()) ? AtomicString(name.string().lower()) : name;

    // allocate attributemap if necessary
    Attribute* old = attributes(false)->getAttributeItem(localName, false);

    document()->incDOMTreeVersion();

    if (localName == idAttr.localName())
        updateId(old ? old->value() : nullAtom, value);
    
    if (old && value.isNull())
        namedAttrMap->removeAttribute(old->name());
    else if (!old && !value.isNull())
        namedAttrMap->addAttribute(createAttribute(QualifiedName(nullAtom, localName, nullAtom), value));
    else if (old && !value.isNull()) {
        old->setValue(value);
        attributeChanged(old);
    }
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value, ExceptionCode&)
{
    document()->incDOMTreeVersion();

    // allocate attributemap if necessary
    Attribute* old = attributes(false)->getAttributeItem(name);

    if (name == idAttr)
        updateId(old ? old->value() : nullAtom, value);
    
    if (old && value.isNull())
        namedAttrMap->removeAttribute(name);
    else if (!old && !value.isNull())
        namedAttrMap->addAttribute(createAttribute(name, value));
    else if (old) {
        old->setValue(value);
        attributeChanged(old);
    }
}

PassRefPtr<Attribute> Element::createAttribute(const QualifiedName& name, const AtomicString& value)
{
    return Attribute::create(name, value);
}

void Element::attributeChanged(Attribute* attr, bool)
{
    if (!document()->axObjectCache()->accessibilityEnabled())
        return;

    const QualifiedName& attrName = attr->name();
    if (attrName == aria_activedescendantAttr) {
        // any change to aria-activedescendant attribute triggers accessibility focus change, but document focus remains intact
        document()->axObjectCache()->handleActiveDescendantChanged(renderer());
    } else if (attrName == roleAttr) {
        // the role attribute can change at any time, and the AccessibilityObject must pick up these changes
        document()->axObjectCache()->handleAriaRoleChanged(renderer());
    }
}

void Element::setAttributeMap(PassRefPtr<NamedAttrMap> list)
{
    document()->incDOMTreeVersion();

    // If setting the whole map changes the id attribute, we need to call updateId.

    Attribute* oldId = namedAttrMap ? namedAttrMap->getAttributeItem(idAttr) : 0;
    Attribute* newId = list ? list->getAttributeItem(idAttr) : 0;

    if (oldId || newId)
        updateId(oldId ? oldId->value() : nullAtom, newId ? newId->value() : nullAtom);

    if (namedAttrMap)
        namedAttrMap->m_element = 0;

    namedAttrMap = list;

    if (namedAttrMap) {
        namedAttrMap->m_element = this;
        unsigned len = namedAttrMap->length();
        for (unsigned i = 0; i < len; i++)
            attributeChanged(namedAttrMap->m_attributes[i].get());
        // FIXME: What about attributes that were in the old map that are not in the new map?
    }
}

bool Element::hasAttributes() const
{
    if (!m_isStyleAttributeValid)
        updateStyleAttribute();

#if ENABLE(SVG)
    if (!m_areSVGAttributesValid)
        updateAnimatedSVGAttribute(String());
#endif

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

KURL Element::baseURI() const
{
    KURL base(getAttribute(baseAttr));
    if (!base.protocol().isEmpty())
        return base;

    Node* parent = parentNode();
    if (!parent)
        return base;

    const KURL& parentBase = parent->baseURI();
    if (parentBase.isNull())
        return base;

    return KURL(parentBase, base.string());
}

void Element::createAttributeMap() const
{
    namedAttrMap = NamedAttrMap::create(const_cast<Element*>(this));
}

bool Element::isURLAttribute(Attribute*) const
{
    return false;
}

const QualifiedName& Element::imageSourceAttributeName() const
{
    return srcAttr;
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
        if (NamedAttrMap* attrs = namedAttrMap.get()) {
            Attribute* idItem = attrs->getAttributeItem(idAttr);
            if (idItem && !idItem->isNull())
                updateId(nullAtom, idItem->value());
        }
    }
}

void Element::removedFromDocument()
{
    if (hasID()) {
        if (NamedAttrMap* attrs = namedAttrMap.get()) {
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
    if (hasRareData()) {   
        ElementRareData* data = rareData();
        if (data->needsFocusAppearanceUpdateSoonAfterAttach()) {
            if (isFocusable() && document()->focusedNode() == this)
                document()->updateFocusAppearanceSoon();
            data->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
        }
    }
}

void Element::detach()
{
    cancelFocusAppearanceUpdate();
    if (hasRareData())
        rareData()->resetComputedStyle();
    ContainerNode::detach();
}

void Element::recalcStyle(StyleChange change)
{
    RenderStyle* currentStyle = renderStyle();
    bool hasParentStyle = parentNode() ? parentNode()->renderStyle() : false;
    bool hasPositionalRules = changed() && currentStyle && currentStyle->childrenAffectedByPositionalRules();
    bool hasDirectAdjacentRules = currentStyle && currentStyle->childrenAffectedByDirectAdjacentRules();

#if ENABLE(SVG)
    if (!hasParentStyle && isShadowNode() && isSVGElement())
        hasParentStyle = true;
#endif

    if ((change > NoChange || changed())) {
        if (hasRareData())
            rareData()->resetComputedStyle();
    }
    if (hasParentStyle && (change >= Inherit || changed())) {
        RefPtr<RenderStyle> newStyle = document()->styleSelector()->styleForElement(this);
        StyleChange ch = diff(currentStyle, newStyle.get());
        if (ch == Detach || !currentStyle) {
            if (attached())
                detach();
            attach(); // FIXME: The style gets computed twice by calling attach. We could do better if we passed the style along.
            // attach recalulates the style for all children. No need to do it twice.
            setChanged(NoStyleChange);
            setHasChangedChild(false);
            return;
        }

        if (currentStyle) {
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
            if (currentStyle->childrenAffectedByDirectAdjacentRules())
                newStyle->setChildrenAffectedByDirectAdjacentRules();
        }

        if (ch != NoChange) {
            setRenderStyle(newStyle);
        } else if (changed() && (styleChangeType() != AnimationStyleChange) && (document()->usesSiblingRules() || document()->usesDescendantRules())) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.  This is only necessary if the document actually uses
            // sibling/descendant rules, since otherwise it isn't possible for ancestor styles to affect sharing of
            // descendants.
            if (renderer())
                renderer()->setStyleInternal(newStyle.get());
            else
                setRenderStyle(newStyle);
        } else if (styleChangeType() == AnimationStyleChange)
             setRenderStyle(newStyle);

        if (change != Force) {
            if ((document()->usesDescendantRules() || hasPositionalRules) && styleChangeType() >= FullStyleChange)
                change = Force;
            else
                change = ch;
        }
    }

    // FIXME: This check is good enough for :hover + foo, but it is not good enough for :hover + foo + bar.
    // For now we will just worry about the common case, since it's a lot trickier to get the second case right
    // without doing way too much re-resolution.
    bool forceCheckOfNextElementSibling = false;
    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        bool childRulesChanged = n->changed() && n->styleChangeType() == FullStyleChange;
        if (forceCheckOfNextElementSibling && n->isElementNode())
            n->setChanged();
        if (change >= Inherit || n->isTextNode() || n->hasChangedChild() || n->changed())
            n->recalcStyle(change);
        if (n->isElementNode())
            forceCheckOfNextElementSibling = childRulesChanged && hasDirectAdjacentRules;
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

static void checkForSiblingStyleChanges(Element* e, RenderStyle* style, bool finishedParsingCallback,
                                        Node* beforeChange, Node* afterChange, int childCountDelta)
{
    if (!style || (e->changed() && style->childrenAffectedByPositionalRules()))
        return;

    // :first-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (style->childrenAffectedByFirstChildRules() && afterChange) {
        // Find our new first child.
        Node* newFirstChild = 0;
        for (newFirstChild = e->firstChild(); newFirstChild && !newFirstChild->isElementNode(); newFirstChild = newFirstChild->nextSibling()) {};
        
        // Find the first element node following |afterChange|
        Node* firstElementAfterInsertion = 0;
        for (firstElementAfterInsertion = afterChange;
             firstElementAfterInsertion && !firstElementAfterInsertion->isElementNode();
             firstElementAfterInsertion = firstElementAfterInsertion->nextSibling()) {};
        
        // This is the insert/append case.
        if (newFirstChild != firstElementAfterInsertion && firstElementAfterInsertion && firstElementAfterInsertion->attached() &&
            firstElementAfterInsertion->renderStyle() && firstElementAfterInsertion->renderStyle()->firstChildState())
            firstElementAfterInsertion->setChanged();
            
        // We also have to handle node removal.
        if (childCountDelta < 0 && newFirstChild == firstElementAfterInsertion && newFirstChild && newFirstChild->renderStyle() && !newFirstChild->renderStyle()->firstChildState())
            newFirstChild->setChanged();
    }

    // :last-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (style->childrenAffectedByLastChildRules() && beforeChange) {
        // Find our new last child.
        Node* newLastChild = 0;
        for (newLastChild = e->lastChild(); newLastChild && !newLastChild->isElementNode(); newLastChild = newLastChild->previousSibling()) {};
        
        // Find the last element node going backwards from |beforeChange|
        Node* lastElementBeforeInsertion = 0;
        for (lastElementBeforeInsertion = beforeChange;
             lastElementBeforeInsertion && !lastElementBeforeInsertion->isElementNode();
             lastElementBeforeInsertion = lastElementBeforeInsertion->previousSibling()) {};
        
        if (newLastChild != lastElementBeforeInsertion && lastElementBeforeInsertion && lastElementBeforeInsertion->attached() &&
            lastElementBeforeInsertion->renderStyle() && lastElementBeforeInsertion->renderStyle()->lastChildState())
            lastElementBeforeInsertion->setChanged();
            
        // We also have to handle node removal.  The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((childCountDelta < 0 || finishedParsingCallback) && newLastChild == lastElementBeforeInsertion && newLastChild && newLastChild->renderStyle() && !newLastChild->renderStyle()->lastChildState())
            newLastChild->setChanged();
    }

    // The + selector.  We need to invalidate the first element following the insertion point.  It is the only possible element
    // that could be affected by this DOM change.
    if (style->childrenAffectedByDirectAdjacentRules() && afterChange) {
        Node* firstElementAfterInsertion = 0;
        for (firstElementAfterInsertion = afterChange;
             firstElementAfterInsertion && !firstElementAfterInsertion->isElementNode();
             firstElementAfterInsertion = firstElementAfterInsertion->nextSibling()) {};
        if (firstElementAfterInsertion && firstElementAfterInsertion->attached())
            firstElementAfterInsertion->setChanged();
    }

    // Forward positional selectors include the ~ selector, nth-child, nth-of-type, first-of-type and only-of-type.
    // Backward positional selectors include nth-last-child, nth-last-of-type, last-of-type and only-of-type.
    // We have to invalidate everything following the insertion point in the forward case, and everything before the insertion point in the
    // backward case.
    // |afterChange| is 0 in the parser callback case, so we won't do any work for the forward case if we don't have to.
    // For performance reasons we just mark the parent node as changed, since we don't want to make childrenChanged O(n^2) by crawling all our kids
    // here.  recalcStyle will then force a walk of the children when it sees that this has happened.
    if ((style->childrenAffectedByForwardPositionalRules() && afterChange) ||
        (style->childrenAffectedByBackwardPositionalRules() && beforeChange))
        e->setChanged();
    
    // :empty selector.
    if (style->affectedByEmpty() && (!style->emptyState() || e->hasChildNodes()))
        e->setChanged();
}

void Element::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (!changedByParser)
        checkForSiblingStyleChanges(this, renderStyle(), false, beforeChange, afterChange, childCountDelta);
}

void Element::finishParsingChildren()
{
    ContainerNode::finishParsingChildren();
    m_parsingChildrenFinished = true;
    checkForSiblingStyleChanges(this, renderStyle(), true, lastChild(), 0, 0);
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

void Element::dispatchAttrAdditionEvent(Attribute*)
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
void Element::formatForDebugger(char* buffer, unsigned length) const
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
          
    strncpy(buffer, result.utf8().data(), length - 1);
}
#endif

PassRefPtr<Attr> Element::setAttributeNode(Attr* attr, ExceptionCode& ec)
{
    if (!attr) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    return static_pointer_cast<Attr>(attributes(false)->setNamedItem(attr, ec));
}

PassRefPtr<Attr> Element::setAttributeNodeNS(Attr* attr, ExceptionCode& ec)
{
    if (!attr) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    return static_pointer_cast<Attr>(attributes(false)->setNamedItem(attr, ec));
}

PassRefPtr<Attr> Element::removeAttributeNode(Attr* attr, ExceptionCode& ec)
{
    if (!attr) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    if (attr->ownerElement() != this) {
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

void Element::setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionCode& ec)
{
    String prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName, ec))
        return;

    QualifiedName qName(prefix, localName, namespaceURI);
    setAttribute(qName, value, ec);
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

    // This call to String::lower() seems to be required but
    // there may be a way to remove it.
    String localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    return attrs->getAttributeItem(localName, false);
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
        ensureRareData()->setNeedsFocusAppearanceUpdateSoonAfterAttach(true);
        return;
    }
        
    cancelFocusAppearanceUpdate();
    updateFocusAppearance(restorePreviousSelection);
}

void Element::updateFocusAppearance(bool /*restorePreviousSelection*/)
{
    if (this == rootEditableElement()) { 
        Frame* frame = document()->frame();
        if (!frame)
            return;

        // FIXME: We should restore the previous selection if there is one.
        Selection newSelection = hasTagName(htmlTag) || hasTagName(bodyTag) ? Selection(Position(this, 0), DOWNSTREAM) : Selection::selectionFromContentsOfNode(this);
        
        if (frame->shouldChangeSelection(newSelection)) {
            frame->selection()->setSelection(newSelection);
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
    return hasRareData() ? rareData()->m_minimumSizeForResizing : defaultMinimumSizeForResizing();
}

void Element::setMinimumSizeForResizing(const IntSize& size)
{
    if (size == defaultMinimumSizeForResizing() && !hasRareData())
        return;
    ensureRareData()->m_minimumSizeForResizing = size;
}

RenderStyle* Element::computedStyle()
{
    if (RenderStyle* usedStyle = renderStyle())
        return usedStyle;

    if (!attached())
        // FIXME: Try to do better than this. Ensure that styleForElement() works for elements that are not in the
        // document tree and figure out when to destroy the computed style for such elements.
        return 0;

    ElementRareData* data = ensureRareData();
    if (!data->m_computedStyle)
        data->m_computedStyle = document()->styleSelector()->styleForElement(this, parent() ? parent()->computedStyle() : 0);
    return data->m_computedStyle.get();
}

void Element::cancelFocusAppearanceUpdate()
{
    if (hasRareData())
        rareData()->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
    if (document()->focusedNode() == this)
        document()->cancelFocusAppearanceUpdate();
}

void Element::normalizeAttributes()
{
    // Normalize attributes.
    NamedAttrMap* attrs = attributes(true);
    if (!attrs)
        return;
    unsigned numAttrs = attrs->length();
    for (unsigned i = 0; i < numAttrs; i++) {
        if (Attr* attr = attrs->attributeItem(i)->attr())
            attr->normalize();
    }
}

// ElementTraversal API
Element* Element::firstElementChild() const
{
    Node* n = firstChild();
    while (n && !n->isElementNode())
        n = n->nextSibling();
    return static_cast<Element*>(n);
}

Element* Element::lastElementChild() const
{
    Node* n = lastChild();
    while (n && !n->isElementNode())
        n = n->previousSibling();
    return static_cast<Element*>(n);
}

Element* Element::previousElementSibling() const
{
    Node* n = previousSibling();
    while (n && !n->isElementNode())
        n = n->previousSibling();
    return static_cast<Element*>(n);
}

Element* Element::nextElementSibling() const
{
    Node* n = nextSibling();
    while (n && !n->isElementNode())
        n = n->nextSibling();
    return static_cast<Element*>(n);
}

unsigned Element::childElementCount() const
{
    unsigned count = 0;
    Node* n = firstChild();
    while (n) {
        count += n->isElementNode();
        n = n->nextSibling();
    }
    return count;
}

}

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
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
#include "Attr.h"
#include "CSSParser.h"
#include "CSSSelectorList.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClassList.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "ContainerNodeAlgorithms.h"
#include "CustomElementRegistry.h"
#include "DOMTokenList.h"
#include "DatasetDOMStringMap.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentSharedObjectPool.h"
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "EventDispatcher.h"
#include "ExceptionCode.h"
#include "FlowThreadController.h"
#include "FocusController.h"
#include "FocusEvent.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNameCollection.h"
#include "HTMLNames.h"
#include "HTMLOptionsCollection.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableRowsCollection.h"
#include "InsertionPoint.h"
#include "InspectorInstrumentation.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "NamedNodeMap.h"
#include "NodeList.h"
#include "NodeRenderStyle.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "PointerLockController.h"
#include "PseudoElement.h"
#include "RenderRegion.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SelectorQuery.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StylePropertySet.h"
#include "StyleResolver.h"
#include "Text.h"
#include "TextIterator.h"
#include "VoidCallback.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include <wtf/BitVector.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;
using namespace XMLNames;

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
}

typedef Vector<RefPtr<Attr> > AttrNodeList;
typedef HashMap<Element*, OwnPtr<AttrNodeList> > AttrNodeListMap;

static AttrNodeListMap& attrNodeListMap()
{
    DEFINE_STATIC_LOCAL(AttrNodeListMap, map, ());
    return map;
}

static AttrNodeList* attrNodeListForElement(Element* element)
{
    if (!element->hasSyntheticAttrChildNodes())
        return 0;
    ASSERT(attrNodeListMap().contains(element));
    return attrNodeListMap().get(element);
}

static AttrNodeList& ensureAttrNodeListForElement(Element* element)
{
    if (element->hasSyntheticAttrChildNodes()) {
        ASSERT(attrNodeListMap().contains(element));
        return *attrNodeListMap().get(element);
    }
    ASSERT(!attrNodeListMap().contains(element));
    element->setHasSyntheticAttrChildNodes(true);
    AttrNodeListMap::AddResult result = attrNodeListMap().add(element, adoptPtr(new AttrNodeList));
    return *result.iterator->value;
}

static void removeAttrNodeListForElement(Element* element)
{
    ASSERT(element->hasSyntheticAttrChildNodes());
    ASSERT(attrNodeListMap().contains(element));
    attrNodeListMap().remove(element);
    element->setHasSyntheticAttrChildNodes(false);
}

static Attr* findAttrNodeInList(AttrNodeList& attrNodeList, const QualifiedName& name)
{
    for (unsigned i = 0; i < attrNodeList.size(); ++i) {
        if (attrNodeList.at(i)->qualifiedName() == name)
            return attrNodeList.at(i).get();
    }
    return 0;
}

PassRefPtr<Element> Element::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new Element(tagName, document, CreateElement));
}

Element::~Element()
{
#ifndef NDEBUG
    if (document() && document()->renderer()) {
        // When the document is not destroyed, an element that was part of a named flow
        // content nodes should have been removed from the content nodes collection
        // and the inNamedFlow flag reset.
        ASSERT(!inNamedFlow());
    }
#endif

    if (hasRareData()) {
        ElementRareData* data = elementRareData();
        data->setPseudoElement(BEFORE, 0);
        data->setPseudoElement(AFTER, 0);
        removeShadowRoot();
    }

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

#if ENABLE(SVG)
    if (hasPendingResources()) {
        document()->accessSVGExtensions()->removeElementFromPendingResources(this);
        ASSERT(!hasPendingResources());
    }
#endif
}

inline ElementRareData* Element::elementRareData() const
{
    ASSERT(hasRareData());
    return static_cast<ElementRareData*>(rareData());
}

inline ElementRareData& Element::ensureElementRareData()
{
    return static_cast<ElementRareData&>(ensureRareData());
}

void Element::clearTabIndexExplicitlyIfNeeded()
{
    if (hasRareData())
        elementRareData()->clearTabIndexExplicitly();
}

void Element::setTabIndexExplicitly(short tabIndex)
{
    ensureElementRareData().setTabIndexExplicitly(tabIndex);
}

bool Element::supportsFocus() const
{
    return hasRareData() && elementRareData()->tabIndexSetExplicitly();
}

Element* Element::focusDelegate()
{
    return this;
}

short Element::tabIndex() const
{
    return hasRareData() ? elementRareData()->tabIndex() : 0;
}

bool Element::isKeyboardFocusable(KeyboardEvent*) const
{
    return isFocusable() && tabIndex() >= 0;
}

bool Element::isMouseFocusable() const
{
    return isFocusable();
}

bool Element::shouldUseInputMethod()
{
    return isContentEditable(UserSelectAllIsAlwaysNonEditable);
}

void Element::dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions eventOptions, SimulatedClickVisualOptions visualOptions)
{
    EventDispatcher::dispatchSimulatedClick(this, underlyingEvent, eventOptions, visualOptions);
}

DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, blur);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, error);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, focus);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, load);

PassRefPtr<Node> Element::cloneNode(bool deep)
{
    return deep ? cloneElementWithChildren() : cloneElementWithoutChildren();
}

PassRefPtr<Element> Element::cloneElementWithChildren()
{
    RefPtr<Element> clone = cloneElementWithoutChildren();
    cloneChildNodes(clone.get());
    return clone.release();
}

PassRefPtr<Element> Element::cloneElementWithoutChildren()
{
    RefPtr<Element> clone = cloneElementWithoutAttributesAndChildren();
    // This will catch HTML elements in the wrong namespace that are not correctly copied.
    // This is a sanity check as HTML overloads some of the DOM methods.
    ASSERT(isHTMLElement() == clone->isHTMLElement());

    clone->cloneDataFromElement(*this);
    return clone.release();
}

PassRefPtr<Element> Element::cloneElementWithoutAttributesAndChildren()
{
    return document()->createElement(tagQName(), false);
}

PassRefPtr<Attr> Element::detachAttribute(unsigned index)
{
    ASSERT(elementData());

    const Attribute& attribute = elementData()->attributeAt(index);

    RefPtr<Attr> attrNode = attrIfExists(attribute.name());
    if (attrNode)
        detachAttrNodeFromElementWithValue(attrNode.get(), attribute.value());
    else
        attrNode = Attr::create(document(), attribute.name(), attribute.value());

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return attrNode.release();
}

void Element::removeAttribute(const QualifiedName& name)
{
    if (!elementData())
        return;

    unsigned index = elementData()->findAttributeIndexByName(name);
    if (index == ElementData::attributeNotFound)
        return;

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::setBooleanAttribute(const QualifiedName& name, bool value)
{
    if (value)
        setAttribute(name, emptyAtom);
    else
        removeAttribute(name);
}

NamedNodeMap* Element::attributes() const
{
    ElementRareData& rareData = const_cast<Element*>(this)->ensureElementRareData();
    if (NamedNodeMap* attributeMap = rareData.attributeMap())
        return attributeMap;

    rareData.setAttributeMap(NamedNodeMap::create(const_cast<Element*>(this)));
    return rareData.attributeMap();
}

Node::NodeType Element::nodeType() const
{
    return ELEMENT_NODE;
}

bool Element::hasAttribute(const QualifiedName& name) const
{
    return hasAttributeNS(name.namespaceURI(), name.localName());
}

void Element::synchronizeAllAttributes() const
{
    if (!elementData())
        return;
    if (elementData()->m_styleAttributeIsDirty) {
        ASSERT(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
    }
#if ENABLE(SVG)
    if (elementData()->m_animatedSVGAttributesAreDirty) {
        ASSERT(isSVGElement());
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(anyQName());
    }
#endif
}

inline void Element::synchronizeAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return;
    if (UNLIKELY(name == styleAttr && elementData()->m_styleAttributeIsDirty)) {
        ASSERT(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
        return;
    }
#if ENABLE(SVG)
    if (UNLIKELY(elementData()->m_animatedSVGAttributesAreDirty)) {
        ASSERT(isSVGElement());
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(name);
    }
#endif
}

inline void Element::synchronizeAttribute(const AtomicString& localName) const
{
    // This version of synchronizeAttribute() is streamlined for the case where you don't have a full QualifiedName,
    // e.g when called from DOM API.
    if (!elementData())
        return;
    if (elementData()->m_styleAttributeIsDirty && equalPossiblyIgnoringCase(localName, styleAttr.localName(), shouldIgnoreAttributeCase(this))) {
        ASSERT(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
        return;
    }
#if ENABLE(SVG)
    if (elementData()->m_animatedSVGAttributesAreDirty) {
        // We're not passing a namespace argument on purpose. SVGNames::*Attr are defined w/o namespaces as well.
        ASSERT(isSVGElement());
        static_cast<const SVGElement*>(this)->synchronizeAnimatedSVGAttribute(QualifiedName(nullAtom, localName, nullAtom));
    }
#endif
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return nullAtom;
    synchronizeAttribute(name);
    if (const Attribute* attribute = findAttributeByName(name))
        return attribute->value();
    return nullAtom;
}

bool Element::isFocusable() const
{
    if (!inDocument() || !supportsFocus())
        return false;

    // Elements in canvas fallback content are not rendered, but they are allowed to be
    // focusable as long as their canvas is displayed and visible.
    if (isInCanvasSubtree()) {
        const Element* e = this;
        while (e && !e->hasLocalName(canvasTag))
            e = e->parentElement();
        ASSERT(e);
        return e->renderer() && e->renderer()->style()->visibility() == VISIBLE;
    }

    if (renderer())
        ASSERT(!renderer()->needsLayout());
    else {
        // If the node is in a display:none tree it might say it needs style recalc but
        // the whole document is actually up to date.
        ASSERT(!document()->childNeedsStyleRecalc());
    }

    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Hyatt wants to fix that some day with a "has visible content" flag or the like.
    if (!renderer() || renderer()->style()->visibility() != VISIBLE)
        return false;

    return true;
}

bool Element::isUserActionElementInActiveChain() const
{
    ASSERT(isUserActionElement());
    return document()->userActionElements().isInActiveChain(this);
}

bool Element::isUserActionElementActive() const
{
    ASSERT(isUserActionElement());
    return document()->userActionElements().isActive(this);
}

bool Element::isUserActionElementFocused() const
{
    ASSERT(isUserActionElement());
    return document()->userActionElements().isFocused(this);
}

bool Element::isUserActionElementHovered() const
{
    ASSERT(isUserActionElement());
    return document()->userActionElements().isHovered(this);
}

void Element::setActive(bool flag, bool pause)
{
    if (flag == active())
        return;

    if (Document* document = this->document())
        document->userActionElements().setActive(this, flag);

    if (!renderer())
        return;

    bool reactsToPress = renderStyle()->affectedByActive() || childrenAffectedByActive();
    if (reactsToPress)
        setNeedsStyleRecalc();

    if (renderer()->style()->hasAppearance() && renderer()->theme()->stateChanged(renderer(), PressedState))
        reactsToPress = true;

    // The rest of this function implements a feature that only works if the
    // platform supports immediate invalidations on the ChromeClient, so bail if
    // that isn't supported.
    if (!document()->page()->chrome().client().supportsImmediateInvalidation())
        return;

    if (reactsToPress && pause) {
        // The delay here is subtle. It relies on an assumption, namely that the amount of time it takes
        // to repaint the "down" state of the control is about the same time as it would take to repaint the
        // "up" state. Once you assume this, you can just delay for 100ms - that time (assuming that after you
        // leave this method, it will be about that long before the flush of the up state happens again).
#ifdef HAVE_FUNC_USLEEP
        double startTime = monotonicallyIncreasingTime();
#endif

        document()->updateStyleIfNeeded();

        // Do an immediate repaint.
        if (renderer())
            renderer()->repaint(true);

        // FIXME: Come up with a less ridiculous way of doing this.
#ifdef HAVE_FUNC_USLEEP
        // Now pause for a small amount of time (1/10th of a second from before we repainted in the pressed state)
        double remainingTime = 0.1 - (monotonicallyIncreasingTime() - startTime);
        if (remainingTime > 0)
            usleep(static_cast<useconds_t>(remainingTime * 1000000.0));
#endif
    }
}

void Element::setFocus(bool flag)
{
    if (flag == focused())
        return;

    if (Document* document = this->document())
        document->userActionElements().setFocused(this, flag);

    setNeedsStyleRecalc();
}

void Element::setHovered(bool flag)
{
    if (flag == hovered())
        return;

    if (Document* document = this->document())
        document->userActionElements().setHovered(this, flag);

    if (!renderer()) {
        // When setting hover to false, the style needs to be recalc'd even when
        // there's no renderer (imagine setting display:none in the :hover class,
        // if a nil renderer would prevent this element from recalculating its
        // style, it would never go back to its normal style and remain
        // stuck in its hovered style).
        if (!flag)
            setNeedsStyleRecalc();

        return;
    }

    if (renderer()->style()->affectedByHover() || childrenAffectedByHover())
        setNeedsStyleRecalc();

    if (renderer()->style()->hasAppearance())
        renderer()->theme()->stateChanged(renderer(), HoverState);
}

void Element::scrollIntoView(bool alignToTop) 
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = boundingBox();
    // Align to the top / bottom and to the closest edge.
    if (alignToTop)
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);
    else
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignBottomAlways);
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = boundingBox();
    if (centerIfNeeded)
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);
    else
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

void Element::scrollByUnits(int units, ScrollGranularity granularity)
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    if (!renderer()->hasOverflowClip())
        return;

    ScrollDirection direction = ScrollDown;
    if (units < 0) {
        direction = ScrollUp;
        units = -units;
    }
    Node* stopNode = this;
    toRenderBox(renderer())->scroll(direction, granularity, units, &stopNode);
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
    float zoomFactor = 1;
    if (renderer->style()->effectiveZoom() != 1) {
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

static int adjustForLocalZoom(LayoutUnit value, RenderObject* renderer)
{
    float zoomFactor = localZoomForRenderer(renderer);
    if (zoomFactor == 1)
        return value;
#if ENABLE(SUBPIXEL_LAYOUT)
    return lroundf(value / zoomFactor);
#else
    // Needed because computeLengthInt truncates (rather than rounds) when scaling up.
    if (zoomFactor > 1)
        value++;
    return static_cast<int>(value / zoomFactor);
#endif
}

int Element::offsetLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustForLocalZoom(renderer->pixelSnappedOffsetLeft(), renderer);
    return 0;
}

int Element::offsetTop()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustForLocalZoom(renderer->pixelSnappedOffsetTop(), renderer);
    return 0;
}

int Element::offsetWidth()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
#if ENABLE(SUBPIXEL_LAYOUT)
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedOffsetWidth(), renderer).round();
#else
        return adjustForAbsoluteZoom(renderer->pixelSnappedOffsetWidth(), renderer);
#endif
    return 0;
}

int Element::offsetHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
#if ENABLE(SUBPIXEL_LAYOUT)
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedOffsetHeight(), renderer).round();
#else
        return adjustForAbsoluteZoom(renderer->pixelSnappedOffsetHeight(), renderer);
#endif
    return 0;
}

Element* Element::bindingsOffsetParent()
{
    Element* element = offsetParent();
    if (!element || !element->isInShadowTree())
        return element;
    return element->containingShadowRoot()->type() == ShadowRoot::UserAgentShadowRoot ? 0 : element;
}

Element* Element::offsetParent()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* renderer = this->renderer()) {
        if (RenderObject* offsetParent = renderer->offsetParent())
            return toElement(offsetParent->node());
    }
    return 0;
}

int Element::clientLeft()
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (RenderBox* renderer = renderBox())
        return adjustForAbsoluteZoom(roundToInt(renderer->clientLeft()), renderer);
    return 0;
}

int Element::clientTop()
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (RenderBox* renderer = renderBox())
        return adjustForAbsoluteZoom(roundToInt(renderer->clientTop()), renderer);
    return 0;
}

int Element::clientWidth()
{
    document()->updateLayoutIgnorePendingStylesheets();

    // When in strict mode, clientWidth for the document element should return the width of the containing frame.
    // When in quirks mode, clientWidth for the body element should return the width of the containing frame.
    bool inQuirksMode = document()->inQuirksMode();
    if ((!inQuirksMode && document()->documentElement() == this) ||
        (inQuirksMode && isHTMLElement() && document()->body() == this)) {
        if (FrameView* view = document()->view()) {
            if (RenderView* renderView = document()->renderView())
                return adjustForAbsoluteZoom(view->layoutWidth(), renderView);
        }
    }
    
    if (RenderBox* renderer = renderBox())
#if ENABLE(SUBPIXEL_LAYOUT)
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedClientWidth(), renderer).round();
#else
        return adjustForAbsoluteZoom(renderer->pixelSnappedClientWidth(), renderer);
#endif
    return 0;
}

int Element::clientHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();

    // When in strict mode, clientHeight for the document element should return the height of the containing frame.
    // When in quirks mode, clientHeight for the body element should return the height of the containing frame.
    bool inQuirksMode = document()->inQuirksMode();     

    if ((!inQuirksMode && document()->documentElement() == this) ||
        (inQuirksMode && isHTMLElement() && document()->body() == this)) {
        if (FrameView* view = document()->view()) {
            if (RenderView* renderView = document()->renderView())
                return adjustForAbsoluteZoom(view->layoutHeight(), renderView);
        }
    }
    
    if (RenderBox* renderer = renderBox())
#if ENABLE(SUBPIXEL_LAYOUT)
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedClientHeight(), renderer).round();
#else
        return adjustForAbsoluteZoom(renderer->pixelSnappedClientHeight(), renderer);
#endif
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

IntRect Element::boundsInRootViewSpace()
{
    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = document()->view();
    if (!view)
        return IntRect();

    Vector<FloatQuad> quads;
#if ENABLE(SVG)
    if (isSVGElement() && renderer()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement* svgElement = toSVGElement(this);
        FloatRect localRect;
        if (svgElement->getBoundingBox(localRect))
            quads.append(renderer()->localToAbsoluteQuad(localRect));
    } else
#endif
    {
        // Get the bounding rectangle from the box model.
        if (renderBoxModelObject())
            renderBoxModelObject()->absoluteQuads(quads);
    }

    if (quads.isEmpty())
        return IntRect();

    IntRect result = quads[0].enclosingBoundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].enclosingBoundingBox());

    result = view->contentsToRootView(result);
    return result;
}

PassRefPtr<ClientRectList> Element::getClientRects()
{
    document()->updateLayoutIgnorePendingStylesheets();

    RenderBoxModelObject* renderBoxModelObject = this->renderBoxModelObject();
    if (!renderBoxModelObject)
        return ClientRectList::create();

    // FIXME: Handle SVG elements.
    // FIXME: Handle table/inline-table with a caption.

    Vector<FloatQuad> quads;
    renderBoxModelObject->absoluteQuads(quads);
    document()->adjustFloatQuadsForScrollAndAbsoluteZoomAndFrameScale(quads, renderBoxModelObject);
    return ClientRectList::create(quads);
}

PassRefPtr<ClientRect> Element::getBoundingClientRect()
{
    document()->updateLayoutIgnorePendingStylesheets();

    Vector<FloatQuad> quads;
#if ENABLE(SVG)
    if (isSVGElement() && renderer() && !renderer()->isSVGRoot()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement* svgElement = toSVGElement(this);
        FloatRect localRect;
        if (svgElement->getBoundingBox(localRect))
            quads.append(renderer()->localToAbsoluteQuad(localRect));
    } else
#endif
    {
        // Get the bounding rectangle from the box model.
        if (renderBoxModelObject())
            renderBoxModelObject()->absoluteQuads(quads);
    }

    if (quads.isEmpty())
        return ClientRect::create();

    FloatRect result = quads[0].boundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].boundingBox());

    document()->adjustFloatRectForScrollAndAbsoluteZoomAndFrameScale(result, renderer());
    return ClientRect::create(result);
}
    
IntRect Element::screenRect() const
{
    if (!renderer())
        return IntRect();
    // FIXME: this should probably respect transforms
    return document()->view()->contentsToScreen(renderer()->absoluteBoundingBoxRectIgnoringTransforms());
}

const AtomicString& Element::getAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return nullAtom;
    synchronizeAttribute(localName);
    if (const Attribute* attribute = elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase(this)))
        return attribute->value();
    return nullAtom;
}

const AtomicString& Element::getAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return getAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

void Element::setAttribute(const AtomicString& localName, const AtomicString& value, ExceptionCode& ec)
{
    if (!Document::isValidName(localName)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }

    synchronizeAttribute(localName);
    const AtomicString& caseAdjustedLocalName = shouldIgnoreAttributeCase(this) ? localName.lower() : localName;

    unsigned index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedLocalName, false) : ElementData::attributeNotFound;
    const QualifiedName& qName = index != ElementData::attributeNotFound ? attributeAt(index).name() : QualifiedName(nullAtom, caseAdjustedLocalName, nullAtom);
    setAttributeInternal(index, qName, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value)
{
    synchronizeAttribute(name);
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setSynchronizedLazyAttribute(const QualifiedName& name, const AtomicString& value)
{
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute);
}

inline void Element::setAttributeInternal(unsigned index, const QualifiedName& name, const AtomicString& newValue, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (newValue.isNull()) {
        if (index != ElementData::attributeNotFound)
            removeAttributeInternal(index, inSynchronizationOfLazyAttribute);
        return;
    }

    if (index == ElementData::attributeNotFound) {
        addAttributeInternal(name, newValue, inSynchronizationOfLazyAttribute);
        return;
    }

    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, attributeAt(index).value(), newValue);

    if (newValue != attributeAt(index).value()) {
        // If there is an Attr node hooked to this attribute, the Attr::setValue() call below
        // will write into the ElementData.
        // FIXME: Refactor this so it makes some sense.
        if (RefPtr<Attr> attrNode = inSynchronizationOfLazyAttribute ? 0 : attrIfExists(name))
            attrNode->setValue(newValue);
        else
            ensureUniqueElementData().attributeAt(index).setValue(newValue);
    }

    if (!inSynchronizationOfLazyAttribute)
        didModifyAttribute(name, newValue);
}

static inline AtomicString makeIdForStyleResolution(const AtomicString& value, bool inQuirksMode)
{
    if (inQuirksMode)
        return value.lower();
    return value;
}

static bool checkNeedsStyleInvalidationForIdChange(const AtomicString& oldId, const AtomicString& newId, StyleResolver* styleResolver)
{
    ASSERT(newId != oldId);
    if (!oldId.isEmpty() && styleResolver->hasSelectorForId(oldId))
        return true;
    if (!newId.isEmpty() && styleResolver->hasSelectorForId(newId))
        return true;
    return false;
}

void Element::attributeChanged(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason)
{
    parseAttribute(name, newValue);

    document()->incDOMTreeVersion();

    StyleResolver* styleResolver = document()->styleResolverIfExists();
    bool testShouldInvalidateStyle = attached() && styleResolver && styleChangeType() < FullStyleChange;
    bool shouldInvalidateStyle = false;

    if (isIdAttributeName(name)) {
        AtomicString oldId = elementData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document()->inQuirksMode());
        if (newId != oldId) {
            elementData()->setIdForStyleResolution(newId);
            shouldInvalidateStyle = testShouldInvalidateStyle && checkNeedsStyleInvalidationForIdChange(oldId, newId, styleResolver);
        }
    } else if (name == classAttr)
        classAttributeChanged(newValue);
    else if (name == HTMLNames::nameAttr)
        elementData()->m_hasNameAttribute = !newValue.isNull();
    else if (name == HTMLNames::pseudoAttr)
        shouldInvalidateStyle |= testShouldInvalidateStyle && isInShadowTree();


    invalidateNodeListCachesInAncestors(&name, this);

    // If there is currently no StyleResolver, we can't be sure that this attribute change won't affect style.
    shouldInvalidateStyle |= !styleResolver;

    if (shouldInvalidateStyle)
        setNeedsStyleRecalc();

    if (AXObjectCache* cache = document()->existingAXObjectCache())
        cache->handleAttributeChanged(name, this);
}

inline void Element::attributeChangedFromParserOrByCloning(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason reason)
{
#if ENABLE(CUSTOM_ELEMENTS)
    if (name == isAttr) {
        if (CustomElementRegistry* registry = document()->registry())
            registry->didGiveTypeExtension(this);
    }
#endif
    attributeChanged(name, newValue, reason);
}

template <typename CharacterType>
static inline bool classStringHasClassName(const CharacterType* characters, unsigned length)
{
    ASSERT(length > 0);

    unsigned i = 0;
    do {
        if (isNotHTMLSpace(characters[i]))
            break;
        ++i;
    } while (i < length);

    return i < length;
}

static inline bool classStringHasClassName(const AtomicString& newClassString)
{
    unsigned length = newClassString.length();

    if (!length)
        return false;

    if (newClassString.is8Bit())
        return classStringHasClassName(newClassString.characters8(), length);
    return classStringHasClassName(newClassString.characters16(), length);
}

template<typename Checker>
static bool checkSelectorForClassChange(const SpaceSplitString& changedClasses, const Checker& checker)
{
    unsigned changedSize = changedClasses.size();
    for (unsigned i = 0; i < changedSize; ++i) {
        if (checker.hasSelectorForClass(changedClasses[i]))
            return true;
    }
    return false;
}

template<typename Checker>
static bool checkSelectorForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, const Checker& checker)
{
    unsigned oldSize = oldClasses.size();
    if (!oldSize)
        return checkSelectorForClassChange(newClasses, checker);
    BitVector remainingClassBits;
    remainingClassBits.ensureSize(oldSize);
    // Class vectors tend to be very short. This is faster than using a hash table.
    unsigned newSize = newClasses.size();
    for (unsigned i = 0; i < newSize; ++i) {
        for (unsigned j = 0; j < oldSize; ++j) {
            if (newClasses[i] == oldClasses[j]) {
                remainingClassBits.quickSet(j);
                continue;
            }
        }
        if (checker.hasSelectorForClass(newClasses[i]))
            return true;
    }
    for (unsigned i = 0; i < oldSize; ++i) {
        // If the bit is not set the the corresponding class has been removed.
        if (remainingClassBits.quickGet(i))
            continue;
        if (checker.hasSelectorForClass(oldClasses[i]))
            return true;
    }
    return false;
}

void Element::classAttributeChanged(const AtomicString& newClassString)
{
    StyleResolver* styleResolver = document()->styleResolverIfExists();
    bool testShouldInvalidateStyle = attached() && styleResolver && styleChangeType() < FullStyleChange;
    bool shouldInvalidateStyle = false;

    if (classStringHasClassName(newClassString)) {
        const bool shouldFoldCase = document()->inQuirksMode();
        const SpaceSplitString oldClasses = ensureUniqueElementData().classNames();
        elementData()->setClass(newClassString, shouldFoldCase);
        const SpaceSplitString& newClasses = elementData()->classNames();
        shouldInvalidateStyle = testShouldInvalidateStyle && checkSelectorForClassChange(oldClasses, newClasses, *styleResolver);
    } else if (elementData()) {
        const SpaceSplitString& oldClasses = elementData()->classNames();
        shouldInvalidateStyle = testShouldInvalidateStyle && checkSelectorForClassChange(oldClasses, *styleResolver);
        elementData()->clearClass();
    }

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();

    if (shouldInvalidateStyle)
        setNeedsStyleRecalc();
}

// Returns true is the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

static inline bool isEventHandlerAttribute(const Attribute& attribute)
{
    return attribute.name().namespaceURI().isNull() && attribute.name().localName().startsWith("on");
}

bool Element::isJavaScriptURLAttribute(const Attribute& attribute) const
{
    return isURLAttribute(attribute) && protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(attribute.value()));
}

void Element::stripScriptingAttributes(Vector<Attribute>& attributeVector) const
{
    size_t destination = 0;
    for (size_t source = 0; source < attributeVector.size(); ++source) {
        if (isEventHandlerAttribute(attributeVector[source])
            || isJavaScriptURLAttribute(attributeVector[source])
            || isHTMLContentAttribute(attributeVector[source]))
            continue;

        if (source != destination)
            attributeVector[destination] = attributeVector[source];

        ++destination;
    }
    attributeVector.shrink(destination);
}

void Element::parserSetAttributes(const Vector<Attribute>& attributeVector)
{
    ASSERT(!inDocument());
    ASSERT(!parentNode());
    ASSERT(!m_elementData);

    if (attributeVector.isEmpty())
        return;

    if (document() && document()->sharedObjectPool())
        m_elementData = document()->sharedObjectPool()->cachedShareableElementDataWithAttributes(attributeVector);
    else
        m_elementData = ShareableElementData::createWithAttributes(attributeVector);

    // Use attributeVector instead of m_elementData because attributeChanged might modify m_elementData.
    for (unsigned i = 0; i < attributeVector.size(); ++i)
        attributeChangedFromParserOrByCloning(attributeVector[i].name(), attributeVector[i].value(), ModifiedDirectly);
}

bool Element::hasAttributes() const
{
    synchronizeAllAttributes();
    return elementData() && elementData()->length();
}

bool Element::hasEquivalentAttributes(const Element* other) const
{
    synchronizeAllAttributes();
    other->synchronizeAllAttributes();
    if (elementData() == other->elementData())
        return true;
    if (elementData())
        return elementData()->isEquivalent(other->elementData());
    if (other->elementData())
        return other->elementData()->isEquivalent(elementData());
    return true;
}

String Element::nodeName() const
{
    return m_tagName.toString();
}

String Element::nodeNamePreservingCase() const
{
    return m_tagName.toString();
}

void Element::setPrefix(const AtomicString& prefix, ExceptionCode& ec)
{
    ec = 0;
    checkSetPrefix(prefix, ec);
    if (ec)
        return;

    m_tagName.setPrefix(prefix.isEmpty() ? AtomicString() : prefix);
}

KURL Element::baseURI() const
{
    const AtomicString& baseAttribute = getAttribute(baseAttr);
    KURL base(KURL(), baseAttribute);
    if (!base.protocol().isEmpty())
        return base;

    ContainerNode* parent = parentNode();
    if (!parent)
        return base;

    const KURL& parentBase = parent->baseURI();
    if (parentBase.isNull())
        return base;

    return KURL(parentBase, baseAttribute);
}

const AtomicString& Element::imageSourceURL() const
{
    return getAttribute(srcAttr);
}

bool Element::rendererIsNeeded(const RenderStyle& style)
{
    return style.display() != NONE;
}

RenderObject* Element::createRenderer(RenderArena*, RenderStyle* style)
{
    return RenderObject::createObject(this, style);
}

bool Element::isDisabledFormControl() const
{
#if ENABLE(DIALOG_ELEMENT)
    // FIXME: disabled and inert are separate concepts in the spec, but now we treat them as the same.
    // For example, an inert, non-disabled form control should not be grayed out.
    if (isInert())
        return true;
#endif
    return false;
}

#if ENABLE(DIALOG_ELEMENT)
bool Element::isInert() const
{
    Element* dialog = document()->activeModalDialog();
    return dialog && !containsIncludingShadowDOM(dialog) && !dialog->containsIncludingShadowDOM(this);
}
#endif

Node::InsertionNotificationRequest Element::insertedInto(ContainerNode* insertionPoint)
{
    bool wasInDocument = inDocument();
    // need to do superclass processing first so inDocument() is true
    // by the time we reach updateId
    ContainerNode::insertedInto(insertionPoint);
    ASSERT(!wasInDocument || inDocument());

#if ENABLE(FULLSCREEN_API)
    if (containsFullScreenElement() && parentElement() && !parentElement()->containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);
#endif

    if (!insertionPoint->isInTreeScope())
        return InsertionDone;

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();

    TreeScope* newScope = insertionPoint->treeScope();
    HTMLDocument* newDocument = !wasInDocument && inDocument() && newScope->documentScope()->isHTMLDocument() ? toHTMLDocument(newScope->documentScope()) : 0;
    if (newScope != treeScope())
        newScope = 0;

    const AtomicString& idValue = getIdAttribute();
    if (!idValue.isNull()) {
        if (newScope)
            updateIdForTreeScope(newScope, nullAtom, idValue);
        if (newDocument)
            updateIdForDocument(newDocument, nullAtom, idValue, AlwaysUpdateHTMLDocumentNamedItemMaps);
    }

    const AtomicString& nameValue = getNameAttribute();
    if (!nameValue.isNull()) {
        if (newScope)
            updateNameForTreeScope(newScope, nullAtom, nameValue);
        if (newDocument)
            updateNameForDocument(newDocument, nullAtom, nameValue);
    }

    if (newScope && hasTagName(labelTag)) {
        if (newScope->shouldCacheLabelsByForAttribute())
            updateLabel(newScope, nullAtom, fastGetAttribute(forAttr));
    }

    return InsertionDone;
}

void Element::removedFrom(ContainerNode* insertionPoint)
{
#if ENABLE(SVG)
    bool wasInDocument = insertionPoint->document();
#endif

#if ENABLE(DIALOG_ELEMENT)
    document()->removeFromTopLayer(this);
#endif
#if ENABLE(FULLSCREEN_API)
    if (containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
#endif
#if ENABLE(POINTER_LOCK)
    if (document()->page())
        document()->page()->pointerLockController()->elementRemoved(this);
#endif

    setSavedLayerScrollOffset(IntSize());

    if (insertionPoint->isInTreeScope()) {
        TreeScope* oldScope = insertionPoint->treeScope();
        HTMLDocument* oldDocument = inDocument() && oldScope->documentScope()->isHTMLDocument() ? toHTMLDocument(oldScope->documentScope()) : 0;
        if (oldScope != treeScope())
            oldScope = 0;

        const AtomicString& idValue = getIdAttribute();
        if (!idValue.isNull()) {
            if (oldScope)
                updateIdForTreeScope(oldScope, idValue, nullAtom);
            if (oldDocument)
                updateIdForDocument(oldDocument, idValue, nullAtom, AlwaysUpdateHTMLDocumentNamedItemMaps);
        }

        const AtomicString& nameValue = getNameAttribute();
        if (!nameValue.isNull()) {
            if (oldScope)
                updateNameForTreeScope(oldScope, nameValue, nullAtom);
            if (oldDocument)
                updateNameForDocument(oldDocument, nameValue, nullAtom);
        }

        if (oldScope && hasTagName(labelTag)) {
            if (oldScope->shouldCacheLabelsByForAttribute())
                updateLabel(oldScope, fastGetAttribute(forAttr), nullAtom);
        }
    }

    ContainerNode::removedFrom(insertionPoint);
#if ENABLE(SVG)
    if (wasInDocument && hasPendingResources())
        document()->accessSVGExtensions()->removeElementFromPendingResources(this);
#endif
}

void Element::unregisterNamedFlowContentNode()
{
    if (document()->cssRegionsEnabled() && inNamedFlow() && document()->renderView())
        document()->renderView()->flowThreadController()->unregisterNamedFlowContentNode(this);
}

void Element::lazyReattach(ShouldSetAttached shouldSetAttached)
{
    Style::AttachContext context;
    context.performingReattach = true;

    if (attached())
        Style::detachRenderTree(this, context);
    lazyAttach(shouldSetAttached);
}

void Element::lazyAttach(ShouldSetAttached shouldSetAttached)
{
    for (Node* node = this; node; node = NodeTraversal::next(node, this)) {
        if (!node->isTextNode() && !node->isElementNode())
            continue;
        if (node->hasChildNodes())
            node->setChildNeedsStyleRecalc();
        if (node->isElementNode())
            toElement(node)->setStyleChange(FullStyleChange);
        if (shouldSetAttached == SetAttached)
            node->setAttached(true);
    }
    markAncestorsWithChildNeedsStyleRecalc();
}

PassRefPtr<RenderStyle> Element::styleForRenderer()
{
    if (hasCustomStyleResolveCallbacks()) {
        if (RefPtr<RenderStyle> style = customStyleForRenderer())
            return style.release();
    }

    return document()->ensureStyleResolver().styleForElement(this);
}

ShadowRoot* Element::shadowRoot() const
{
    return hasRareData() ? elementRareData()->shadowRoot() : 0;
}

void Element::didAffectSelector(AffectedSelectorMask)
{
    setNeedsStyleRecalc();
}

void Element::addShadowRoot(PassRefPtr<ShadowRoot> newShadowRoot)
{
    ASSERT(!shadowRoot());

    ShadowRoot* shadowRoot = newShadowRoot.get();
    ensureElementRareData().setShadowRoot(newShadowRoot);

    shadowRoot->setHostElement(this);
    shadowRoot->setParentTreeScope(treeScope());
    shadowRoot->distributor().didShadowBoundaryChange(this);

    ChildNodeInsertionNotifier(this).notify(shadowRoot);

    // Existence of shadow roots requires the host and its children to do traversal using ComposedShadowTreeWalker.
    setNeedsShadowTreeWalker();

    // FIXME(94905): ShadowHost should be reattached during recalcStyle.
    // Set some flag here and recreate shadow hosts' renderer in
    // Element::recalcStyle.
    if (attached())
        lazyReattach();

    InspectorInstrumentation::didPushShadowRoot(this, shadowRoot);
}

void Element::removeShadowRoot()
{
    RefPtr<ShadowRoot> oldRoot = shadowRoot();
    if (!oldRoot)
        return;
    InspectorInstrumentation::willPopShadowRoot(this, oldRoot.get());
    document()->removeFocusedNodeOfSubtree(oldRoot.get());

    ASSERT(!oldRoot->attached());

    elementRareData()->clearShadowRoot();

    oldRoot->setHostElement(0);
    oldRoot->setParentTreeScope(document());

    ChildNodeRemovalNotifier(this).notify(oldRoot.get());

    oldRoot->distributor().invalidateDistribution(this);
}

PassRefPtr<ShadowRoot> Element::createShadowRoot(ExceptionCode& ec)
{
    if (alwaysCreateUserAgentShadowRoot())
        ensureUserAgentShadowRoot();

#if ENABLE(SHADOW_DOM)
    if (RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled()) {
        addShadowRoot(ShadowRoot::create(document(), ShadowRoot::AuthorShadowRoot));
        return shadowRoot();
    }
#endif

    // Since some elements recreates shadow root dynamically, multiple shadow
    // subtrees won't work well in that element. Until they are fixed, we disable
    // adding author shadow root for them.
    if (!areAuthorShadowsAllowed()) {
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }
    addShadowRoot(ShadowRoot::create(document(), ShadowRoot::AuthorShadowRoot));

    return shadowRoot();
}

ShadowRoot* Element::authorShadowRoot() const
{
    ShadowRoot* shadowRoot = this->shadowRoot();
    if (shadowRoot->type() == ShadowRoot::AuthorShadowRoot)
        return shadowRoot;
    return 0;
}

ShadowRoot* Element::userAgentShadowRoot() const
{
    if (ShadowRoot* shadowRoot = this->shadowRoot()) {
        ASSERT(shadowRoot->type() == ShadowRoot::UserAgentShadowRoot);
        return shadowRoot;
    }
    return 0;
}

ShadowRoot& Element::ensureUserAgentShadowRoot()
{
    ShadowRoot* shadowRoot = userAgentShadowRoot();
    if (!shadowRoot) {
        addShadowRoot(ShadowRoot::create(document(), ShadowRoot::UserAgentShadowRoot));
        shadowRoot = userAgentShadowRoot();
        didAddUserAgentShadowRoot(shadowRoot);
    }
    return *shadowRoot;
}

const AtomicString& Element::shadowPseudoId() const
{
    return pseudo();
}

bool Element::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case TEXT_NODE:
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case CDATA_SECTION_NODE:
    case ENTITY_REFERENCE_NODE:
        return true;
    default:
        break;
    }
    return false;
}

static void checkForEmptyStyleChange(Element* element, RenderStyle* style)
{
    if (!style && !element->styleAffectedByEmpty())
        return;

    if (!style || (element->styleAffectedByEmpty() && (!style->emptyState() || element->hasChildNodes())))
        element->setNeedsStyleRecalc();
}

static void checkForSiblingStyleChanges(Element* e, RenderStyle* style, bool finishedParsingCallback,
                                        Node* beforeChange, Node* afterChange, int childCountDelta)
{
    // :empty selector.
    checkForEmptyStyleChange(e, style);
    
    if (!style || (e->needsStyleRecalc() && e->childrenAffectedByPositionalRules()))
        return;

    // :first-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (e->childrenAffectedByFirstChildRules() && afterChange) {
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
            firstElementAfterInsertion->setNeedsStyleRecalc();
            
        // We also have to handle node removal.
        if (childCountDelta < 0 && newFirstChild == firstElementAfterInsertion && newFirstChild && (!newFirstChild->renderStyle() || !newFirstChild->renderStyle()->firstChildState()))
            newFirstChild->setNeedsStyleRecalc();
    }

    // :last-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (e->childrenAffectedByLastChildRules() && beforeChange) {
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
            lastElementBeforeInsertion->setNeedsStyleRecalc();
            
        // We also have to handle node removal.  The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((childCountDelta < 0 || finishedParsingCallback) && newLastChild == lastElementBeforeInsertion && newLastChild && (!newLastChild->renderStyle() || !newLastChild->renderStyle()->lastChildState()))
            newLastChild->setNeedsStyleRecalc();
    }

    // The + selector.  We need to invalidate the first element following the insertion point.  It is the only possible element
    // that could be affected by this DOM change.
    if (e->childrenAffectedByDirectAdjacentRules() && afterChange) {
        Node* firstElementAfterInsertion = 0;
        for (firstElementAfterInsertion = afterChange;
             firstElementAfterInsertion && !firstElementAfterInsertion->isElementNode();
             firstElementAfterInsertion = firstElementAfterInsertion->nextSibling()) {};
        if (firstElementAfterInsertion && firstElementAfterInsertion->attached())
            firstElementAfterInsertion->setNeedsStyleRecalc();
    }

    // Forward positional selectors include the ~ selector, nth-child, nth-of-type, first-of-type and only-of-type.
    // Backward positional selectors include nth-last-child, nth-last-of-type, last-of-type and only-of-type.
    // We have to invalidate everything following the insertion point in the forward case, and everything before the insertion point in the
    // backward case.
    // |afterChange| is 0 in the parser callback case, so we won't do any work for the forward case if we don't have to.
    // For performance reasons we just mark the parent node as changed, since we don't want to make childrenChanged O(n^2) by crawling all our kids
    // here.  recalcStyle will then force a walk of the children when it sees that this has happened.
    if ((e->childrenAffectedByForwardPositionalRules() && afterChange)
        || (e->childrenAffectedByBackwardPositionalRules() && beforeChange))
        e->setNeedsStyleRecalc();
}

void Element::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (changedByParser)
        checkForEmptyStyleChange(this, renderStyle());
    else
        checkForSiblingStyleChanges(this, renderStyle(), false, beforeChange, afterChange, childCountDelta);

    if (ShadowRoot* shadowRoot = this->shadowRoot())
        shadowRoot->invalidateDistribution();
}

void Element::removeAllEventListeners()
{
    ContainerNode::removeAllEventListeners();
    if (ShadowRoot* shadowRoot = this->shadowRoot())
        shadowRoot->removeAllEventListeners();
}

void Element::beginParsingChildren()
{
    clearIsParsingChildrenFinished();
    StyleResolver* styleResolver = document()->styleResolverIfExists();
    if (styleResolver && attached())
        styleResolver->pushParentElement(this);
}

void Element::finishParsingChildren()
{
    ContainerNode::finishParsingChildren();
    setIsParsingChildrenFinished();
    checkForSiblingStyleChanges(this, renderStyle(), true, lastChild(), 0, 0);
    if (StyleResolver* styleResolver = document()->styleResolverIfExists())
        styleResolver->popParentElement(this);
}

#ifndef NDEBUG
void Element::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;
    String s;

    result.append(nodeName());

    s = getIdAttribute();
    if (s.length() > 0) {
        if (result.length() > 0)
            result.appendLiteral("; ");
        result.appendLiteral("id=");
        result.append(s);
    }

    s = getAttribute(classAttr);
    if (s.length() > 0) {
        if (result.length() > 0)
            result.appendLiteral("; ");
        result.appendLiteral("class=");
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}
#endif

const Vector<RefPtr<Attr> >& Element::attrNodeList()
{
    ASSERT(hasSyntheticAttrChildNodes());
    return *attrNodeListForElement(this);
}

PassRefPtr<Attr> Element::setAttributeNode(Attr* attrNode, ExceptionCode& ec)
{
    if (!attrNode) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }

    RefPtr<Attr> oldAttrNode = attrIfExists(attrNode->qualifiedName());
    if (oldAttrNode.get() == attrNode)
        return attrNode; // This Attr is already attached to the element.

    // INUSE_ATTRIBUTE_ERR: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode->ownerElement()) {
        ec = INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    synchronizeAllAttributes();
    UniqueElementData& elementData = ensureUniqueElementData();

    unsigned index = elementData.findAttributeIndexByNameForAttributeNode(attrNode, shouldIgnoreAttributeCase(this));
    if (index != ElementData::attributeNotFound) {
        if (oldAttrNode)
            detachAttrNodeFromElementWithValue(oldAttrNode.get(), elementData.attributeAt(index).value());
        else
            oldAttrNode = Attr::create(document(), attrNode->qualifiedName(), elementData.attributeAt(index).value());
    }

    setAttributeInternal(index, attrNode->qualifiedName(), attrNode->value(), NotInSynchronizationOfLazyAttribute);

    attrNode->attachToElement(this);
    treeScope()->adoptIfNeeded(attrNode);
    ensureAttrNodeListForElement(this).append(attrNode);

    return oldAttrNode.release();
}

PassRefPtr<Attr> Element::setAttributeNodeNS(Attr* attr, ExceptionCode& ec)
{
    return setAttributeNode(attr, ec);
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

    ASSERT(document() == attr->document());

    synchronizeAttribute(attr->qualifiedName());

    unsigned index = elementData()->findAttributeIndexByNameForAttributeNode(attr);
    if (index == ElementData::attributeNotFound) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    RefPtr<Attr> attrNode = attr;
    detachAttrNodeFromElementWithValue(attr, elementData()->attributeAt(index).value());
    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return attrNode.release();
}

bool Element::parseAttributeName(QualifiedName& out, const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionCode& ec)
{
    String prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName, ec))
        return false;
    ASSERT(!ec);

    QualifiedName qName(prefix, localName, namespaceURI);

    if (!Document::hasValidNamespaceForAttributes(qName)) {
        ec = NAMESPACE_ERR;
        return false;
    }

    out = qName;
    return true;
}

void Element::setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionCode& ec)
{
    QualifiedName parsedName = anyName;
    if (!parseAttributeName(parsedName, namespaceURI, qualifiedName, ec))
        return;
    setAttribute(parsedName, value);
}

void Element::removeAttributeInternal(unsigned index, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < attributeCount());

    UniqueElementData& elementData = ensureUniqueElementData();

    QualifiedName name = elementData.attributeAt(index).name();
    AtomicString valueBeingRemoved = elementData.attributeAt(index).value();

    if (!inSynchronizationOfLazyAttribute) {
        if (!valueBeingRemoved.isNull())
            willModifyAttribute(name, valueBeingRemoved, nullAtom);
    }

    if (RefPtr<Attr> attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode.get(), elementData.attributeAt(index).value());

    elementData.removeAttribute(index);

    if (!inSynchronizationOfLazyAttribute)
        didRemoveAttribute(name);
}

void Element::addAttributeInternal(const QualifiedName& name, const AtomicString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, nullAtom, value);
    ensureUniqueElementData().addAttribute(name, value);
    if (!inSynchronizationOfLazyAttribute)
        didAddAttribute(name, value);
}

void Element::removeAttribute(const AtomicString& name)
{
    if (!elementData())
        return;

    AtomicString localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    unsigned index = elementData()->findAttributeIndexByName(localName, false);
    if (index == ElementData::attributeNotFound) {
        if (UNLIKELY(localName == styleAttr) && elementData()->m_styleAttributeIsDirty && isStyledElement())
            static_cast<StyledElement*>(this)->removeAllInlineStyleProperties();
        return;
    }

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::removeAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    removeAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

PassRefPtr<Attr> Element::getAttributeNode(const AtomicString& localName)
{
    if (!elementData())
        return 0;
    synchronizeAttribute(localName);
    const Attribute* attribute = elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase(this));
    if (!attribute)
        return 0;
    return ensureAttr(attribute->name());
}

PassRefPtr<Attr> Element::getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (!elementData())
        return 0;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    const Attribute* attribute = elementData()->findAttributeByName(qName);
    if (!attribute)
        return 0;
    return ensureAttr(attribute->name());
}

bool Element::hasAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return false;
    synchronizeAttribute(localName);
    return elementData()->findAttributeByName(shouldIgnoreAttributeCase(this) ? localName.lower() : localName, false);
}

bool Element::hasAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    if (!elementData())
        return false;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    return elementData()->findAttributeByName(qName);
}

CSSStyleDeclaration *Element::style()
{
    return 0;
}

void Element::focus(bool restorePreviousSelection, FocusDirection direction)
{
    if (!inDocument())
        return;

    Document* doc = document();
    if (doc->focusedElement() == this)
        return;

    // If the stylesheets have already been loaded we can reliably check isFocusable.
    // If not, we continue and set the focused node on the focus controller below so
    // that it can be updated soon after attach. 
    if (doc->haveStylesheetsLoaded()) {
        doc->updateLayoutIgnorePendingStylesheets();
        if (!isFocusable())
            return;
    }

    if (!supportsFocus())
        return;

    RefPtr<Node> protect;
    if (Page* page = doc->page()) {
        // Focus and change event handlers can cause us to lose our last ref.
        // If a focus event handler changes the focus to a different node it
        // does not make sense to continue and update appearence.
        protect = this;
        if (!page->focusController().setFocusedElement(this, doc->frame(), direction))
            return;
    }

    // Setting the focused node above might have invalidated the layout due to scripts.
    doc->updateLayoutIgnorePendingStylesheets();

    if (!isFocusable()) {
        ensureElementRareData().setNeedsFocusAppearanceUpdateSoonAfterAttach(true);
        return;
    }
        
    cancelFocusAppearanceUpdate();
    updateFocusAppearance(restorePreviousSelection);
}

void Element::updateFocusAppearanceAfterAttachIfNeeded()
{
    if (!hasRareData())
        return;
    ElementRareData* data = elementRareData();
    if (!data->needsFocusAppearanceUpdateSoonAfterAttach())
        return;
    if (isFocusable() && document()->focusedElement() == this)
        document()->updateFocusAppearanceSoon(false /* don't restore selection */);
    data->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
}

void Element::updateFocusAppearance(bool /*restorePreviousSelection*/)
{
    if (isRootEditableElement()) {
        Frame* frame = document()->frame();
        if (!frame)
            return;
        
        // When focusing an editable element in an iframe, don't reset the selection if it already contains a selection.
        if (this == frame->selection().rootEditableElement())
            return;

        // FIXME: We should restore the previous selection if there is one.
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this), DOWNSTREAM);
        
        if (frame->selection().shouldChangeSelection(newSelection)) {
            frame->selection().setSelection(newSelection);
            frame->selection().revealSelection();
        }
    } else if (renderer() && !renderer()->isWidget())
        renderer()->scrollRectToVisible(boundingBox());
}

void Element::blur()
{
    cancelFocusAppearanceUpdate();
    Document* doc = document();
    if (treeScope()->focusedElement() == this) {
        if (doc->frame())
            doc->frame()->page()->focusController().setFocusedElement(0, doc->frame());
        else
            doc->setFocusedElement(0);
    }
}

void Element::dispatchFocusInEvent(const AtomicString& eventType, PassRefPtr<Element> oldFocusedElement)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(eventType == eventNames().focusinEvent || eventType == eventNames().DOMFocusInEvent);
    dispatchScopedEventDispatchMediator(FocusInEventDispatchMediator::create(FocusEvent::create(eventType, true, false, document()->defaultView(), 0, oldFocusedElement)));
}

void Element::dispatchFocusOutEvent(const AtomicString& eventType, PassRefPtr<Element> newFocusedElement)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(eventType == eventNames().focusoutEvent || eventType == eventNames().DOMFocusOutEvent);
    dispatchScopedEventDispatchMediator(FocusOutEventDispatchMediator::create(FocusEvent::create(eventType, true, false, document()->defaultView(), 0, newFocusedElement)));
}

void Element::dispatchFocusEvent(PassRefPtr<Element> oldFocusedElement, FocusDirection)
{
    if (document()->page())
        document()->page()->chrome().client().elementDidFocus(this);

    RefPtr<FocusEvent> event = FocusEvent::create(eventNames().focusEvent, false, false, document()->defaultView(), 0, oldFocusedElement);
    EventDispatcher::dispatchEvent(this, FocusEventDispatchMediator::create(event.release()));
}

void Element::dispatchBlurEvent(PassRefPtr<Element> newFocusedElement)
{
    if (document()->page())
        document()->page()->chrome().client().elementDidBlur(this);

    RefPtr<FocusEvent> event = FocusEvent::create(eventNames().blurEvent, false, false, document()->defaultView(), 0, newFocusedElement);
    EventDispatcher::dispatchEvent(this, BlurEventDispatchMediator::create(event.release()));
}


String Element::innerText()
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    document()->updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return textContent(true);

    return plainText(rangeOfContents(const_cast<Element*>(this)).get());
}

String Element::outerText()
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

const AtomicString& Element::pseudo() const
{
    return getAttribute(pseudoAttr);
}

void Element::setPseudo(const AtomicString& value)
{
    setAttribute(pseudoAttr, value);
}

LayoutSize Element::minimumSizeForResizing() const
{
    return hasRareData() ? elementRareData()->minimumSizeForResizing() : defaultMinimumSizeForResizing();
}

void Element::setMinimumSizeForResizing(const LayoutSize& size)
{
    if (!hasRareData() && size == defaultMinimumSizeForResizing())
        return;
    ensureElementRareData().setMinimumSizeForResizing(size);
}

RenderStyle* Element::computedStyle(PseudoId pseudoElementSpecifier)
{
    if (PseudoElement* element = pseudoElement(pseudoElementSpecifier))
        return element->computedStyle();

    // FIXME: Find and use the renderer from the pseudo element instead of the actual element so that the 'length'
    // properties, which are only known by the renderer because it did the layout, will be correct and so that the
    // values returned for the ":selection" pseudo-element will be correct.
    if (RenderStyle* usedStyle = renderStyle()) {
        if (pseudoElementSpecifier) {
            RenderStyle* cachedPseudoStyle = usedStyle->getCachedPseudoStyle(pseudoElementSpecifier);
            return cachedPseudoStyle ? cachedPseudoStyle : usedStyle;
         } else
            return usedStyle;
    }

    if (!attached())
        // FIXME: Try to do better than this. Ensure that styleForElement() works for elements that are not in the
        // document tree and figure out when to destroy the computed style for such elements.
        return 0;

    ElementRareData& data = ensureElementRareData();
    if (!data.computedStyle())
        data.setComputedStyle(document()->styleForElementIgnoringPendingStylesheets(this));
    return pseudoElementSpecifier ? data.computedStyle()->getCachedPseudoStyle(pseudoElementSpecifier) : data.computedStyle();
}

void Element::setStyleAffectedByEmpty()
{
    ensureElementRareData().setStyleAffectedByEmpty(true);
}

void Element::setChildrenAffectedByHover(bool value)
{
    if (value || hasRareData())
        ensureElementRareData().setChildrenAffectedByHover(value);
}

void Element::setChildrenAffectedByActive(bool value)
{
    if (value || hasRareData())
        ensureElementRareData().setChildrenAffectedByActive(value);
}

void Element::setChildrenAffectedByDrag(bool value)
{
    if (value || hasRareData())
        ensureElementRareData().setChildrenAffectedByDrag(value);
}

void Element::setChildrenAffectedByFirstChildRules()
{
    ensureElementRareData().setChildrenAffectedByFirstChildRules(true);
}

void Element::setChildrenAffectedByLastChildRules()
{
    ensureElementRareData().setChildrenAffectedByLastChildRules(true);
}

void Element::setChildrenAffectedByDirectAdjacentRules()
{
    ensureElementRareData().setChildrenAffectedByDirectAdjacentRules(true);
}

void Element::setChildrenAffectedByForwardPositionalRules()
{
    ensureElementRareData().setChildrenAffectedByForwardPositionalRules(true);
}

void Element::setChildrenAffectedByBackwardPositionalRules()
{
    ensureElementRareData().setChildrenAffectedByBackwardPositionalRules(true);
}

void Element::setChildIndex(unsigned index)
{
    ElementRareData& rareData = ensureElementRareData();
    if (RenderStyle* style = renderStyle())
        style->setUnique();
    rareData.setChildIndex(index);
}

bool Element::hasFlagsSetDuringStylingOfChildren() const
{
    if (!hasRareData())
        return false;
    return rareDataChildrenAffectedByHover()
        || rareDataChildrenAffectedByActive()
        || rareDataChildrenAffectedByDrag()
        || rareDataChildrenAffectedByFirstChildRules()
        || rareDataChildrenAffectedByLastChildRules()
        || rareDataChildrenAffectedByDirectAdjacentRules()
        || rareDataChildrenAffectedByForwardPositionalRules()
        || rareDataChildrenAffectedByBackwardPositionalRules();
}

bool Element::rareDataStyleAffectedByEmpty() const
{
    ASSERT(hasRareData());
    return elementRareData()->styleAffectedByEmpty();
}

bool Element::rareDataChildrenAffectedByHover() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByHover();
}

bool Element::rareDataChildrenAffectedByActive() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByActive();
}

bool Element::rareDataChildrenAffectedByDrag() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByDrag();
}

bool Element::rareDataChildrenAffectedByFirstChildRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByFirstChildRules();
}

bool Element::rareDataChildrenAffectedByLastChildRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByLastChildRules();
}

bool Element::rareDataChildrenAffectedByDirectAdjacentRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByDirectAdjacentRules();
}

bool Element::rareDataChildrenAffectedByForwardPositionalRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByForwardPositionalRules();
}

bool Element::rareDataChildrenAffectedByBackwardPositionalRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByBackwardPositionalRules();
}

unsigned Element::rareDataChildIndex() const
{
    ASSERT(hasRareData());
    return elementRareData()->childIndex();
}

void Element::setIsInCanvasSubtree(bool isInCanvasSubtree)
{
    ensureElementRareData().setIsInCanvasSubtree(isInCanvasSubtree);
}

bool Element::isInCanvasSubtree() const
{
    return hasRareData() && elementRareData()->isInCanvasSubtree();
}

void Element::setIsInsideRegion(bool value)
{
    if (value == isInsideRegion())
        return;

    ensureElementRareData().setIsInsideRegion(value);
}

bool Element::isInsideRegion() const
{
    return hasRareData() ? elementRareData()->isInsideRegion() : false;
}

void Element::setRegionOversetState(RegionOversetState state)
{
    ensureElementRareData().setRegionOversetState(state);
}

RegionOversetState Element::regionOversetState() const
{
    return hasRareData() ? elementRareData()->regionOversetState() : RegionUndefined;
}

AtomicString Element::computeInheritedLanguage() const
{
    const Node* n = this;
    AtomicString value;
    // The language property is inherited, so we iterate over the parents to find the first language.
    do {
        if (n->isElementNode()) {
            if (const ElementData* elementData = toElement(n)->elementData()) {
                // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
                if (const Attribute* attribute = elementData->findAttributeByName(XMLNames::langAttr))
                    value = attribute->value();
                else if (const Attribute* attribute = elementData->findAttributeByName(HTMLNames::langAttr))
                    value = attribute->value();
            }
        } else if (n->isDocumentNode()) {
            // checking the MIME content-language
            value = toDocument(n)->contentLanguage();
        }

        n = n->parentNode();
    } while (n && value.isNull());

    return value;
}

Locale& Element::locale() const
{
    return document()->getCachedLocale(computeInheritedLanguage());
}

void Element::cancelFocusAppearanceUpdate()
{
    if (hasRareData())
        elementRareData()->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
    if (document()->focusedElement() == this)
        document()->cancelFocusAppearanceUpdate();
}

void Element::normalizeAttributes()
{
    if (!hasAttributes())
        return;
    for (unsigned i = 0; i < attributeCount(); ++i) {
        if (RefPtr<Attr> attr = attrIfExists(attributeAt(i).name()))
            attr->normalize();
    }
}

void Element::updatePseudoElement(PseudoId pseudoId, Style::Change change)
{
    PseudoElement* existing = pseudoElement(pseudoId);
    if (existing) {
        // PseudoElement styles hang off their parent element's style so if we needed
        // a style recalc we should Force one on the pseudo.
        Style::resolveTree(existing, needsStyleRecalc() ? Style::Force : change);

        // Wait until our parent is not displayed or pseudoElementRendererIsNeeded
        // is false, otherwise we could continously create and destroy PseudoElements
        // when RenderObject::isChildAllowed on our parent returns false for the
        // PseudoElement's renderer for each style recalc.
        if (!renderer() || !pseudoElementRendererIsNeeded(renderer()->getCachedPseudoStyle(pseudoId)))
            setPseudoElement(pseudoId, 0);
    } else if (RefPtr<PseudoElement> element = createPseudoElementIfNeeded(pseudoId)) {
        Style::attachRenderTree(element.get());
        setPseudoElement(pseudoId, element.release());
    }
}

PassRefPtr<PseudoElement> Element::createPseudoElementIfNeeded(PseudoId pseudoId)
{
    if (!document()->styleSheetCollection()->usesBeforeAfterRules())
        return 0;

    if (!renderer() || !renderer()->canHaveGeneratedChildren())
        return 0;

    if (isPseudoElement())
        return 0;

    if (!pseudoElementRendererIsNeeded(renderer()->getCachedPseudoStyle(pseudoId)))
        return 0;

    return PseudoElement::create(this, pseudoId);
}

bool Element::hasPseudoElements() const
{
    return hasRareData() && elementRareData()->hasPseudoElements();
}

PseudoElement* Element::pseudoElement(PseudoId pseudoId) const
{
    return hasRareData() ? elementRareData()->pseudoElement(pseudoId) : 0;
}

void Element::setPseudoElement(PseudoId pseudoId, PassRefPtr<PseudoElement> element)
{
    ensureElementRareData().setPseudoElement(pseudoId, element);
    resetNeedsShadowTreeWalker();
}

RenderObject* Element::pseudoElementRenderer(PseudoId pseudoId) const
{
    if (PseudoElement* element = pseudoElement(pseudoId))
        return element->renderer();
    return 0;
}

// ElementTraversal API
Element* Element::firstElementChild() const
{
    return ElementTraversal::firstWithin(this);
}

Element* Element::lastElementChild() const
{
    return ElementTraversal::lastWithin(this);
}

Element* Element::previousElementSibling() const
{
    return ElementTraversal::previousSibling(this);
}

Element* Element::nextElementSibling() const
{
    return ElementTraversal::nextSibling(this);
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

bool Element::matchesReadOnlyPseudoClass() const
{
    return false;
}

bool Element::matchesReadWritePseudoClass() const
{
    return false;
}

bool Element::webkitMatchesSelector(const String& selector, ExceptionCode& ec)
{
    if (selector.isEmpty()) {
        ec = SYNTAX_ERR;
        return false;
    }

    SelectorQuery* selectorQuery = document()->selectorQueryCache().add(selector, document(), ec);
    if (!selectorQuery)
        return false;
    return selectorQuery->matches(this);
}

bool Element::shouldAppearIndeterminate() const
{
    return false;
}

DOMTokenList* Element::classList()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.classList())
        data.setClassList(ClassList::create(this));
    return data.classList();
}

DOMStringMap* Element::dataset()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.dataset())
        data.setDataset(DatasetDOMStringMap::create(this));
    return data.dataset();
}

KURL Element::getURLAttribute(const QualifiedName& name) const
{
#if !ASSERT_DISABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    return document()->completeURL(stripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
}

KURL Element::getNonEmptyURLAttribute(const QualifiedName& name) const
{
#if !ASSERT_DISABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    String value = stripLeadingAndTrailingHTMLSpaces(getAttribute(name));
    if (value.isEmpty())
        return KURL();
    return document()->completeURL(value);
}

int Element::getIntegralAttribute(const QualifiedName& attributeName) const
{
    return getAttribute(attributeName).string().toInt();
}

void Element::setIntegralAttribute(const QualifiedName& attributeName, int value)
{
    // FIXME: Need an AtomicString version of String::number.
    setAttribute(attributeName, String::number(value));
}

unsigned Element::getUnsignedIntegralAttribute(const QualifiedName& attributeName) const
{
    return getAttribute(attributeName).string().toUInt();
}

void Element::setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value)
{
    // FIXME: Need an AtomicString version of String::number.
    setAttribute(attributeName, String::number(value));
}

#if ENABLE(INDIE_UI)
void Element::setUIActions(const AtomicString& actions)
{
    setAttribute(uiactionsAttr, actions);
}

const AtomicString& Element::UIActions() const
{
    return getAttribute(uiactionsAttr);
}
#endif

    
#if ENABLE(SVG)
bool Element::childShouldCreateRenderer(const NodeRenderingContext& childContext) const
{
    // Only create renderers for SVG elements whose parents are SVG elements, or for proper <svg xmlns="svgNS"> subdocuments.
    if (childContext.node()->isSVGElement())
        return childContext.node()->hasTagName(SVGNames::svgTag) || isSVGElement();

    return ContainerNode::childShouldCreateRenderer(childContext);
}
#endif

#if ENABLE(FULLSCREEN_API)
void Element::webkitRequestFullscreen()
{
    document()->requestFullScreenForElement(this, ALLOW_KEYBOARD_INPUT, Document::EnforceIFrameAllowFullScreenRequirement);
}

void Element::webkitRequestFullScreen(unsigned short flags)
{
    document()->requestFullScreenForElement(this, (flags | LEGACY_MOZILLA_REQUEST), Document::EnforceIFrameAllowFullScreenRequirement);
}

bool Element::containsFullScreenElement() const
{
    return hasRareData() && elementRareData()->containsFullScreenElement();
}

void Element::setContainsFullScreenElement(bool flag)
{
    ensureElementRareData().setContainsFullScreenElement(flag);
    setNeedsStyleRecalc(SyntheticStyleChange);
}

static Element* parentCrossingFrameBoundaries(Element* element)
{
    ASSERT(element);
    return element->parentElement() ? element->parentElement() : element->document()->ownerElement();
}

void Element::setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool flag)
{
    Element* element = this;
    while ((element = parentCrossingFrameBoundaries(element)))
        element->setContainsFullScreenElement(flag);
}
#endif    

#if ENABLE(DIALOG_ELEMENT)
bool Element::isInTopLayer() const
{
    return hasRareData() && elementRareData()->isInTopLayer();
}

void Element::setIsInTopLayer(bool inTopLayer)
{
    if (isInTopLayer() == inTopLayer)
        return;
    ensureElementRareData().setIsInTopLayer(inTopLayer);

    // We must ensure a reattach occurs so the renderer is inserted in the correct sibling order under RenderView according to its
    // top layer position, or in its usual place if not in the top layer.
    reattachIfAttached();
}
#endif

#if ENABLE(POINTER_LOCK)
void Element::webkitRequestPointerLock()
{
    if (document()->page())
        document()->page()->pointerLockController()->requestPointerLock(this);
}
#endif

SpellcheckAttributeState Element::spellcheckAttributeState() const
{
    const AtomicString& value = getAttribute(HTMLNames::spellcheckAttr);
    if (value == nullAtom)
        return SpellcheckAttributeDefault;
    if (equalIgnoringCase(value, "true") || equalIgnoringCase(value, ""))
        return SpellcheckAttributeTrue;
    if (equalIgnoringCase(value, "false"))
        return SpellcheckAttributeFalse;

    return SpellcheckAttributeDefault;
}

bool Element::isSpellCheckingEnabled() const
{
    for (const Element* element = this; element; element = element->parentOrShadowHostElement()) {
        switch (element->spellcheckAttributeState()) {
        case SpellcheckAttributeTrue:
            return true;
        case SpellcheckAttributeFalse:
            return false;
        case SpellcheckAttributeDefault:
            break;
        }
    }

    return true;
}

RenderRegion* Element::renderRegion() const
{
    if (renderer() && renderer()->isRenderRegion())
        return toRenderRegion(renderer());

    return 0;
}

#if ENABLE(CSS_REGIONS)

bool Element::shouldMoveToFlowThread(RenderStyle* styleToUse) const
{
    ASSERT(styleToUse);

#if ENABLE(FULLSCREEN_API)
    if (document()->webkitIsFullScreen() && document()->webkitCurrentFullScreenElement() == this)
        return false;
#endif

    if (isInShadowTree())
        return false;

    if (styleToUse->flowThread().isEmpty())
        return false;

    return !isRegisteredWithNamedFlow();
}

const AtomicString& Element::webkitRegionOverset() const
{
    document()->updateLayoutIgnorePendingStylesheets();

    DEFINE_STATIC_LOCAL(AtomicString, undefinedState, ("undefined", AtomicString::ConstructFromLiteral));
    if (!document()->cssRegionsEnabled() || !renderRegion())
        return undefinedState;

    switch (renderRegion()->regionOversetState()) {
    case RegionFit: {
        DEFINE_STATIC_LOCAL(AtomicString, fitState, ("fit", AtomicString::ConstructFromLiteral));
        return fitState;
    }
    case RegionEmpty: {
        DEFINE_STATIC_LOCAL(AtomicString, emptyState, ("empty", AtomicString::ConstructFromLiteral));
        return emptyState;
    }
    case RegionOverset: {
        DEFINE_STATIC_LOCAL(AtomicString, overflowState, ("overset", AtomicString::ConstructFromLiteral));
        return overflowState;
    }
    case RegionUndefined:
        return undefinedState;
    }

    ASSERT_NOT_REACHED();
    return undefinedState;
}

Vector<RefPtr<Range> > Element::webkitGetRegionFlowRanges() const
{
    document()->updateLayoutIgnorePendingStylesheets();

    Vector<RefPtr<Range> > rangeObjects;
    if (document()->cssRegionsEnabled() && renderer() && renderer()->isRenderRegion()) {
        RenderRegion* region = toRenderRegion(renderer());
        if (region->isValid())
            region->getRanges(rangeObjects);
    }

    return rangeObjects;
}

#endif

#ifndef NDEBUG
bool Element::fastAttributeLookupAllowed(const QualifiedName& name) const
{
    if (name == HTMLNames::styleAttr)
        return false;

#if ENABLE(SVG)
    if (isSVGElement())
        return !static_cast<const SVGElement*>(this)->isAnimatableAttribute(name);
#endif

    return true;
}
#endif

#ifdef DUMP_NODE_STATISTICS
bool Element::hasNamedNodeMap() const
{
    return hasRareData() && elementRareData()->attributeMap();
}
#endif

inline void Element::updateName(const AtomicString& oldName, const AtomicString& newName)
{
    if (!isInTreeScope())
        return;

    if (oldName == newName)
        return;

    updateNameForTreeScope(treeScope(), oldName, newName);

    if (!inDocument())
        return;
    Document* htmlDocument = document();
    if (!htmlDocument->isHTMLDocument())
        return;
    updateNameForDocument(toHTMLDocument(htmlDocument), oldName, newName);
}

void Element::updateNameForTreeScope(TreeScope* scope, const AtomicString& oldName, const AtomicString& newName)
{
    ASSERT(isInTreeScope());
    ASSERT(oldName != newName);

    if (!oldName.isEmpty())
        scope->removeElementByName(oldName, this);
    if (!newName.isEmpty())
        scope->addElementByName(newName, this);
}

void Element::updateNameForDocument(HTMLDocument* document, const AtomicString& oldName, const AtomicString& newName)
{
    ASSERT(inDocument());
    ASSERT(oldName != newName);

    if (WindowNameCollection::nodeMatchesIfNameAttributeMatch(this)) {
        const AtomicString& id = WindowNameCollection::nodeMatchesIfIdAttributeMatch(this) ? getIdAttribute() : nullAtom;
        if (!oldName.isEmpty() && oldName != id)
            document->removeWindowNamedItem(oldName, this);
        if (!newName.isEmpty() && newName != id)
            document->addWindowNamedItem(newName, this);
    }

    if (DocumentNameCollection::nodeMatchesIfNameAttributeMatch(this)) {
        const AtomicString& id = DocumentNameCollection::nodeMatchesIfIdAttributeMatch(this) ? getIdAttribute() : nullAtom;
        if (!oldName.isEmpty() && oldName != id)
            document->removeDocumentNamedItem(oldName, this);
        if (!newName.isEmpty() && newName != id)
            document->addDocumentNamedItem(newName, this);
    }
}

inline void Element::updateId(const AtomicString& oldId, const AtomicString& newId)
{
    if (!isInTreeScope())
        return;

    if (oldId == newId)
        return;

    updateIdForTreeScope(treeScope(), oldId, newId);

    if (!inDocument())
        return;
    Document* htmlDocument = document();
    if (!htmlDocument->isHTMLDocument())
        return;
    updateIdForDocument(toHTMLDocument(htmlDocument), oldId, newId, UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute);
}

void Element::updateIdForTreeScope(TreeScope* scope, const AtomicString& oldId, const AtomicString& newId)
{
    ASSERT(isInTreeScope());
    ASSERT(oldId != newId);

    if (!oldId.isEmpty())
        scope->removeElementById(oldId, this);
    if (!newId.isEmpty())
        scope->addElementById(newId, this);
}

void Element::updateIdForDocument(HTMLDocument* document, const AtomicString& oldId, const AtomicString& newId, HTMLDocumentNamedItemMapsUpdatingCondition condition)
{
    ASSERT(inDocument());
    ASSERT(oldId != newId);

    if (WindowNameCollection::nodeMatchesIfIdAttributeMatch(this)) {
        const AtomicString& name = condition == UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute && WindowNameCollection::nodeMatchesIfNameAttributeMatch(this) ? getNameAttribute() : nullAtom;
        if (!oldId.isEmpty() && oldId != name)
            document->removeWindowNamedItem(oldId, this);
        if (!newId.isEmpty() && newId != name)
            document->addWindowNamedItem(newId, this);
    }

    if (DocumentNameCollection::nodeMatchesIfIdAttributeMatch(this)) {
        const AtomicString& name = condition == UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute && DocumentNameCollection::nodeMatchesIfNameAttributeMatch(this) ? getNameAttribute() : nullAtom;
        if (!oldId.isEmpty() && oldId != name)
            document->removeDocumentNamedItem(oldId, this);
        if (!newId.isEmpty() && newId != name)
            document->addDocumentNamedItem(newId, this);
    }
}

void Element::updateLabel(TreeScope* scope, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue)
{
    ASSERT(hasTagName(labelTag));

    if (!inDocument())
        return;

    if (oldForAttributeValue == newForAttributeValue)
        return;

    if (!oldForAttributeValue.isEmpty())
        scope->removeLabel(oldForAttributeValue, toHTMLLabelElement(this));
    if (!newForAttributeValue.isEmpty())
        scope->addLabel(newForAttributeValue, toHTMLLabelElement(this));
}

void Element::willModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (isIdAttributeName(name))
        updateId(oldValue, newValue);
    else if (name == HTMLNames::nameAttr)
        updateName(oldValue, newValue);
    else if (name == HTMLNames::forAttr && hasTagName(labelTag)) {
        TreeScope* scope = treeScope();
        if (scope->shouldCacheLabelsByForAttribute())
            updateLabel(scope, oldValue, newValue);
    }

    if (oldValue != newValue) {
        if (attached() && document()->styleResolverIfExists() && document()->styleResolverIfExists()->hasSelectorForAttribute(name.localName()))
            setNeedsStyleRecalc();
    }

    if (OwnPtr<MutationObserverInterestGroup> recipients = MutationObserverInterestGroup::createForAttributesMutation(this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(this, name, oldValue));

#if ENABLE(INSPECTOR)
    InspectorInstrumentation::willModifyDOMAttr(document(), this, oldValue, newValue);
#endif
}

void Element::didAddAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, value);
    InspectorInstrumentation::didModifyDOMAttr(document(), this, name.localName(), value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, value);
    InspectorInstrumentation::didModifyDOMAttr(document(), this, name.localName(), value);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name)
{
    attributeChanged(name, nullAtom);
    InspectorInstrumentation::didRemoveDOMAttr(document(), this, name.localName());
    dispatchSubtreeModifiedEvent();
}

PassRefPtr<HTMLCollection> Element::ensureCachedHTMLCollection(CollectionType type)
{
    if (HTMLCollection* collection = cachedHTMLCollection(type))
        return collection;

    RefPtr<HTMLCollection> collection;
    if (type == TableRows) {
        ASSERT(hasTagName(tableTag));
        return ensureRareData().ensureNodeLists().addCacheWithAtomicName<HTMLTableRowsCollection>(this, type);
    } else if (type == SelectOptions) {
        ASSERT(hasTagName(selectTag));
        return ensureRareData().ensureNodeLists().addCacheWithAtomicName<HTMLOptionsCollection>(this, type);
    } else if (type == FormControls) {
        ASSERT(hasTagName(formTag) || hasTagName(fieldsetTag));
        return ensureRareData().ensureNodeLists().addCacheWithAtomicName<HTMLFormControlsCollection>(this, type);
    }
    return ensureRareData().ensureNodeLists().addCacheWithAtomicName<HTMLCollection>(this, type);
}

HTMLCollection* Element::cachedHTMLCollection(CollectionType type)
{
    return hasRareData() && rareData()->nodeLists() ? rareData()->nodeLists()->cacheWithAtomicName<HTMLCollection>(type) : 0;
}

IntSize Element::savedLayerScrollOffset() const
{
    return hasRareData() ? elementRareData()->savedLayerScrollOffset() : IntSize();
}

void Element::setSavedLayerScrollOffset(const IntSize& size)
{
    if (size.isZero() && !hasRareData())
        return;
    ensureElementRareData().setSavedLayerScrollOffset(size);
}

PassRefPtr<Attr> Element::attrIfExists(const QualifiedName& name)
{
    if (AttrNodeList* attrNodeList = attrNodeListForElement(this))
        return findAttrNodeInList(*attrNodeList, name);
    return 0;
}

PassRefPtr<Attr> Element::ensureAttr(const QualifiedName& name)
{
    AttrNodeList& attrNodeList = ensureAttrNodeListForElement(this);
    RefPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, name);
    if (!attrNode) {
        attrNode = Attr::create(this, name);
        treeScope()->adoptIfNeeded(attrNode.get());
        attrNodeList.append(attrNode);
    }
    return attrNode.release();
}

void Element::detachAttrNodeFromElementWithValue(Attr* attrNode, const AtomicString& value)
{
    ASSERT(hasSyntheticAttrChildNodes());
    attrNode->detachFromElementWithValue(value);

    AttrNodeList* attrNodeList = attrNodeListForElement(this);
    for (unsigned i = 0; i < attrNodeList->size(); ++i) {
        if (attrNodeList->at(i)->qualifiedName() == attrNode->qualifiedName()) {
            attrNodeList->remove(i);
            if (attrNodeList->isEmpty())
                removeAttrNodeListForElement(this);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void Element::detachAllAttrNodesFromElement()
{
    AttrNodeList* attrNodeList = attrNodeListForElement(this);
    ASSERT(attrNodeList);

    for (unsigned i = 0; i < attributeCount(); ++i) {
        const Attribute& attribute = attributeAt(i);
        if (RefPtr<Attr> attrNode = findAttrNodeInList(*attrNodeList, attribute.name()))
            attrNode->detachFromElementWithValue(attribute.value());
    }

    removeAttrNodeListForElement(this);
}

void Element::resetComputedStyle()
{
    if (!hasRareData())
        return;
    elementRareData()->resetComputedStyle();
}

void Element::clearStyleDerivedDataBeforeDetachingRenderer()
{
    unregisterNamedFlowContentNode();
    cancelFocusAppearanceUpdate();
    if (!hasRareData())
        return;
    ElementRareData* data = elementRareData();
    data->setPseudoElement(BEFORE, 0);
    data->setPseudoElement(AFTER, 0);
    data->setIsInCanvasSubtree(false);
    data->resetComputedStyle();
    data->resetDynamicRestyleObservations();
    data->setIsInsideRegion(false);
}

void Element::clearHoverAndActiveStatusBeforeDetachingRenderer()
{
    if (!isUserActionElement())
        return;
    if (hovered())
        document()->hoveredElementDidDetach(this);
    if (inActiveChain())
        document()->elementInActiveChainDidDetach(this);
    document()->userActionElements().didDetach(this);
}

bool Element::willRecalcStyle(Style::Change)
{
    ASSERT(hasCustomStyleResolveCallbacks());
    return true;
}

void Element::didRecalcStyle(Style::Change)
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::willAttachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::didAttachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::willDetachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::didDetachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

PassRefPtr<RenderStyle> Element::customStyleForRenderer()
{
    ASSERT(hasCustomStyleResolveCallbacks());
    return 0;
}

void Element::cloneAttributesFromElement(const Element& other)
{
    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    other.synchronizeAllAttributes();
    if (!other.m_elementData) {
        m_elementData.clear();
        return;
    }

    // We can't update window and document's named item maps since the presence of image and object elements depend on other attributes and children.
    // Fortunately, those named item maps are only updated when this element is in the document, which should never be the case.
    ASSERT(!inDocument());

    const AtomicString& oldID = getIdAttribute();
    const AtomicString& newID = other.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        updateId(oldID, newID);

    const AtomicString& oldName = getNameAttribute();
    const AtomicString& newName = other.getNameAttribute();

    if (!oldName.isNull() || !newName.isNull())
        updateName(oldName, newName);

    // If 'other' has a mutable ElementData, convert it to an immutable one so we can share it between both elements.
    // We can only do this if there is no CSSOM wrapper for other's inline style, and there are no presentation attributes.
    if (other.m_elementData->isUnique()
        && !other.m_elementData->presentationAttributeStyle()
        && (!other.m_elementData->inlineStyle() || !other.m_elementData->inlineStyle()->hasCSSOMWrapper()))
        const_cast<Element&>(other).m_elementData = static_cast<const UniqueElementData*>(other.m_elementData.get())->makeShareableCopy();

    if (!other.m_elementData->isUnique())
        m_elementData = other.m_elementData;
    else
        m_elementData = other.m_elementData->makeUniqueCopy();

    for (unsigned i = 0; i < m_elementData->length(); ++i) {
        const Attribute& attribute = const_cast<const ElementData*>(m_elementData.get())->attributeAt(i);
        attributeChangedFromParserOrByCloning(attribute.name(), attribute.value(), ModifiedByCloning);
    }
}

void Element::cloneDataFromElement(const Element& other)
{
    cloneAttributesFromElement(other);
    copyNonAttributePropertiesFromElement(other);
}

void Element::createUniqueElementData()
{
    if (!m_elementData)
        m_elementData = UniqueElementData::create();
    else {
        ASSERT(!m_elementData->isUnique());
        m_elementData = static_cast<ShareableElementData*>(m_elementData.get())->makeUniqueCopy();
    }
}

#if ENABLE(SVG)
bool Element::hasPendingResources() const
{
    return hasRareData() && elementRareData()->hasPendingResources();
}

void Element::setHasPendingResources()
{
    ensureElementRareData().setHasPendingResources(true);
}

void Element::clearHasPendingResources()
{
    ensureElementRareData().setHasPendingResources(false);
}
#endif

} // namespace WebCore

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
#include "ClassList.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "DOMTokenList.h"
#include "DatasetDOMStringMap.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentSharedObjectPool.h"
#include "ElementRareData.h"
#include "ExceptionCode.h"
#include "FlowThreadController.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLLabelElement.h"
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
#include "RenderView.h"
#include "RenderWidget.h"
#include "SelectorQuery.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "Text.h"
#include "TextIterator.h"
#include "VoidCallback.h"
#include "WebCoreMemoryInstrumentation.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include <wtf/BitVector.h>
#include <wtf/text/CString.h>

#if ENABLE(SVG)
#include "SVGElement.h"
#include "SVGNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;
using namespace XMLNames;
    
class StyleResolverParentPusher {
public:
    StyleResolverParentPusher(Element* parent)
        : m_parent(parent)
        , m_pushedStyleResolver(0)
    {
    }
    void push()
    {
        if (m_pushedStyleResolver)
            return;
        m_pushedStyleResolver = m_parent->document()->styleResolver();
        m_pushedStyleResolver->pushParentElement(m_parent);
    }
    ~StyleResolverParentPusher()
    {

        if (!m_pushedStyleResolver)
            return;

        // This tells us that our pushed style selector is in a bad state,
        // so we should just bail out in that scenario.
        ASSERT(m_pushedStyleResolver == m_parent->document()->styleResolver());
        if (m_pushedStyleResolver != m_parent->document()->styleResolver())
            return;

        m_pushedStyleResolver->popParentElement(m_parent);
    }

private:
    Element* m_parent;
    StyleResolver* m_pushedStyleResolver;
};

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

static AttrNodeList* ensureAttrNodeListForElement(Element* element)
{
    if (element->hasSyntheticAttrChildNodes()) {
        ASSERT(attrNodeListMap().contains(element));
        return attrNodeListMap().get(element);
    }
    ASSERT(!attrNodeListMap().contains(element));
    element->setHasSyntheticAttrChildNodes(true);
    AttrNodeListMap::AddResult result = attrNodeListMap().add(element, adoptPtr(new AttrNodeList));
    return result.iterator->value.get();
}

static void removeAttrNodeListForElement(Element* element)
{
    ASSERT(element->hasSyntheticAttrChildNodes());
    ASSERT(attrNodeListMap().contains(element));
    attrNodeListMap().remove(element);
    element->setHasSyntheticAttrChildNodes(false);
}

static Attr* findAttrNodeInList(AttrNodeList* attrNodeList, const QualifiedName& name)
{
    for (unsigned i = 0; i < attrNodeList->size(); ++i) {
        if (attrNodeList->at(i)->qualifiedName() == name)
            return attrNodeList->at(i).get();
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
    }

    if (ElementShadow* elementShadow = shadow()) {
        elementShadow->removeAllShadowRoots();
        elementRareData()->setShadow(nullptr);
    }

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();
}

inline ElementRareData* Element::elementRareData() const
{
    ASSERT(hasRareData());
    return static_cast<ElementRareData*>(rareData());
}

inline ElementRareData* Element::ensureElementRareData()
{
    return static_cast<ElementRareData*>(ensureRareData());
}

PassOwnPtr<NodeRareData> Element::createRareData()
{
    return adoptPtr(new ElementRareData(documentInternal()));
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

PassRefPtr<Attr> Element::detachAttribute(size_t index)
{
    ASSERT(attributeData());

    const Attribute* attribute = attributeData()->attributeItem(index);
    ASSERT(attribute);

    RefPtr<Attr> attrNode = attrIfExists(attribute->name());
    if (attrNode)
        detachAttrNodeFromElementWithValue(attrNode.get(), attribute->value());
    else
        attrNode = Attr::create(document(), attribute->name(), attribute->value());

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return attrNode.release();
}

void Element::removeAttribute(const QualifiedName& name)
{
    if (!attributeData())
        return;

    size_t index = attributeData()->getAttributeItemIndex(name);
    if (index == notFound)
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
    ensureUpdatedAttributeData();
    ElementRareData* rareData = const_cast<Element*>(this)->ensureElementRareData();
    if (NamedNodeMap* attributeMap = rareData->attributeMap())
        return attributeMap;

    rareData->setAttributeMap(NamedNodeMap::create(const_cast<Element*>(this)));
    return rareData->attributeMap();
}

Node::NodeType Element::nodeType() const
{
    return ELEMENT_NODE;
}

bool Element::hasAttribute(const QualifiedName& name) const
{
    return hasAttributeNS(name.namespaceURI(), name.localName());
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const
{
    if (!attributeData())
        return nullAtom;

    if (UNLIKELY(name == styleAttr && attributeData()->m_styleAttributeIsDirty))
        updateStyleAttribute();

#if ENABLE(SVG)
    if (UNLIKELY(attributeData()->m_animatedSVGAttributesAreDirty))
        updateAnimatedSVGAttribute(name);
#endif

    if (const Attribute* attribute = getAttributeItem(name))
        return attribute->value();
    return nullAtom;
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
        return adjustForAbsoluteZoom(renderer->pixelSnappedOffsetWidth(), renderer);
    return 0;
}

int Element::offsetHeight()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustForAbsoluteZoom(renderer->pixelSnappedOffsetHeight(), renderer);
    return 0;
}

Element* Element::offsetParent()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (RenderObject* rend = renderer())
        if (RenderObject* offsetParent = rend->offsetParent())
            return static_cast<Element*>(offsetParent->node());
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
        return adjustForAbsoluteZoom(renderer->pixelSnappedClientWidth(), renderer);
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
        return adjustForAbsoluteZoom(renderer->pixelSnappedClientHeight(), renderer);
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
        SVGElement* svgElement = static_cast<SVGElement*>(this);
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
        SVGElement* svgElement = static_cast<SVGElement*>(this);
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

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
}

const AtomicString& Element::getAttribute(const AtomicString& name) const
{
    if (!attributeData())
        return nullAtom;

    bool ignoreCase = shouldIgnoreAttributeCase(this);

    // Update the 'style' attribute if it's invalid and being requested:
    if (attributeData()->m_styleAttributeIsDirty && equalPossiblyIgnoringCase(name, styleAttr.localName(), ignoreCase))
        updateStyleAttribute();

#if ENABLE(SVG)
    if (attributeData()->m_animatedSVGAttributesAreDirty) {
        // We're not passing a namespace argument on purpose. SVGNames::*Attr are defined w/o namespaces as well.
        updateAnimatedSVGAttribute(QualifiedName(nullAtom, name, nullAtom));
    }
#endif

    if (const Attribute* attribute = attributeData()->getAttributeItem(name, ignoreCase))
        return attribute->value();
    return nullAtom;
}

const AtomicString& Element::getAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return getAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

void Element::setAttribute(const AtomicString& name, const AtomicString& value, ExceptionCode& ec)
{
    if (!Document::isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }

    const AtomicString& localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;

    size_t index = ensureUpdatedAttributeData()->getAttributeItemIndex(localName, false);
    const QualifiedName& qName = index != notFound ? attributeItem(index)->name() : QualifiedName(nullAtom, localName, nullAtom);
    setAttributeInternal(index, qName, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value)
{
    setAttributeInternal(ensureUpdatedAttributeData()->getAttributeItemIndex(name), name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setSynchronizedLazyAttribute(const QualifiedName& name, const AtomicString& value)
{
    setAttributeInternal(mutableAttributeData()->getAttributeItemIndex(name), name, value, InSynchronizationOfLazyAttribute);
}

inline void Element::setAttributeInternal(size_t index, const QualifiedName& name, const AtomicString& newValue, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (newValue.isNull()) {
        if (index != notFound)
            removeAttributeInternal(index, inSynchronizationOfLazyAttribute);
        return;
    }

    if (index == notFound) {
        addAttributeInternal(name, newValue, inSynchronizationOfLazyAttribute);
        return;
    }

    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, attributeItem(index)->value(), newValue);

    if (newValue != attributeItem(index)->value()) {
        // If there is an Attr node hooked to this attribute, the Attr::setValue() call below
        // will write into the ElementAttributeData.
        // FIXME: Refactor this so it makes some sense.
        if (RefPtr<Attr> attrNode = attrIfExists(name))
            attrNode->setValue(newValue);
        else
            mutableAttributeData()->attributeItem(index)->setValue(newValue);
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

void Element::attributeChanged(const QualifiedName& name, const AtomicString& newValue)
{
    if (ElementShadow* parentElementShadow = shadowOfParentForDistribution(this)) {
        if (shouldInvalidateDistributionWhenAttributeChanged(parentElementShadow, name, newValue))
            parentElementShadow->invalidateDistribution();
    }

    parseAttribute(name, newValue);

    document()->incDOMTreeVersion();

    StyleResolver* styleResolver = document()->styleResolverIfExists();
    bool testShouldInvalidateStyle = attached() && styleResolver && styleChangeType() < FullStyleChange;
    bool shouldInvalidateStyle = false;

    if (isIdAttributeName(name)) {
        AtomicString oldId = attributeData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document()->inQuirksMode());
        if (newId != oldId) {
            attributeData()->setIdForStyleResolution(newId);
            shouldInvalidateStyle = testShouldInvalidateStyle && checkNeedsStyleInvalidationForIdChange(oldId, newId, styleResolver);
        }
    } else if (name == classAttr)
        classAttributeChanged(newValue);
    else if (name == HTMLNames::nameAttr)
        setHasName(!newValue.isNull());
    else if (name == HTMLNames::pseudoAttr)
        shouldInvalidateStyle |= testShouldInvalidateStyle && isInShadowTree();

    shouldInvalidateStyle |= testShouldInvalidateStyle && styleResolver->hasSelectorForAttribute(name.localName());

    invalidateNodeListCachesInAncestors(&name, this);

    // If there is currently no StyleResolver, we can't be sure that this attribute change won't affect style.
    shouldInvalidateStyle |= !styleResolver;

    if (shouldInvalidateStyle)
        setNeedsStyleRecalc();

    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->handleAttributeChanged(name, this);
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
        const ElementAttributeData* attributeData = ensureAttributeData();
        const bool shouldFoldCase = document()->inQuirksMode();
        const SpaceSplitString oldClasses = attributeData->classNames();

        attributeData->setClass(newClassString, shouldFoldCase);

        const SpaceSplitString& newClasses = attributeData->classNames();
        shouldInvalidateStyle = testShouldInvalidateStyle && checkSelectorForClassChange(oldClasses, newClasses, *styleResolver);
    } else if (const ElementAttributeData* attributeData = this->attributeData()) {
        const SpaceSplitString& oldClasses = attributeData->classNames();
        shouldInvalidateStyle = testShouldInvalidateStyle && checkSelectorForClassChange(oldClasses, *styleResolver);

        attributeData->clearClass();
    }

    if (DOMTokenList* classList = optionalClassList())
        static_cast<ClassList*>(classList)->reset(newClassString);

    if (shouldInvalidateStyle)
        setNeedsStyleRecalc();
}

bool Element::shouldInvalidateDistributionWhenAttributeChanged(ElementShadow* elementShadow, const QualifiedName& name, const AtomicString& newValue)
{
    ASSERT(elementShadow);
    elementShadow->ensureSelectFeatureSetCollected();

    if (isIdAttributeName(name)) {
        AtomicString oldId = attributeData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document()->inQuirksMode());
        if (newId != oldId) {
            if (!oldId.isEmpty() && elementShadow->selectRuleFeatureSet().hasSelectorForId(oldId))
                return true;
            if (!newId.isEmpty() && elementShadow->selectRuleFeatureSet().hasSelectorForId(newId))
                return true;
        }
    }

    if (name == HTMLNames::classAttr) {
        const AtomicString& newClassString = newValue;
        if (classStringHasClassName(newClassString)) {
            const ElementAttributeData* attributeData = ensureAttributeData();
            const bool shouldFoldCase = document()->inQuirksMode();
            const SpaceSplitString& oldClasses = attributeData->classNames();
            const SpaceSplitString newClasses(newClassString, shouldFoldCase);
            if (checkSelectorForClassChange(oldClasses, newClasses, elementShadow->selectRuleFeatureSet()))
                return true;
        } else if (const ElementAttributeData* attributeData = this->attributeData()) {
            const SpaceSplitString& oldClasses = attributeData->classNames();
            if (checkSelectorForClassChange(oldClasses, elementShadow->selectRuleFeatureSet()))
                return true;
        }
    }

    return elementShadow->selectRuleFeatureSet().hasSelectorForAttribute(name.localName());
}

// Returns true is the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

static bool isEventHandlerAttribute(const QualifiedName& name)
{
    return name.namespaceURI().isNull() && name.localName().startsWith("on");
}

// FIXME: Share code with Element::isURLAttribute.
static bool isAttributeToRemove(const QualifiedName& name, const AtomicString& value)
{
    return (name.localName() == hrefAttr.localName() || name.localName() == nohrefAttr.localName()
        || name == srcAttr || name == actionAttr || name == formactionAttr) && protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(value));
}

void Element::parserSetAttributes(const Vector<Attribute>& attributeVector, FragmentScriptingPermission scriptingPermission)
{
    ASSERT(!inDocument());
    ASSERT(!parentNode());

    ASSERT(!m_attributeData);

    if (attributeVector.isEmpty())
        return;

    Vector<Attribute> filteredAttributes = attributeVector;

    // If the element is created as result of a paste or drag-n-drop operation
    // we want to remove all the script and event handlers.
    if (scriptingPermission == DisallowScriptingContent) {
        unsigned i = 0;
        while (i < filteredAttributes.size()) {
            Attribute& attribute = filteredAttributes[i];
            if (isEventHandlerAttribute(attribute.name())) {
                filteredAttributes.remove(i);
                continue;
            }

            if (isAttributeToRemove(attribute.name(), attribute.value()))
                attribute.setValue(emptyAtom);
            i++;
        }
    }

    if (document() && document()->sharedObjectPool())
        m_attributeData = document()->sharedObjectPool()->cachedImmutableElementAttributeData(filteredAttributes);
    else
        m_attributeData = ElementAttributeData::createImmutable(filteredAttributes);

    // Iterate over the set of attributes we already have on the stack in case
    // attributeChanged mutates m_attributeData.
    // FIXME: Find a way so we don't have to do this.
    for (unsigned i = 0; i < filteredAttributes.size(); ++i)
        attributeChanged(filteredAttributes[i].name(), filteredAttributes[i].value());
}

bool Element::hasAttributes() const
{
    updateInvalidAttributes();
    return attributeData() && attributeData()->length();
}

bool Element::hasEquivalentAttributes(const Element* other) const
{
    const ElementAttributeData* attributeData = updatedAttributeData();
    const ElementAttributeData* otherAttributeData = other->updatedAttributeData();
    if (attributeData == otherAttributeData)
        return true;
    if (attributeData)
        return attributeData->isEquivalent(otherAttributeData);
    if (otherAttributeData)
        return otherAttributeData->isEquivalent(attributeData);
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

const QualifiedName& Element::imageSourceAttributeName() const
{
    return srcAttr;
}

bool Element::rendererIsNeeded(const NodeRenderingContext& context)
{
    return context.style()->display() != NONE;
}

RenderObject* Element::createRenderer(RenderArena*, RenderStyle* style)
{
    return RenderObject::createObject(this, style);
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
bool Element::isDateTimeFieldElement() const
{
    return false;
}
#endif

bool Element::wasChangedSinceLastFormControlChangeEvent() const
{
    return false;
}

void Element::setChangedSinceLastFormControlChangeEvent(bool)
{
}

Node::InsertionNotificationRequest Element::insertedInto(ContainerNode* insertionPoint)
{
    // need to do superclass processing first so inDocument() is true
    // by the time we reach updateId
    ContainerNode::insertedInto(insertionPoint);

#if ENABLE(FULLSCREEN_API)
    if (containsFullScreenElement() && parentElement() && !parentElement()->containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);
#endif

    if (!insertionPoint->inDocument())
        return InsertionDone;

    const AtomicString& idValue = getIdAttribute();
    if (!idValue.isNull())
        updateId(nullAtom, idValue);

    const AtomicString& nameValue = getNameAttribute();
    if (!nameValue.isNull())
        updateName(nullAtom, nameValue);

    if (hasTagName(labelTag)) {
        TreeScope* scope = treeScope();
        if (scope->shouldCacheLabelsByForAttribute())
            updateLabel(scope, nullAtom, fastGetAttribute(forAttr));
    }

    return InsertionDone;
}

void Element::removedFrom(ContainerNode* insertionPoint)
{
#if ENABLE(DIALOG_ELEMENT)
    setIsInTopLayer(false);
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

    if (insertionPoint->inDocument()) {
        const AtomicString& idValue = getIdAttribute();
        if (!idValue.isNull() && inDocument())
            updateId(insertionPoint->treeScope(), idValue, nullAtom);

        const AtomicString& nameValue = getNameAttribute();
        if (!nameValue.isNull())
            updateName(nameValue, nullAtom);

        if (hasTagName(labelTag)) {
            TreeScope* treeScope = insertionPoint->treeScope();
            if (treeScope->shouldCacheLabelsByForAttribute())
                updateLabel(treeScope, fastGetAttribute(forAttr), nullAtom);
        }
    }

    ContainerNode::removedFrom(insertionPoint);
}

void Element::createRendererIfNeeded()
{
    NodeRenderingContext(this).createRendererForElementIfNeeded();
}

void Element::attach()
{
    PostAttachCallbackDisabler callbackDisabler(this);
    StyleResolverParentPusher parentPusher(this);
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    createRendererIfNeeded();

    if (parentElement() && parentElement()->isInCanvasSubtree())
        setIsInCanvasSubtree(true);

    // When a shadow root exists, it does the work of attaching the children.
    if (ElementShadow* shadow = this->shadow()) {
        parentPusher.push();
        shadow->attach();
    } else if (firstChild())
        parentPusher.push();

    ContainerNode::attach();

    if (hasRareData()) {   
        ElementRareData* data = elementRareData();
        if (data->needsFocusAppearanceUpdateSoonAfterAttach()) {
            if (isFocusable() && document()->focusedNode() == this)
                document()->updateFocusAppearanceSoon(false /* don't restore selection */);
            data->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
        }
    }
}

void Element::unregisterNamedFlowContentNode()
{
    if (document()->cssRegionsEnabled() && inNamedFlow() && document()->renderView())
        document()->renderView()->flowThreadController()->unregisterNamedFlowContentNode(this);
}

void Element::detach()
{
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
    unregisterNamedFlowContentNode();
    cancelFocusAppearanceUpdate();
    if (hasRareData()) {
        ElementRareData* data = elementRareData();
        data->setPseudoElement(BEFORE, 0);
        data->setPseudoElement(AFTER, 0);
        data->setIsInCanvasSubtree(false);
        data->resetComputedStyle();
        data->resetDynamicRestyleObservations();
    }

    if (ElementShadow* shadow = this->shadow()) {
        detachChildrenIfNeeded();
        shadow->detach();
    }
    ContainerNode::detach();
}

bool Element::pseudoStyleCacheIsInvalid(const RenderStyle* currentStyle, RenderStyle* newStyle)
{
    ASSERT(currentStyle == renderStyle());
    ASSERT(renderer());

    if (!currentStyle)
        return false;

    const PseudoStyleCache* pseudoStyleCache = currentStyle->cachedPseudoStyles();
    if (!pseudoStyleCache)
        return false;

    size_t cacheSize = pseudoStyleCache->size();
    for (size_t i = 0; i < cacheSize; ++i) {
        RefPtr<RenderStyle> newPseudoStyle;
        PseudoId pseudoId = pseudoStyleCache->at(i)->styleType();
        if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED)
            newPseudoStyle = renderer()->uncachedFirstLineStyle(newStyle);
        else
            newPseudoStyle = renderer()->getUncachedPseudoStyle(pseudoId, newStyle, newStyle);
        if (!newPseudoStyle)
            return true;
        if (*newPseudoStyle != *pseudoStyleCache->at(i)) {
            if (pseudoId < FIRST_INTERNAL_PSEUDOID)
                newStyle->setHasPseudoStyle(pseudoId);
            newStyle->addCachedPseudoStyle(newPseudoStyle);
            if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED) {
                // FIXME: We should do an actual diff to determine whether a repaint vs. layout
                // is needed, but for now just assume a layout will be required.  The diff code
                // in RenderObject::setStyle would need to be factored out so that it could be reused.
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
            }
            return true;
        }
    }
    return false;
}

PassRefPtr<RenderStyle> Element::styleForRenderer()
{
    if (hasCustomCallbacks()) {
        if (RefPtr<RenderStyle> style = customStyleForRenderer())
            return style.release();
    }

    return document()->styleResolver()->styleForElement(this);
}

void Element::recalcStyle(StyleChange change)
{
    if (hasCustomCallbacks()) {
        if (!willRecalcStyle(change))
            return;
    }

    // Ref currentStyle in case it would otherwise be deleted when setting the new style in the renderer.
    RefPtr<RenderStyle> currentStyle(renderStyle());
    bool hasParentStyle = parentNodeForRenderingAndStyle() ? static_cast<bool>(parentNodeForRenderingAndStyle()->renderStyle()) : false;
    bool hasDirectAdjacentRules = childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = childrenAffectedByForwardPositionalRules();

    if ((change > NoChange || needsStyleRecalc())) {
        if (hasRareData())
            elementRareData()->resetComputedStyle();
    }
    if (hasParentStyle && (change >= Inherit || needsStyleRecalc())) {
        RefPtr<RenderStyle> newStyle = styleForRenderer();
        StyleChange ch = Node::diff(currentStyle.get(), newStyle.get(), document());
        if (ch == Detach || !currentStyle) {
            // FIXME: The style gets computed twice by calling attach. We could do better if we passed the style along.
            reattach();
            // attach recalculates the style for all children. No need to do it twice.
            clearNeedsStyleRecalc();
            clearChildNeedsStyleRecalc();

            if (hasCustomCallbacks())
                didRecalcStyle(change);
            return;
        }

        if (RenderObject* renderer = this->renderer()) {
            if (ch != NoChange || pseudoStyleCacheIsInvalid(currentStyle.get(), newStyle.get()) || (change == Force && renderer->requiresForcedStyleRecalcPropagation()) || styleChangeType() == SyntheticStyleChange)
                renderer->setAnimatableStyle(newStyle.get());
            else if (needsStyleRecalc()) {
                // Although no change occurred, we use the new style so that the cousin style sharing code won't get
                // fooled into believing this style is the same.
                renderer->setStyleInternal(newStyle.get());
            }
        }

        // If "rem" units are used anywhere in the document, and if the document element's font size changes, then go ahead and force font updating
        // all the way down the tree. This is simpler than having to maintain a cache of objects (and such font size changes should be rare anyway).
        if (document()->styleSheetCollection()->usesRemUnits() && document()->documentElement() == this && ch != NoChange && currentStyle && newStyle && currentStyle->fontSize() != newStyle->fontSize()) {
            // Cached RenderStyles may depend on the rem units.
            document()->styleResolver()->invalidateMatchedPropertiesCache();
            change = Force;
        }

        if (change != Force) {
            if (styleChangeType() >= FullStyleChange)
                change = Force;
            else
                change = ch;
        }
    }
    StyleResolverParentPusher parentPusher(this);

    // FIXME: This does not care about sibling combinators. Will be necessary in XBL2 world.
    if (ElementShadow* shadow = this->shadow()) {
        if (change >= Inherit || shadow->childNeedsStyleRecalc() || shadow->needsStyleRecalc()) {
            parentPusher.push();
            shadow->recalcStyle(change);
        }
    }

    // FIXME: This check is good enough for :hover + foo, but it is not good enough for :hover + foo + bar.
    // For now we will just worry about the common case, since it's a lot trickier to get the second case right
    // without doing way too much re-resolution.
    bool forceCheckOfNextElementSibling = false;
    bool forceCheckOfAnyElementSibling = false;
    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode()) {
            toText(n)->recalcTextStyle(change);
            continue;
        } 
        if (!n->isElementNode()) 
            continue;
        Element* element = static_cast<Element*>(n);
        bool childRulesChanged = element->needsStyleRecalc() && element->styleChangeType() == FullStyleChange;
        if ((forceCheckOfNextElementSibling || forceCheckOfAnyElementSibling))
            element->setNeedsStyleRecalc();
        if (change >= Inherit || element->childNeedsStyleRecalc() || element->needsStyleRecalc()) {
            parentPusher.push();
            element->recalcStyle(change);
        }
        forceCheckOfNextElementSibling = childRulesChanged && hasDirectAdjacentRules;
        forceCheckOfAnyElementSibling = forceCheckOfAnyElementSibling || (childRulesChanged && hasIndirectAdjacentRules);
    }

    clearNeedsStyleRecalc();
    clearChildNeedsStyleRecalc();
    
    if (hasCustomCallbacks())
        didRecalcStyle(change);
}

ElementShadow* Element::shadow() const
{
    if (!hasRareData())
        return 0;

    return elementRareData()->shadow();
}

ElementShadow* Element::ensureShadow()
{
    if (ElementShadow* shadow = ensureElementRareData()->shadow())
        return shadow;

    ElementRareData* data = elementRareData();
    data->setShadow(adoptPtr(new ElementShadow()));
    return data->shadow();
}

PassRefPtr<ShadowRoot> Element::createShadowRoot(ExceptionCode& ec)
{
    return ShadowRoot::create(this, ec);
}

ShadowRoot* Element::shadowRoot() const
{
    ElementShadow* elementShadow = shadow();
    if (!elementShadow)
        return 0;
    ShadowRoot* shadowRoot = elementShadow->youngestShadowRoot();
    if (!shadowRoot->isAccessible())
        return 0;
    return shadowRoot;
}

ShadowRoot* Element::userAgentShadowRoot() const
{
    if (ElementShadow* elementShadow = shadow()) {
        if (ShadowRoot* shadowRoot = elementShadow->oldestShadowRoot()) {
            ASSERT(shadowRoot->type() == ShadowRoot::UserAgentShadowRoot);
            return shadowRoot;
        }
    }

    return 0;
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

    if (ElementShadow * shadow = this->shadow())
        shadow->invalidateDistribution();
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

    updateInvalidAttributes();
    ElementAttributeData* attributeData = mutableAttributeData();

    size_t index = attributeData->getAttributeItemIndex(attrNode->qualifiedName());
    if (index != notFound) {
        if (oldAttrNode)
            detachAttrNodeFromElementWithValue(oldAttrNode.get(), attributeData->attributeItem(index)->value());
        else
            oldAttrNode = Attr::create(document(), attrNode->qualifiedName(), attributeData->attributeItem(index)->value());
    }

    setAttributeInternal(index, attrNode->qualifiedName(), attrNode->value(), NotInSynchronizationOfLazyAttribute);

    attrNode->attachToElement(this);
    ensureAttrNodeListForElement(this)->append(attrNode);

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

    const ElementAttributeData* attributeData = updatedAttributeData();
    ASSERT(attributeData);

    size_t index = attributeData->getAttributeItemIndex(attr->qualifiedName());
    if (index == notFound) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    return detachAttribute(index);
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

void Element::removeAttributeInternal(size_t index, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT(index < attributeCount());

    ElementAttributeData* attributeData = mutableAttributeData();

    QualifiedName name = attributeData->attributeItem(index)->name();
    AtomicString valueBeingRemoved = attributeData->attributeItem(index)->value();

    if (!inSynchronizationOfLazyAttribute) {
        if (!valueBeingRemoved.isNull())
            willModifyAttribute(name, valueBeingRemoved, nullAtom);
    }

    if (RefPtr<Attr> attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode.get(), attributeData->attributeItem(index)->value());

    attributeData->removeAttribute(index);

    if (!inSynchronizationOfLazyAttribute)
        didRemoveAttribute(name);
}

void Element::addAttributeInternal(const QualifiedName& name, const AtomicString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, nullAtom, value);
    mutableAttributeData()->addAttribute(Attribute(name, value));
    if (!inSynchronizationOfLazyAttribute)
        didAddAttribute(name, value);
}

void Element::removeAttribute(const AtomicString& name)
{
    if (!attributeData())
        return;

    AtomicString localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    size_t index = attributeData()->getAttributeItemIndex(localName, false);
    if (index == notFound) {
        if (UNLIKELY(localName == styleAttr) && attributeData()->m_styleAttributeIsDirty && isStyledElement())
            static_cast<StyledElement*>(this)->removeAllInlineStyleProperties();
        return;
    }

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::removeAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    removeAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

PassRefPtr<Attr> Element::getAttributeNode(const AtomicString& name)
{
    const ElementAttributeData* attributeData = updatedAttributeData();
    if (!attributeData)
        return 0;
    const Attribute* attribute = attributeData->getAttributeItem(name, shouldIgnoreAttributeCase(this));
    if (!attribute)
        return 0;
    return ensureAttr(attribute->name());
}

PassRefPtr<Attr> Element::getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    const ElementAttributeData* attributeData = updatedAttributeData();
    if (!attributeData)
        return 0;
    const Attribute* attribute = attributeData->getAttributeItem(QualifiedName(nullAtom, localName, namespaceURI));
    if (!attribute)
        return 0;
    return ensureAttr(attribute->name());
}

bool Element::hasAttribute(const AtomicString& name) const
{
    if (!attributeData())
        return false;

    // This call to String::lower() seems to be required but
    // there may be a way to remove it.
    AtomicString localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    return updatedAttributeData()->getAttributeItem(localName, false);
}

bool Element::hasAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    const ElementAttributeData* attributeData = updatedAttributeData();
    if (!attributeData)
        return false;
    return attributeData->getAttributeItem(QualifiedName(nullAtom, localName, namespaceURI));
}

CSSStyleDeclaration *Element::style()
{
    return 0;
}

void Element::focus(bool restorePreviousSelection)
{
    if (!inDocument())
        return;

    Document* doc = document();
    if (doc->focusedNode() == this)
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
        if (!page->focusController()->setFocusedNode(this, doc->frame()))
            return;
    }

    // Setting the focused node above might have invalidated the layout due to scripts.
    doc->updateLayoutIgnorePendingStylesheets();

    if (!isFocusable()) {
        ensureElementRareData()->setNeedsFocusAppearanceUpdateSoonAfterAttach(true);
        return;
    }
        
    cancelFocusAppearanceUpdate();
    updateFocusAppearance(restorePreviousSelection);
}

void Element::updateFocusAppearance(bool /*restorePreviousSelection*/)
{
    if (isRootEditableElement()) {
        Frame* frame = document()->frame();
        if (!frame)
            return;
        
        // When focusing an editable element in an iframe, don't reset the selection if it already contains a selection.
        if (this == frame->selection()->rootEditableElement())
            return;

        // FIXME: We should restore the previous selection if there is one.
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this), DOWNSTREAM);
        
        if (frame->selection()->shouldChangeSelection(newSelection)) {
            frame->selection()->setSelection(newSelection);
            frame->selection()->revealSelection();
        }
    } else if (renderer() && !renderer()->isWidget())
        renderer()->scrollRectToVisible(boundingBox());
}

void Element::blur()
{
    cancelFocusAppearanceUpdate();
    Document* doc = document();
    if (treeScope()->focusedNode() == this) {
        if (doc->frame())
            doc->frame()->page()->focusController()->setFocusedNode(0, doc->frame());
        else
            doc->setFocusedNode(0);
    }
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
    ensureElementRareData()->setMinimumSizeForResizing(size);
}

RenderStyle* Element::computedStyle(PseudoId pseudoElementSpecifier)
{
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

    ElementRareData* data = ensureElementRareData();
    if (!data->computedStyle())
        data->setComputedStyle(document()->styleForElementIgnoringPendingStylesheets(this));
    return pseudoElementSpecifier ? data->computedStyle()->getCachedPseudoStyle(pseudoElementSpecifier) : data->computedStyle();
}

void Element::setStyleAffectedByEmpty()
{
    ensureElementRareData()->setStyleAffectedByEmpty(true);
}

void Element::setChildrenAffectedByHover(bool value)
{
    if (value || hasRareData())
        ensureElementRareData()->setChildrenAffectedByHover(value);
}

void Element::setChildrenAffectedByActive(bool value)
{
    if (value || hasRareData())
        ensureElementRareData()->setChildrenAffectedByActive(value);
}

void Element::setChildrenAffectedByDrag(bool value)
{
    if (value || hasRareData())
        ensureElementRareData()->setChildrenAffectedByDrag(value);
}

void Element::setChildrenAffectedByFirstChildRules()
{
    ensureElementRareData()->setChildrenAffectedByFirstChildRules(true);
}

void Element::setChildrenAffectedByLastChildRules()
{
    ensureElementRareData()->setChildrenAffectedByLastChildRules(true);
}

void Element::setChildrenAffectedByDirectAdjacentRules()
{
    ensureElementRareData()->setChildrenAffectedByDirectAdjacentRules(true);
}

void Element::setChildrenAffectedByForwardPositionalRules()
{
    ensureElementRareData()->setChildrenAffectedByForwardPositionalRules(true);
}

void Element::setChildrenAffectedByBackwardPositionalRules()
{
    ensureElementRareData()->setChildrenAffectedByBackwardPositionalRules(true);
}

void Element::setChildIndex(unsigned index)
{
    ElementRareData* rareData = ensureElementRareData();
    if (RenderStyle* style = renderStyle())
        style->setUnique();
    rareData->setChildIndex(index);
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
    ensureElementRareData()->setIsInCanvasSubtree(isInCanvasSubtree);
}

bool Element::isInCanvasSubtree() const
{
    return hasRareData() && elementRareData()->isInCanvasSubtree();
}

AtomicString Element::computeInheritedLanguage() const
{
    const Node* n = this;
    AtomicString value;
    // The language property is inherited, so we iterate over the parents to find the first language.
    do {
        if (n->isElementNode()) {
            if (const ElementAttributeData* attributeData = static_cast<const Element*>(n)->attributeData()) {
                // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
                if (const Attribute* attribute = attributeData->getAttributeItem(XMLNames::langAttr))
                    value = attribute->value();
                else if (const Attribute* attribute = attributeData->getAttributeItem(HTMLNames::langAttr))
                    value = attribute->value();
            }
        } else if (n->isDocumentNode()) {
            // checking the MIME content-language
            value = static_cast<const Document*>(n)->contentLanguage();
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
    if (document()->focusedNode() == this)
        document()->cancelFocusAppearanceUpdate();
}

void Element::normalizeAttributes()
{
    updateInvalidAttributes();
    if (AttrNodeList* attrNodeList = attrNodeListForElement(this)) {
        for (unsigned i = 0; i < attrNodeList->size(); ++i)
            attrNodeList->at(i)->normalize();
    }
}

void Element::updatePseudoElement(PseudoId pseudoId, StyleChange change)
{
    PseudoElement* existing = hasRareData() ? elementRareData()->pseudoElement(pseudoId) : 0;
    if (existing) {
        // PseudoElement styles hang off their parent element's style so if we needed
        // a style recalc we should Force one on the pseudo.
        existing->recalcStyle(needsStyleRecalc() ? Force : change);

        // Wait until our parent is not displayed or pseudoElementRendererIsNeeded
        // is false, otherwise we could continously create and destroy PseudoElements
        // when RenderObject::isChildAllowed on our parent returns false for the
        // PseudoElement's renderer for each style recalc.
        if (!renderer() || !pseudoElementRendererIsNeeded(renderer()->getCachedPseudoStyle(pseudoId)))
            elementRareData()->setPseudoElement(pseudoId, 0);
    } else if (RefPtr<PseudoElement> element = createPseudoElementIfNeeded(pseudoId)) {
        element->attach();
        ensureElementRareData()->setPseudoElement(pseudoId, element.release());
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

PseudoElement* Element::beforePseudoElement() const
{
    return hasRareData() ? elementRareData()->pseudoElement(BEFORE) : 0;
}

PseudoElement* Element::afterPseudoElement() const
{
    return hasRareData() ? elementRareData()->pseudoElement(AFTER) : 0;
}

// ElementTraversal API
Element* Element::firstElementChild() const
{
    return WebCore::firstElementChild(this);
}

Element* Element::lastElementChild() const
{
    Node* n = lastChild();
    while (n && !n->isElementNode())
        n = n->previousSibling();
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

bool Element::shouldMatchReadOnlySelector() const
{
    return false;
}

bool Element::shouldMatchReadWriteSelector() const
{
    return false;
}

bool Element::webkitMatchesSelector(const String& selector, ExceptionCode& ec)
{
    if (selector.isEmpty()) {
        ec = SYNTAX_ERR;
        return false;
    }

    SelectorQuery* selectorQuery = document()->selectorQueryCache()->add(selector, document(), ec);
    if (!selectorQuery)
        return false;
    return selectorQuery->matches(this);
}

DOMTokenList* Element::classList()
{
    ElementRareData* data = ensureElementRareData();
    if (!data->classList())
        data->setClassList(ClassList::create(this));
    return data->classList();
}

DOMTokenList* Element::optionalClassList() const
{
    if (!hasRareData())
        return 0;
    return elementRareData()->classList();
}

DOMStringMap* Element::dataset()
{
    ElementRareData* data = ensureElementRareData();
    if (!data->dataset())
        data->setDataset(DatasetDOMStringMap::create(this));
    return data->dataset();
}

KURL Element::getURLAttribute(const QualifiedName& name) const
{
#if !ASSERT_DISABLED
    if (attributeData()) {
        if (const Attribute* attribute = getAttributeItem(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    return document()->completeURL(stripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
}

KURL Element::getNonEmptyURLAttribute(const QualifiedName& name) const
{
#if !ASSERT_DISABLED
    if (attributeData()) {
        if (const Attribute* attribute = getAttributeItem(name))
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
    ensureElementRareData()->setContainsFullScreenElement(flag);
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
    ensureElementRareData()->setIsInTopLayer(inTopLayer);
    setNeedsStyleRecalc(SyntheticStyleChange);
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
    for (const Element* element = this; element; element = element->parentOrHostElement()) {
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

const AtomicString& Element::webkitRegionOverset() const
{
    document()->updateLayoutIgnorePendingStylesheets();

    DEFINE_STATIC_LOCAL(AtomicString, undefinedState, ("undefined", AtomicString::ConstructFromLiteral));
    if (!document()->cssRegionsEnabled() || !renderRegion())
        return undefinedState;

    switch (renderRegion()->regionState()) {
    case RenderRegion::RegionFit: {
        DEFINE_STATIC_LOCAL(AtomicString, fitState, ("fit", AtomicString::ConstructFromLiteral));
        return fitState;
    }
    case RenderRegion::RegionEmpty: {
        DEFINE_STATIC_LOCAL(AtomicString, emptyState, ("empty", AtomicString::ConstructFromLiteral));
        return emptyState;
    }
    case RenderRegion::RegionOverset: {
        DEFINE_STATIC_LOCAL(AtomicString, overflowState, ("overset", AtomicString::ConstructFromLiteral));
        return overflowState;
    }
    case RenderRegion::RegionUndefined:
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
        return !SVGElement::isAnimatableAttribute(name);
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

void Element::updateLabel(TreeScope* scope, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue)
{
    ASSERT(hasTagName(labelTag));

    if (!inDocument())
        return;

    if (oldForAttributeValue == newForAttributeValue)
        return;

    if (!oldForAttributeValue.isEmpty())
        scope->removeLabel(oldForAttributeValue, static_cast<HTMLLabelElement*>(this));
    if (!newForAttributeValue.isEmpty())
        scope->addLabel(newForAttributeValue, static_cast<HTMLLabelElement*>(this));
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

#if ENABLE(MUTATION_OBSERVERS)
    if (OwnPtr<MutationObserverInterestGroup> recipients = MutationObserverInterestGroup::createForAttributesMutation(this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(this, name, oldValue));
#endif

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


void Element::updateNamedItemRegistration(const AtomicString& oldName, const AtomicString& newName)
{
    if (!document()->isHTMLDocument())
        return;

    if (!oldName.isEmpty())
        static_cast<HTMLDocument*>(document())->removeNamedItem(oldName);

    if (!newName.isEmpty())
        static_cast<HTMLDocument*>(document())->addNamedItem(newName);
}

void Element::updateExtraNamedItemRegistration(const AtomicString& oldId, const AtomicString& newId)
{
    if (!document()->isHTMLDocument())
        return;

    if (!oldId.isEmpty())
        static_cast<HTMLDocument*>(document())->removeExtraNamedItem(oldId);

    if (!newId.isEmpty())
        static_cast<HTMLDocument*>(document())->addExtraNamedItem(newId);
}

PassRefPtr<HTMLCollection> Element::ensureCachedHTMLCollection(CollectionType type)
{
    if (HTMLCollection* collection = cachedHTMLCollection(type))
        return collection;

    RefPtr<HTMLCollection> collection;
    if (type == TableRows) {
        ASSERT(hasTagName(tableTag));
        return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLTableRowsCollection>(this, type);
    } else if (type == SelectOptions) {
        ASSERT(hasTagName(selectTag));
        return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLOptionsCollection>(this, type);
    } else if (type == FormControls) {
        ASSERT(hasTagName(formTag) || hasTagName(fieldsetTag));
        return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLFormControlsCollection>(this, type);
#if ENABLE(MICRODATA)
    } else if (type == ItemProperties) {
        return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLPropertiesCollection>(this, type);
#endif
    }
    return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLCollection>(this, type);
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
    ensureElementRareData()->setSavedLayerScrollOffset(size);
}

PassRefPtr<Attr> Element::attrIfExists(const QualifiedName& name)
{
    if (AttrNodeList* attrNodeList = attrNodeListForElement(this))
        return findAttrNodeInList(attrNodeList, name);
    return 0;
}

PassRefPtr<Attr> Element::ensureAttr(const QualifiedName& name)
{
    AttrNodeList* attrNodeList = ensureAttrNodeListForElement(this);
    RefPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, name);
    if (!attrNode) {
        attrNode = Attr::create(this, name);
        attrNodeList->append(attrNode);
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
        const Attribute* attribute = attributeItem(i);
        if (RefPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, attribute->name()))
            attrNode->detachFromElementWithValue(attribute->value());
    }

    removeAttrNodeListForElement(this);
}

bool Element::willRecalcStyle(StyleChange)
{
    ASSERT(hasCustomCallbacks());
    return true;
}

void Element::didRecalcStyle(StyleChange)
{
    ASSERT(hasCustomCallbacks());
}


PassRefPtr<RenderStyle> Element::customStyleForRenderer()
{
    ASSERT(hasCustomCallbacks());
    return 0;
}

void Element::cloneAttributesFromElement(const Element& other)
{
    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    other.updateInvalidAttributes();
    if (!other.m_attributeData) {
        m_attributeData.clear();
        return;
    }

    const AtomicString& oldID = getIdAttribute();
    const AtomicString& newID = other.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        updateId(oldID, newID);

    const AtomicString& oldName = getNameAttribute();
    const AtomicString& newName = other.getNameAttribute();

    if (!oldName.isNull() || !newName.isNull())
        updateName(oldName, newName);

    // If 'other' has a mutable ElementAttributeData, convert it to an immutable one so we can share it between both elements.
    // We can only do this if there is no CSSOM wrapper for other's inline style, and there are no presentation attributes.
    if (other.m_attributeData->isMutable()
        && !other.m_attributeData->presentationAttributeStyle()
        && (!other.m_attributeData->inlineStyle() || !other.m_attributeData->inlineStyle()->hasCSSOMWrapper()))
        const_cast<Element&>(other).m_attributeData = other.m_attributeData->makeImmutableCopy();

    if (!other.m_attributeData->isMutable())
        m_attributeData = other.m_attributeData;
    else
        m_attributeData = other.m_attributeData->makeMutableCopy();

    for (unsigned i = 0; i < m_attributeData->length(); ++i) {
        const Attribute* attribute = const_cast<const ElementAttributeData*>(m_attributeData.get())->attributeItem(i);
        attributeChanged(attribute->name(), attribute->value());
    }
}

void Element::cloneDataFromElement(const Element& other)
{
    cloneAttributesFromElement(other);
    copyNonAttributePropertiesFromElement(other);
}

void Element::createMutableAttributeData()
{
    if (!m_attributeData)
        m_attributeData = ElementAttributeData::create();
    else
        m_attributeData = m_attributeData->makeMutableCopy();
}

void Element::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
    ContainerNode::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_tagName);
    info.addMember(m_attributeData);
}

#if ENABLE(SVG)
bool Element::hasPendingResources() const
{
    return hasRareData() && elementRareData()->hasPendingResources();
}

void Element::setHasPendingResources()
{
    ensureElementRareData()->setHasPendingResources(true);
}

void Element::clearHasPendingResources()
{
    ensureElementRareData()->setHasPendingResources(false);
}
#endif

} // namespace WebCore

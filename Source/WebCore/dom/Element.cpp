/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "ElementRareData.h"
#include "ExceptionCode.h"
#include "FlowThreadController.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLFormCollection.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "HTMLOptionsCollection.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableRowsCollection.h"
#include "InspectorInstrumentation.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "NamedNodeMap.h"
#include "NodeList.h"
#include "NodeRenderStyle.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "PointerLockController.h"
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
#include "WebKitAnimationList.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "htmlediting.h"
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

    if (ElementShadow* elementShadow = shadow()) {
        elementShadow->removeAllShadowRoots();
        elementRareData()->m_shadow.clear();
    }

    if (hasAttrList()) {
        ASSERT(m_attributeData);
        m_attributeData->detachAttrObjectsFromElement(this);
    }
}

inline ElementRareData* Element::elementRareData() const
{
    ASSERT(hasRareData());
    return static_cast<ElementRareData*>(NodeRareData::rareDataFromMap(this));
}
    
inline ElementRareData* Element::ensureElementRareData()
{
    return static_cast<ElementRareData*>(ensureRareData());
}
    
OwnPtr<NodeRareData> Element::createRareData()
{
    return adoptPtr(new ElementRareData);
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

    RefPtr<Attr> attr = attrIfExists(attribute->name());
    if (attr)
        attr->detachFromElementWithValue(attribute->value());
    else
        attr = Attr::create(document(), attribute->name(), attribute->value());

    mutableAttributeData()->removeAttribute(index, this);
    return attr.release();
}

void Element::removeAttribute(const QualifiedName& name)
{
    if (!attributeData())
        return;

    if (RefPtr<Attr> attr = attrIfExists(name))
        attr->detachFromElementWithValue(attr->value());

    size_t index = attributeData()->getAttributeItemIndex(name);
    if (index == notFound)
        return;

    mutableAttributeData()->removeAttribute(index, this);
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
    if (NamedNodeMap* attributeMap = rareData->m_attributeMap.get())
        return attributeMap;

    rareData->m_attributeMap = NamedNodeMap::create(const_cast<Element*>(this));
    return rareData->m_attributeMap.get();
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
    if (UNLIKELY(name == styleAttr) && !isStyleAttributeValid())
        updateStyleAttribute();

#if ENABLE(SVG)
    if (UNLIKELY(!areSVGAttributesValid()))
        updateAnimatedSVGAttribute(name);
#endif

    if (attributeData()) {
        if (const Attribute* attribute = getAttributeItem(name))
            return attribute->value();
    }
    return nullAtom;
}

void Element::scrollIntoView(bool alignToTop) 
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = getRect();
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

    LayoutRect bounds = getRect();
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
        if (svgElement->boundingBox(localRect))
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
        if (svgElement->boundingBox(localRect))
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
    return renderer()->view()->frameView()->contentsToScreen(renderer()->absoluteBoundingBoxRectIgnoringTransforms());
}

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
}

const AtomicString& Element::getAttribute(const AtomicString& name) const
{
    bool ignoreCase = shouldIgnoreAttributeCase(this);

    // Update the 'style' attribute if it's invalid and being requested:
    if (!isStyleAttributeValid() && equalPossiblyIgnoringCase(name, styleAttr.localName(), ignoreCase))
        updateStyleAttribute();

#if ENABLE(SVG)
    if (!areSVGAttributesValid()) {
        // We're not passing a namespace argument on purpose. SVGNames::*Attr are defined w/o namespaces as well.
        updateAnimatedSVGAttribute(QualifiedName(nullAtom, name, nullAtom));
    }
#endif

    if (attributeData()) {
        if (const Attribute* attribute = attributeData()->getAttributeItem(name, ignoreCase))
            return attribute->value();
    }

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

inline void Element::setAttributeInternal(size_t index, const QualifiedName& name, const AtomicString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ElementAttributeData* attributeData = mutableAttributeData();

    Attribute* old = index != notFound ? attributeData->attributeItem(index) : 0;
    if (value.isNull()) {
        if (old)
            attributeData->removeAttribute(index, this, inSynchronizationOfLazyAttribute);
        return;
    }

    if (!old) {
        attributeData->addAttribute(Attribute(name, value), this, inSynchronizationOfLazyAttribute);
        return;
    }

    if (inSynchronizationOfLazyAttribute == NotInSynchronizationOfLazyAttribute)
        willModifyAttribute(name, old->value(), value);

    if (RefPtr<Attr> attrNode = attrIfExists(name))
        attrNode->setValue(value);
    else
        old->setValue(value);

    if (inSynchronizationOfLazyAttribute == NotInSynchronizationOfLazyAttribute)
        didModifyAttribute(Attribute(old->name(), old->value()));
}

void Element::attributeChanged(const Attribute& attribute)
{
    document()->incDOMTreeVersion();

    if (isIdAttributeName(attribute.name())) {
        if (attribute.value() != attributeData()->idForStyleResolution()) {
            if (attribute.isNull())
                attributeData()->setIdForStyleResolution(nullAtom);
            else if (document()->inQuirksMode())
                attributeData()->setIdForStyleResolution(attribute.value().lower());
            else
                attributeData()->setIdForStyleResolution(attribute.value());
            setNeedsStyleRecalc();
        }
    } else if (attribute.name() == HTMLNames::nameAttr)
        setHasName(!attribute.isNull());

    if (!needsStyleRecalc() && document()->attached()) {
        StyleResolver* styleResolver = document()->styleResolverIfExists();
        if (!styleResolver || styleResolver->hasSelectorForAttribute(attribute.name().localName()))
            setNeedsStyleRecalc();
    }

    invalidateNodeListCachesInAncestors(&attribute.name(), this);

    if (!AXObjectCache::accessibilityEnabled())
        return;

    const QualifiedName& attrName = attribute.name();
    if (attrName == aria_activedescendantAttr) {
        // any change to aria-activedescendant attribute triggers accessibility focus change, but document focus remains intact
        document()->axObjectCache()->handleActiveDescendantChanged(this);
    } else if (attrName == roleAttr) {
        // the role attribute can change at any time, and the AccessibilityObject must pick up these changes
        document()->axObjectCache()->handleAriaRoleChanged(this);
    } else if (attrName == aria_valuenowAttr) {
        // If the valuenow attribute changes, AX clients need to be notified.
        document()->axObjectCache()->postNotification(this, AXObjectCache::AXValueChanged, true);
    } else if (attrName == aria_labelAttr || attrName == aria_labeledbyAttr || attrName == altAttr || attrName == titleAttr) {
        // If the content of an element changes due to an attribute change, notify accessibility.
        document()->axObjectCache()->contentChanged(this);
    } else if (attrName == aria_checkedAttr)
        document()->axObjectCache()->checkedStateChanged(this);
    else if (attrName == aria_selectedAttr)
        document()->axObjectCache()->selectedChildrenChanged(this);
    else if (attrName == aria_expandedAttr)
        document()->axObjectCache()->handleAriaExpandedChange(this);
    else if (attrName == aria_hiddenAttr)
        document()->axObjectCache()->childrenChanged(this);
    else if (attrName == aria_invalidAttr)
        document()->axObjectCache()->postNotification(this, AXObjectCache::AXInvalidStatusChanged, true);
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

    m_attributeData = ElementAttributeData::createImmutable(filteredAttributes);

    // Iterate over the set of attributes we already have on the stack in case
    // attributeChanged mutates m_attributeData.
    // FIXME: Find a way so we don't have to do this.
    for (unsigned i = 0; i < filteredAttributes.size(); ++i)
        attributeChanged(filteredAttributes[i]);
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

    return InsertionDone;
}

static inline TreeScope* treeScopeOfParent(Node* node, ContainerNode* insertionPoint)
{
    if (Node* parent = node->parentNode())
        parent->treeScope();
    return insertionPoint->treeScope();
}


void Element::removedFrom(ContainerNode* insertionPoint)
{
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
            updateId(treeScopeOfParent(this, insertionPoint), idValue, nullAtom);

        const AtomicString& nameValue = getNameAttribute();
        if (!nameValue.isNull())
            updateName(nameValue, nullAtom);
    }

    ContainerNode::removedFrom(insertionPoint);
}

void Element::attach()
{
    suspendPostAttachCallbacks();
    RenderWidget::suspendWidgetHierarchyUpdates();

    createRendererIfNeeded();
    StyleResolverParentPusher parentPusher(this);

    if (parentElement() && parentElement()->isInCanvasSubtree())
        setIsInCanvasSubtree(true);

    // When a shadow root exists, it does the work of attaching the children.
    if (ElementShadow* shadow = this->shadow()) {
        parentPusher.push();
        shadow->attach();
        attachChildrenIfNeeded();
        attachAsNode();
    } else {
        if (firstChild())
            parentPusher.push();
        ContainerNode::attach();
    }

    if (hasRareData()) {   
        ElementRareData* data = elementRareData();
        if (data->needsFocusAppearanceUpdateSoonAfterAttach()) {
            if (isFocusable() && document()->focusedNode() == this)
                document()->updateFocusAppearanceSoon(false /* don't restore selection */);
            data->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
        }
    }

    RenderWidget::resumeWidgetHierarchyUpdates();
    resumePostAttachCallbacks();
}

void Element::unregisterNamedFlowContentNode()
{
    if (document()->cssRegionsEnabled() && inNamedFlow()) {
        if (document()->renderer() && document()->renderer()->view())
            document()->renderer()->view()->flowThreadController()->unregisterNamedFlowContentNode(this);
    }
}

void Element::detach()
{
    RenderWidget::suspendWidgetHierarchyUpdates();
    unregisterNamedFlowContentNode();
    cancelFocusAppearanceUpdate();
    if (hasRareData()) {
        setIsInCanvasSubtree(false);
        elementRareData()->resetComputedStyle();
    }

    if (ElementShadow* shadow = this->shadow()) {
        detachChildrenIfNeeded();
        shadow->detach();
        detachAsNode();
    } else
        ContainerNode::detach();

    RenderWidget::resumeWidgetHierarchyUpdates();
}

bool Element::pseudoStyleCacheIsInvalid(const RenderStyle* currentStyle, RenderStyle* newStyle)
{
    ASSERT(currentStyle == renderStyle());

    if (!renderer() || !currentStyle)
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

    // Ref currentStyle in case it would otherwise be deleted when setRenderStyle() is called.
    RefPtr<RenderStyle> currentStyle(renderStyle());
    bool hasParentStyle = parentNodeForRenderingAndStyle() ? static_cast<bool>(parentNodeForRenderingAndStyle()->renderStyle()) : false;
    bool hasDirectAdjacentRules = currentStyle && currentStyle->childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = currentStyle && currentStyle->childrenAffectedByForwardPositionalRules();

    if ((change > NoChange || needsStyleRecalc())) {
        if (hasRareData()) {
            ElementRareData* data = elementRareData();
            data->resetComputedStyle();
            data->m_styleAffectedByEmpty = false;
        }
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

        if (ch != NoChange || pseudoStyleCacheIsInvalid(currentStyle.get(), newStyle.get()) || (change == Force && renderer() && renderer()->requiresForcedStyleRecalcPropagation())) {
            setRenderStyle(newStyle);
        } else if (needsStyleRecalc() && styleChangeType() != SyntheticStyleChange) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.
            if (renderer())
                renderer()->setStyleInternal(newStyle.get());
            else
                setRenderStyle(newStyle);
        } else if (styleChangeType() == SyntheticStyleChange)
             setRenderStyle(newStyle);

        // If "rem" units are used anywhere in the document, and if the document element's font size changes, then go ahead and force font updating
        // all the way down the tree. This is simpler than having to maintain a cache of objects (and such font size changes should be rare anyway).
        if (document()->usesRemUnits() && document()->documentElement() == this && ch != NoChange && currentStyle && newStyle && currentStyle->fontSize() != newStyle->fontSize()) {
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

    return elementRareData()->m_shadow.get();
}

ElementShadow* Element::ensureShadow()
{
    if (ElementShadow* shadow = ensureElementRareData()->m_shadow.get())
        return shadow;

    elementRareData()->m_shadow = adoptPtr(new ElementShadow());
    return elementRareData()->m_shadow.get();
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
    return hasRareData() ? elementRareData()->m_shadowPseudoId : nullAtom;
}

void Element::setShadowPseudoId(const AtomicString& id, ExceptionCode& ec)
{
    if (!hasRareData() && id == nullAtom)
        return;

    if (!CSSSelector::isUnknownPseudoType(id)) {
        ec = SYNTAX_ERR;
        return;
    }

    ensureElementRareData()->m_shadowPseudoId = id;
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

    if (!style || (style->affectedByEmpty() && (!style->emptyState() || element->hasChildNodes())))
        element->setNeedsStyleRecalc();
}

static void checkForSiblingStyleChanges(Element* e, RenderStyle* style, bool finishedParsingCallback,
                                        Node* beforeChange, Node* afterChange, int childCountDelta)
{
    // :empty selector.
    checkForEmptyStyleChange(e, style);
    
    if (!style || (e->needsStyleRecalc() && style->childrenAffectedByPositionalRules()))
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
            firstElementAfterInsertion->setNeedsStyleRecalc();
            
        // We also have to handle node removal.
        if (childCountDelta < 0 && newFirstChild == firstElementAfterInsertion && newFirstChild && (!newFirstChild->renderStyle() || !newFirstChild->renderStyle()->firstChildState()))
            newFirstChild->setNeedsStyleRecalc();
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
            lastElementBeforeInsertion->setNeedsStyleRecalc();
            
        // We also have to handle node removal.  The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((childCountDelta < 0 || finishedParsingCallback) && newLastChild == lastElementBeforeInsertion && newLastChild && (!newLastChild->renderStyle() || !newLastChild->renderStyle()->lastChildState()))
            newLastChild->setNeedsStyleRecalc();
    }

    // The + selector.  We need to invalidate the first element following the insertion point.  It is the only possible element
    // that could be affected by this DOM change.
    if (style->childrenAffectedByDirectAdjacentRules() && afterChange) {
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
    if ((style->childrenAffectedByForwardPositionalRules() && afterChange) ||
        (style->childrenAffectedByBackwardPositionalRules() && beforeChange))
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
    String result;
    String s;
    
    s = nodeName();
    if (s.length() > 0) {
        result += s;
    }
          
    s = getIdAttribute();
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

    RefPtr<Attr> oldAttr = attrIfExists(attr->qualifiedName());
    if (oldAttr.get() == attr)
        return attr; // This Attr is already attached to the element.

    // INUSE_ATTRIBUTE_ERR: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attr->ownerElement()) {
        ec = INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    updateInvalidAttributes();
    ElementAttributeData* attributeData = mutableAttributeData();

    size_t index = attributeData->getAttributeItemIndex(attr->qualifiedName());
    Attribute* oldAttribute = index != notFound ? attributeData->attributeItem(index) : 0;

    if (!oldAttribute) {
        attributeData->addAttribute(Attribute(attr->qualifiedName(), attr->value()), this);
        attributeData->setAttr(this, attr->qualifiedName(), attr);
        return 0;
    }

    if (oldAttr)
        oldAttr->detachFromElementWithValue(oldAttribute->value());
    else
        oldAttr = Attr::create(document(), oldAttribute->name(), oldAttribute->value());

    attributeData->replaceAttribute(index, Attribute(attr->name(), attr->value()), this);
    attributeData->setAttr(this, attr->qualifiedName(), attr);
    return oldAttr.release();
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

void Element::removeAttribute(size_t index)
{
    ASSERT(attributeData());
    ASSERT(index <= attributeCount());
    mutableAttributeData()->removeAttribute(index, this);
}

void Element::removeAttribute(const AtomicString& name)
{
    if (!attributeData())
        return;

    AtomicString localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    size_t index = attributeData()->getAttributeItemIndex(localName, false);
    if (index == notFound)
        return;

    mutableAttributeData()->removeAttribute(index, this);
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
    return attributeData->getAttributeNode(name, shouldIgnoreAttributeCase(this), this);
}

PassRefPtr<Attr> Element::getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    const ElementAttributeData* attributeData = updatedAttributeData();
    if (!attributeData)
        return 0;
    return attributeData->getAttributeNode(QualifiedName(nullAtom, localName, namespaceURI), this);
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
        renderer()->scrollRectToVisible(getRect());
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

LayoutSize Element::minimumSizeForResizing() const
{
    return hasRareData() ? elementRareData()->m_minimumSizeForResizing : defaultMinimumSizeForResizing();
}

void Element::setMinimumSizeForResizing(const LayoutSize& size)
{
    if (size == defaultMinimumSizeForResizing() && !hasRareData())
        return;
    ensureElementRareData()->m_minimumSizeForResizing = size;
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
    if (!data->m_computedStyle)
        data->m_computedStyle = document()->styleForElementIgnoringPendingStylesheets(this);
    return pseudoElementSpecifier ? data->m_computedStyle->getCachedPseudoStyle(pseudoElementSpecifier) : data->m_computedStyle.get();
}

void Element::setStyleAffectedByEmpty()
{
    ElementRareData* data = ensureElementRareData();
    data->m_styleAffectedByEmpty = true;
}

bool Element::styleAffectedByEmpty() const
{
    return hasRareData() && elementRareData()->m_styleAffectedByEmpty;
}

void Element::setIsInCanvasSubtree(bool isInCanvasSubtree)
{
    ElementRareData* data = ensureElementRareData();
    data->m_isInCanvasSubtree = isInCanvasSubtree;
}

bool Element::isInCanvasSubtree() const
{
    return hasRareData() && elementRareData()->m_isInCanvasSubtree;
}

AtomicString Element::computeInheritedLanguage() const
{
    const Node* n = this;
    AtomicString value;
    // The language property is inherited, so we iterate over the parents to find the first language.
    while (n && value.isNull()) {
        if (n->isElementNode()) {
            // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
            value = static_cast<const Element*>(n)->fastGetAttribute(XMLNames::langAttr);
            if (value.isNull())
                value = static_cast<const Element*>(n)->fastGetAttribute(HTMLNames::langAttr);
        } else if (n->isDocumentNode()) {
            // checking the MIME content-language
            value = static_cast<const Document*>(n)->contentLanguage();
        }

        n = n->parentNode();
    }

    return value;
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
    if (!hasAttrList())
        return;

    const ElementAttributeData* attributeData = updatedAttributeData();
    ASSERT(attributeData);

    for (size_t i = 0; i < attributeData->length(); ++i) {
        if (RefPtr<Attr> attr = attrIfExists(attributeData->attributeItem(i)->name()))
            attr->normalize();
    }
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
    if (!data->m_classList)
        data->m_classList = ClassList::create(this);
    return data->m_classList.get();
}

DOMTokenList* Element::optionalClassList() const
{
    if (!hasRareData())
        return 0;
    return elementRareData()->m_classList.get();
}

DOMStringMap* Element::dataset()
{
    ElementRareData* data = ensureElementRareData();
    if (!data->m_datasetDOMStringMap)
        data->m_datasetDOMStringMap = DatasetDOMStringMap::create(this);
    return data->m_datasetDOMStringMap.get();
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

    return Node::childShouldCreateRenderer(childContext);
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
    return hasRareData() ? elementRareData()->m_containsFullScreenElement : false;
}

void Element::setContainsFullScreenElement(bool flag)
{
    ensureElementRareData()->m_containsFullScreenElement = flag;
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
    const Element* element = this;
    while (element) {
        switch (element->spellcheckAttributeState()) {
        case SpellcheckAttributeTrue:
            return true;
        case SpellcheckAttributeFalse:
            return false;
        case SpellcheckAttributeDefault:
            break;
        }

        ContainerNode* parent = const_cast<Element*>(element)->parentOrHostNode();
        if (parent && parent->isElementNode())
            element = toElement(parent);
        else if (parent && parent->isShadowRoot())
            element = toElement(parent->parentOrHostNode());
        else
            element = 0;
    }

    return true;
}

PassRefPtr<WebKitAnimationList> Element::webkitGetAnimations() const
{
    if (!renderer())
        return 0;

    AnimationController* animController = renderer()->animation();

    if (!animController)
        return 0;
    
    return animController->animationsForRenderer(renderer());
}

RenderRegion* Element::renderRegion() const
{
    if (renderer() && renderer()->isRenderRegion())
        return toRenderRegion(renderer());

    return 0;
}

const AtomicString& Element::webkitRegionOverset() const
{
    document()->updateLayoutIgnorePendingStylesheets();

    DEFINE_STATIC_LOCAL(AtomicString, undefinedState, ("undefined"));
    if (!document()->cssRegionsEnabled() || !renderRegion())
        return undefinedState;

    switch (renderRegion()->regionState()) {
    case RenderRegion::RegionFit: {
        DEFINE_STATIC_LOCAL(AtomicString, fitState, ("fit"));
        return fitState;
    }
    case RenderRegion::RegionEmpty: {
        DEFINE_STATIC_LOCAL(AtomicString, emptyState, ("empty"));
        return emptyState;
    }
    case RenderRegion::RegionOverset: {
        DEFINE_STATIC_LOCAL(AtomicString, overflowState, ("overset"));
        return overflowState;
    }
    case RenderRegion::RegionUndefined:
        return undefinedState;
    }

    ASSERT_NOT_REACHED();
    return undefinedState;
}

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
    return hasRareData() && elementRareData()->m_attributeMap;
}
#endif

void Element::willModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (isIdAttributeName(name))
        updateId(oldValue, newValue);
    else if (name == HTMLNames::nameAttr)
        updateName(oldValue, newValue);

#if ENABLE(MUTATION_OBSERVERS)
    if (OwnPtr<MutationObserverInterestGroup> recipients = MutationObserverInterestGroup::createForAttributesMutation(this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(this, name, oldValue));
#endif

#if ENABLE(INSPECTOR)
    InspectorInstrumentation::willModifyDOMAttr(document(), this, oldValue, newValue);
#endif
}

void Element::didAddAttribute(const Attribute& attribute)
{
    attributeChanged(attribute);
    InspectorInstrumentation::didModifyDOMAttr(document(), this, attribute.localName(), attribute.value());
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const Attribute& attribute)
{
    attributeChanged(attribute);
    InspectorInstrumentation::didModifyDOMAttr(document(), this, attribute.localName(), attribute.value());
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name)
{
    attributeChanged(Attribute(name, nullAtom));
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
    return ensureElementRareData()->ensureCachedHTMLCollection(this, type);
}

PassRefPtr<HTMLCollection> ElementRareData::ensureCachedHTMLCollection(Element* element, CollectionType type)
{
    if (!m_cachedCollections) {
        m_cachedCollections = adoptPtr(new CachedHTMLCollectionArray);
        for (unsigned i = 0; i < NumNodeCollectionTypes; i++)
            (*m_cachedCollections)[i] = 0;
    }

    if (HTMLCollection* collection = (*m_cachedCollections)[type - FirstNodeCollectionType])
        return collection;

    RefPtr<HTMLCollection> collection;
    if (type == TableRows) {
        ASSERT(element->hasTagName(tableTag));
        collection = HTMLTableRowsCollection::create(element);
    } else if (type == SelectOptions) {
        ASSERT(element->hasTagName(selectTag));
        collection = HTMLOptionsCollection::create(element);
    } else if (type == FormControls) {
        ASSERT(element->hasTagName(formTag) || element->hasTagName(fieldsetTag));
        collection = HTMLFormCollection::create(element);
#if ENABLE(MICRODATA)
    } else if (type == ItemProperties) {
        collection = HTMLPropertiesCollection::create(element);
#endif
    } else
        collection = HTMLCollection::create(element, type);
    (*m_cachedCollections)[type - FirstNodeCollectionType] = collection.get();
    return collection.release();
}

HTMLCollection* Element::cachedHTMLCollection(CollectionType type)
{
    return hasRareData() ? elementRareData()->cachedHTMLCollection(type) : 0;
}

void Element::removeCachedHTMLCollection(HTMLCollection* collection, CollectionType type)
{
    ASSERT(hasRareData());
    elementRareData()->removeCachedHTMLCollection(collection, type);
}

IntSize Element::savedLayerScrollOffset() const
{
    return hasRareData() ? elementRareData()->m_savedLayerScrollOffset : IntSize();
}

void Element::setSavedLayerScrollOffset(const IntSize& size)
{
    if (size.isZero() && !hasRareData())
        return;
    ensureElementRareData()->m_savedLayerScrollOffset = size;
}

PassRefPtr<Attr> Element::attrIfExists(const QualifiedName& name)
{
    if (!hasAttrList())
        return 0;
    ASSERT(attributeData());
    return attributeData()->attrIfExists(this, name);
}

PassRefPtr<Attr> Element::ensureAttr(const QualifiedName& name)
{
    ASSERT(attributeData());
    return attributeData()->ensureAttr(this, name);
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
    if (const ElementAttributeData* attributeData = other.updatedAttributeData())
        mutableAttributeData()->cloneDataFrom(*attributeData, other, *this);
    else if (m_attributeData) {
        m_attributeData->clearAttributes(this);
        m_attributeData.clear();
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
        m_attributeData = m_attributeData->makeMutable();
}

#if ENABLE(UNDO_MANAGER)
bool Element::undoScope() const
{
    return hasRareData() && elementRareData()->m_undoScope;
}

void Element::setUndoScope(bool undoScope)
{
    ElementRareData* data = ensureElementRareData();
    data->m_undoScope = undoScope;
    if (!undoScope)
        disconnectUndoManager();
}

PassRefPtr<UndoManager> Element::undoManager()
{
    if (!undoScope() || (isContentEditable() && !isRootEditableElement())) {
        disconnectUndoManager();
        return 0;
    }
    ElementRareData* data = ensureElementRareData();
    if (!data->m_undoManager)
        data->m_undoManager = UndoManager::create(document(), this);
    return data->m_undoManager;
}

void Element::disconnectUndoManager()
{
    if (!hasRareData())
        return;
    ElementRareData* data = elementRareData();
    UndoManager* undoManager = data->m_undoManager.get();
    if (!undoManager)
        return;
    undoManager->disconnect();
    data->m_undoManager.clear();
}

void Element::disconnectUndoManagersInSubtree()
{
    Node* node = firstChild();
    while (node) {
        if (node->isElementNode()) {
            Element* element = toElement(node);
            if (element->hasRareData() && element->elementRareData()->m_undoManager) {
                if (!node->isContentEditable()) {
                    node = node->traverseNextSibling(this);
                    continue;
                }
                element->disconnectUndoManager();
            }
        }
        node = node->traverseNextNode(this);
    }
}
#endif

} // namespace WebCore

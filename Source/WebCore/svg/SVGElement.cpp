/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGElement.h"

#include "Attr.h"
#include "CSSCursorImageValue.h"
#include "CSSParser.h"
#include "DOMImplementation.h"
#include "Document.h"
#include "ElementTraversal.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMasker.h"
#include "SVGCursorElement.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementInstance.h"
#include "SVGElementRareData.h"
#include "SVGGraphicsElement.h"
#include "SVGImageElement.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "ScriptEventListener.h"
#include "ShadowRoot.h"
#include "XLinkNames.h"
#include "XMLNames.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>


namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGElement, HTMLNames::classAttr, ClassName, className)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(className)
END_REGISTER_ANIMATED_PROPERTIES

using namespace HTMLNames;
using namespace SVGNames;

void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, CSSPropertyID>* propertyNameToIdMap, const QualifiedName& attrName)
{
    // FIXME: when CSS supports "transform-origin" the special case for transform_originAttr can be removed.
    CSSPropertyID propertyId = cssPropertyID(attrName.localName());
    if (!propertyId && attrName == transform_originAttr)
        propertyId = CSSPropertyWebkitTransformOrigin; // cssPropertyID("-webkit-transform-origin")
    ASSERT(propertyId > 0);
    propertyNameToIdMap->set(attrName.localName().impl(), propertyId);
}

typedef HashMap<QualifiedName, AnimatedPropertyType> AttributeToPropertyTypeMap;
static inline AttributeToPropertyTypeMap& cssPropertyToTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_cssPropertyMap, ());

    if (!s_cssPropertyMap.isEmpty())
        return s_cssPropertyMap;

    // Fill the map for the first use.
    s_cssPropertyMap.set(alignment_baselineAttr, AnimatedString);
    s_cssPropertyMap.set(baseline_shiftAttr, AnimatedString);
    s_cssPropertyMap.set(buffered_renderingAttr, AnimatedString);
    s_cssPropertyMap.set(clipAttr, AnimatedRect);
    s_cssPropertyMap.set(clip_pathAttr, AnimatedString);
    s_cssPropertyMap.set(clip_ruleAttr, AnimatedString);
    s_cssPropertyMap.set(SVGNames::colorAttr, AnimatedColor);
    s_cssPropertyMap.set(color_interpolationAttr, AnimatedString);
    s_cssPropertyMap.set(color_interpolation_filtersAttr, AnimatedString);
    s_cssPropertyMap.set(color_profileAttr, AnimatedString);
    s_cssPropertyMap.set(color_renderingAttr, AnimatedString);
    s_cssPropertyMap.set(cursorAttr, AnimatedString);
    s_cssPropertyMap.set(displayAttr, AnimatedString);
    s_cssPropertyMap.set(dominant_baselineAttr, AnimatedString);
    s_cssPropertyMap.set(fillAttr, AnimatedColor);
    s_cssPropertyMap.set(fill_opacityAttr, AnimatedNumber);
    s_cssPropertyMap.set(fill_ruleAttr, AnimatedString);
    s_cssPropertyMap.set(filterAttr, AnimatedString);
    s_cssPropertyMap.set(flood_colorAttr, AnimatedColor);
    s_cssPropertyMap.set(flood_opacityAttr, AnimatedNumber);
    s_cssPropertyMap.set(font_familyAttr, AnimatedString);
    s_cssPropertyMap.set(font_sizeAttr, AnimatedLength);
    s_cssPropertyMap.set(font_stretchAttr, AnimatedString);
    s_cssPropertyMap.set(font_styleAttr, AnimatedString);
    s_cssPropertyMap.set(font_variantAttr, AnimatedString);
    s_cssPropertyMap.set(font_weightAttr, AnimatedString);
    s_cssPropertyMap.set(image_renderingAttr, AnimatedString);
    s_cssPropertyMap.set(kerningAttr, AnimatedLength);
    s_cssPropertyMap.set(letter_spacingAttr, AnimatedLength);
    s_cssPropertyMap.set(lighting_colorAttr, AnimatedColor);
    s_cssPropertyMap.set(marker_endAttr, AnimatedString);
    s_cssPropertyMap.set(marker_midAttr, AnimatedString);
    s_cssPropertyMap.set(marker_startAttr, AnimatedString);
    s_cssPropertyMap.set(maskAttr, AnimatedString);
    s_cssPropertyMap.set(mask_typeAttr, AnimatedString);
    s_cssPropertyMap.set(opacityAttr, AnimatedNumber);
    s_cssPropertyMap.set(overflowAttr, AnimatedString);
    s_cssPropertyMap.set(pointer_eventsAttr, AnimatedString);
    s_cssPropertyMap.set(shape_renderingAttr, AnimatedString);
    s_cssPropertyMap.set(stop_colorAttr, AnimatedColor);
    s_cssPropertyMap.set(stop_opacityAttr, AnimatedNumber);
    s_cssPropertyMap.set(strokeAttr, AnimatedColor);
    s_cssPropertyMap.set(stroke_dasharrayAttr, AnimatedLengthList);
    s_cssPropertyMap.set(stroke_dashoffsetAttr, AnimatedLength);
    s_cssPropertyMap.set(stroke_linecapAttr, AnimatedString);
    s_cssPropertyMap.set(stroke_linejoinAttr, AnimatedString);
    s_cssPropertyMap.set(stroke_miterlimitAttr, AnimatedNumber);
    s_cssPropertyMap.set(stroke_opacityAttr, AnimatedNumber);
    s_cssPropertyMap.set(stroke_widthAttr, AnimatedLength);
    s_cssPropertyMap.set(text_anchorAttr, AnimatedString);
    s_cssPropertyMap.set(text_decorationAttr, AnimatedString);
    s_cssPropertyMap.set(text_renderingAttr, AnimatedString);
    s_cssPropertyMap.set(vector_effectAttr, AnimatedString);
    s_cssPropertyMap.set(visibilityAttr, AnimatedString);
    s_cssPropertyMap.set(word_spacingAttr, AnimatedLength);
    return s_cssPropertyMap;
}

SVGElement::SVGElement(const QualifiedName& tagName, Document* document)
    : StyledElement(tagName, document, CreateSVGElement)
{
    registerAnimatedPropertiesForSVGElement();
}

SVGElement::~SVGElement()
{
    if (!hasSVGRareData())
        ASSERT(!SVGElementRareData::rareDataMap().contains(this));
    else {
        ASSERT(document());
        SVGElementRareData::SVGElementRareDataMap& rareDataMap = SVGElementRareData::rareDataMap();
        SVGElementRareData::SVGElementRareDataMap::iterator it = rareDataMap.find(this);
        ASSERT(it != rareDataMap.end());

        SVGElementRareData* rareData = it->value;
        rareData->destroyAnimatedSMILStyleProperties();
        if (SVGCursorElement* cursorElement = rareData->cursorElement())
            cursorElement->removeClient(this);
        if (CSSCursorImageValue* cursorImageValue = rareData->cursorImageValue())
            cursorImageValue->removeReferencedElement(this);

        delete rareData;

        it = rareDataMap.find(this);
        ASSERT(it != rareDataMap.end());
        rareDataMap.remove(it);

        // Clear HasSVGRareData flag now so that we are in a consistent state when
        // calling rebuildAllElementReferencesForTarget() and
        // removeAllElementReferencesForTarget() below.
        clearHasSVGRareData();
    }
    ASSERT(document());
    document()->accessSVGExtensions()->rebuildAllElementReferencesForTarget(this);
    document()->accessSVGExtensions()->removeAllElementReferencesForTarget(this);
}

bool SVGElement::willRecalcStyle(Style::Change change)
{
    if (!hasSVGRareData() || styleChangeType() == SyntheticStyleChange)
        return true;
    // If the style changes because of a regular property change (not induced by SMIL animations themselves)
    // reset the "computed style without SMIL style properties", so the base value change gets reflected.
    if (change > Style::NoChange || needsStyleRecalc())
        svgRareData()->setNeedsOverrideComputedStyleUpdate();
    return true;
}

SVGElementRareData* SVGElement::svgRareData() const
{
    ASSERT(hasSVGRareData());
    return SVGElementRareData::rareDataFromMap(this);
}

SVGElementRareData& SVGElement::ensureSVGRareData()
{
    if (hasSVGRareData())
        return *svgRareData();

    ASSERT(!SVGElementRareData::rareDataMap().contains(this));
    SVGElementRareData* data = new SVGElementRareData;
    SVGElementRareData::rareDataMap().set(this, data);
    setHasSVGRareData();
    return *data;
}

bool SVGElement::isOutermostSVGSVGElement() const
{
    if (!hasTagName(SVGNames::svgTag))
        return false;

    // If we're living in a shadow tree, we're a <svg> element that got created as replacement
    // for a <symbol> element or a cloned <svg> element in the referenced tree. In that case
    // we're always an inner <svg> element.
    if (isInShadowTree() && parentOrShadowHostElement() && parentOrShadowHostElement()->isSVGElement())
        return false;

    // Element may not be in the document, pretend we're outermost for viewport(), getCTM(), etc.
    if (!parentNode())
        return true;

    // We act like an outermost SVG element, if we're a direct child of a <foreignObject> element.
    if (parentNode()->hasTagName(SVGNames::foreignObjectTag))
        return true;

    // This is true whenever this is the outermost SVG, even if there are HTML elements outside it
    return !parentNode()->isSVGElement();
}

void SVGElement::reportAttributeParsingError(SVGParsingError error, const QualifiedName& name, const AtomicString& value)
{
    if (error == NoError)
        return;

    String errorString = "<" + tagName() + "> attribute " + name.toString() + "=\"" + value + "\"";
    SVGDocumentExtensions* extensions = document()->accessSVGExtensions();

    if (error == NegativeValueForbiddenError) {
        extensions->reportError("Invalid negative value for " + errorString);
        return;
    }

    if (error == ParsingAttributeFailedError) {
        extensions->reportError("Invalid value for " + errorString);
        return;
    }

    ASSERT_NOT_REACHED();
}


bool SVGElement::isSupported(StringImpl* feature, StringImpl* version) const
{
    return DOMImplementation::hasFeature(feature, version);
}

String SVGElement::xmlbase() const
{
    return fastGetAttribute(XMLNames::baseAttr);
}

void SVGElement::setXmlbase(const String& value, ExceptionCode&)
{
    setAttribute(XMLNames::baseAttr, value);
}

void SVGElement::removedFrom(ContainerNode* rootParent)
{
    bool wasInDocument = rootParent->inDocument();
    if (wasInDocument)
        updateRelativeLengthsInformation(false, this);

    StyledElement::removedFrom(rootParent);

    if (wasInDocument) {
        document()->accessSVGExtensions()->rebuildAllElementReferencesForTarget(this);
        document()->accessSVGExtensions()->removeAllElementReferencesForTarget(this);
    }
    SVGElementInstance::invalidateAllInstancesOfElement(this);
}

SVGSVGElement* SVGElement::ownerSVGElement() const
{
    ContainerNode* n = parentOrShadowHostNode();
    while (n) {
        if (n->hasTagName(SVGNames::svgTag))
            return toSVGSVGElement(n);

        n = n->parentOrShadowHostNode();
    }

    return 0;
}

SVGElement* SVGElement::viewportElement() const
{
    // This function needs shadow tree support - as RenderSVGContainer uses this function
    // to determine the "overflow" property. <use> on <symbol> wouldn't work otherwhise.
    ContainerNode* n = parentOrShadowHostNode();
    while (n) {
        if (n->hasTagName(SVGNames::svgTag) || isSVGImageElement(n) || n->hasTagName(SVGNames::symbolTag))
            return toSVGElement(n);

        n = n->parentOrShadowHostNode();
    }

    return 0;
}

SVGDocumentExtensions* SVGElement::accessDocumentSVGExtensions()
{
    // This function is provided for use by SVGAnimatedProperty to avoid
    // global inclusion of Document.h in SVG code.
    return document() ? document()->accessSVGExtensions() : 0;
}
 
void SVGElement::mapInstanceToElement(SVGElementInstance* instance)
{
    ASSERT(instance);

    HashSet<SVGElementInstance*>& instances = ensureSVGRareData().elementInstances();
    ASSERT(!instances.contains(instance));

    instances.add(instance);
}
 
void SVGElement::removeInstanceMapping(SVGElementInstance* instance)
{
    ASSERT(instance);
    ASSERT(hasSVGRareData());

    HashSet<SVGElementInstance*>& instances = svgRareData()->elementInstances();
    ASSERT(instances.contains(instance));

    instances.remove(instance);
}

const HashSet<SVGElementInstance*>& SVGElement::instancesForElement() const
{
    if (!hasSVGRareData()) {
        DEFINE_STATIC_LOCAL(HashSet<SVGElementInstance*>, emptyInstances, ());
        return emptyInstances;
    }
    return svgRareData()->elementInstances();
}

bool SVGElement::getBoundingBox(FloatRect& rect, SVGLocatable::StyleUpdateStrategy styleUpdateStrategy)
{
    if (isSVGGraphicsElement()) {
        rect = toSVGGraphicsElement(this)->getBBox(styleUpdateStrategy);
        return true;
    }
    return false;
}

void SVGElement::setCursorElement(SVGCursorElement* cursorElement)
{
    SVGElementRareData& rareData = ensureSVGRareData();
    if (SVGCursorElement* oldCursorElement = rareData.cursorElement()) {
        if (cursorElement == oldCursorElement)
            return;
        oldCursorElement->removeReferencedElement(this);
    }
    rareData.setCursorElement(cursorElement);
}

void SVGElement::cursorElementRemoved() 
{
    ASSERT(hasSVGRareData());
    svgRareData()->setCursorElement(0);
}

void SVGElement::setCursorImageValue(CSSCursorImageValue* cursorImageValue)
{
    SVGElementRareData& rareData = ensureSVGRareData();
    if (CSSCursorImageValue* oldCursorImageValue = rareData.cursorImageValue()) {
        if (cursorImageValue == oldCursorImageValue)
            return;
        oldCursorImageValue->removeReferencedElement(this);
    }
    rareData.setCursorImageValue(cursorImageValue);
}

void SVGElement::cursorImageValueRemoved()
{
    ASSERT(hasSVGRareData());
    svgRareData()->setCursorImageValue(0);
}

SVGElement* SVGElement::correspondingElement()
{
    ASSERT(!hasSVGRareData() || !svgRareData()->correspondingElement() || containingShadowRoot());
    return hasSVGRareData() ? svgRareData()->correspondingElement() : 0;
}

void SVGElement::setCorrespondingElement(SVGElement* correspondingElement)
{
    ensureSVGRareData().setCorrespondingElement(correspondingElement);
}

void SVGElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    // standard events
    if (name == onloadAttr)
        setAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(this, name, value));
    else if (name == onclickAttr)
        setAttributeEventListener(eventNames().clickEvent, createAttributeEventListener(this, name, value));
    else if (name == onmousedownAttr)
        setAttributeEventListener(eventNames().mousedownEvent, createAttributeEventListener(this, name, value));
    else if (name == onmouseenterAttr)
        setAttributeEventListener(eventNames().mouseenterEvent, createAttributeEventListener(this, name, value));
    else if (name == onmouseleaveAttr)
        setAttributeEventListener(eventNames().mouseleaveEvent, createAttributeEventListener(this, name, value));
    else if (name == onmousemoveAttr)
        setAttributeEventListener(eventNames().mousemoveEvent, createAttributeEventListener(this, name, value));
    else if (name == onmouseoutAttr)
        setAttributeEventListener(eventNames().mouseoutEvent, createAttributeEventListener(this, name, value));
    else if (name == onmouseoverAttr)
        setAttributeEventListener(eventNames().mouseoverEvent, createAttributeEventListener(this, name, value));
    else if (name == onmouseupAttr)
        setAttributeEventListener(eventNames().mouseupEvent, createAttributeEventListener(this, name, value));
    else if (name == SVGNames::onfocusinAttr)
        setAttributeEventListener(eventNames().focusinEvent, createAttributeEventListener(this, name, value));
    else if (name == SVGNames::onfocusoutAttr)
        setAttributeEventListener(eventNames().focusoutEvent, createAttributeEventListener(this, name, value));
    else if (name == SVGNames::onactivateAttr)
        setAttributeEventListener(eventNames().DOMActivateEvent, createAttributeEventListener(this, name, value));
    else if (name == HTMLNames::classAttr)
        setClassNameBaseValue(value);
    else if (SVGLangSpace::parseAttribute(name, value)) {
    } else
        StyledElement::parseAttribute(name, value);
}

void SVGElement::animatedPropertyTypeForAttribute(const QualifiedName& attributeName, Vector<AnimatedPropertyType>& propertyTypes)
{
    localAttributeToPropertyMap().animatedPropertyTypeForAttribute(attributeName, propertyTypes);

    if (!propertyTypes.isEmpty())
        return;

    AttributeToPropertyTypeMap& cssPropertyTypeMap = cssPropertyToTypeMap();
    if (cssPropertyTypeMap.contains(attributeName))
        propertyTypes.append(cssPropertyTypeMap.get(attributeName));
}

bool SVGElement::haveLoadedRequiredResources()
{
    Node* child = firstChild();
    while (child) {
        if (child->isSVGElement() && !toSVGElement(child)->haveLoadedRequiredResources())
            return false;
        child = child->nextSibling();
    }
    return true;
}

static inline void collectInstancesForSVGElement(SVGElement* element, HashSet<SVGElementInstance*>& instances)
{
    ASSERT(element);
    if (element->containingShadowRoot())
        return;

    ASSERT(!element->instanceUpdatesBlocked());

    instances = element->instancesForElement();
}

bool SVGElement::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> prpListener, bool useCapture)
{
    RefPtr<EventListener> listener = prpListener;
    
    // Add event listener to regular DOM element
    if (!Node::addEventListener(eventType, listener, useCapture))
        return false;

    // Add event listener to all shadow tree DOM element instances
    HashSet<SVGElementInstance*> instances;
    collectInstancesForSVGElement(this, instances);    
    const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
        ASSERT((*it)->shadowTreeElement());
        ASSERT((*it)->correspondingElement() == this);

        bool result = (*it)->shadowTreeElement()->Node::addEventListener(eventType, listener, useCapture);
        ASSERT_UNUSED(result, result);
    }

    return true;
}

bool SVGElement::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    HashSet<SVGElementInstance*> instances;
    collectInstancesForSVGElement(this, instances);
    if (instances.isEmpty())
        return Node::removeEventListener(eventType, listener, useCapture);

    // EventTarget::removeEventListener creates a PassRefPtr around the given EventListener
    // object when creating a temporary RegisteredEventListener object used to look up the
    // event listener in a cache. If we want to be able to call removeEventListener() multiple
    // times on different nodes, we have to delay its immediate destruction, which would happen
    // after the first call below.
    RefPtr<EventListener> protector(listener);

    // Remove event listener from regular DOM element
    if (!Node::removeEventListener(eventType, listener, useCapture))
        return false;

    // Remove event listener from all shadow tree DOM element instances
    const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
        ASSERT((*it)->correspondingElement() == this);

        SVGElement* shadowTreeElement = (*it)->shadowTreeElement();
        ASSERT(shadowTreeElement);

        if (shadowTreeElement->Node::removeEventListener(eventType, listener, useCapture))
            continue;

        // This case can only be hit for event listeners created from markup
        ASSERT(listener->wasCreatedFromMarkup());

        // If the event listener 'listener' has been created from markup and has been fired before
        // then JSLazyEventListener::parseCode() has been called and m_jsFunction of that listener
        // has been created (read: it's not 0 anymore). During shadow tree creation, the event
        // listener DOM attribute has been cloned, and another event listener has been setup in
        // the shadow tree. If that event listener has not been used yet, m_jsFunction is still 0,
        // and tryRemoveEventListener() above will fail. Work around that very seldom problem.
        EventTargetData* data = shadowTreeElement->eventTargetData();
        ASSERT(data);

        data->eventListenerMap.removeFirstEventListenerCreatedFromMarkup(eventType);
    }

    return true;
}

static bool hasLoadListener(Element* element)
{
    if (element->hasEventListeners(eventNames().loadEvent))
        return true;

    for (element = element->parentOrShadowHostElement(); element; element = element->parentOrShadowHostElement()) {
        const EventListenerVector& entry = element->getEventListeners(eventNames().loadEvent);
        for (size_t i = 0; i < entry.size(); ++i) {
            if (entry[i].useCapture)
                return true;
        }
    }

    return false;
}

#if ENABLE(CSS_REGIONS)
bool SVGElement::shouldMoveToFlowThread(RenderStyle* styleToUse) const
{
    // Allow only svg root elements to be directly collected by a render flow thread.
    return parentNode() && !parentNode()->isSVGElement() && hasTagName(SVGNames::svgTag) && Element::shouldMoveToFlowThread(styleToUse);
}
#endif

void SVGElement::sendSVGLoadEventIfPossible(bool sendParentLoadEvents)
{
    RefPtr<SVGElement> currentTarget = this;
    while (currentTarget && currentTarget->haveLoadedRequiredResources()) {
        RefPtr<Element> parent;
        if (sendParentLoadEvents)
            parent = currentTarget->parentOrShadowHostElement(); // save the next parent to dispatch too incase dispatching the event changes the tree
        if (hasLoadListener(currentTarget.get()))
            currentTarget->dispatchEvent(Event::create(eventNames().loadEvent, false, false));
        currentTarget = (parent && parent->isSVGElement()) ? static_pointer_cast<SVGElement>(parent) : RefPtr<SVGElement>();
        SVGElement* element = toSVGElement(currentTarget.get());
        if (!element || !element->isOutermostSVGSVGElement())
            continue;

        // Consider <svg onload="foo()"><image xlink:href="foo.png" externalResourcesRequired="true"/></svg>.
        // If foo.png is not yet loaded, the first SVGLoad event will go to the <svg> element, sent through
        // Document::implicitClose(). Then the SVGLoad event will fire for <image>, once its loaded.
        ASSERT(sendParentLoadEvents);

        // If the load event was not sent yet by Document::implicitClose(), but the <image> from the example
        // above, just appeared, don't send the SVGLoad event to the outermost <svg>, but wait for the document
        // to be "ready to render", first.
        if (!document()->loadEventFinished())
            break;
    }
}

void SVGElement::sendSVGLoadEventIfPossibleAsynchronously()
{
    svgLoadEventTimer()->startOneShot(0);
}

void SVGElement::svgLoadEventTimerFired(Timer<SVGElement>*)
{
    sendSVGLoadEventIfPossible();
}

Timer<SVGElement>* SVGElement::svgLoadEventTimer()
{
    ASSERT_NOT_REACHED();
    return 0;
}

void SVGElement::finishParsingChildren()
{
    StyledElement::finishParsingChildren();

    // The outermost SVGSVGElement SVGLoad event is fired through Document::dispatchWindowLoadEvent.
    if (isOutermostSVGSVGElement())
        return;

    // finishParsingChildren() is called when the close tag is reached for an element (e.g. </svg>)
    // we send SVGLoad events here if we can, otherwise they'll be sent when any required loads finish
    sendSVGLoadEventIfPossible();
}

bool SVGElement::childShouldCreateRenderer(const Node* child) const
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, invalidTextContent, ());

    if (invalidTextContent.isEmpty()) {
        invalidTextContent.add(SVGNames::textPathTag);
#if ENABLE(SVG_FONTS)
        invalidTextContent.add(SVGNames::altGlyphTag);
#endif
        invalidTextContent.add(SVGNames::trefTag);
        invalidTextContent.add(SVGNames::tspanTag);
    }
    if (child->isSVGElement()) {
        const SVGElement* svgChild = toSVGElement(child);
        if (invalidTextContent.contains(svgChild->tagQName()))
            return false;

        return svgChild->isValid();
    }
    return false;
}

void SVGElement::attributeChanged(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason)
{
    StyledElement::attributeChanged(name, newValue);

    if (isIdAttributeName(name))
        document()->accessSVGExtensions()->rebuildAllElementReferencesForTarget(this);

    // Changes to the style attribute are processed lazily (see Element::getAttribute() and related methods),
    // so we don't want changes to the style attribute to result in extra work here.
    if (name != HTMLNames::styleAttr)
        svgAttributeChanged(name);
}

void SVGElement::synchronizeAnimatedSVGAttribute(const QualifiedName& name) const
{
    if (!elementData() || !elementData()->m_animatedSVGAttributesAreDirty)
        return;

    SVGElement* nonConstThis = const_cast<SVGElement*>(this);
    if (name == anyQName()) {
        nonConstThis->localAttributeToPropertyMap().synchronizeProperties(nonConstThis);
        elementData()->m_animatedSVGAttributesAreDirty = false;
    } else
        nonConstThis->localAttributeToPropertyMap().synchronizeProperty(nonConstThis, name);
}

void SVGElement::synchronizeRequiredFeatures(SVGElement* contextElement)
{
    ASSERT(contextElement);
    contextElement->synchronizeRequiredFeatures();
}

void SVGElement::synchronizeRequiredExtensions(SVGElement* contextElement)
{
    ASSERT(contextElement);
    contextElement->synchronizeRequiredExtensions();
}

void SVGElement::synchronizeSystemLanguage(SVGElement* contextElement)
{
    ASSERT(contextElement);
    contextElement->synchronizeSystemLanguage();
}

PassRefPtr<RenderStyle> SVGElement::customStyleForRenderer()
{
    if (!correspondingElement())
        return document()->ensureStyleResolver().styleForElement(this);

    RenderStyle* style = 0;
    if (Element* parent = parentOrShadowHostElement()) {
        if (RenderObject* renderer = parent->renderer())
            style = renderer->style();
    }

    return document()->ensureStyleResolver().styleForElement(correspondingElement(), style, DisallowStyleSharing);
}

MutableStylePropertySet* SVGElement::animatedSMILStyleProperties() const
{
    if (hasSVGRareData())
        return svgRareData()->animatedSMILStyleProperties();
    return 0;
}

MutableStylePropertySet& SVGElement::ensureAnimatedSMILStyleProperties()
{
    return ensureSVGRareData().ensureAnimatedSMILStyleProperties();
}

void SVGElement::setUseOverrideComputedStyle(bool value)
{
    if (hasSVGRareData())
        svgRareData()->setUseOverrideComputedStyle(value);
}

RenderStyle* SVGElement::computedStyle(PseudoId pseudoElementSpecifier)
{
    if (!hasSVGRareData() || !svgRareData()->useOverrideComputedStyle())
        return Element::computedStyle(pseudoElementSpecifier);

    RenderStyle* parentStyle = 0;
    if (Element* parent = parentOrShadowHostElement()) {
        if (RenderObject* renderer = parent->renderer())
            parentStyle = renderer->style();
    }

    return svgRareData()->overrideComputedStyle(this, parentStyle);
}

#ifndef NDEBUG
bool SVGElement::isAnimatableAttribute(const QualifiedName& name) const
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, animatableAttributes, ());

    if (animatableAttributes.isEmpty()) {
        animatableAttributes.add(XLinkNames::hrefAttr);
        animatableAttributes.add(SVGNames::amplitudeAttr);
        animatableAttributes.add(SVGNames::azimuthAttr);
        animatableAttributes.add(SVGNames::baseFrequencyAttr);
        animatableAttributes.add(SVGNames::biasAttr);
        animatableAttributes.add(SVGNames::clipPathUnitsAttr);
        animatableAttributes.add(SVGNames::cxAttr);
        animatableAttributes.add(SVGNames::cyAttr);
        animatableAttributes.add(SVGNames::diffuseConstantAttr);
        animatableAttributes.add(SVGNames::divisorAttr);
        animatableAttributes.add(SVGNames::dxAttr);
        animatableAttributes.add(SVGNames::dyAttr);
        animatableAttributes.add(SVGNames::edgeModeAttr);
        animatableAttributes.add(SVGNames::elevationAttr);
        animatableAttributes.add(SVGNames::exponentAttr);
        animatableAttributes.add(SVGNames::externalResourcesRequiredAttr);
        animatableAttributes.add(SVGNames::filterResAttr);
        animatableAttributes.add(SVGNames::filterUnitsAttr);
        animatableAttributes.add(SVGNames::fxAttr);
        animatableAttributes.add(SVGNames::fyAttr);
        animatableAttributes.add(SVGNames::gradientTransformAttr);
        animatableAttributes.add(SVGNames::gradientUnitsAttr);
        animatableAttributes.add(SVGNames::heightAttr);
        animatableAttributes.add(SVGNames::in2Attr);
        animatableAttributes.add(SVGNames::inAttr);
        animatableAttributes.add(SVGNames::interceptAttr);
        animatableAttributes.add(SVGNames::k1Attr);
        animatableAttributes.add(SVGNames::k2Attr);
        animatableAttributes.add(SVGNames::k3Attr);
        animatableAttributes.add(SVGNames::k4Attr);
        animatableAttributes.add(SVGNames::kernelMatrixAttr);
        animatableAttributes.add(SVGNames::kernelUnitLengthAttr);
        animatableAttributes.add(SVGNames::lengthAdjustAttr);
        animatableAttributes.add(SVGNames::limitingConeAngleAttr);
        animatableAttributes.add(SVGNames::markerHeightAttr);
        animatableAttributes.add(SVGNames::markerUnitsAttr);
        animatableAttributes.add(SVGNames::markerWidthAttr);
        animatableAttributes.add(SVGNames::maskContentUnitsAttr);
        animatableAttributes.add(SVGNames::maskUnitsAttr);
        animatableAttributes.add(SVGNames::methodAttr);
        animatableAttributes.add(SVGNames::modeAttr);
        animatableAttributes.add(SVGNames::numOctavesAttr);
        animatableAttributes.add(SVGNames::offsetAttr);
        animatableAttributes.add(SVGNames::operatorAttr);
        animatableAttributes.add(SVGNames::orderAttr);
        animatableAttributes.add(SVGNames::orientAttr);
        animatableAttributes.add(SVGNames::pathLengthAttr);
        animatableAttributes.add(SVGNames::patternContentUnitsAttr);
        animatableAttributes.add(SVGNames::patternTransformAttr);
        animatableAttributes.add(SVGNames::patternUnitsAttr);
        animatableAttributes.add(SVGNames::pointsAtXAttr);
        animatableAttributes.add(SVGNames::pointsAtYAttr);
        animatableAttributes.add(SVGNames::pointsAtZAttr);
        animatableAttributes.add(SVGNames::preserveAlphaAttr);
        animatableAttributes.add(SVGNames::preserveAspectRatioAttr);
        animatableAttributes.add(SVGNames::primitiveUnitsAttr);
        animatableAttributes.add(SVGNames::radiusAttr);
        animatableAttributes.add(SVGNames::rAttr);
        animatableAttributes.add(SVGNames::refXAttr);
        animatableAttributes.add(SVGNames::refYAttr);
        animatableAttributes.add(SVGNames::resultAttr);
        animatableAttributes.add(SVGNames::rotateAttr);
        animatableAttributes.add(SVGNames::rxAttr);
        animatableAttributes.add(SVGNames::ryAttr);
        animatableAttributes.add(SVGNames::scaleAttr);
        animatableAttributes.add(SVGNames::seedAttr);
        animatableAttributes.add(SVGNames::slopeAttr);
        animatableAttributes.add(SVGNames::spacingAttr);
        animatableAttributes.add(SVGNames::specularConstantAttr);
        animatableAttributes.add(SVGNames::specularExponentAttr);
        animatableAttributes.add(SVGNames::spreadMethodAttr);
        animatableAttributes.add(SVGNames::startOffsetAttr);
        animatableAttributes.add(SVGNames::stdDeviationAttr);
        animatableAttributes.add(SVGNames::stitchTilesAttr);
        animatableAttributes.add(SVGNames::surfaceScaleAttr);
        animatableAttributes.add(SVGNames::tableValuesAttr);
        animatableAttributes.add(SVGNames::targetAttr);
        animatableAttributes.add(SVGNames::targetXAttr);
        animatableAttributes.add(SVGNames::targetYAttr);
        animatableAttributes.add(SVGNames::transformAttr);
        animatableAttributes.add(SVGNames::typeAttr);
        animatableAttributes.add(SVGNames::valuesAttr);
        animatableAttributes.add(SVGNames::viewBoxAttr);
        animatableAttributes.add(SVGNames::widthAttr);
        animatableAttributes.add(SVGNames::x1Attr);
        animatableAttributes.add(SVGNames::x2Attr);
        animatableAttributes.add(SVGNames::xAttr);
        animatableAttributes.add(SVGNames::xChannelSelectorAttr);
        animatableAttributes.add(SVGNames::y1Attr);
        animatableAttributes.add(SVGNames::y2Attr);
        animatableAttributes.add(SVGNames::yAttr);
        animatableAttributes.add(SVGNames::yChannelSelectorAttr);
        animatableAttributes.add(SVGNames::zAttr);
    }

    if (name == classAttr)
        return true;

    return animatableAttributes.contains(name);
}
#endif

String SVGElement::title() const
{
    // According to spec, we should not return titles when hovering over root <svg> elements (those
    // <title> elements are the title of the document, not a tooltip) so we instantly return.
    if (isOutermostSVGSVGElement())
        return String();

    // Walk up the tree, to find out whether we're inside a <use> shadow tree, to find the right title.
    if (isInShadowTree()) {
        Element* shadowHostElement = toShadowRoot(treeScope()->rootNode())->hostElement();
        // At this time, SVG nodes are not allowed in non-<use> shadow trees, so any shadow root we do
        // have should be a use. The assert and following test is here to catch future shadow DOM changes
        // that do enable SVG in a shadow tree.
        ASSERT(!shadowHostElement || shadowHostElement->hasTagName(SVGNames::useTag));
        if (shadowHostElement && shadowHostElement->hasTagName(SVGNames::useTag)) {
            SVGUseElement* useElement = toSVGUseElement(shadowHostElement);

            // If the <use> title is not empty we found the title to use.
            String useTitle(useElement->title());
            if (!useTitle.isEmpty())
                return useTitle;
        }
    }

    // If we aren't an instance in a <use> or the <use> title was not found, then find the first
    // <title> child of this element.
    // If a title child was found, return the text contents.
    if (SVGTitleElement* titleElement = Traversal<SVGTitleElement>::firstWithin(this))
        return titleElement->innerText();

    // Otherwise return a null/empty string.
    return String();
}

bool SVGElement::rendererIsNeeded(const RenderStyle& style)
{
    // http://www.w3.org/TR/SVG/extend.html#PrivateData
    // Prevent anything other than SVG renderers from appearing in our render tree
    // Spec: SVG allows inclusion of elements from foreign namespaces anywhere
    // with the SVG content. In general, the SVG user agent will include the unknown
    // elements in the DOM but will otherwise ignore unknown elements.
    if (!parentOrShadowHostElement() || parentOrShadowHostElement()->isSVGElement())
        return StyledElement::rendererIsNeeded(style);

    return false;
}

CSSPropertyID SVGElement::cssPropertyIdForSVGAttributeName(const QualifiedName& attrName)
{
    if (!attrName.namespaceURI().isNull())
        return CSSPropertyInvalid;

    static HashMap<AtomicStringImpl*, CSSPropertyID>* propertyNameToIdMap = 0;
    if (!propertyNameToIdMap) {
        propertyNameToIdMap = new HashMap<AtomicStringImpl*, CSSPropertyID>;
        // This is a list of all base CSS and SVG CSS properties which are exposed as SVG XML attributes
        mapAttributeToCSSProperty(propertyNameToIdMap, alignment_baselineAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, baseline_shiftAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, buffered_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clipAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clip_pathAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clip_ruleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, SVGNames::colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolationAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolation_filtersAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_profileAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, cursorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, SVGNames::directionAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, displayAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, dominant_baselineAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, enable_backgroundAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fillAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fill_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fill_ruleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, filterAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, flood_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, flood_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_familyAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_sizeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_stretchAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_styleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_variantAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_weightAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, glyph_orientation_horizontalAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, glyph_orientation_verticalAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, image_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, kerningAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, letter_spacingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, lighting_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_endAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_midAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_startAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, maskAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, mask_typeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, overflowAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, pointer_eventsAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, shape_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stop_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stop_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, strokeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_dasharrayAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_dashoffsetAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_linecapAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_linejoinAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_miterlimitAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_widthAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_anchorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_decorationAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, transform_originAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, unicode_bidiAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, vector_effectAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, visibilityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, word_spacingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, writing_modeAttr);
    }

    return propertyNameToIdMap->get(attrName.localName().impl());
}

bool SVGElement::isAnimatableCSSProperty(const QualifiedName& attrName)
{
    return cssPropertyToTypeMap().contains(attrName);
}

bool SVGElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (cssPropertyIdForSVGAttributeName(name) > 0)
        return true;
    return StyledElement::isPresentationAttribute(name);
}

void SVGElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    CSSPropertyID propertyID = cssPropertyIdForSVGAttributeName(name);
    if (propertyID > 0)
        addPropertyToPresentationAttributeStyle(style, propertyID, value);
}

bool SVGElement::isKnownAttribute(const QualifiedName& attrName)
{
    return isIdAttributeName(attrName);
}

void SVGElement::svgAttributeChanged(const QualifiedName& attrName)
{
    CSSPropertyID propId = cssPropertyIdForSVGAttributeName(attrName);
    if (propId > 0) {
        SVGElementInstance::invalidateAllInstancesOfElement(this);
        return;
    }

    if (attrName == HTMLNames::classAttr) {
        classAttributeChanged(className());
        SVGElementInstance::invalidateAllInstancesOfElement(this);
        return;
    }

    if (isIdAttributeName(attrName)) {
        RenderObject* object = renderer();
        // Notify resources about id changes, this is important as we cache resources by id in SVGDocumentExtensions
        if (object && object->isSVGResourceContainer())
            object->toRenderSVGResourceContainer()->idChanged();
        if (inDocument())
            buildPendingResourcesIfNeeded();
        SVGElementInstance::invalidateAllInstancesOfElement(this);
        return;
    }
}

Node::InsertionNotificationRequest SVGElement::insertedInto(ContainerNode* rootParent)
{
    StyledElement::insertedInto(rootParent);
    updateRelativeLengthsInformation();
    buildPendingResourcesIfNeeded();
    return InsertionDone;
}

void SVGElement::buildPendingResourcesIfNeeded()
{
    Document* document = this->document();
    if (!needsPendingResourceHandling() || !document || !inDocument() || isInShadowTree())
        return;

    SVGDocumentExtensions* extensions = document->accessSVGExtensions();
    String resourceId = getIdAttribute();
    if (!extensions->hasPendingResource(resourceId))
        return;

    // Mark pending resources as pending for removal.
    extensions->markPendingResourcesForRemoval(resourceId);

    // Rebuild pending resources for each client of a pending resource that is being removed.
    while (Element* clientElement = extensions->removeElementFromPendingResourcesForRemoval(resourceId)) {
        ASSERT(clientElement->hasPendingResources());
        if (clientElement->hasPendingResources()) {
            clientElement->buildPendingResource();
            extensions->clearHasPendingResourcesIfPossible(clientElement);
        }
    }
}

void SVGElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    StyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    // Invalidate all SVGElementInstances associated with us
    if (!changedByParser)
        SVGElementInstance::invalidateAllInstancesOfElement(this);
}

PassRefPtr<CSSValue> SVGElement::getPresentationAttribute(const String& name)
{
    if (!hasAttributesWithoutUpdate())
        return 0;

    QualifiedName attributeName(nullAtom, name, nullAtom);
    const Attribute* attribute = findAttributeByName(attributeName);
    if (!attribute)
        return 0;

    RefPtr<MutableStylePropertySet> style = MutableStylePropertySet::create(SVGAttributeMode);
    CSSPropertyID propertyID = cssPropertyIdForSVGAttributeName(attribute->name());
    style->setProperty(propertyID, attribute->value());
    RefPtr<CSSValue> cssValue = style->getPropertyCSSValue(propertyID);
    return cssValue ? cssValue->cloneForCSSOM() : 0;
}

bool SVGElement::instanceUpdatesBlocked() const
{
    return hasSVGRareData() && svgRareData()->instanceUpdatesBlocked();
}

void SVGElement::setInstanceUpdatesBlocked(bool value)
{
    if (hasSVGRareData())
        svgRareData()->setInstanceUpdatesBlocked(value);
}

AffineTransform SVGElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope) const
{
    // To be overriden by SVGGraphicsElement (or as special case SVGTextElement and SVGPatternElement)
    return AffineTransform();
}

void SVGElement::updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement* element)
{
    // If we're not yet in a document, this function will be called again from insertedInto(). Do nothing now.
    if (!inDocument())
        return;

    // An element wants to notify us that its own relative lengths state changed.
    // Register it in the relative length map, and register us in the parent relative length map.
    // Register the parent in the grandparents map, etc. Repeat procedure until the root of the SVG tree.

    if (hasRelativeLengths)
        m_elementsWithRelativeLengths.add(element);
    else {
        if (!m_elementsWithRelativeLengths.contains(element)) {
            // We were never registered. Do nothing.
            return;
        }

        m_elementsWithRelativeLengths.remove(element);
    }

    // Find first styled parent node, and notify it that we've changed our relative length state.
    ContainerNode* node = parentNode();
    while (node) {
        if (!node->isSVGElement())
            break;

        // Register us in the parent element map.
        toSVGElement(node)->updateRelativeLengthsInformation(hasRelativeLengths, this);
        break;
    }
}

bool SVGElement::isMouseFocusable() const
{
    if (!isFocusable())
        return false;
    Element* eventTarget = const_cast<SVGElement*>(this);
    return eventTarget->hasEventListeners(eventNames().focusinEvent) || eventTarget->hasEventListeners(eventNames().focusoutEvent);
}

bool SVGElement::isKeyboardFocusable(KeyboardEvent*) const
{
    return isMouseFocusable();
}

}

#endif // ENABLE(SVG)

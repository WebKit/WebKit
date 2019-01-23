/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGElement.h"

#include "CSSPropertyParser.h"
#include "DeprecatedCSSOMValue.h"
#include "Document.h"
#include "ElementIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMasker.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementRareData.h"
#include "SVGGraphicsElement.h"
#include "SVGImageElement.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "ShadowRoot.h"
#include "XMLNames.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>


namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGElement);

static NEVER_INLINE HashMap<AtomicStringImpl*, CSSPropertyID> createAttributeNameToCSSPropertyIDMap()
{
    using namespace HTMLNames;
    using namespace SVGNames;

    // This list should include all base CSS and SVG CSS properties which are exposed as SVG XML attributes.
    static const QualifiedName* const attributeNames[] = {
        &alignment_baselineAttr.get(),
        &baseline_shiftAttr.get(),
        &buffered_renderingAttr.get(),
        &clipAttr.get(),
        &clip_pathAttr.get(),
        &clip_ruleAttr.get(),
        &SVGNames::colorAttr.get(),
        &color_interpolationAttr.get(),
        &color_interpolation_filtersAttr.get(),
        &color_profileAttr.get(),
        &color_renderingAttr.get(),
        &cursorAttr.get(),
        &cxAttr.get(),
        &cyAttr.get(),
        &SVGNames::directionAttr.get(),
        &displayAttr.get(),
        &dominant_baselineAttr.get(),
        &enable_backgroundAttr.get(),
        &fillAttr.get(),
        &fill_opacityAttr.get(),
        &fill_ruleAttr.get(),
        &filterAttr.get(),
        &flood_colorAttr.get(),
        &flood_opacityAttr.get(),
        &font_familyAttr.get(),
        &font_sizeAttr.get(),
        &font_stretchAttr.get(),
        &font_styleAttr.get(),
        &font_variantAttr.get(),
        &font_weightAttr.get(),
        &glyph_orientation_horizontalAttr.get(),
        &glyph_orientation_verticalAttr.get(),
        &image_renderingAttr.get(),
        &SVGNames::heightAttr.get(),
        &kerningAttr.get(),
        &letter_spacingAttr.get(),
        &lighting_colorAttr.get(),
        &marker_endAttr.get(),
        &marker_midAttr.get(),
        &marker_startAttr.get(),
        &maskAttr.get(),
        &mask_typeAttr.get(),
        &opacityAttr.get(),
        &overflowAttr.get(),
        &paint_orderAttr.get(),
        &pointer_eventsAttr.get(),
        &rAttr.get(),
        &rxAttr.get(),
        &ryAttr.get(),
        &shape_renderingAttr.get(),
        &stop_colorAttr.get(),
        &stop_opacityAttr.get(),
        &strokeAttr.get(),
        &stroke_dasharrayAttr.get(),
        &stroke_dashoffsetAttr.get(),
        &stroke_linecapAttr.get(),
        &stroke_linejoinAttr.get(),
        &stroke_miterlimitAttr.get(),
        &stroke_opacityAttr.get(),
        &stroke_widthAttr.get(),
        &text_anchorAttr.get(),
        &text_decorationAttr.get(),
        &text_renderingAttr.get(),
        &unicode_bidiAttr.get(),
        &vector_effectAttr.get(),
        &visibilityAttr.get(),
        &SVGNames::widthAttr.get(),
        &word_spacingAttr.get(),
        &writing_modeAttr.get(),
        &xAttr.get(),
        &yAttr.get(),
    };

    HashMap<AtomicStringImpl*, CSSPropertyID> map;

    for (auto& name : attributeNames) {
        const AtomicString& localName = name->localName();
        map.add(localName.impl(), cssPropertyID(localName));
    }

    // FIXME: When CSS supports "transform-origin" this special case can be removed,
    // and we can add transform_originAttr to the table above instead.
    map.add(transform_originAttr->localName().impl(), CSSPropertyTransformOrigin);

    return map;
}

static NEVER_INLINE HashMap<QualifiedName::QualifiedNameImpl*, AnimatedPropertyType> createAttributeNameToAnimatedPropertyTypeMap()
{
    using namespace HTMLNames;
    using namespace SVGNames;

    struct TableEntry {
        const QualifiedName& attributeName;
        AnimatedPropertyType type;
    };

    static const TableEntry table[] = {
        { alignment_baselineAttr, AnimatedString },
        { baseline_shiftAttr, AnimatedString },
        { buffered_renderingAttr, AnimatedString },
        { clipAttr, AnimatedRect },
        { clip_pathAttr, AnimatedString },
        { clip_ruleAttr, AnimatedString },
        { SVGNames::colorAttr, AnimatedColor },
        { color_interpolationAttr, AnimatedString },
        { color_interpolation_filtersAttr, AnimatedString },
        { color_profileAttr, AnimatedString },
        { color_renderingAttr, AnimatedString },
        { cursorAttr, AnimatedString },
        { displayAttr, AnimatedString },
        { dominant_baselineAttr, AnimatedString },
        { fillAttr, AnimatedColor },
        { fill_opacityAttr, AnimatedNumber },
        { fill_ruleAttr, AnimatedString },
        { filterAttr, AnimatedString },
        { flood_colorAttr, AnimatedColor },
        { flood_opacityAttr, AnimatedNumber },
        { font_familyAttr, AnimatedString },
        { font_sizeAttr, AnimatedLength },
        { font_stretchAttr, AnimatedString },
        { font_styleAttr, AnimatedString },
        { font_variantAttr, AnimatedString },
        { font_weightAttr, AnimatedString },
        { image_renderingAttr, AnimatedString },
        { kerningAttr, AnimatedLength },
        { letter_spacingAttr, AnimatedLength },
        { lighting_colorAttr, AnimatedColor },
        { marker_endAttr, AnimatedString },
        { marker_midAttr, AnimatedString },
        { marker_startAttr, AnimatedString },
        { maskAttr, AnimatedString },
        { mask_typeAttr, AnimatedString },
        { opacityAttr, AnimatedNumber },
        { overflowAttr, AnimatedString },
        { paint_orderAttr, AnimatedString },
        { pointer_eventsAttr, AnimatedString },
        { shape_renderingAttr, AnimatedString },
        { stop_colorAttr, AnimatedColor },
        { stop_opacityAttr, AnimatedNumber },
        { strokeAttr, AnimatedColor },
        { stroke_dasharrayAttr, AnimatedLengthList },
        { stroke_dashoffsetAttr, AnimatedLength },
        { stroke_linecapAttr, AnimatedString },
        { stroke_linejoinAttr, AnimatedString },
        { stroke_miterlimitAttr, AnimatedNumber },
        { stroke_opacityAttr, AnimatedNumber },
        { stroke_widthAttr, AnimatedLength },
        { text_anchorAttr, AnimatedString },
        { text_decorationAttr, AnimatedString },
        { text_renderingAttr, AnimatedString },
        { vector_effectAttr, AnimatedString },
        { visibilityAttr, AnimatedString },
        { word_spacingAttr, AnimatedLength },
    };

    HashMap<QualifiedName::QualifiedNameImpl*, AnimatedPropertyType> map;
    for (auto& entry : table)
        map.add(entry.attributeName.impl(), entry.type);
    return map;
}

static const HashMap<QualifiedName::QualifiedNameImpl*, AnimatedPropertyType>& attributeNameToAnimatedPropertyTypeMap()
{
    static const auto map = makeNeverDestroyed(createAttributeNameToAnimatedPropertyTypeMap());
    return map;
}

static NEVER_INLINE HashMap<QualifiedName::QualifiedNameImpl*, AnimatedPropertyType> createCSSPropertyWithSVGDOMNameToAnimatedPropertyTypeMap()
{
    using namespace HTMLNames;
    using namespace SVGNames;

    struct TableEntry {
        const QualifiedName& attributeName;
        AnimatedPropertyType type;
    };

    static const TableEntry table[] = {
        { cxAttr, AnimatedLength },
        { cyAttr, AnimatedLength },
        { rAttr, AnimatedLength },
        { rxAttr, AnimatedLength },
        { ryAttr, AnimatedLength },
        { SVGNames::heightAttr, AnimatedLength },
        { SVGNames::widthAttr, AnimatedLength },
        { xAttr, AnimatedLength },
        { yAttr, AnimatedLength },
    };

    HashMap<QualifiedName::QualifiedNameImpl*, AnimatedPropertyType> map;
    for (auto& entry : table)
        map.add(entry.attributeName.impl(), entry.type);
    return map;
}

static inline const HashMap<QualifiedName::QualifiedNameImpl*, AnimatedPropertyType>& cssPropertyWithSVGDOMNameToAnimatedPropertyTypeMap()
{
    static const auto map = makeNeverDestroyed(createCSSPropertyWithSVGDOMNameToAnimatedPropertyTypeMap());
    return map;
}

SVGElement::SVGElement(const QualifiedName& tagName, Document& document)
    : StyledElement(tagName, document, CreateSVGElement)
    , SVGLangSpace(this)
{
    registerAttributes();
}

SVGElement::~SVGElement()
{
    if (m_svgRareData) {
        for (SVGElement* instance : m_svgRareData->instances())
            instance->m_svgRareData->setCorrespondingElement(nullptr);
        if (auto correspondingElement = makeRefPtr(m_svgRareData->correspondingElement()))
            correspondingElement->m_svgRareData->instances().remove(this);

        m_svgRareData = nullptr;
    }
    document().accessSVGExtensions().rebuildAllElementReferencesForTarget(*this);
    document().accessSVGExtensions().removeAllElementReferencesForTarget(*this);
}

int SVGElement::tabIndex() const
{
    if (supportsFocus())
        return Element::tabIndex();
    return -1;
}

void SVGElement::willRecalcStyle(Style::Change change)
{
    if (!m_svgRareData || styleResolutionShouldRecompositeLayer())
        return;
    // If the style changes because of a regular property change (not induced by SMIL animations themselves)
    // reset the "computed style without SMIL style properties", so the base value change gets reflected.
    if (change > Style::NoChange || needsStyleRecalc())
        m_svgRareData->setNeedsOverrideComputedStyleUpdate();
}

SVGElementRareData& SVGElement::ensureSVGRareData()
{
    if (!m_svgRareData)
        m_svgRareData = std::make_unique<SVGElementRareData>();
    return *m_svgRareData;
}

bool SVGElement::isOutermostSVGSVGElement() const
{
    if (!is<SVGSVGElement>(*this))
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
    SVGDocumentExtensions& extensions = document().accessSVGExtensions();

    if (error == NegativeValueForbiddenError) {
        extensions.reportError("Invalid negative value for " + errorString);
        return;
    }

    if (error == ParsingAttributeFailedError) {
        extensions.reportError("Invalid value for " + errorString);
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.disconnectedFromDocument)
        updateRelativeLengthsInformation(false, this);

    StyledElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (removalType.disconnectedFromDocument) {
        document().accessSVGExtensions().clearTargetDependencies(*this);
        document().accessSVGExtensions().removeAllElementReferencesForTarget(*this);
    }
    invalidateInstances();
}

SVGSVGElement* SVGElement::ownerSVGElement() const
{
    ContainerNode* node = parentOrShadowHostNode();
    while (node) {
        if (is<SVGSVGElement>(*node))
            return downcast<SVGSVGElement>(node);

        node = node->parentOrShadowHostNode();
    }

    return nullptr;
}

SVGElement* SVGElement::viewportElement() const
{
    // This function needs shadow tree support - as RenderSVGContainer uses this function
    // to determine the "overflow" property. <use> on <symbol> wouldn't work otherwhise.
    ContainerNode* node = parentOrShadowHostNode();
    while (node) {
        if (is<SVGSVGElement>(*node) || is<SVGImageElement>(*node) || node->hasTagName(SVGNames::symbolTag))
            return downcast<SVGElement>(node);

        node = node->parentOrShadowHostNode();
    }

    return nullptr;
}
 
const HashSet<SVGElement*>& SVGElement::instances() const
{
    if (!m_svgRareData) {
        static NeverDestroyed<HashSet<SVGElement*>> emptyInstances;
        return emptyInstances;
    }
    return m_svgRareData->instances();
}

bool SVGElement::getBoundingBox(FloatRect& rect, SVGLocatable::StyleUpdateStrategy styleUpdateStrategy)
{
    if (is<SVGGraphicsElement>(*this)) {
        rect = downcast<SVGGraphicsElement>(*this).getBBox(styleUpdateStrategy);
        return true;
    }
    return false;
}

SVGElement* SVGElement::correspondingElement() const
{
    return m_svgRareData ? m_svgRareData->correspondingElement() : nullptr;
}

RefPtr<SVGUseElement> SVGElement::correspondingUseElement() const
{
    auto* root = containingShadowRoot();
    if (!root)
        return nullptr;
    if (root->mode() != ShadowRootMode::UserAgent)
        return nullptr;
    auto* host = root->host();
    if (!is<SVGUseElement>(host))
        return nullptr;
    return &downcast<SVGUseElement>(*host);
}

void SVGElement::setCorrespondingElement(SVGElement* correspondingElement)
{
    if (m_svgRareData) {
        if (auto oldCorrespondingElement = makeRefPtr(m_svgRareData->correspondingElement()))
            oldCorrespondingElement->m_svgRareData->instances().remove(this);
    }
    if (m_svgRareData || correspondingElement)
        ensureSVGRareData().setCorrespondingElement(correspondingElement);
    if (correspondingElement)
        correspondingElement->ensureSVGRareData().instances().add(this);
}

void SVGElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<HTMLNames::classAttr, &SVGElement::m_className>();
}

void SVGElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == HTMLNames::classAttr) {
        m_className.setValue(value);
        return;
    }

    if (name == HTMLNames::tabindexAttr) {
        if (value.isEmpty())
            clearTabIndexExplicitlyIfNeeded();
        else if (auto optionalTabIndex = parseHTMLInteger(value))
            setTabIndexExplicitly(optionalTabIndex.value());
        return;
    }

    auto& eventName = HTMLElement::eventNameForEventHandlerAttribute(name);
    if (!eventName.isNull()) {
        setAttributeEventListener(eventName, name, value);
        return;
    }

    SVGLangSpace::parseAttribute(name, value);
}

Vector<AnimatedPropertyType> SVGElement::animatedPropertyTypesForAttribute(const QualifiedName& attributeName)
{
    auto types = animatedTypes(attributeName);
    if (!types.isEmpty())
        return types;

    {
        auto& map = attributeNameToAnimatedPropertyTypeMap();
        auto it = map.find(attributeName.impl());
        if (it != map.end()) {
            types.append(it->value);
            return types;
        }
    }

    {
        auto& map = cssPropertyWithSVGDOMNameToAnimatedPropertyTypeMap();
        auto it = map.find(attributeName.impl());
        if (it != map.end()) {
            types.append(it->value);
            return types;
        }
    }

    return types;
}

bool SVGElement::haveLoadedRequiredResources()
{
    for (auto& child : childrenOfType<SVGElement>(*this)) {
        if (!child.haveLoadedRequiredResources())
            return false;
    }
    return true;
}

bool SVGElement::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{   
    // Add event listener to regular DOM element
    if (!Node::addEventListener(eventType, listener.copyRef(), options))
        return false;

    if (containingShadowRoot())
        return true;

    // Add event listener to all shadow tree DOM element instances
    ASSERT(!instanceUpdatesBlocked());
    for (auto* instance : instances()) {
        ASSERT(instance->correspondingElement() == this);
        bool result = instance->Node::addEventListener(eventType, listener.copyRef(), options);
        ASSERT_UNUSED(result, result);
    }

    return true;
}

bool SVGElement::removeEventListener(const AtomicString& eventType, EventListener& listener, const ListenerOptions& options)
{
    if (containingShadowRoot())
        return Node::removeEventListener(eventType, listener, options);

    // EventTarget::removeEventListener creates a Ref around the given EventListener
    // object when creating a temporary RegisteredEventListener object used to look up the
    // event listener in a cache. If we want to be able to call removeEventListener() multiple
    // times on different nodes, we have to delay its immediate destruction, which would happen
    // after the first call below.
    Ref<EventListener> protector(listener);

    // Remove event listener from regular DOM element
    if (!Node::removeEventListener(eventType, listener, options))
        return false;

    // Remove event listener from all shadow tree DOM element instances
    ASSERT(!instanceUpdatesBlocked());
    for (auto& instance : instances()) {
        ASSERT(instance->correspondingElement() == this);

        if (instance->Node::removeEventListener(eventType, listener, options))
            continue;

        // This case can only be hit for event listeners created from markup
        ASSERT(listener.wasCreatedFromMarkup());

        // If the event listener 'listener' has been created from markup and has been fired before
        // then JSLazyEventListener::parseCode() has been called and m_jsFunction of that listener
        // has been created (read: it's not 0 anymore). During shadow tree creation, the event
        // listener DOM attribute has been cloned, and another event listener has been setup in
        // the shadow tree. If that event listener has not been used yet, m_jsFunction is still 0,
        // and tryRemoveEventListener() above will fail. Work around that very rare problem.
        ASSERT(instance->eventTargetData());
        instance->eventTargetData()->eventListenerMap.removeFirstEventListenerCreatedFromMarkup(eventType);
    }

    return true;
}

static bool hasLoadListener(Element* element)
{
    if (element->hasEventListeners(eventNames().loadEvent))
        return true;

    for (element = element->parentOrShadowHostElement(); element; element = element->parentOrShadowHostElement()) {
        if (element->hasCapturingEventListeners(eventNames().loadEvent))
            return true;
    }

    return false;
}

void SVGElement::sendSVGLoadEventIfPossible(bool sendParentLoadEvents)
{
    if (!isConnected() || !document().frame())
        return;

    RefPtr<SVGElement> currentTarget = this;
    while (currentTarget && currentTarget->haveLoadedRequiredResources()) {
        RefPtr<Element> parent;
        if (sendParentLoadEvents)
            parent = currentTarget->parentOrShadowHostElement(); // save the next parent to dispatch too incase dispatching the event changes the tree
        if (hasLoadListener(currentTarget.get()))
            currentTarget->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
        currentTarget = (parent && parent->isSVGElement()) ? static_pointer_cast<SVGElement>(parent) : RefPtr<SVGElement>();
        SVGElement* element = currentTarget.get();
        if (!element || !element->isOutermostSVGSVGElement())
            continue;

        // Consider <svg onload="foo()"><image xlink:href="foo.png" externalResourcesRequired="true"/></svg>.
        // If foo.png is not yet loaded, the first SVGLoad event will go to the <svg> element, sent through
        // Document::implicitClose(). Then the SVGLoad event will fire for <image>, once its loaded.
        ASSERT(sendParentLoadEvents);

        // If the load event was not sent yet by Document::implicitClose(), but the <image> from the example
        // above, just appeared, don't send the SVGLoad event to the outermost <svg>, but wait for the document
        // to be "ready to render", first.
        if (!document().loadEventFinished())
            break;
    }
}

void SVGElement::sendSVGLoadEventIfPossibleAsynchronously()
{
    svgLoadEventTimer()->startOneShot(0_s);
}

void SVGElement::svgLoadEventTimerFired()
{
    sendSVGLoadEventIfPossible();
}

Timer* SVGElement::svgLoadEventTimer()
{
    ASSERT_NOT_REACHED();
    return nullptr;
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

    // Notify all the elements which have references to this element to rebuild their shadow and render
    // trees, e.g. a <use> element references a target element before this target element is defined.
    invalidateInstances();
}

bool SVGElement::childShouldCreateRenderer(const Node& child) const
{
    if (!child.isSVGElement())
        return false;
    auto& svgChild = downcast<SVGElement>(child);

    static const QualifiedName* const invalidTextContent[] {
#if ENABLE(SVG_FONTS)
        &SVGNames::altGlyphTag.get(),
#endif
        &SVGNames::textPathTag.get(),
        &SVGNames::trefTag.get(),
        &SVGNames::tspanTag.get(),
    };
    auto& name = svgChild.localName();
    for (auto* tag : invalidTextContent) {
        if (name == tag->localName())
            return false;
    }

    return svgChild.isValid();
}

void SVGElement::attributeChanged(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason)
{
    StyledElement::attributeChanged(name, oldValue, newValue);

    if (name == HTMLNames::idAttr)
        document().accessSVGExtensions().rebuildAllElementReferencesForTarget(*this);

    // Changes to the style attribute are processed lazily (see Element::getAttribute() and related methods),
    // so we don't want changes to the style attribute to result in extra work here except invalidateInstances().
    if (name == HTMLNames::styleAttr)
        invalidateInstances();
    else
        svgAttributeChanged(name);
}

void SVGElement::synchronizeAllAnimatedSVGAttribute(SVGElement* svgElement)
{
    ASSERT(svgElement->elementData());
    ASSERT(svgElement->elementData()->animatedSVGAttributesAreDirty());

    svgElement->synchronizeAttributes();
    svgElement->elementData()->setAnimatedSVGAttributesAreDirty(false);
}

void SVGElement::synchronizeAnimatedSVGAttribute(const QualifiedName& name) const
{
    if (!elementData() || !elementData()->animatedSVGAttributesAreDirty())
        return;

    SVGElement* nonConstThis = const_cast<SVGElement*>(this);
    if (name == anyQName())
        synchronizeAllAnimatedSVGAttribute(nonConstThis);
    else
        nonConstThis->synchronizeAttribute(name);
}

Optional<ElementStyle> SVGElement::resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle*)
{
    // If the element is in a <use> tree we get the style from the definition tree.
    if (auto styleElement = makeRefPtr(this->correspondingElement())) {
        Optional<ElementStyle> style = styleElement->resolveStyle(&parentStyle);
        StyleResolver::adjustSVGElementStyle(*this, *style->renderStyle);
        return style;
    }

    return resolveStyle(&parentStyle);
}

MutableStyleProperties* SVGElement::animatedSMILStyleProperties() const
{
    if (m_svgRareData)
        return m_svgRareData->animatedSMILStyleProperties();
    return 0;
}

MutableStyleProperties& SVGElement::ensureAnimatedSMILStyleProperties()
{
    return ensureSVGRareData().ensureAnimatedSMILStyleProperties();
}

void SVGElement::setUseOverrideComputedStyle(bool value)
{
    if (m_svgRareData)
        m_svgRareData->setUseOverrideComputedStyle(value);
}

const RenderStyle* SVGElement::computedStyle(PseudoId pseudoElementSpecifier)
{
    if (!m_svgRareData || !m_svgRareData->useOverrideComputedStyle())
        return Element::computedStyle(pseudoElementSpecifier);

    const RenderStyle* parentStyle = nullptr;
    if (auto parent = makeRefPtr(parentOrShadowHostElement())) {
        if (auto renderer = parent->renderer())
            parentStyle = &renderer->style();
    }

    return m_svgRareData->overrideComputedStyle(*this, parentStyle);
}

QualifiedName SVGElement::animatableAttributeForName(const AtomicString& localName)
{
    static const auto animatableAttributes = makeNeverDestroyed([] {
        static const QualifiedName* const names[] = {
            &HTMLNames::classAttr.get(),
            &SVGNames::amplitudeAttr.get(),
            &SVGNames::azimuthAttr.get(),
            &SVGNames::baseFrequencyAttr.get(),
            &SVGNames::biasAttr.get(),
            &SVGNames::clipPathUnitsAttr.get(),
            &SVGNames::cxAttr.get(),
            &SVGNames::cyAttr.get(),
            &SVGNames::diffuseConstantAttr.get(),
            &SVGNames::divisorAttr.get(),
            &SVGNames::dxAttr.get(),
            &SVGNames::dyAttr.get(),
            &SVGNames::edgeModeAttr.get(),
            &SVGNames::elevationAttr.get(),
            &SVGNames::exponentAttr.get(),
            &SVGNames::externalResourcesRequiredAttr.get(),
            &SVGNames::filterUnitsAttr.get(),
            &SVGNames::fxAttr.get(),
            &SVGNames::fyAttr.get(),
            &SVGNames::gradientTransformAttr.get(),
            &SVGNames::gradientUnitsAttr.get(),
            &SVGNames::heightAttr.get(),
            &SVGNames::in2Attr.get(),
            &SVGNames::inAttr.get(),
            &SVGNames::interceptAttr.get(),
            &SVGNames::k1Attr.get(),
            &SVGNames::k2Attr.get(),
            &SVGNames::k3Attr.get(),
            &SVGNames::k4Attr.get(),
            &SVGNames::kernelMatrixAttr.get(),
            &SVGNames::kernelUnitLengthAttr.get(),
            &SVGNames::lengthAdjustAttr.get(),
            &SVGNames::limitingConeAngleAttr.get(),
            &SVGNames::markerHeightAttr.get(),
            &SVGNames::markerUnitsAttr.get(),
            &SVGNames::markerWidthAttr.get(),
            &SVGNames::maskContentUnitsAttr.get(),
            &SVGNames::maskUnitsAttr.get(),
            &SVGNames::methodAttr.get(),
            &SVGNames::modeAttr.get(),
            &SVGNames::numOctavesAttr.get(),
            &SVGNames::offsetAttr.get(),
            &SVGNames::operatorAttr.get(),
            &SVGNames::orderAttr.get(),
            &SVGNames::orientAttr.get(),
            &SVGNames::pathLengthAttr.get(),
            &SVGNames::patternContentUnitsAttr.get(),
            &SVGNames::patternTransformAttr.get(),
            &SVGNames::patternUnitsAttr.get(),
            &SVGNames::pointsAtXAttr.get(),
            &SVGNames::pointsAtYAttr.get(),
            &SVGNames::pointsAtZAttr.get(),
            &SVGNames::preserveAlphaAttr.get(),
            &SVGNames::preserveAspectRatioAttr.get(),
            &SVGNames::primitiveUnitsAttr.get(),
            &SVGNames::radiusAttr.get(),
            &SVGNames::rAttr.get(),
            &SVGNames::refXAttr.get(),
            &SVGNames::refYAttr.get(),
            &SVGNames::resultAttr.get(),
            &SVGNames::rotateAttr.get(),
            &SVGNames::rxAttr.get(),
            &SVGNames::ryAttr.get(),
            &SVGNames::scaleAttr.get(),
            &SVGNames::seedAttr.get(),
            &SVGNames::slopeAttr.get(),
            &SVGNames::spacingAttr.get(),
            &SVGNames::specularConstantAttr.get(),
            &SVGNames::specularExponentAttr.get(),
            &SVGNames::spreadMethodAttr.get(),
            &SVGNames::startOffsetAttr.get(),
            &SVGNames::stdDeviationAttr.get(),
            &SVGNames::stitchTilesAttr.get(),
            &SVGNames::surfaceScaleAttr.get(),
            &SVGNames::tableValuesAttr.get(),
            &SVGNames::targetAttr.get(),
            &SVGNames::targetXAttr.get(),
            &SVGNames::targetYAttr.get(),
            &SVGNames::transformAttr.get(),
            &SVGNames::typeAttr.get(),
            &SVGNames::valuesAttr.get(),
            &SVGNames::viewBoxAttr.get(),
            &SVGNames::widthAttr.get(),
            &SVGNames::x1Attr.get(),
            &SVGNames::x2Attr.get(),
            &SVGNames::xAttr.get(),
            &SVGNames::xChannelSelectorAttr.get(),
            &SVGNames::y1Attr.get(),
            &SVGNames::y2Attr.get(),
            &SVGNames::yAttr.get(),
            &SVGNames::yChannelSelectorAttr.get(),
            &SVGNames::zAttr.get(),
            &SVGNames::hrefAttr.get(),
        };
        HashMap<AtomicString, QualifiedName> map;
        for (auto& name : names) {
            auto addResult = map.add(name->localName(), *name);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }
        return map;
    }());
    return animatableAttributes.get().get(localName);
}

#ifndef NDEBUG

bool SVGElement::isAnimatableAttribute(const QualifiedName& name) const
{
    if (animatableAttributeForName(name.localName()) == name)
        return !filterOutAnimatableAttribute(name);
    return false;
}

bool SVGElement::filterOutAnimatableAttribute(const QualifiedName&) const
{
    return false;
}

#endif

String SVGElement::title() const
{
    // According to spec, for stand-alone SVG documents we should not return a title when
    // hovering over the rootmost SVG element (the first <title> element is the title of
    // the document, not a tooltip) so we instantly return.
    if (isOutermostSVGSVGElement() && document().topDocument().isSVGDocument())
        return String();
    auto firstTitle = childrenOfType<SVGTitleElement>(*this).first();
    return firstTitle ? const_cast<SVGTitleElement*>(firstTitle)->innerText() : String();
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

    static const auto properties = makeNeverDestroyed(createAttributeNameToCSSPropertyIDMap());
    return properties.get().get(attrName.localName().impl());
}

bool SVGElement::isAnimatableCSSProperty(const QualifiedName& attributeName)
{
    return attributeNameToAnimatedPropertyTypeMap().contains(attributeName.impl())
        || cssPropertyWithSVGDOMNameToAnimatedPropertyTypeMap().contains(attributeName.impl());
}

bool SVGElement::isPresentationAttributeWithSVGDOM(const QualifiedName& attributeName)
{
    return !animatedTypes(attributeName).isEmpty();
}

bool SVGElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (cssPropertyIdForSVGAttributeName(name) > 0)
        return true;
    return StyledElement::isPresentationAttribute(name);
}

void SVGElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    CSSPropertyID propertyID = cssPropertyIdForSVGAttributeName(name);
    if (propertyID > 0)
        addPropertyToPresentationAttributeStyle(style, propertyID, value);
}

void SVGElement::svgAttributeChanged(const QualifiedName& attrName)
{
    CSSPropertyID propId = cssPropertyIdForSVGAttributeName(attrName);
    if (propId > 0) {
        invalidateInstances();
        return;
    }

    if (attrName == HTMLNames::classAttr) {
        classAttributeChanged(className());
        invalidateInstances();
        return;
    }

    if (attrName == HTMLNames::idAttr) {
        auto renderer = this->renderer();
        // Notify resources about id changes, this is important as we cache resources by id in SVGDocumentExtensions
        if (is<RenderSVGResourceContainer>(renderer))
            downcast<RenderSVGResourceContainer>(*renderer).idChanged();
        if (isConnected())
            buildPendingResourcesIfNeeded();
        invalidateInstances();
        return;
    }

    SVGLangSpace::svgAttributeChanged(attrName);
}

Node::InsertedIntoAncestorResult SVGElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    StyledElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    updateRelativeLengthsInformation();
    buildPendingResourcesIfNeeded();
    return InsertedIntoAncestorResult::Done;
}

void SVGElement::buildPendingResourcesIfNeeded()
{
    if (!needsPendingResourceHandling() || !isConnected() || isInShadowTree())
        return;

    SVGDocumentExtensions& extensions = document().accessSVGExtensions();
    String resourceId = getIdAttribute();
    if (!extensions.isIdOfPendingResource(resourceId))
        return;

    // Mark pending resources as pending for removal.
    extensions.markPendingResourcesForRemoval(resourceId);

    // Rebuild pending resources for each client of a pending resource that is being removed.
    while (auto clientElement = extensions.removeElementFromPendingResourcesForRemovalMap(resourceId)) {
        ASSERT(clientElement->hasPendingResources());
        if (clientElement->hasPendingResources()) {
            clientElement->buildPendingResource();
            extensions.clearHasPendingResourcesIfPossible(*clientElement);
        }
    }
}

void SVGElement::childrenChanged(const ChildChange& change)
{
    StyledElement::childrenChanged(change);

    if (change.source == ChildChangeSource::Parser)
        return;
    invalidateInstances();
}

RefPtr<DeprecatedCSSOMValue> SVGElement::getPresentationAttribute(const String& name)
{
    if (!hasAttributesWithoutUpdate())
        return 0;

    QualifiedName attributeName(nullAtom(), name, nullAtom());
    const Attribute* attribute = findAttributeByName(attributeName);
    if (!attribute)
        return 0;

    auto style = MutableStyleProperties::create(SVGAttributeMode);
    CSSPropertyID propertyID = cssPropertyIdForSVGAttributeName(attribute->name());
    style->setProperty(propertyID, attribute->value());
    auto cssValue = style->getPropertyCSSValue(propertyID);
    if (!cssValue)
        return nullptr;
    return cssValue->createDeprecatedCSSOMWrapper(style->ensureCSSStyleDeclaration());
}

bool SVGElement::instanceUpdatesBlocked() const
{
    return m_svgRareData && m_svgRareData->instanceUpdatesBlocked();
}

void SVGElement::setInstanceUpdatesBlocked(bool value)
{
    // Catch any callers that calls setInstanceUpdatesBlocked(true) twice in a row.
    // That probably indicates nested use of InstanceUpdateBlocker and a bug.
    ASSERT(!value || !instanceUpdatesBlocked());

    if (m_svgRareData)
        m_svgRareData->setInstanceUpdatesBlocked(value);
}

AffineTransform SVGElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope) const
{
    // To be overridden by SVGGraphicsElement (or as special case SVGTextElement and SVGPatternElement)
    return AffineTransform();
}

void SVGElement::updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement* element)
{
    // If we're not yet in a document, this function will be called again from insertedIntoAncestor(). Do nothing now.
    if (!isConnected())
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

    if (!element->isSVGGraphicsElement())
        return;

    // Find first styled parent node, and notify it that we've changed our relative length state.
    auto node = makeRefPtr(parentNode());
    while (node) {
        if (!node->isSVGElement())
            break;

        // Register us in the parent element map.
        downcast<SVGElement>(*node).updateRelativeLengthsInformation(hasRelativeLengths, this);
        break;
    }
}

bool SVGElement::hasFocusEventListeners() const
{
    Element* eventTarget = const_cast<SVGElement*>(this);
    return eventTarget->hasEventListeners(eventNames().focusinEvent)
        || eventTarget->hasEventListeners(eventNames().focusoutEvent)
        || eventTarget->hasEventListeners(eventNames().focusEvent)
        || eventTarget->hasEventListeners(eventNames().blurEvent);
}

bool SVGElement::isMouseFocusable() const
{
    if (!isFocusable())
        return false;
    Element* eventTarget = const_cast<SVGElement*>(this);
    return hasFocusEventListeners()
        || eventTarget->hasEventListeners(eventNames().keydownEvent)
        || eventTarget->hasEventListeners(eventNames().keyupEvent)
        || eventTarget->hasEventListeners(eventNames().keypressEvent);
}
    
void SVGElement::accessKeyAction(bool sendMouseEvents)
{
    dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

void SVGElement::invalidateInstances()
{
    if (instanceUpdatesBlocked())
        return;

    auto& instances = this->instances();
    while (!instances.isEmpty()) {
        auto instance = makeRefPtr(*instances.begin());
        if (auto useElement = instance->correspondingUseElement())
            useElement->invalidateShadowTree();
        instance->setCorrespondingElement(nullptr);
    } while (!instances.isEmpty());
}

}

/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#include "Document.h"
#include "ElementChildIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "RenderAncestorIterator.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMasker.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementRareData.h"
#include "SVGForeignObjectElement.h"
#include "SVGGraphicsElement.h"
#include "SVGImageElement.h"
#include "SVGNames.h"
#include "SVGPropertyAnimatorFactory.h"
#include "SVGRenderStyle.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "ShadowRoot.h"
#include "StyleAdjuster.h"
#include "XMLNames.h"
#include <wtf/HashMap.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGElement);

static NEVER_INLINE MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, CSSPropertyID> createAttributeNameToCSSPropertyIDMap()
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

    MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, CSSPropertyID> map;

    for (auto& name : attributeNames) {
        const AtomString& localName = name->localName();
        map.add(localName, cssPropertyID(localName));
    }

    // FIXME: When CSS supports "transform-origin" this special case can be removed,
    // and we can add transform_originAttr to the table above instead.
    map.add(transform_originAttr->localName(), CSSPropertyTransformOrigin);

    return map;
}

SVGElement::SVGElement(const QualifiedName& tagName, Document& document, ConstructionType constructionType)
    : StyledElement(tagName, document, constructionType)
    , m_propertyAnimatorFactory(makeUnique<SVGPropertyAnimatorFactory>())
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<HTMLNames::classAttr, &SVGElement::m_className>();
    });
}

SVGElement::~SVGElement()
{
    if (m_svgRareData) {
        RELEASE_ASSERT(m_svgRareData->referencingElements().computesEmpty());
        for (SVGElement& instance : copyToVectorOf<Ref<SVGElement>>(instances()))
            instance.m_svgRareData->setCorrespondingElement(nullptr);
        RELEASE_ASSERT(!m_svgRareData->correspondingElement());
        m_svgRareData = nullptr;
    }
    document().accessSVGExtensions().removeElementToRebuild(*this);
}

void SVGElement::willRecalcStyle(Style::Change change)
{
    if (!m_svgRareData || styleResolutionShouldRecompositeLayer())
        return;
    // If the style changes because of a regular property change (not induced by SMIL animations themselves)
    // reset the "computed style without SMIL style properties", so the base value change gets reflected.
    if (change > Style::Change::None || needsStyleRecalc())
        m_svgRareData->setNeedsOverrideComputedStyleUpdate();
}

SVGElementRareData& SVGElement::ensureSVGRareData()
{
    if (!m_svgRareData)
        m_svgRareData = makeUnique<SVGElementRareData>();
    return *m_svgRareData;
}

bool SVGElement::isOutermostSVGSVGElement() const
{
    if (!is<SVGSVGElement>(*this))
        return false;

    // Element may not be in the document, pretend we're outermost for viewport(), getCTM(), etc.
    if (!parentNode())
        return true;

    // We act like an outermost SVG element, if we're a direct child of a <foreignObject> element.
    if (is<SVGForeignObjectElement>(*parentNode()))
        return true;

    // If we're inside the shadow tree of a <use> element, we're always an inner <svg> element.
    if (isInShadowTree() && is<SVGUseElement>(shadowHost()))
        return false;

    // This is true whenever this is the outermost SVG, even if there are HTML elements outside it
    return !is<SVGElement>(*parentNode());
}

void SVGElement::reportAttributeParsingError(SVGParsingError error, const QualifiedName& name, const AtomString& value)
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
        updateRelativeLengthsInformation(false, *this);

    StyledElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (removalType.disconnectedFromDocument) {
        auto& extensions = document().accessSVGExtensions();
        if (m_svgRareData) {
            for (auto& element : m_svgRareData->takeReferencingElements()) {
                extensions.addElementToRebuild(element);
                makeRef(element)->clearTarget();
            }
            RELEASE_ASSERT(m_svgRareData->referencingElements().computesEmpty());
        }
        extensions.removeElementToRebuild(*this);
    }
    invalidateInstances();

    if (removalType.treeScopeChanged && oldParentOfRemovedTree.isUserAgentShadowRoot())
        setCorrespondingElement(nullptr);
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
 
const WeakHashSet<SVGElement>& SVGElement::instances() const
{
    if (!m_svgRareData) {
        static NeverDestroyed<WeakHashSet<SVGElement>> emptyInstances;
        return emptyInstances;
    }
    return m_svgRareData->instances();
}

std::optional<FloatRect> SVGElement::getBoundingBox() const
{
    if (is<SVGGraphicsElement>(*this)) {
        if (auto renderer = this->renderer())
            return renderer->objectBoundingBox();
    }
    return std::nullopt;
}

Vector<Ref<SVGElement>> SVGElement::referencingElements() const
{
    if (!m_svgRareData)
        return { };
    return copyToVectorOf<Ref<SVGElement>>(m_svgRareData->referencingElements());
}

void SVGElement::addReferencingElement(SVGElement& element)
{
    ensureSVGRareData().addReferencingElement(element);
    auto& rareDataOfReferencingElement = element.ensureSVGRareData();
    RELEASE_ASSERT(!rareDataOfReferencingElement.referenceTarget());
    rareDataOfReferencingElement.setReferenceTarget(makeWeakPtr(*this));
}

void SVGElement::removeReferencingElement(SVGElement& element)
{
    ensureSVGRareData().removeReferencingElement(element);
    element.ensureSVGRareData().setReferenceTarget(nullptr);
}

void SVGElement::removeElementReference()
{
    if (!m_svgRareData)
        return;
    if (auto destination = makeRefPtr(m_svgRareData->referenceTarget()))
        destination->removeReferencingElement(*this);
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
            oldCorrespondingElement->m_svgRareData->removeInstance(*this);
    }
    if (m_svgRareData || correspondingElement)
        ensureSVGRareData().setCorrespondingElement(correspondingElement);
    if (correspondingElement)
        correspondingElement->ensureSVGRareData().addInstance(*this);
}

void SVGElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == HTMLNames::classAttr) {
        m_className->setBaseValInternal(value);
        return;
    }

    if (name == HTMLNames::tabindexAttr) {
        if (value.isEmpty())
            setTabIndexExplicitly(std::nullopt);
        else if (auto optionalTabIndex = parseHTMLInteger(value))
            setTabIndexExplicitly(optionalTabIndex.value());
        return;
    }

    auto& eventName = HTMLElement::eventNameForEventHandlerAttribute(name);
    if (!eventName.isNull()) {
        setAttributeEventListener(eventName, name, value);
        return;
    }
}

bool SVGElement::haveLoadedRequiredResources()
{
    for (auto& child : childrenOfType<SVGElement>(*this)) {
        if (!child.haveLoadedRequiredResources())
            return false;
    }
    return true;
}

bool SVGElement::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{   
    // Add event listener to regular DOM element
    if (!Node::addEventListener(eventType, listener.copyRef(), options))
        return false;

    if (containingShadowRoot())
        return true;

    // Add event listener to all shadow tree DOM element instances
    ASSERT(!instanceUpdatesBlocked());
    for (auto& instance : copyToVectorOf<Ref<SVGElement>>(instances())) {
        ASSERT(instance->correspondingElement() == this);
        ASSERT(instance->isInUserAgentShadowTree());
        bool result = instance->Node::addEventListener(eventType, listener.copyRef(), options);
        ASSERT_UNUSED(result, result);
    }

    return true;
}

bool SVGElement::removeEventListener(const AtomString& eventType, EventListener& listener, const EventListenerOptions& options)
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
    for (auto& instance : copyToVectorOf<Ref<SVGElement>>(instances())) {
        ASSERT(instance->correspondingElement() == this);
        ASSERT(instance->isInUserAgentShadowTree());

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

void SVGElement::sendLoadEventIfPossible()
{
    if (!isConnected() || !document().frame())
        return;

    if (!haveLoadedRequiredResources() || !hasLoadListener(this))
        return;

    dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void SVGElement::loadEventTimerFired()
{
    sendLoadEventIfPossible();
}

Timer* SVGElement::loadEventTimer()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void SVGElement::finishParsingChildren()
{
    StyledElement::finishParsingChildren();

    if (isOutermostSVGSVGElement())
        return;

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
        &SVGNames::altGlyphTag.get(),
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

void SVGElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason)
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

void SVGElement::synchronizeAttribute(const QualifiedName& name)
{
    // If the value of the property has changed, serialize the new value to the attribute.
    if (auto value = propertyRegistry().synchronize(name))
        setSynchronizedLazyAttribute(name, *value);
}
    
void SVGElement::synchronizeAllAttributes()
{
    // SVGPropertyRegistry::synchronizeAllAttributes() returns the new values of
    // the properties which have changed but not committed yet.
    auto map = propertyRegistry().synchronizeAllAttributes();
    for (const auto& entry : map)
        setSynchronizedLazyAttribute(entry.key, entry.value);
}

void SVGElement::commitPropertyChange(SVGProperty* property)
{
    // We want to dirty the top-level property when a descendant changes. For example
    // a change in an SVGLength item in SVGLengthList should set the dirty flag on
    // SVGLengthList and not the SVGLength.
    property->setDirty();

    invalidateSVGAttributes();
    svgAttributeChanged(propertyRegistry().propertyAttributeName(*property));
}

void SVGElement::commitPropertyChange(SVGAnimatedProperty& animatedProperty)
{
    QualifiedName attributeName = propertyRegistry().animatedPropertyAttributeName(animatedProperty);
    ASSERT(attributeName != nullQName());

    // A change in a style property, e.g SVGRectElement::x should be serialized to
    // the attribute immediately. Otherwise it is okay to be lazy in this regard.
    if (!propertyRegistry().isAnimatedStylePropertyAttribute(attributeName))
        propertyRegistry().setAnimatedPropertyDirty(attributeName, animatedProperty);
    else
        setSynchronizedLazyAttribute(attributeName, animatedProperty.baseValAsString());

    invalidateSVGAttributes();
    svgAttributeChanged(attributeName);
}

bool SVGElement::isAnimatedPropertyAttribute(const QualifiedName& attributeName) const
{
    return propertyRegistry().isAnimatedPropertyAttribute(attributeName);
}

bool SVGElement::isAnimatedAttribute(const QualifiedName& attributeName) const
{
    return SVGPropertyAnimatorFactory::isKnownAttribute(attributeName) || isAnimatedPropertyAttribute(attributeName);
}

bool SVGElement::isAnimatedStyleAttribute(const QualifiedName& attributeName) const
{
    return SVGPropertyAnimatorFactory::isKnownAttribute(attributeName) || propertyRegistry().isAnimatedStylePropertyAttribute(attributeName);
}

RefPtr<SVGAttributeAnimator> SVGElement::createAnimator(const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
{
    // Property animator, e.g. "fill" or "fill-opacity".
    if (auto animator = propertyAnimatorFactory().createAnimator(attributeName, animationMode, calcMode, isAccumulated, isAdditive))
        return animator;
    
    // Animated property animator.
    auto animator = propertyRegistry().createAnimator(attributeName, animationMode, calcMode, isAccumulated, isAdditive);
    if (!animator)
        return animator;
    for (auto& instance : copyToVectorOf<Ref<SVGElement>>(instances()))
        instance->propertyRegistry().appendAnimatedInstance(attributeName, *animator);
    return animator;
}
    
void SVGElement::animatorWillBeDeleted(const QualifiedName& attributeName)
{
    propertyAnimatorFactory().animatorWillBeDeleted(attributeName);
}

std::optional<Style::ElementStyle> SVGElement::resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle*)
{
    // If the element is in a <use> tree we get the style from the definition tree.
    if (auto styleElement = makeRefPtr(this->correspondingElement())) {
        auto style = styleElement->resolveStyle(&parentStyle);
        Style::Adjuster::adjustSVGElementStyle(*style.renderStyle, *this);
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

QualifiedName SVGElement::animatableAttributeForName(const AtomString& localName)
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
        MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, QualifiedName> map;
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
    if (!parentOrShadowHostElement() || is<SVGElement>(*parentOrShadowHostElement()))
        return StyledElement::rendererIsNeeded(style);

    return false;
}

CSSPropertyID SVGElement::cssPropertyIdForSVGAttributeName(const QualifiedName& attrName)
{
    if (!attrName.namespaceURI().isNull())
        return CSSPropertyInvalid;

    static const auto properties = makeNeverDestroyed(createAttributeNameToCSSPropertyIDMap());
    return properties.get().get(attrName.localName());
}

bool SVGElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (cssPropertyIdForSVGAttributeName(name) > 0)
        return true;
    return StyledElement::hasPresentationalHintsForAttribute(name);
}

void SVGElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    CSSPropertyID propertyID = cssPropertyIdForSVGAttributeName(name);
    if (propertyID > 0)
        addPropertyToPresentationalHintStyle(style, propertyID, value);
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
}

Node::InsertedIntoAncestorResult SVGElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    StyledElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    updateRelativeLengthsInformation();

    if (needsPendingResourceHandling() && insertionType.connectedToDocument && !isInShadowTree()) {
        SVGDocumentExtensions& extensions = document().accessSVGExtensions();
        String resourceId = getIdAttribute();
        if (extensions.isIdOfPendingResource(resourceId))
            return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    }

    return InsertedIntoAncestorResult::Done;
}

void SVGElement::didFinishInsertingNode()
{
    buildPendingResourcesIfNeeded();
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
    while (auto clientElement = extensions.takeElementFromPendingResourcesForRemovalMap(resourceId)) {
        ASSERT(clientElement->hasPendingResources());
        if (clientElement->hasPendingResources()) {
            clientElement->buildPendingResource();
            if (auto renderer = clientElement->renderer()) {
                for (auto& ancestor : ancestorsOfType<RenderSVGResourceContainer>(*renderer))
                    ancestor.markAllClientsForRepaint();
            }
            extensions.clearHasPendingResourcesIfPossible(*clientElement);
        }
    }
}

void SVGElement::childrenChanged(const ChildChange& change)
{
    StyledElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;
    invalidateInstances();
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

void SVGElement::updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement& element)
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
        bool neverRegistered = !m_elementsWithRelativeLengths.contains(element);
        if (neverRegistered)
            return;

        m_elementsWithRelativeLengths.remove(element);
    }

    if (is<SVGGraphicsElement>(element)) {
        if (auto parent = makeRefPtr(parentNode()); is<SVGElement>(parent))
            downcast<SVGElement>(*parent).updateRelativeLengthsInformation(hasRelativeLengths, *this);
    }
}

bool SVGElement::accessKeyAction(bool sendMouseEvents)
{
    return dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

void SVGElement::invalidateInstances()
{
    if (instanceUpdatesBlocked())
        return;

    for (auto& instance : copyToVectorOf<Ref<SVGElement>>(instances())) {
        if (auto useElement = instance->correspondingUseElement())
            useElement->invalidateShadowTree();
        instance->setCorrespondingElement(nullptr);
    }
}

}

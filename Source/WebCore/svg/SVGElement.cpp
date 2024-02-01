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

#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParser.h"
#include "ComputedStyleExtractor.h"
#include "Document.h"
#include "ElementChildIteratorInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "JSEventListener.h"
#include "LegacyRenderSVGResourceContainer.h"
#include "NodeName.h"
#include "RenderAncestorIterator.h"
#include "ResolvedStyle.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementRareData.h"
#include "SVGElementTypeHelpers.h"
#include "SVGForeignObjectElement.h"
#include "SVGGraphicsElement.h"
#include "SVGImageElement.h"
#include "SVGNames.h"
#include "SVGPropertyAnimatorFactory.h"
#include "SVGRenderStyle.h"
#include "SVGRenderSupport.h"
#include "SVGResourceElementClient.h"
#include "SVGSVGElement.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "ShadowRoot.h"
#include "StyleAdjuster.h"
#include "StyleResolver.h"
#include "XMLNames.h"
#include <wtf/HashMap.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGElement);

SVGElement::SVGElement(const QualifiedName& tagName, Document& document, UniqueRef<SVGPropertyRegistry>&& propertyRegistry, OptionSet<TypeFlag> typeFlags)
    : StyledElement(tagName, document, typeFlags | TypeFlag::IsSVGElement | TypeFlag::HasCustomStyleResolveCallbacks)
    , m_propertyAnimatorFactory(makeUnique<SVGPropertyAnimatorFactory>())
    , m_propertyRegistry(WTFMove(propertyRegistry))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<HTMLNames::classAttr, &SVGElement::m_className>();
    });
}

SVGElement::~SVGElement()
{
    if (m_svgRareData) {
        RELEASE_ASSERT(m_svgRareData->referencingElements().isEmptyIgnoringNullReferences());
        for (SVGElement& instance : copyToVectorOf<Ref<SVGElement>>(instances()))
            instance.m_svgRareData->setCorrespondingElement(nullptr);
        RELEASE_ASSERT(!m_svgRareData->correspondingElement());
        m_svgRareData = nullptr;
    }
    document().accessSVGExtensions().removeElementToRebuild(*this);

    if (hasPendingResources()) {
        treeScopeForSVGReferences().removeElementFromPendingSVGResources(*this);
        ASSERT(!hasPendingResources());
    }
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

bool SVGElement::isInnerSVGSVGElement() const
{
    if (!is<SVGSVGElement>(this))
        return false;

    // Element may not be in the document, pretend we're outermost for viewport(), getCTM(), etc.
    if (!parentNode())
        return false;

    return is<SVGElement>(parentNode());
}

bool SVGElement::isOutermostSVGSVGElement() const
{
    if (!is<SVGSVGElement>(this))
        return false;

    // Element may not be in the document, pretend we're outermost for viewport(), getCTM(), etc.
    if (!parentNode())
        return true;

    // We act like an outermost SVG element, if we're a direct child of a <foreignObject> element.
    if (is<SVGForeignObjectElement>(parentNode()))
        return true;

    // If we're inside the shadow tree of a <use> element, we're always an inner <svg> element.
    if (isInShadowTree() && is<SVGUseElement>(shadowHost()))
        return false;

    // This is true whenever this is the outermost SVG, even if there are HTML elements outside it
    return !is<SVGElement>(parentNode());
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
    StyledElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (!parentNode()) {
        m_hasRegisteredWithParentForRelativeLengths = false;
        if (RefPtr oldParent = dynamicDowncast<SVGElement>(oldParentOfRemovedTree))
            oldParent->updateRelativeLengthsInformationForChild(false, *this);
    }

    if (hasPendingResources())
        treeScopeForSVGReferences().removeElementFromPendingSVGResources(*this);

    if (removalType.disconnectedFromDocument) {
        auto& extensions = document().accessSVGExtensions();
        if (m_svgRareData) {
            for (auto& element : m_svgRareData->takeReferencingElements()) {
                extensions.addElementToRebuild(element);
                Ref { element }->clearTarget();
            }
            RELEASE_ASSERT(m_svgRareData->referencingElements().isEmptyIgnoringNullReferences());
        }
        extensions.removeElementToRebuild(*this);
    }
    invalidateInstances();

    if (removalType.treeScopeChanged && oldParentOfRemovedTree.isUserAgentShadowRoot())
        setCorrespondingElement(nullptr);
}

SVGSVGElement* SVGElement::ownerSVGElement() const
{
    auto* node = parentNode();
    while (node) {
        if (auto* svg = dynamicDowncast<SVGSVGElement>(*node))
            return svg;

        node = node->parentOrShadowHostNode();
    }

    return nullptr;
}

SVGElement* SVGElement::viewportElement() const
{
    // This function needs shadow tree support - as RenderSVGContainer uses this function
    // to determine the "overflow" property. <use> on <symbol> wouldn't work otherwhise.
    auto* node = parentNode();
    while (node) {
        if (is<SVGSVGElement>(*node) || is<SVGImageElement>(*node) || node->hasTagName(SVGNames::symbolTag))
            return downcast<SVGElement>(node);

        node = node->parentOrShadowHostNode();
    }

    return nullptr;
}

const WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>& SVGElement::instances() const
{
    if (!m_svgRareData) {
        static NeverDestroyed<WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>> emptyInstances;
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
    rareDataOfReferencingElement.setReferenceTarget(*this);
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
    if (RefPtr destination = m_svgRareData->referenceTarget())
        destination->removeReferencingElement(*this);
}

Vector<WeakPtr<SVGResourceElementClient>> SVGElement::referencingCSSClients() const
{
    if (!m_svgRareData)
        return { };
    return copyToVector(m_svgRareData->referencingCSSClients());
}

void SVGElement::addReferencingCSSClient(SVGResourceElementClient& client)
{
    ensureSVGRareData().addReferencingCSSClient(client);
}

void SVGElement::removeReferencingCSSClient(SVGResourceElementClient& client)
{
    if (!m_svgRareData)
        return;
    ensureSVGRareData().removeReferencingCSSClient(client);
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
    return dynamicDowncast<SVGUseElement>(root->host());
}

void SVGElement::setCorrespondingElement(SVGElement* correspondingElement)
{
    if (m_svgRareData) {
        if (RefPtr oldCorrespondingElement = m_svgRareData->correspondingElement())
            oldCorrespondingElement->m_svgRareData->removeInstance(*this);
    }
    if (m_svgRareData || correspondingElement)
        ensureSVGRareData().setCorrespondingElement(correspondingElement);
    if (correspondingElement)
        correspondingElement->ensureSVGRareData().addInstance(*this);
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
        ASSERT(JSEventListener::wasCreatedFromMarkup(listener));

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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
static inline bool isSVGLayerAwareElement(const SVGElement& element)
{
    using namespace ElementNames;

    switch (element.elementName()) {
    case SVG::a:
    case SVG::altGlyph:
    case SVG::circle:
    case SVG::clipPath:
    case SVG::defs:
    case SVG::ellipse:
    case SVG::foreignObject:
    case SVG::g:
    case SVG::image:
    case SVG::line:
    case SVG::linearGradient:
    case SVG::marker:
    case SVG::mask:
    case SVG::path:
    case SVG::pattern:
    case SVG::polygon:
    case SVG::polyline:
    case SVG::radialGradient:
    case SVG::rect:
    case SVG::stop:
    case SVG::svg:
    case SVG::switch_:
    case SVG::symbol:
    case SVG::text:
    case SVG::textPath:
    case SVG::tref:
    case SVG::tspan:
    case SVG::use:
        return true;
    default:
        break;
    }
    return false;
}
#endif

bool SVGElement::childShouldCreateRenderer(const Node& child) const
{
    auto* svgChild = dynamicDowncast<SVGElement>(child);
    if (!svgChild)
        return false;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // If the layer based SVG engine is enabled, all renderers that do not support the
    // RenderLayer aware layout / painting / hit-testing mode ('LBSE-mode') have to be skipped.
    // FIXME: [LBSE] Upstream support for all elements, and remove 'isSVGLayerAwareElement' check afterwards.
    if (document().settings().layerBasedSVGEngineEnabled() && !isSVGLayerAwareElement(*svgChild))
        return false;
#endif

    switch (svgChild->elementName()) {
    case ElementNames::SVG::altGlyph:
    case ElementNames::SVG::textPath:
    case ElementNames::SVG::tref:
    case ElementNames::SVG::tspan:
        return false;
    default:
        break;
    }
    return svgChild->isValid();
}

void SVGElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    StyledElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    switch (name.nodeName()) {
    case AttributeNames::idAttr:
        document().accessSVGExtensions().rebuildAllElementReferencesForTarget(*this);
        break;
    case AttributeNames::classAttr:
        m_className->setBaseValInternal(newValue);
        break;
    case AttributeNames::tabindexAttr:
        if (newValue.isEmpty())
            setTabIndexExplicitly(std::nullopt);
        else if (auto optionalTabIndex = parseHTMLInteger(newValue))
            setTabIndexExplicitly(optionalTabIndex.value());
        break;
    default:
        if (auto& eventName = HTMLElement::eventNameForEventHandlerAttribute(name); !eventName.isNull())
            setAttributeEventListener(eventName, name, newValue);
        break;
    }

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
        setSynchronizedLazyAttribute(name, AtomString { *value });
}

void SVGElement::synchronizeAllAttributes()
{
    // SVGPropertyRegistry::synchronizeAllAttributes() returns the new values of
    // the properties which have changed but not committed yet.
    auto map = propertyRegistry().synchronizeAllAttributes();
    for (const auto& entry : map)
        setSynchronizedLazyAttribute(entry.key, AtomString { entry.value });
}

void SVGElement::commitPropertyChange(SVGProperty* property)
{
    // We want to dirty the top-level property when a descendant changes. For example
    // a change in an SVGLength item in SVGLengthList should set the dirty flag on
    // SVGLengthList and not the SVGLength.
    property->setDirty();

    setAnimatedSVGAttributesAreDirty();
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
        setSynchronizedLazyAttribute(attributeName, AtomString { animatedProperty.baseValAsString() });

    setAnimatedSVGAttributesAreDirty();
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

std::optional<Style::ResolvedStyle> SVGElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle*)
{
    // If the element is in a <use> tree we get the style from the definition tree.
    if (RefPtr styleElement = this->correspondingElement()) {
        auto styleElementResolutionContext = resolutionContext;
        // Can't use the state since we are going to another part of the tree.
        styleElementResolutionContext.selectorMatchingState = nullptr;
        styleElementResolutionContext.isSVGUseTreeRoot = true;
        auto resolvedStyle = styleElement->resolveStyle(styleElementResolutionContext);
        Style::Adjuster::adjustSVGElementStyle(*resolvedStyle.style, *this);
        return resolvedStyle;
    }

    return resolveStyle(resolutionContext);
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

inline const RenderStyle* SVGElementRareData::overrideComputedStyle(Element& element, const RenderStyle* parentStyle)
{
    if (!m_useOverrideComputedStyle)
        return nullptr;
    if (!m_overrideComputedStyle || m_needsOverrideComputedStyleUpdate) {
        // The style computed here contains no CSS Animations/Transitions or SMIL induced rules - this is needed to compute the "base value" for the SMIL animation sandwhich model.
        m_overrideComputedStyle = element.styleResolver().styleForElement(element, { parentStyle }, RuleMatchingBehavior::MatchAllRulesExcludingSMIL).style;
        m_needsOverrideComputedStyleUpdate = false;
    }
    ASSERT(m_overrideComputedStyle);
    return m_overrideComputedStyle.get();
}

const RenderStyle* SVGElement::computedStyle(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    if (!m_svgRareData || !m_svgRareData->useOverrideComputedStyle())
        return Element::computedStyle(pseudoElementIdentifier);

    const RenderStyle* parentStyle = nullptr;
    if (RefPtr parent = parentOrShadowHostElement()) {
        if (auto renderer = parent->renderer())
            parentStyle = &renderer->style();
    }

    return m_svgRareData->overrideComputedStyle(*this, parentStyle);
}

ColorInterpolation SVGElement::colorInterpolation() const
{
    if (auto renderer = this->renderer())
        return renderer->style().svgStyle().colorInterpolationFilters();

    // Try to determine the property value from the computed style.
    if (auto value = ComputedStyleExtractor(const_cast<SVGElement*>(this)).propertyValue(CSSPropertyColorInterpolationFilters, ComputedStyleExtractor::UpdateLayout::No))
        return fromCSSValue<ColorInterpolation>(*value);

    return ColorInterpolation::Auto;
}

QualifiedName SVGElement::animatableAttributeForName(const AtomString& localName)
{
    static NeverDestroyed animatableAttributes = [] {
        static constexpr std::array names {
            &HTMLNames::classAttr,
            &SVGNames::amplitudeAttr,
            &SVGNames::azimuthAttr,
            &SVGNames::baseFrequencyAttr,
            &SVGNames::biasAttr,
            &SVGNames::clipPathUnitsAttr,
            &SVGNames::cxAttr,
            &SVGNames::cyAttr,
            &SVGNames::diffuseConstantAttr,
            &SVGNames::divisorAttr,
            &SVGNames::dxAttr,
            &SVGNames::dyAttr,
            &SVGNames::edgeModeAttr,
            &SVGNames::elevationAttr,
            &SVGNames::exponentAttr,
            &SVGNames::filterUnitsAttr,
            &SVGNames::fxAttr,
            &SVGNames::fyAttr,
            &SVGNames::gradientTransformAttr,
            &SVGNames::gradientUnitsAttr,
            &SVGNames::heightAttr,
            &SVGNames::in2Attr,
            &SVGNames::inAttr,
            &SVGNames::interceptAttr,
            &SVGNames::k1Attr,
            &SVGNames::k2Attr,
            &SVGNames::k3Attr,
            &SVGNames::k4Attr,
            &SVGNames::kernelMatrixAttr,
            &SVGNames::kernelUnitLengthAttr,
            &SVGNames::lengthAdjustAttr,
            &SVGNames::limitingConeAngleAttr,
            &SVGNames::markerHeightAttr,
            &SVGNames::markerUnitsAttr,
            &SVGNames::markerWidthAttr,
            &SVGNames::maskContentUnitsAttr,
            &SVGNames::maskUnitsAttr,
            &SVGNames::methodAttr,
            &SVGNames::modeAttr,
            &SVGNames::numOctavesAttr,
            &SVGNames::offsetAttr,
            &SVGNames::operatorAttr,
            &SVGNames::orderAttr,
            &SVGNames::orientAttr,
            &SVGNames::pathLengthAttr,
            &SVGNames::patternContentUnitsAttr,
            &SVGNames::patternTransformAttr,
            &SVGNames::patternUnitsAttr,
            &SVGNames::pointsAtXAttr,
            &SVGNames::pointsAtYAttr,
            &SVGNames::pointsAtZAttr,
            &SVGNames::preserveAlphaAttr,
            &SVGNames::preserveAspectRatioAttr,
            &SVGNames::primitiveUnitsAttr,
            &SVGNames::radiusAttr,
            &SVGNames::rAttr,
            &SVGNames::refXAttr,
            &SVGNames::refYAttr,
            &SVGNames::resultAttr,
            &SVGNames::rotateAttr,
            &SVGNames::rxAttr,
            &SVGNames::ryAttr,
            &SVGNames::scaleAttr,
            &SVGNames::seedAttr,
            &SVGNames::slopeAttr,
            &SVGNames::spacingAttr,
            &SVGNames::specularConstantAttr,
            &SVGNames::specularExponentAttr,
            &SVGNames::spreadMethodAttr,
            &SVGNames::startOffsetAttr,
            &SVGNames::stdDeviationAttr,
            &SVGNames::stitchTilesAttr,
            &SVGNames::surfaceScaleAttr,
            &SVGNames::tableValuesAttr,
            &SVGNames::targetAttr,
            &SVGNames::targetXAttr,
            &SVGNames::targetYAttr,
            &SVGNames::transformAttr,
            &SVGNames::typeAttr,
            &SVGNames::valuesAttr,
            &SVGNames::viewBoxAttr,
            &SVGNames::widthAttr,
            &SVGNames::x1Attr,
            &SVGNames::x2Attr,
            &SVGNames::xAttr,
            &SVGNames::xChannelSelectorAttr,
            &SVGNames::y1Attr,
            &SVGNames::y2Attr,
            &SVGNames::yAttr,
            &SVGNames::yChannelSelectorAttr,
            &SVGNames::zAttr,
            &SVGNames::hrefAttr,
        };
        MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, QualifiedName> map;
        for (auto& name : names) {
            auto addResult = map.add(name->get().localName(), *name);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }
        return map;
    }();
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

    switch (attrName.nodeName()) {
    case AttributeNames::alignment_baselineAttr:
        return CSSPropertyAlignmentBaseline;
    case AttributeNames::baseline_shiftAttr:
        return CSSPropertyBaselineShift;
    case AttributeNames::buffered_renderingAttr:
        return CSSPropertyBufferedRendering;
    case AttributeNames::clipAttr:
        return CSSPropertyClip;
    case AttributeNames::clip_pathAttr:
        return CSSPropertyClipPath;
    case AttributeNames::clip_ruleAttr:
        return CSSPropertyClipRule;
    case AttributeNames::colorAttr:
        return CSSPropertyColor;
    case AttributeNames::color_interpolationAttr:
        return CSSPropertyColorInterpolation;
    case AttributeNames::color_interpolation_filtersAttr:
        return CSSPropertyColorInterpolationFilters;
    case AttributeNames::cursorAttr:
        return CSSPropertyCursor;
    case AttributeNames::cxAttr:
        return CSSPropertyCx;
    case AttributeNames::cyAttr:
        return CSSPropertyCy;
    case AttributeNames::directionAttr:
        return CSSPropertyDirection;
    case AttributeNames::displayAttr:
        return CSSPropertyDisplay;
    case AttributeNames::dominant_baselineAttr:
        return CSSPropertyDominantBaseline;
    case AttributeNames::fillAttr:
        return CSSPropertyFill;
    case AttributeNames::fill_opacityAttr:
        return CSSPropertyFillOpacity;
    case AttributeNames::fill_ruleAttr:
        return CSSPropertyFillRule;
    case AttributeNames::filterAttr:
        return CSSPropertyFilter;
    case AttributeNames::flood_colorAttr:
        return CSSPropertyFloodColor;
    case AttributeNames::flood_opacityAttr:
        return CSSPropertyFloodOpacity;
    case AttributeNames::font_familyAttr:
        return CSSPropertyFontFamily;
    case AttributeNames::font_sizeAttr:
        return CSSPropertyFontSize;
    case AttributeNames::font_size_adjustAttr:
        return CSSPropertyFontSizeAdjust;
    case AttributeNames::font_stretchAttr:
        return CSSPropertyFontStretch;
    case AttributeNames::font_styleAttr:
        return CSSPropertyFontStyle;
    case AttributeNames::font_variantAttr:
        return CSSPropertyFontVariant;
    case AttributeNames::font_weightAttr:
        return CSSPropertyFontWeight;
    case AttributeNames::glyph_orientation_horizontalAttr:
        return CSSPropertyGlyphOrientationHorizontal;
    case AttributeNames::glyph_orientation_verticalAttr:
        return CSSPropertyGlyphOrientationVertical;
    case AttributeNames::image_renderingAttr:
        return CSSPropertyImageRendering;
    case AttributeNames::heightAttr:
        return CSSPropertyHeight;
    case AttributeNames::kerningAttr:
        return CSSPropertyKerning;
    case AttributeNames::letter_spacingAttr:
        return CSSPropertyLetterSpacing;
    case AttributeNames::lighting_colorAttr:
        return CSSPropertyLightingColor;
    case AttributeNames::marker_endAttr:
        return CSSPropertyMarkerEnd;
    case AttributeNames::marker_midAttr:
        return CSSPropertyMarkerMid;
    case AttributeNames::marker_startAttr:
        return CSSPropertyMarkerStart;
    case AttributeNames::maskAttr:
        return CSSPropertyMask;
    case AttributeNames::mask_typeAttr:
        return CSSPropertyMaskType;
    case AttributeNames::opacityAttr:
        return CSSPropertyOpacity;
    case AttributeNames::overflowAttr:
        return CSSPropertyOverflow;
    case AttributeNames::paint_orderAttr:
        return CSSPropertyPaintOrder;
    case AttributeNames::pointer_eventsAttr:
        return CSSPropertyPointerEvents;
    case AttributeNames::rAttr:
        return CSSPropertyR;
    case AttributeNames::rxAttr:
        return CSSPropertyRx;
    case AttributeNames::ryAttr:
        return CSSPropertyRy;
    case AttributeNames::shape_renderingAttr:
        return CSSPropertyShapeRendering;
    case AttributeNames::stop_colorAttr:
        return CSSPropertyStopColor;
    case AttributeNames::stop_opacityAttr:
        return CSSPropertyStopOpacity;
    case AttributeNames::strokeAttr:
        return CSSPropertyStroke;
    case AttributeNames::stroke_dasharrayAttr:
        return CSSPropertyStrokeDasharray;
    case AttributeNames::stroke_dashoffsetAttr:
        return CSSPropertyStrokeDashoffset;
    case AttributeNames::stroke_linecapAttr:
        return CSSPropertyStrokeLinecap;
    case AttributeNames::stroke_linejoinAttr:
        return CSSPropertyStrokeLinejoin;
    case AttributeNames::stroke_miterlimitAttr:
        return CSSPropertyStrokeMiterlimit;
    case AttributeNames::stroke_opacityAttr:
        return CSSPropertyStrokeOpacity;
    case AttributeNames::stroke_widthAttr:
        return CSSPropertyStrokeWidth;
    case AttributeNames::text_anchorAttr:
        return CSSPropertyTextAnchor;
    case AttributeNames::text_decorationAttr:
        return CSSPropertyTextDecoration;
    case AttributeNames::text_renderingAttr:
        return CSSPropertyTextRendering;
    case AttributeNames::unicode_bidiAttr:
        return CSSPropertyUnicodeBidi;
    case AttributeNames::vector_effectAttr:
        return CSSPropertyVectorEffect;
    case AttributeNames::visibilityAttr:
        return CSSPropertyVisibility;
    case AttributeNames::widthAttr:
        return CSSPropertyWidth;
    case AttributeNames::word_spacingAttr:
        return CSSPropertyWordSpacing;
    case AttributeNames::writing_modeAttr:
        return CSSPropertyWritingMode;
    case AttributeNames::xAttr:
        return CSSPropertyX;
    case AttributeNames::yAttr:
        return CSSPropertyY;
    case AttributeNames::transform_originAttr:
        return CSSPropertyTransformOrigin;
    default:
        break;
    }

    return CSSPropertyInvalid;
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

void SVGElement::updateSVGRendererForElementChange()
{
    document().updateSVGRenderer(*this);
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
        if (CheckedPtr container = dynamicDowncast<LegacyRenderSVGResourceContainer>(renderer))
            container->idChanged();
        if (isConnected())
            buildPendingResourcesIfNeeded();
        invalidateInstances();
        return;
    }
}

Node::InsertedIntoAncestorResult SVGElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    StyledElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (!m_hasInitializedRelativeLengthsState)
        updateRelativeLengthsInformation();
    else if (RefPtr parentElement = dynamicDowncast<SVGElement>(parentNode()); parentElement && &parentOfInsertedTree == parentNode())
        parentElement->updateRelativeLengthsInformationForChild(hasRelativeLengths(), *this);

    if (needsPendingResourceHandling() && insertionType.connectedToDocument && !isInShadowTree()) {
        if (treeScopeForSVGReferences().isIdOfPendingSVGResource(getIdAttribute()))
            return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    }

    hideNonce();

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

    auto& treeScope = treeScopeForSVGReferences();
    auto resourceId = getIdAttribute();
    if (!treeScope.isIdOfPendingSVGResource(resourceId))
        return;

    treeScope.markPendingSVGResourcesForRemoval(resourceId);

    // Rebuild pending resources for each client of a pending resource that is being removed.
    while (auto clientElement = treeScope.takeElementFromPendingSVGResourcesForRemovalMap(resourceId)) {
        ASSERT(clientElement->hasPendingResources());
        if (clientElement->hasPendingResources()) {
            clientElement->buildPendingResource();
            if (auto renderer = clientElement->renderer()) {
                for (auto& ancestor : ancestorsOfType<LegacyRenderSVGResourceContainer>(*renderer))
                    ancestor.markAllClientsForRepaint();
            }
            treeScope.clearHasPendingSVGResourcesIfPossible(*clientElement);
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

void SVGElement::updateRelativeLengthsInformation()
{
    m_hasInitializedRelativeLengthsState = true;
    m_selfHasRelativeLengths = selfHasRelativeLengths();
    if (RefPtr parent = dynamicDowncast<SVGElement>(parentNode()))
        parent->updateRelativeLengthsInformationForChild(hasRelativeLengths(), *this);
}

void SVGElement::updateRelativeLengthsInformationForChild(bool hasRelativeLengths, SVGElement& element)
{
    ASSERT(element.parentNode() == this || (!hasRelativeLengths && !element.parentNode()));
    bool notifyParent = false;
    if (hasRelativeLengths) {
        m_childElementsWithRelativeLengths.add(element);
        notifyParent = !m_hasRegisteredWithParentForRelativeLengths;
    } else {
        m_childElementsWithRelativeLengths.remove(element);
        notifyParent = m_childElementsWithRelativeLengths.isEmptyIgnoringNullReferences() && m_hasRegisteredWithParentForRelativeLengths;
    }

    if (!notifyParent)
        return;

    if (RefPtr parent = dynamicDowncast<SVGElement>(parentNode())) {
        m_hasRegisteredWithParentForRelativeLengths = hasRelativeLengths;
        parent->updateRelativeLengthsInformationForChild(hasRelativeLengths, *this);
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

SVGConditionalProcessingAttributes& SVGElement::conditionalProcessingAttributes()
{
    return ensureSVGRareData().conditionalProcessingAttributes(*this);
}

SVGConditionalProcessingAttributes* SVGElement::conditionalProcessingAttributesIfExists() const
{
    if (!m_svgRareData)
        return nullptr;
    return m_svgRareData->conditionalProcessingAttributesIfExists();
}

bool SVGElement::hasAssociatedSVGLayoutBox() const
{
    if (!renderer())
        return false;

    // Legacy SVG engine specific condition.
    if (renderer()->isLegacyRenderSVGRoot())
        return false;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // LBSE specific condition.
    if (document().settings().layerBasedSVGEngineEnabled())
        return false;
#endif
    return true;
}

}

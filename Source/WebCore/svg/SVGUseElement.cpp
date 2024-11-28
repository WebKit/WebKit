/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Google Inc. All rights reserved.
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
#include "SVGUseElement.h"

#include "CachedResourceLoader.h"
#include "CachedSVGDocument.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "LegacyRenderSVGResource.h"
#include "LegacyRenderSVGTransformableContainer.h"
#include "NodeName.h"
#include "RenderSVGTransformableContainer.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGElement.h"
#include "SVGSVGElement.h"
#include "SVGSymbolElement.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "XLinkNames.h"
#include <wtf/RobinHoodHashSet.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGUseElement);

inline SVGUseElement::SVGUseElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGURIReference(this)
    , m_loadEventTimer(*this, &SVGElement::loadEventTimerFired)
{
    ASSERT(hasCustomStyleResolveCallbacks());
    ASSERT(hasTagName(SVGNames::useTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGUseElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGUseElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGUseElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGUseElement::m_height>();
    });
}

Ref<SVGUseElement> SVGUseElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGUseElement(tagName, document));
}

SVGUseElement::~SVGUseElement()
{
    if (CachedResourceHandle externalDocument = m_externalDocument)
        externalDocument->removeClient(*this);
}

void SVGUseElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;

    switch (name.nodeName()) {
    case AttributeNames::xAttr:
        Ref { m_x }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::yAttr:
        Ref { m_y }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::widthAttr:
        Ref { m_width }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    case AttributeNames::heightAttr:
        Ref { m_height }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGURIReference::parseAttribute(name, newValue);
    SVGGraphicsElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

Node::InsertedIntoAncestorResult SVGUseElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = SVGGraphicsElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument) {
        if (m_shadowTreeNeedsUpdate)
            protectedDocument()->addElementWithPendingUserAgentShadowTreeUpdate(*this);
        invalidateShadowTree();
        // FIXME: Move back the call to updateExternalDocument() here once notifyFinished is made always async.
        return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    }
    return result;
}

void SVGUseElement::didFinishInsertingNode()
{
    SVGGraphicsElement::didFinishInsertingNode();
    updateExternalDocument();
}

void SVGUseElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    // Check m_shadowTreeNeedsUpdate before calling SVGElement::removedFromAncestor which calls SVGElement::invalidateInstances
    // and SVGUseElement::updateExternalDocument which calls invalidateShadowTree().
    if (removalType.disconnectedFromDocument) {
        if (m_shadowTreeNeedsUpdate) {
            Ref<Document> document = this->document();
            document->removeElementWithPendingUserAgentShadowTreeUpdate(*this);
        }
    }
    SVGGraphicsElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument) {
        clearShadowTree();
        updateExternalDocument();
    }
}

inline Document* SVGUseElement::externalDocument() const
{
    return m_externalDocument ? m_externalDocument->document() : nullptr;
}

void SVGUseElement::transferSizeAttributesToTargetClone(SVGElement& shadowElement) const
{
    // Use |shadowElement| for checking the element type, because we will
    // have replaced a <symbol> with an <svg> in the instance tree.
    if (!is<SVGSymbolElement>(shadowElement) && !is<SVGSVGElement>(shadowElement))
        return;

    // "The width and height properties on the 'use' element override the values
    // for the corresponding properties on a referenced 'svg' or 'symbol' element
    // when determining the used value for that property on the instance root
    // element. However, if the computed value for the property on the 'use'
    // element is auto, then the property is computed as normal for the element
    // instance. ... Because auto is the initial value, if dimensions are not
    // explicitly set on the 'use' element, the values set on the 'svg' or
    // 'symbol' will be used as defaults."
    // https://svgwg.org/svg2-draft/struct.html#UseElement

    RefPtr correspondingElement = shadowElement.correspondingElement();
    shadowElement.setAttribute(SVGNames::widthAttr, width().valueInSpecifiedUnits() ? width().valueAsAtomString() : (correspondingElement ? correspondingElement->getAttribute(SVGNames::widthAttr) : nullAtom()));
    shadowElement.setAttribute(SVGNames::heightAttr, height().valueInSpecifiedUnits() ? height().valueAsAtomString() : (correspondingElement ? correspondingElement->getAttribute(SVGNames::heightAttr) : nullAtom()));
}

void SVGUseElement::svgAttributeChanged(const QualifiedName& attrName)
{
    InstanceInvalidationGuard guard(*this);

    if (PropertyRegistry::isKnownAttribute(attrName)) {
        updateRelativeLengthsInformation();
        if (attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr) {
            // FIXME: It's unnecessarily inefficient to update both width and height each time either is changed.
            if (auto targetClone = this->targetClone())
                transferSizeAttributesToTargetClone(*targetClone);
        }
        updateSVGRendererForElementChange();
        invalidateResourceImageBuffersIfNeeded();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        updateExternalDocument();
        invalidateShadowTree();
        invalidateResourceImageBuffersIfNeeded();
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

static inline bool isDisallowedElement(const SVGElement& element)
{
    using namespace ElementNames;

    // Spec: "Any 'svg', 'symbol', 'g', graphics element or other 'use' is potentially a template object that can be re-used
    // (i.e., "instanced") in the SVG document via a 'use' element."
    // "Graphics Element" is defined as 'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline', 'rect', 'text'
    // Excluded are anything that is used by reference or that only make sense to appear once in a document.
    switch (element.elementName()) {
    case SVG::a:
    case SVG::circle:
    case SVG::desc:
    case SVG::ellipse:
    case SVG::g:
    case SVG::image:
    case SVG::line:
    case SVG::metadata:
    case SVG::path:
    case SVG::polygon:
    case SVG::polyline:
    case SVG::rect:
    case SVG::svg:
    case SVG::switch_:
    case SVG::symbol:
    case SVG::text:
    case SVG::textPath:
    case SVG::title:
    case SVG::tref:
    case SVG::tspan:
    case SVG::use:
        return false;
    default:
        break;
    }
    return true;
}

static inline bool isDisallowedElement(const Element& element)
{
    RefPtr svgElement = dynamicDowncast<SVGElement>(element);
    return !svgElement || isDisallowedElement(*svgElement);
}

void SVGUseElement::clearShadowTree()
{
    if (RefPtr root = userAgentShadowRoot()) {
        // Safe because SVG use element's shadow tree is never used to fire synchronous events during layout or DOM mutations.
        ScriptDisallowedScope::EventAllowedScope scope(*root);
        root->removeChildren();
    }
}

void SVGUseElement::buildPendingResource()
{
    invalidateShadowTree();
}

void SVGUseElement::updateUserAgentShadowTree()
{
    m_shadowTreeNeedsUpdate = false;

    // FIXME: It's expensive to re-clone the entire tree every time. We should find a more efficient way to handle this.
    clearShadowTree();

    if (!isConnected())
        return;
    protectedDocument()->removeElementWithPendingUserAgentShadowTreeUpdate(*this);

    AtomString targetID;
    RefPtr target = findTarget(&targetID);
    if (!target) {
        treeScopeForSVGReferences().addPendingSVGResource(targetID, *this);
        return;
    }

    RELEASE_ASSERT(!isDescendantOf(target.get()));
    {
        Ref shadowRoot = ensureUserAgentShadowRoot();
        ScriptDisallowedScope::EventAllowedScope eventAllowedScope { shadowRoot };
        cloneTarget(shadowRoot, *target);
        expandUseElementsInShadowTree();
        expandSymbolElementsInShadowTree();
        updateRelativeLengthsInformation();
    }

    transferEventListenersToShadowTree();

    // When we invalidate the other shadow trees, it's important that we don't
    // follow any cycles and invalidate ourselves. To avoid that, we temporarily
    // set m_shadowTreeNeedsUpdate to true so invalidateShadowTree will
    // quickly return and do nothing.
    ASSERT(!m_shadowTreeNeedsUpdate);
    m_shadowTreeNeedsUpdate = true;
    invalidateDependentShadowTrees();
    m_shadowTreeNeedsUpdate = false;
}

RefPtr<SVGElement> SVGUseElement::targetClone() const
{
    RefPtr root = userAgentShadowRoot();
    return root ? childrenOfType<SVGElement>(*root).first() : nullptr;
}

RenderPtr<RenderElement> SVGUseElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGTransformableContainer>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGTransformableContainer>(*this, WTFMove(style));
}

static bool isDirectReference(const SVGElement& element)
{
    using namespace SVGNames;
    return element.hasTagName(circleTag)
        || element.hasTagName(ellipseTag)
        || element.hasTagName(pathTag)
        || element.hasTagName(polygonTag)
        || element.hasTagName(polylineTag)
        || element.hasTagName(rectTag)
        || element.hasTagName(textTag);
}

Path SVGUseElement::toClipPath()
{
    RELEASE_ASSERT(!document().settings().layerBasedSVGEngineEnabled());

    RefPtr targetClone = dynamicDowncast<SVGGraphicsElement>(this->targetClone());
    if (!targetClone)
        return { };

    if (!isDirectReference(*targetClone)) {
        // Spec: Indirect references are an error (14.3.5)
        protectedDocument()->checkedSVGExtensions()->reportError("Not allowed to use indirect reference in <clip-path>"_s);
        return { };
    }

    Path path = targetClone->toClipPath();
    SVGLengthContext lengthContext(this);
    // FIXME: Find a way to do this without manual resolution of x/y here. It's potentially incorrect.
    path.translate(FloatSize(x().value(lengthContext), y().value(lengthContext)));
    path.transform(animatedLocalTransform());
    return path;
}

RefPtr<SVGElement> SVGUseElement::clipChild() const
{
    RefPtr targetClone = this->targetClone();
    if (!targetClone || !isDirectReference(*targetClone))
        return nullptr;
    return targetClone;
}

RenderElement* SVGUseElement::rendererClipChild() const
{
    RefPtr targetClone = this->targetClone();
    if (!targetClone)
        return nullptr;
    if (!isDirectReference(*targetClone))
        return nullptr;
    return targetClone->renderer();
}

static inline void disassociateAndRemoveClones(const Vector<Ref<Element>>& clones)
{
    for (Ref clone : clones) {
        for (Ref descendant : descendantsOfType<SVGElement>(clone.get()))
            descendant->setCorrespondingElement(nullptr);
        if (RefPtr svgClone = dynamicDowncast<SVGElement>(clone.get()))
            svgClone->setCorrespondingElement(nullptr);
        clone->remove();
    }
}

static void removeDisallowedElementsFromSubtree(SVGElement& subtree)
{
    // Remove disallowed elements after the fact rather than not cloning them in the first place.
    // This optimizes for the normal case where none of those elements are present.

    // This function is used only on elements in subtrees that are not yet in documents, so
    // mutation events are not a factor; there are no event listeners to handle those events.
    // Assert that it's not in a document to make sure callers are still using it this way.
    ASSERT(!subtree.isConnected());

    Vector<Ref<Element>> disallowedElements;
    for (auto it = descendantsOfType<Element>(subtree).begin(); it; ) {
        if (isDisallowedElement(*it)) {
            disallowedElements.append(*it);
            it.traverseNextSkippingChildren();
            continue;
        }
        ++it;
    }

    disassociateAndRemoveClones(disallowedElements);
}

static void removeSymbolElementsFromSubtree(SVGElement& subtree)
{
    // Symbol elements inside the subtree should not be cloned for two reasons: 1) They are invisible and
    // don't need to be cloned to get correct rendering. 2) expandSymbolElementsInShadowTree will turn them
    // into <svg> elements, which is correct for symbol elements directly referenced by use elements,
    // but incorrect for ones that just happen to be in a subtree.
    Vector<Ref<Element>> symbolElements;
    for (auto it = descendantsOfType<Element>(subtree).begin(); it; ) {
        if (is<SVGSymbolElement>(*it)) {
            symbolElements.append(*it);
            it.traverseNextSkippingChildren();
            continue;
        }
        ++it;
    }
    disassociateAndRemoveClones(symbolElements);
}

static void associateClonesWithOriginals(SVGElement& clone, SVGElement& original)
{
    // This assertion checks that we don't call this with the arguments backwards.
    // The clone is new and so it's not installed in a parent yet.
    ASSERT(!clone.parentNode());

    // The loop below works because we are associating these clones immediately, before
    // doing transformations like removing disallowed elements or expanding elements.
    clone.setCorrespondingElement(&original);
    for (auto pair : descendantsOfType<SVGElement>(clone, original))
        pair.first.setCorrespondingElement(Ref { pair.second }.ptr());
}

static void associateReplacementCloneWithOriginal(SVGElement& replacementClone, SVGElement& originalClone)
{
    RefPtr correspondingElement = originalClone.correspondingElement();
    ASSERT(correspondingElement);
    originalClone.setCorrespondingElement(nullptr);
    replacementClone.setCorrespondingElement(correspondingElement.get());
}

static void associateReplacementClonesWithOriginals(SVGElement& replacementClone, SVGElement& originalClone)
{
    // This assertion checks that we don't call this with the arguments backwards.
    // The replacement clone is new and so it's not installed in a parent yet.
    ASSERT(!replacementClone.parentNode());

    // The loop below works because we are associating these clones immediately, before
    // doing transformations like removing disallowed elements or expanding elements.
    associateReplacementCloneWithOriginal(replacementClone, originalClone);
    for (auto pair : descendantsOfType<SVGElement>(replacementClone, originalClone))
        associateReplacementCloneWithOriginal(Ref { pair.first }, Ref { pair.second });
}

RefPtr<SVGElement> SVGUseElement::findTarget(AtomString* targetID) const
{
    RefPtr correspondingElement = this->correspondingElement();
    Ref original = correspondingElement ? downcast<SVGUseElement>(*correspondingElement) : *this;

    auto targetResult = targetElementFromIRIString(original->href(), original->treeScope(), original->externalDocument());
    if (targetID) {
        *targetID = WTFMove(targetResult.identifier);
        // If the reference is external, don't return the target ID to the caller.
        // The caller would use the target ID to wait for a pending resource on the wrong document.
        // If we ever want the change that and let the caller to wait on the external document,
        // we should change this function so it returns the appropriate document to go with the ID.
        if (!targetID->isNull() && isExternalURIReference(original->href(), original->protectedDocument()))
            *targetID = nullAtom();
    }
    RefPtr target = dynamicDowncast<SVGElement>(targetResult.element.get());
    if (!target)
        return nullptr;

    if (!target->isConnected() || isDisallowedElement(*target))
        return nullptr;

    if (correspondingElement) {
        for (auto& ancestor : lineageOfType<SVGElement>(*this)) {
            if (ancestor.correspondingElement() == target)
                return nullptr;
        }
    } else {
        if (target->contains(this))
            return nullptr;
        // Target should only refer to a node in the same tree or a node in another document.
        ASSERT(!isDescendantOrShadowDescendantOf(target.get()));
    }

    return target;
}

void SVGUseElement::cloneTarget(ContainerNode& container, SVGElement& target) const
{
    Ref targetClone = static_cast<SVGElement&>(target.cloneElementWithChildren(protectedDocument()).get());
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { targetClone };
    associateClonesWithOriginals(targetClone.get(), target);
    removeDisallowedElementsFromSubtree(targetClone.get());
    removeSymbolElementsFromSubtree(targetClone.get());
    transferSizeAttributesToTargetClone(targetClone.get());
    container.appendChild(targetClone);
}

static void cloneDataAndChildren(SVGElement& replacementClone, SVGElement& originalClone)
{
    // This assertion checks that we don't call this with the arguments backwards.
    // The replacement clone is new and so it's not installed in a parent yet.
    ASSERT(!replacementClone.parentNode());

    replacementClone.cloneDataFromElement(originalClone);
    originalClone.cloneChildNodes(replacementClone);
    associateReplacementClonesWithOriginals(replacementClone, originalClone);
    removeDisallowedElementsFromSubtree(replacementClone);
}

void SVGUseElement::expandUseElementsInShadowTree() const
{
    auto descendants = descendantsOfType<SVGUseElement>(*protectedUserAgentShadowRoot());
    for (auto it = descendants.begin(); it; ) {
        Ref originalClone = *it;
        it.dropAssertions();

        RefPtr target = originalClone->findTarget();

        // Spec: In the generated content, the 'use' will be replaced by 'g', where all attributes from the
        // 'use' element except for x, y, width, height and xlink:href are transferred to the generated 'g' element.

        Ref replacementClone = SVGGElement::create(protectedDocument());
        ScriptDisallowedScope::EventAllowedScope eventAllowedScope { replacementClone };

        cloneDataAndChildren(replacementClone.get(), originalClone);

        replacementClone->removeAttribute(SVGNames::xAttr);
        replacementClone->removeAttribute(SVGNames::yAttr);
        replacementClone->removeAttribute(SVGNames::widthAttr);
        replacementClone->removeAttribute(SVGNames::heightAttr);
        replacementClone->removeAttribute(SVGNames::hrefAttr);
        replacementClone->removeAttribute(XLinkNames::hrefAttr);

        if (target)
            originalClone->cloneTarget(replacementClone.get(), *target);

        originalClone->protectedParentNode()->replaceChild(replacementClone, originalClone);

        // Resume iterating, starting just inside the replacement clone.
        it = descendants.from(replacementClone.get());
    }
}

void SVGUseElement::expandSymbolElementsInShadowTree() const
{
    auto descendants = descendantsOfType<SVGSymbolElement>(*protectedUserAgentShadowRoot());
    for (auto it = descendants.begin(); it; ) {
        Ref originalClone = *it;
        it.dropAssertions();

        // Spec: The referenced 'symbol' and its contents are deep-cloned into the generated tree,
        // with the exception that the 'symbol' is replaced by an 'svg'. This generated 'svg' will
        // always have explicit values for attributes width and height. If attributes width and/or
        // height are provided on the 'use' element, then these attributes will be transferred to
        // the generated 'svg'. If attributes width and/or height are not specified, the generated
        // 'svg' element will use values of 100% for these attributes.

        Ref replacementClone = SVGSVGElement::create(protectedDocument());
        ScriptDisallowedScope::EventAllowedScope eventAllowedScope { replacementClone };

        cloneDataAndChildren(replacementClone.get(), originalClone);

        originalClone->protectedParentNode()->replaceChild(replacementClone, originalClone);

        // Resume iterating, starting just inside the replacement clone.
        it = descendants.from(replacementClone.get());
    }
}

void SVGUseElement::transferEventListenersToShadowTree() const
{
    // FIXME: Don't directly add event listeners on each descendant. Copy event listeners on the use element instead.
    for (Ref descendant : descendantsOfType<SVGElement>(*protectedUserAgentShadowRoot())) {
        if (EventTargetData* data = descendant->correspondingElement()->eventTargetData())
            data->eventListenerMap.copyEventListenersNotCreatedFromMarkupToTarget(descendant.ptr());
    }
}

void SVGUseElement::invalidateShadowTree()
{
    if (m_shadowTreeNeedsUpdate)
        return;
    m_shadowTreeNeedsUpdate = true;
    invalidateStyleAndRenderersForSubtree();
    invalidateDependentShadowTrees();
    if (isConnected()) {
        Ref<Document> document = this->document();
        document->addElementWithPendingUserAgentShadowTreeUpdate(*this);
    }
}

void SVGUseElement::invalidateDependentShadowTrees()
{
    for (auto& instance : copyToVectorOf<Ref<SVGElement>>(instances())) {
        if (RefPtr element = instance->correspondingUseElement())
            element->invalidateShadowTree();
    }
}

bool SVGUseElement::selfHasRelativeLengths() const
{
    if (x().isRelative() || y().isRelative() || width().isRelative() || height().isRelative())
        return true;

    RefPtr targetClone = this->targetClone();
    return targetClone && targetClone->hasRelativeLengths();
}

void SVGUseElement::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    ASSERT(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    invalidateShadowTree();
    if (resource.errorOccurred()) {
        setErrorOccurred(true);
        dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
    } else if (!resource.wasCanceled())
        SVGURIReference::dispatchLoadEvent();
}

void SVGUseElement::updateExternalDocument()
{
    URL externalDocumentURL;
    Ref<Document> document = this->document();
    // FIXME: This early exit should be removed once the ASSERT(!url.protocolIsData()) is removed from isExternalURIReference().
    if (document->completeURL(href()).protocolIsData())
        return;
    if (isConnected() && isExternalURIReference(href(), document)) {
        externalDocumentURL = document->completeURL(href());
        if (!externalDocumentURL.hasFragmentIdentifier())
            externalDocumentURL = URL();
    }

    if (externalDocumentURL == (m_externalDocument ? m_externalDocument->url() : URL()))
        return;

    if (CachedResourceHandle externalDocument = m_externalDocument)
        externalDocument->removeClient(*this);

    if (externalDocumentURL.isNull())
        m_externalDocument = nullptr;
    else {
        ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
        options.contentSecurityPolicyImposition = isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
        options.mode = FetchOptions::Mode::SameOrigin;
        options.destination = FetchOptions::Destination::Image;
        options.sniffContent = ContentSniffingPolicy::DoNotSniffContent;
        CachedResourceRequest request { ResourceRequest { externalDocumentURL }, options };
        request.setInitiator(*this);
        m_externalDocument = document->protectedCachedResourceLoader()->requestSVGDocument(WTFMove(request)).value_or(nullptr);
        if (CachedResourceHandle externalDocument = m_externalDocument)
            externalDocument->addClient(*this);
    }

    invalidateShadowTree();
}

}

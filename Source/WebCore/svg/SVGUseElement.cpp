/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "ElementIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "RenderSVGResource.h"
#include "RenderSVGTransformableContainer.h"
#include "SVGDocumentExtensions.h"
#include "SVGGElement.h"
#include "SVGSVGElement.h"
#include "SVGSymbolElement.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "XLinkNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGUseElement);

inline SVGUseElement::SVGUseElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , SVGExternalResourcesRequired(this)
    , SVGURIReference(this)
    , m_svgLoadEventTimer(*this, &SVGElement::svgLoadEventTimerFired)
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
    if (m_externalDocument)
        m_externalDocument->removeClient(*this);
}

void SVGUseElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        m_x->setBaseValInternal(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        m_y->setBaseValInternal(SVGLengthValue::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::widthAttr)
        m_width->setBaseValInternal(SVGLengthValue::construct(LengthModeWidth, value, parseError, ForbidNegativeLengths));
    else if (name == SVGNames::heightAttr)
        m_height->setBaseValInternal(SVGLengthValue::construct(LengthModeHeight, value, parseError, ForbidNegativeLengths));

    reportAttributeParsingError(parseError, name, value);

    SVGExternalResourcesRequired::parseAttribute(name, value);
    SVGGraphicsElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
}

Node::InsertedIntoAncestorResult SVGUseElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGGraphicsElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument) {
        if (m_shadowTreeNeedsUpdate)
            document().addSVGUseElement(*this);
        SVGExternalResourcesRequired::insertedIntoDocument();
        invalidateShadowTree();
        // FIXME: Move back the call to updateExternalDocument() here once notifyFinished is made always async.
        return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    }
    return InsertedIntoAncestorResult::Done;
}

void SVGUseElement::didFinishInsertingNode()
{
    updateExternalDocument();
}

void SVGUseElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    // Check m_shadowTreeNeedsUpdate before calling SVGElement::removedFromAncestor which calls SVGElement::invalidateInstances
    // and SVGUseElement::updateExternalDocument which calls invalidateShadowTree().
    if (removalType.disconnectedFromDocument) {
        if (m_shadowTreeNeedsUpdate)
            document().removeSVGUseElement(*this);
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
    // FIXME: The check for valueInSpecifiedUnits being non-zero below is a workaround for the fact
    // that we currently have no good way to tell whether a particular animatable attribute is a value
    // indicating it was unspecified, or specified but could not be parsed. Would be nice to fix that some day.
    if (is<SVGSymbolElement>(shadowElement)) {
        // Spec (<use> on <symbol>): This generated 'svg' will always have explicit values for attributes width and height.
        // If attributes width and/or height are provided on the 'use' element, then these attributes
        // will be transferred to the generated 'svg'. If attributes width and/or height are not specified,
        // the generated 'svg' element will use values of 100% for these attributes.
        shadowElement.setAttribute(SVGNames::widthAttr, width().valueInSpecifiedUnits() ? AtomString(width().valueAsString()) : "100%");
        shadowElement.setAttribute(SVGNames::heightAttr, height().valueInSpecifiedUnits() ? AtomString(height().valueAsString()) : "100%");
    } else if (is<SVGSVGElement>(shadowElement)) {
        // Spec (<use> on <svg>): If attributes width and/or height are provided on the 'use' element, then these
        // values will override the corresponding attributes on the 'svg' in the generated tree.
        auto correspondingElement = makeRefPtr(shadowElement.correspondingElement());
        shadowElement.setAttribute(SVGNames::widthAttr, width().valueInSpecifiedUnits() ? AtomString(width().valueAsString()) : (correspondingElement ? correspondingElement->getAttribute(SVGNames::widthAttr) : nullAtom()));
        shadowElement.setAttribute(SVGNames::heightAttr, height().valueInSpecifiedUnits() ? AtomString(height().valueAsString()) : (correspondingElement ? correspondingElement->getAttribute(SVGNames::heightAttr) : nullAtom()));
    }
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
        if (auto* renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        updateExternalDocument();
        invalidateShadowTree();
        return;
    }

    if (SVGLangSpace::isKnownAttribute(attrName) || SVGExternalResourcesRequired::isKnownAttribute(attrName))
        invalidateShadowTree();

    SVGGraphicsElement::svgAttributeChanged(attrName);
    SVGExternalResourcesRequired::svgAttributeChanged(attrName);
}

static HashSet<AtomString> createAllowedElementSet()
{
    // Spec: "Any 'svg', 'symbol', 'g', graphics element or other 'use' is potentially a template object that can be re-used
    // (i.e., "instanced") in the SVG document via a 'use' element."
    // "Graphics Element" is defined as 'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline', 'rect', 'text'
    // Excluded are anything that is used by reference or that only make sense to appear once in a document.
    using namespace SVGNames;
    HashSet<AtomString> set;
    for (auto& tag : { aTag.get(), circleTag.get(), descTag.get(), ellipseTag.get(), gTag.get(), imageTag.get(), lineTag.get(), metadataTag.get(), pathTag.get(), polygonTag.get(), polylineTag.get(), rectTag.get(), svgTag.get(), switchTag.get(), symbolTag.get(), textTag.get(), textPathTag.get(), titleTag.get(), trefTag.get(), tspanTag.get(), useTag.get() })
        set.add(tag.localName());
    return set;
}

static inline bool isDisallowedElement(const SVGElement& element)
{
    static NeverDestroyed<HashSet<AtomString>> set = createAllowedElementSet();
    return !set.get().contains(element.localName());
}

static inline bool isDisallowedElement(const Element& element)
{
    return !element.isSVGElement() || isDisallowedElement(downcast<SVGElement>(element));
}

void SVGUseElement::clearShadowTree()
{
    if (auto root = userAgentShadowRoot()) {
        // Safe because SVG use element's shadow tree is never used to fire synchronous events during layout or DOM mutations.
        ScriptDisallowedScope::EventAllowedScope scope(*root);
        root->removeChildren();
    }
}

void SVGUseElement::buildPendingResource()
{
    invalidateShadowTree();
}

void SVGUseElement::updateShadowTree()
{
    m_shadowTreeNeedsUpdate = false;

    // FIXME: It's expensive to re-clone the entire tree every time. We should find a more efficient way to handle this.
    clearShadowTree();

    if (!isConnected())
        return;
    document().removeSVGUseElement(*this);

    String targetID;
    auto* target = findTarget(&targetID);
    if (!target) {
        document().accessSVGExtensions().addPendingResource(targetID, *this);
        return;
    }

    RELEASE_ASSERT(!isDescendantOf(target));
    {
        auto& shadowRoot = ensureUserAgentShadowRoot();
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
    auto root = userAgentShadowRoot();
    if (!root)
        return nullptr;
    return childrenOfType<SVGElement>(*root).first();
}

RenderPtr<RenderElement> SVGUseElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGTransformableContainer>(*this, WTFMove(style));
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
    auto targetClone = this->targetClone();
    if (!is<SVGGraphicsElement>(targetClone))
        return { };

    if (!isDirectReference(*targetClone)) {
        // Spec: Indirect references are an error (14.3.5)
        document().accessSVGExtensions().reportError("Not allowed to use indirect reference in <clip-path>"_s);
        return { };
    }

    Path path = downcast<SVGGraphicsElement>(*targetClone).toClipPath();
    SVGLengthContext lengthContext(this);
    // FIXME: Find a way to do this without manual resolution of x/y here. It's potentially incorrect.
    path.translate(FloatSize(x().value(lengthContext), y().value(lengthContext)));
    path.transform(animatedLocalTransform());
    return path;
}

RenderElement* SVGUseElement::rendererClipChild() const
{
    auto targetClone = this->targetClone();
    if (!targetClone)
        return nullptr;
    if (!isDirectReference(*targetClone))
        return nullptr;
    return targetClone->renderer();
}

static inline void disassociateAndRemoveClones(const Vector<Element*>& clones)
{
    for (auto& clone : clones) {
        for (auto& descendant : descendantsOfType<SVGElement>(*clone))
            descendant.setCorrespondingElement(nullptr);
        if (is<SVGElement>(clone))
            downcast<SVGElement>(*clone).setCorrespondingElement(nullptr);
        clone->parentNode()->removeChild(*clone);
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

    Vector<Element*> disallowedElements;
    auto descendants = descendantsOfType<Element>(subtree);
    for (auto it = descendants.begin(), end = descendants.end(); it != end; ) {
        if (isDisallowedElement(*it)) {
            disallowedElements.append(&*it);
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
    Vector<Element*> symbolElements;
    for (auto& descendant : descendantsOfType<SVGSymbolElement>(subtree))
        symbolElements.append(&descendant);
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
        pair.first.setCorrespondingElement(&pair.second);
}

static void associateReplacementCloneWithOriginal(SVGElement& replacementClone, SVGElement& originalClone)
{
    auto* correspondingElement = originalClone.correspondingElement();
    ASSERT(correspondingElement);
    originalClone.setCorrespondingElement(nullptr);
    replacementClone.setCorrespondingElement(correspondingElement);
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
        associateReplacementCloneWithOriginal(pair.first, pair.second);
}

SVGElement* SVGUseElement::findTarget(String* targetID) const
{
    auto* correspondingElement = this->correspondingElement();
    auto& original = correspondingElement ? downcast<SVGUseElement>(*correspondingElement) : *this;

    auto targetResult = targetElementFromIRIString(original.href(), original.treeScope(), original.externalDocument());
    if (targetID) {
        *targetID = WTFMove(targetResult.identifier);
        // If the reference is external, don't return the target ID to the caller.
        // The caller would use the target ID to wait for a pending resource on the wrong document.
        // If we ever want the change that and let the caller to wait on the external document,
        // we should change this function so it returns the appropriate document to go with the ID.
        if (!targetID->isNull() && isExternalURIReference(original.href(), original.document()))
            *targetID = String { };
    }
    if (!is<SVGElement>(targetResult.element))
        return nullptr;
    auto& target = downcast<SVGElement>(*targetResult.element);

    if (!target.isConnected() || isDisallowedElement(target))
        return nullptr;

    if (correspondingElement) {
        for (auto& ancestor : lineageOfType<SVGElement>(*this)) {
            if (ancestor.correspondingElement() == &target)
                return nullptr;
        }
    } else {
        if (target.contains(this))
            return nullptr;
        // Target should only refer to a node in the same tree or a node in another document.
        ASSERT(!isDescendantOrShadowDescendantOf(&target));
    }

    return &target;
}

void SVGUseElement::cloneTarget(ContainerNode& container, SVGElement& target) const
{
    Ref<SVGElement> targetClone = static_cast<SVGElement&>(target.cloneElementWithChildren(document()).get());
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
    auto descendants = descendantsOfType<SVGUseElement>(*userAgentShadowRoot());
    for (auto it = descendants.begin(), end = descendants.end(); it != end; ) {
        SVGUseElement& originalClone = *it;
        it = end; // Efficiently quiets assertions due to the outstanding iterator.

        auto* target = originalClone.findTarget();

        // Spec: In the generated content, the 'use' will be replaced by 'g', where all attributes from the
        // 'use' element except for x, y, width, height and xlink:href are transferred to the generated 'g' element.

        auto replacementClone = SVGGElement::create(document());

        cloneDataAndChildren(replacementClone.get(), originalClone);

        replacementClone->removeAttribute(SVGNames::xAttr);
        replacementClone->removeAttribute(SVGNames::yAttr);
        replacementClone->removeAttribute(SVGNames::widthAttr);
        replacementClone->removeAttribute(SVGNames::heightAttr);
        replacementClone->removeAttribute(SVGNames::hrefAttr);
        replacementClone->removeAttribute(XLinkNames::hrefAttr);

        if (target)
            originalClone.cloneTarget(replacementClone.get(), *target);

        originalClone.parentNode()->replaceChild(replacementClone, originalClone);

        // Resume iterating, starting just inside the replacement clone.
        it = descendants.from(replacementClone.get());
    }
}

void SVGUseElement::expandSymbolElementsInShadowTree() const
{
    auto descendants = descendantsOfType<SVGSymbolElement>(*userAgentShadowRoot());
    for (auto it = descendants.begin(), end = descendants.end(); it != end; ) {
        SVGSymbolElement& originalClone = *it;
        it = end; // Efficiently quiets assertions due to the outstanding iterator.

        // Spec: The referenced 'symbol' and its contents are deep-cloned into the generated tree,
        // with the exception that the 'symbol' is replaced by an 'svg'. This generated 'svg' will
        // always have explicit values for attributes width and height. If attributes width and/or
        // height are provided on the 'use' element, then these attributes will be transferred to
        // the generated 'svg'. If attributes width and/or height are not specified, the generated
        // 'svg' element will use values of 100% for these attributes.

        auto replacementClone = SVGSVGElement::create(document());
        cloneDataAndChildren(replacementClone.get(), originalClone);

        originalClone.parentNode()->replaceChild(replacementClone, originalClone);

        // Resume iterating, starting just inside the replacement clone.
        it = descendants.from(replacementClone.get());
    }
}

void SVGUseElement::transferEventListenersToShadowTree() const
{
    // FIXME: Don't directly add event listeners on each descendant. Copy event listeners on the use element instead.
    for (auto& descendant : descendantsOfType<SVGElement>(*userAgentShadowRoot())) {
        if (EventTargetData* data = descendant.correspondingElement()->eventTargetData())
            data->eventListenerMap.copyEventListenersNotCreatedFromMarkupToTarget(&descendant);
    }
}

void SVGUseElement::invalidateShadowTree()
{
    if (m_shadowTreeNeedsUpdate)
        return;
    m_shadowTreeNeedsUpdate = true;
    invalidateStyleAndRenderersForSubtree();
    invalidateDependentShadowTrees();
    if (isConnected())
        document().addSVGUseElement(*this);
}

void SVGUseElement::invalidateDependentShadowTrees()
{
    for (auto* instance : instances()) {
        if (auto element = instance->correspondingUseElement())
            element->invalidateShadowTree();
    }
}

bool SVGUseElement::selfHasRelativeLengths() const
{
    if (x().isRelative() || y().isRelative() || width().isRelative() || height().isRelative())
        return true;

    auto targetClone = this->targetClone();
    return targetClone && targetClone->hasRelativeLengths();
}

void SVGUseElement::notifyFinished(CachedResource& resource)
{
    ASSERT(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    invalidateShadowTree();
    if (resource.errorOccurred())
        dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
    else if (!resource.wasCanceled())
        SVGExternalResourcesRequired::dispatchLoadEvent();
}

void SVGUseElement::finishParsingChildren()
{
    SVGGraphicsElement::finishParsingChildren();
    SVGExternalResourcesRequired::finishParsingChildren();
}

void SVGUseElement::updateExternalDocument()
{
    URL externalDocumentURL;
    if (isConnected() && isExternalURIReference(href(), document())) {
        externalDocumentURL = document().completeURL(href());
        if (!externalDocumentURL.hasFragmentIdentifier())
            externalDocumentURL = URL();
    }

    if (externalDocumentURL == (m_externalDocument ? m_externalDocument->url() : URL()))
        return;

    if (m_externalDocument)
        m_externalDocument->removeClient(*this);

    if (externalDocumentURL.isNull())
        m_externalDocument = nullptr;
    else {
        ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
        options.contentSecurityPolicyImposition = isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
        options.mode = FetchOptions::Mode::SameOrigin;
        CachedResourceRequest request { ResourceRequest { externalDocumentURL }, options };
        request.setInitiator(*this);
        m_externalDocument = document().cachedResourceLoader().requestSVGDocument(WTFMove(request)).value_or(nullptr);
        if (m_externalDocument)
            m_externalDocument->addClient(*this);
    }

    invalidateShadowTree();
}

bool SVGUseElement::isValid() const
{
    return SVGTests::isValid();
}

bool SVGUseElement::haveLoadedRequiredResources()
{
    return SVGExternalResourcesRequired::haveLoadedRequiredResources();
}

void SVGUseElement::setHaveFiredLoadEvent(bool haveFiredLoadEvent)
{
    m_haveFiredLoadEvent = haveFiredLoadEvent;
}

bool SVGUseElement::haveFiredLoadEvent() const
{
    return m_haveFiredLoadEvent;
}

Timer* SVGUseElement::svgLoadEventTimer()
{
    return &m_svgLoadEventTimer;
}

}

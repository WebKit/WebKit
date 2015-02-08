/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "CachedResourceRequest.h"
#include "CachedSVGDocument.h"
#include "Document.h"
#include "ElementIterator.h"
#include "Event.h"
#include "EventListener.h"
#include "HTMLNames.h"
#include "NodeRenderStyle.h"
#include "RegisteredEventListener.h"
#include "RenderSVGResource.h"
#include "RenderSVGTransformableContainer.h"
#include "ShadowRoot.h"
#include "SVGElementInstance.h"
#include "SVGElementRareData.h"
#include "SVGGElement.h"
#include "SVGLengthContext.h"
#include "SVGNames.h"
#include "SVGSMILElement.h"
#include "SVGSVGElement.h"
#include "SVGSymbolElement.h"
#include "StyleResolver.h"
#include "XLinkNames.h"
#include "XMLDocumentParser.h"
#include "XMLSerializer.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGUseElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGUseElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_LENGTH(SVGUseElement, SVGNames::widthAttr, Width, width)
DEFINE_ANIMATED_LENGTH(SVGUseElement, SVGNames::heightAttr, Height, height)
DEFINE_ANIMATED_STRING(SVGUseElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGUseElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGUseElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(width)
    REGISTER_LOCAL_ANIMATED_PROPERTY(height)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGUseElement::SVGUseElement(const QualifiedName& tagName, Document& document, bool wasInsertedByParser)
    : SVGGraphicsElement(tagName, document)
    , m_x(LengthModeWidth)
    , m_y(LengthModeHeight)
    , m_width(LengthModeWidth)
    , m_height(LengthModeHeight)
    , m_wasInsertedByParser(wasInsertedByParser)
    , m_haveFiredLoadEvent(false)
    , m_needsShadowTreeRecreation(false)
    , m_svgLoadEventTimer(*this, &SVGElement::svgLoadEventTimerFired)
{
    ASSERT(hasCustomStyleResolveCallbacks());
    ASSERT(hasTagName(SVGNames::useTag));
    registerAnimatedPropertiesForSVGUseElement();
}

Ref<SVGUseElement> SVGUseElement::create(const QualifiedName& tagName, Document& document, bool wasInsertedByParser)
{
    // Always build a #shadow-root for SVGUseElement.
    Ref<SVGUseElement> use = adoptRef(*new SVGUseElement(tagName, document, wasInsertedByParser));
    use->ensureUserAgentShadowRoot();
    return use;
}

SVGUseElement::~SVGUseElement()
{
    setCachedDocument(0);

    clearResourceReferences();
}

bool SVGUseElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed<HashSet<QualifiedName>> supportedAttributes;
    if (supportedAttributes.get().isEmpty()) {
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.get().add(SVGNames::xAttr);
        supportedAttributes.get().add(SVGNames::yAttr);
        supportedAttributes.get().add(SVGNames::widthAttr);
        supportedAttributes.get().add(SVGNames::heightAttr);
    }
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGUseElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGGraphicsElement::parseAttribute(name, value);
    else if (name == SVGNames::xAttr)
        setXBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        setYBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength::construct(LengthModeWidth, value, parseError, ForbidNegativeLengths));
    else if (name == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength::construct(LengthModeHeight, value, parseError, ForbidNegativeLengths));
    else if (SVGLangSpace::parseAttribute(name, value)
             || SVGExternalResourcesRequired::parseAttribute(name, value)
             || SVGURIReference::parseAttribute(name, value)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

#if !ASSERT_DISABLED
static inline bool isWellFormedDocument(Document& document)
{
    if (document.isSVGDocument() || document.isXHTMLDocument())
        return static_cast<XMLDocumentParser*>(document.parser())->wellFormed();
    return true;
}
#endif

Node::InsertionNotificationRequest SVGUseElement::insertedInto(ContainerNode& rootParent)
{
    // This functions exists to assure assumptions made in the code regarding SVGElementInstance creation/destruction are satisfied.
    SVGGraphicsElement::insertedInto(rootParent);
    if (!rootParent.inDocument())
        return InsertionDone;
    ASSERT(!hasPendingResources() || !isWellFormedDocument(document()));
    SVGExternalResourcesRequired::insertedIntoDocument(this);
    if (!m_wasInsertedByParser)
        return InsertionShouldCallDidNotifySubtreeInsertions;
    return InsertionDone;
}

void SVGUseElement::didNotifySubtreeInsertions(ContainerNode*)
{
    buildPendingResource();
}

void SVGUseElement::removedFrom(ContainerNode& rootParent)
{
    SVGGraphicsElement::removedFrom(rootParent);
    if (rootParent.inDocument())
        clearResourceReferences();
}

Document* SVGUseElement::referencedDocument() const
{
    if (!isExternalURIReference(href(), document()))
        return &document();
    return externalDocument();
}

Document* SVGUseElement::externalDocument() const
{
    if (m_cachedDocument && m_cachedDocument->isLoaded()) {
        // Gracefully handle error condition.
        if (m_cachedDocument->errorOccurred())
            return 0;
        ASSERT(m_cachedDocument->document());
        return m_cachedDocument->document();
    }
    return 0;
}

void SVGUseElement::transferSizeAttributesToShadowTreeTargetClone(SVGElement& shadowElement) const
{
    // FIXME: The check for valueInSpecifiedUnits being non-zero below is a workaround for the fact
    // that we currently have no good way to tell whether a particular animatable attribute is a value
    // indicating it was unspecified, or specified but could not be parsed. Would be nice to fix that some day.
    if (is<SVGSymbolElement>(shadowElement)) {
        // Spec (<use> on <symbol>): This generated 'svg' will always have explicit values for attributes width and height.
        // If attributes width and/or height are provided on the 'use' element, then these attributes
        // will be transferred to the generated 'svg'. If attributes width and/or height are not specified,
        // the generated 'svg' element will use values of 100% for these attributes.
        shadowElement.setAttribute(SVGNames::widthAttr, (widthIsValid() && width().valueInSpecifiedUnits()) ? AtomicString(width().valueAsString()) : "100%");
        shadowElement.setAttribute(SVGNames::heightAttr, (heightIsValid() && height().valueInSpecifiedUnits()) ? AtomicString(height().valueAsString()) : "100%");
    } else if (is<SVGSVGElement>(shadowElement)) {
        // Spec (<use> on <svg>): If attributes width and/or height are provided on the 'use' element, then these
        // values will override the corresponding attributes on the 'svg' in the generated tree.
        shadowElement.setAttribute(SVGNames::widthAttr, (widthIsValid() && width().valueInSpecifiedUnits()) ? AtomicString(width().valueAsString()) : shadowElement.correspondingElement()->getAttribute(SVGNames::widthAttr));
        shadowElement.setAttribute(SVGNames::heightAttr, (heightIsValid() && height().valueInSpecifiedUnits()) ? AtomicString(height().valueAsString()) : shadowElement.correspondingElement()->getAttribute(SVGNames::heightAttr));
    }
}

void SVGUseElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    InstanceInvalidationGuard guard(*this);

    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr || attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr) {
        updateRelativeLengthsInformation();
        if (SVGElement* shadowTreeTargetClone = this->shadowTreeTargetClone()) {
            // FIXME: It's unnecessarily inefficient to do this work any time we change "x" or "y".
            // FIXME: It's unnecessarily inefficient to update both width and height each time either is changed.
            transferSizeAttributesToShadowTreeTargetClone(*shadowTreeTargetClone);
        }
        if (auto* renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    if (SVGExternalResourcesRequired::handleAttributeChange(this, attrName))
        return;

    if (SVGURIReference::isKnownAttribute(attrName)) {
        bool isExternalReference = isExternalURIReference(href(), document());
        if (isExternalReference) {
            URL url = document().completeURL(href());
            if (url.hasFragmentIdentifier()) {
                CachedResourceRequest request(ResourceRequest(url.string()));
                request.setInitiator(this);
                setCachedDocument(document().cachedResourceLoader().requestSVGDocument(request));
            }
        } else
            setCachedDocument(0);

        if (!m_wasInsertedByParser)
            buildPendingResource();

        return;
    }

    if (SVGLangSpace::isKnownAttribute(attrName)
        || SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        invalidateShadowTree();
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGUseElement::willAttachRenderers()
{
    if (m_needsShadowTreeRecreation)
        buildPendingResource();
}

static bool isDisallowedElement(const Element& element)
{
    // Spec: "Any 'svg', 'symbol', 'g', graphics element or other 'use' is potentially a template object that can be re-used
    // (i.e., "instanced") in the SVG document via a 'use' element."
    // "Graphics Element" is defined as 'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline', 'rect', 'text'
    // Excluded are anything that is used by reference or that only make sense to appear once in a document.

    if (!element.isSVGElement())
        return true;

    static NeverDestroyed<HashSet<QualifiedName>> allowedElementTags;
    if (allowedElementTags.get().isEmpty()) {
        allowedElementTags.get().add(SVGNames::aTag);
        allowedElementTags.get().add(SVGNames::circleTag);
        allowedElementTags.get().add(SVGNames::descTag);
        allowedElementTags.get().add(SVGNames::ellipseTag);
        allowedElementTags.get().add(SVGNames::gTag);
        allowedElementTags.get().add(SVGNames::imageTag);
        allowedElementTags.get().add(SVGNames::lineTag);
        allowedElementTags.get().add(SVGNames::metadataTag);
        allowedElementTags.get().add(SVGNames::pathTag);
        allowedElementTags.get().add(SVGNames::polygonTag);
        allowedElementTags.get().add(SVGNames::polylineTag);
        allowedElementTags.get().add(SVGNames::rectTag);
        allowedElementTags.get().add(SVGNames::svgTag);
        allowedElementTags.get().add(SVGNames::switchTag);
        allowedElementTags.get().add(SVGNames::symbolTag);
        allowedElementTags.get().add(SVGNames::textTag);
        allowedElementTags.get().add(SVGNames::textPathTag);
        allowedElementTags.get().add(SVGNames::titleTag);
        allowedElementTags.get().add(SVGNames::trefTag);
        allowedElementTags.get().add(SVGNames::tspanTag);
        allowedElementTags.get().add(SVGNames::useTag);
    }
    return !allowedElementTags.get().contains<SVGAttributeHashTranslator>(element.tagQName());
}

void SVGUseElement::clearResourceReferences()
{
    // FIXME: It's expensive to re-clone the entire tree every time. We should find a more efficient way to handle this.
    if (ShadowRoot* root = userAgentShadowRoot())
        root->removeChildren();
    m_needsShadowTreeRecreation = false;
}

void SVGUseElement::buildPendingResource()
{
    if (isInShadowTree())
        return;
    if (!referencedDocument())
        return;
    clearResourceReferences();
    if (!inDocument())
        return;

    String id;
    Element* target = SVGURIReference::targetElementFromIRIString(href(), document(), &id, externalDocument());
    if (!isValidTarget(target)) {
        if (externalDocument()) {
            // We can't find the target in an external document, so just give up and don't try to do it again.
            // We should not attempt to observe if an element with ID shows up in the external document later.
            return;
        }
        referencedDocument()->accessSVGExtensions().addPendingResource(id, this);
        return;
    }

    buildShadowTree(downcast<SVGElement>(*target));
    expandUseElementsInShadowTree();
    expandSymbolElementsInShadowTree();
    transferSizeAttributesToShadowTreeTargetClone(*shadowTreeTargetClone());
    transferEventListenersToShadowTree();
    updateRelativeLengthsInformation();

    // When we invalidate the other shadow trees, it's important that we don't
    // follow any cycles and invalidate ourselves. To avoid that, we temporarily
    // set m_needsShadowTreeRecreation to true so invalidateShadowTree will
    // quickly return and do nothing.
    ASSERT(!m_needsShadowTreeRecreation);
    m_needsShadowTreeRecreation = true;
    invalidateDependentShadowTrees();
    m_needsShadowTreeRecreation = false;
}

SVGElement* SVGUseElement::shadowTreeTargetClone() const
{
    auto* root = userAgentShadowRoot();
    if (!root)
        return nullptr;
    return downcast<SVGElement>(root->firstChild());
}

RenderPtr<RenderElement> SVGUseElement::createElementRenderer(Ref<RenderStyle>&& style)
{
    return createRenderer<RenderSVGTransformableContainer>(*this, WTF::move(style));
}

static bool isDirectReference(const SVGElement& element)
{
    return element.hasTagName(SVGNames::pathTag)
        || element.hasTagName(SVGNames::rectTag)
        || element.hasTagName(SVGNames::circleTag)
        || element.hasTagName(SVGNames::ellipseTag)
        || element.hasTagName(SVGNames::polygonTag)
        || element.hasTagName(SVGNames::polylineTag)
        || element.hasTagName(SVGNames::textTag);
}

void SVGUseElement::toClipPath(Path& path)
{
    ASSERT(path.isEmpty());

    SVGElement* element = shadowTreeTargetClone();
    if (is<SVGGraphicsElement>(element)) {
        if (!isDirectReference(*element)) {
            // Spec: Indirect references are an error (14.3.5)
            document().accessSVGExtensions().reportError("Not allowed to use indirect reference in <clip-path>");
        } else {
            downcast<SVGGraphicsElement>(*element).toClipPath(path);
            // FIXME: Avoid manual resolution of x/y here. Its potentially harmful.
            SVGLengthContext lengthContext(this);
            path.translate(FloatSize(x().value(lengthContext), y().value(lengthContext)));
            path.transform(animatedLocalTransform());
        }
    }
}

RenderElement* SVGUseElement::rendererClipChild() const
{
    auto* element = shadowTreeTargetClone();
    if (!element)
        return nullptr;
    if (!isDirectReference(*element))
        return nullptr;
    return element->renderer();
}

static void removeDisallowedElementsFromSubtree(SVGElement& subtree)
{
    // Remove disallowed elements after the fact rather than not cloning them in the first place.
    // This optimizes for the normal case where none of those elements are present.

    // This function is used only on elements in subtrees that are not yet in documents, so
    // mutation events are not a factor; there are no event listeners to handle those events.
    // Assert that it's not in a document to make sure callers are still using it this way.
    ASSERT(!subtree.inDocument());

    Vector<Element*> disallowedElements;
    auto descendants = descendantsOfType<Element>(subtree);
    auto end = descendants.end();
    for (auto it = descendants.begin(); it != end; ) {
        if (isDisallowedElement(*it)) {
            disallowedElements.append(&*it);
            it.traverseNextSkippingChildren();
            continue;
        }
        ++it;
    }
    for (Element* element : disallowedElements)
        element->parentNode()->removeChild(element);
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
    SVGElement* correspondingElement = originalClone.correspondingElement();
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

void SVGUseElement::buildShadowTree(SVGElement& target)
{
    Ref<SVGElement> clonedTarget = static_pointer_cast<SVGElement>(target.cloneElementWithChildren(document())).releaseNonNull();
    associateClonesWithOriginals(clonedTarget.get(), target);
    removeDisallowedElementsFromSubtree(clonedTarget.get());
    ensureUserAgentShadowRoot().appendChild(WTF::move(clonedTarget));
}

bool SVGUseElement::isValidTarget(Element* target) const
{
    if (!is<SVGElement>(target))
        return false;
    if (!target->inDocument())
        return false;
    SVGElement& castedTarget = downcast<SVGElement>(*target);
    if (&castedTarget == this)
        return false;
    if (isDisallowedElement(castedTarget))
        return false;
    // Reject any target that would create a cycle.
    for (auto& ancestor : lineageOfType<SVGElement>(*this)) {
        if (ancestor.correspondingElement() == &castedTarget)
            return false;
    }
    return true;
}

void SVGUseElement::expandUseElementsInShadowTree()
{
    // FIXME: Combine this with buildShadowTree.

    if (cachedDocumentIsStillLoading())
        return;

    auto descendants = descendantsOfType<SVGUseElement>(*userAgentShadowRoot());
    auto end = descendants.end();
    for (auto it = descendants.begin(); it != end; ) {
        Ref<SVGUseElement> original = *it;
        it = end; // Efficiently quiets assertions due to the outstanding iterator.

        if (original->cachedDocumentIsStillLoading())
            return;

        // Spec: In the generated content, the 'use' will be replaced by 'g', where all attributes from the
        // 'use' element except for x, y, width, height and xlink:href are transferred to the generated 'g' element.

        // FIXME: Is it right to use referencedDocument() here instead of just document()?
        // Can a shadow tree within this document really contain elements that are in a
        // different document?
        ASSERT(referencedDocument());
        auto replacement = SVGGElement::create(SVGNames::gTag, *referencedDocument());
        original->transferAttributesToShadowTreeReplacement(replacement.get());
        original->cloneChildNodes(replacement.ptr());
        associateReplacementClonesWithOriginals(replacement.get(), original.get());

        RefPtr<SVGElement> clonedTarget;
        Element* targetCandidate = SVGURIReference::targetElementFromIRIString(original->href(), *referencedDocument());
        if (original->isValidTarget(targetCandidate)) {
            SVGElement& originalTarget = downcast<SVGElement>(*targetCandidate);
            clonedTarget = static_pointer_cast<SVGElement>(originalTarget.cloneElementWithChildren(document()));
            associateClonesWithOriginals(*clonedTarget, originalTarget);
            replacement->appendChild(clonedTarget);
        }

        removeDisallowedElementsFromSubtree(replacement.get());

        // Replace <use> with the <g> element we created.
        original->parentNode()->replaceChild(replacement.ptr(), original.ptr());

        // Call transferSizeAttributesToShadowTreeTargetClone after putting the cloned elements into the
        // shadow tree so it can use SVGElement::correspondingElement without triggering an assertion.
        if (clonedTarget)
            original->transferSizeAttributesToShadowTreeTargetClone(*clonedTarget);

        // Continue iterating from the <g> element since the <use> element was replaced.
        it = descendants.from(replacement.get());
    }
}

void SVGUseElement::expandSymbolElementsInShadowTree()
{
    auto descendants = descendantsOfType<SVGSymbolElement>(*userAgentShadowRoot());
    auto end = descendants.end();
    for (auto it = descendants.begin(); it != end; ) {
        SVGSymbolElement& original = *it;
        it = end; // Efficiently quiets assertions due to the outstanding iterator.

        // Spec: The referenced 'symbol' and its contents are deep-cloned into the generated tree,
        // with the exception that the 'symbol' is replaced by an 'svg'. This generated 'svg' will
        // always have explicit values for attributes width and height. If attributes width and/or
        // height are provided on the 'use' element, then these attributes will be transferred to
        // the generated 'svg'. If attributes width and/or height are not specified, the generated
        // 'svg' element will use values of 100% for these attributes.

        // FIXME: Is it right to use referencedDocument() here instead of just document()?
        // Can a shadow tree within this document really contain elements that are in a
        // different document?
        ASSERT(referencedDocument());
        auto replacement = SVGSVGElement::create(SVGNames::svgTag, *referencedDocument());
        replacement->cloneDataFromElement(original);
        original.cloneChildNodes(replacement.ptr());
        associateReplacementClonesWithOriginals(replacement.get(), original);

        removeDisallowedElementsFromSubtree(replacement.get());

        // Replace <symbol> with the <svg> element we created.
        original.parentNode()->replaceChild(replacement.ptr(), &original);

        // Continue iterating from the <svg> element since the <symbol> element was replaced.
        it = descendants.from(replacement.get());
    }
}

void SVGUseElement::transferEventListenersToShadowTree()
{
    ASSERT(userAgentShadowRoot());
    for (auto& descendant : descendantsOfType<SVGElement>(*userAgentShadowRoot())) {
        ASSERT(descendant.correspondingElement());
        if (EventTargetData* data = descendant.correspondingElement()->eventTargetData())
            data->eventListenerMap.copyEventListenersNotCreatedFromMarkupToTarget(&descendant);
    }
}

void SVGUseElement::invalidateShadowTree()
{
    if (m_needsShadowTreeRecreation)
        return;
    m_needsShadowTreeRecreation = true;
    setNeedsStyleRecalc(ReconstructRenderTree);
    invalidateDependentShadowTrees();
}

void SVGUseElement::invalidateDependentShadowTrees()
{
    for (auto* instance : instances()) {
        if (SVGUseElement* element = instance->correspondingUseElement()) {
            ASSERT(element->inDocument());
            element->invalidateShadowTree();
        }
    }
}

void SVGUseElement::transferAttributesToShadowTreeReplacement(SVGGElement& replacement) const
{
    replacement.cloneDataFromElement(*this);

    replacement.removeAttribute(SVGNames::xAttr);
    replacement.removeAttribute(SVGNames::yAttr);
    replacement.removeAttribute(SVGNames::widthAttr);
    replacement.removeAttribute(SVGNames::heightAttr);
    replacement.removeAttribute(XLinkNames::hrefAttr);
}

bool SVGUseElement::selfHasRelativeLengths() const
{
    if (x().isRelative() || y().isRelative() || width().isRelative() || height().isRelative())
        return true;

    auto* target = shadowTreeTargetClone();
    return target && target->hasRelativeLengths();
}

void SVGUseElement::notifyFinished(CachedResource* resource)
{
    if (!inDocument())
        return;

    invalidateShadowTree();
    if (resource->errorOccurred())
        dispatchEvent(Event::create(eventNames().errorEvent, false, false));
    else if (!resource->wasCanceled())
        SVGExternalResourcesRequired::dispatchLoadEvent(this);
}

bool SVGUseElement::cachedDocumentIsStillLoading()
{
    if (m_cachedDocument && m_cachedDocument->isLoading())
        return true;
    return false;
}

bool SVGUseElement::instanceTreeIsLoading(SVGElementInstance* targetElementInstance)
{
    for (SVGElementInstance* instance = targetElementInstance->firstChild(); instance; instance = instance->nextSibling()) {
        if (SVGUseElement* use = instance->correspondingUseElement()) {
             if (use->cachedDocumentIsStillLoading())
                 return true;
        }
        if (instance->hasChildNodes())
            instanceTreeIsLoading(instance);
    }
    return false;
}

void SVGUseElement::finishParsingChildren()
{
    SVGGraphicsElement::finishParsingChildren();
    SVGExternalResourcesRequired::finishParsingChildren();
    if (m_wasInsertedByParser) {
        buildPendingResource();
        m_wasInsertedByParser = false;
    }
}

void SVGUseElement::setCachedDocument(CachedResourceHandle<CachedSVGDocument> cachedDocument)
{
    if (m_cachedDocument == cachedDocument)
        return;

    if (m_cachedDocument)
        m_cachedDocument->removeClient(this);

    m_cachedDocument = cachedDocument;
    if (m_cachedDocument) {
        // We don't need the SVG document to create a new frame because the new document belongs to the parent UseElement.
        m_cachedDocument->setShouldCreateFrameForDocument(false);
        m_cachedDocument->addClient(this);
    }
}

}

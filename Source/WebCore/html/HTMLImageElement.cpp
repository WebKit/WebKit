/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2015 Google Inc. All rights reserved.
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
#include "HTMLImageElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonAtomStrings.h"
#include "Editor.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLMapElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLPictureElement.h"
#include "HTMLSourceElement.h"
#include "HTMLSrcsetParser.h"
#include "JSRequestPriority.h"
#include "LazyLoadImageObserver.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaQueryEvaluator.h"
#include "MouseEvent.h"
#include "NodeName.h"
#include "NodeTraversal.h"
#include "PlatformMouseEvent.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include "RenderView.h"
#include "RequestPriority.h"
#include "ScriptController.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SizesAttributeParser.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
#include "SpatialImageControls.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLImageElement);

using namespace HTMLNames;

HTMLImageElement::HTMLImageElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLElement(tagName, document, { TypeFlag::HasCustomStyleResolveCallbacks, TypeFlag::HasDidMoveToNewDocument })
    , FormAssociatedElement(form)
    , ActiveDOMObject(document)
    , m_imageLoader(makeUniqueWithoutRefCountedCheck<HTMLImageLoader>(*this))
    , m_compositeOperator(CompositeOperator::SourceOver)
    , m_imageDevicePixelRatio(1.0f)
{
    ASSERT(hasTagName(imgTag));
}

Ref<HTMLImageElement> HTMLImageElement::create(Document& document)
{
    auto image = adoptRef(*new HTMLImageElement(imgTag, document));
    image->suspendIfNeeded();
    return image;
}

Ref<HTMLImageElement> HTMLImageElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    auto image = adoptRef(*new HTMLImageElement(tagName, document, form));
    image->suspendIfNeeded();
    return image;
}

HTMLImageElement::~HTMLImageElement()
{
    disconnectFromIntersectionObservers();
    document().removeDynamicMediaQueryDependentImage(*this);
    setForm(nullptr);
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (RefPtr page = document().page())
        page->removeIndividuallyPlayingAnimationElement(*this);
#endif
}

void HTMLImageElement::resetFormOwner()
{
    setForm(HTMLFormElement::findClosestFormAncestor(*this));
}

void HTMLImageElement::setFormInternal(RefPtr<HTMLFormElement>&& newForm)
{
    if (auto* form = FormAssociatedElement::form())
        form->unregisterImgElement(*this);
    FormAssociatedElement::setFormInternal(newForm.copyRef());
    if (newForm)
        newForm->registerImgElement(*this);
}

void HTMLImageElement::formOwnerRemovedFromTree(const Node& formRoot)
{
    auto& rootNode = traverseToRootNode(); // Do not rely on rootNode() because our IsInTreeScope can be outdated.
    if (&rootNode != &formRoot)
        setForm(nullptr);
}

Ref<HTMLImageElement> HTMLImageElement::createForLegacyFactoryFunction(Document& document, std::optional<unsigned> width, std::optional<unsigned> height)
{
    auto image = adoptRef(*new HTMLImageElement(imgTag, document));
    if (width)
        image->setWidth(width.value());
    if (height)
        image->setHeight(height.value());
    image->suspendIfNeeded();
    return image;
}

bool HTMLImageElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
    case AttributeNames::heightAttr:
    case AttributeNames::borderAttr:
    case AttributeNames::vspaceAttr:
    case AttributeNames::hspaceAttr:
    case AttributeNames::valignAttr:
        return true;
    default:
        break;
    }
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLImageElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
        addHTMLMultiLengthToStyle(style, CSSPropertyWidth, value);
        applyAspectRatioFromWidthAndHeightAttributesToStyle(value, attributeWithoutSynchronization(heightAttr), style);
        break;
    case AttributeNames::heightAttr:
        addHTMLMultiLengthToStyle(style, CSSPropertyHeight, value);
        applyAspectRatioFromWidthAndHeightAttributesToStyle(attributeWithoutSynchronization(widthAttr), value, style);
        break;
    case AttributeNames::borderAttr:
        applyBorderAttributeToStyle(value, style);
        break;
    case AttributeNames::vspaceAttr:
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
        break;
    case AttributeNames::hspaceAttr:
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
        break;
    case AttributeNames::alignAttr:
        applyAlignmentAttributeToStyle(value, style);
        break;
    case AttributeNames::valignAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, value);
        break;
    default:
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

void HTMLImageElement::collectExtraStyleForPresentationalHints(MutableStyleProperties& style)
{
    if (!sourceElement())
        return;
    auto& widthAttrFromSource = sourceElement()->attributeWithoutSynchronization(widthAttr);
    auto& heightAttrFromSource = sourceElement()->attributeWithoutSynchronization(heightAttr);
    // If both width and height attributes of <source> is undefined, the style's value should not
    // be overwritten. Otherwise, <souce> will overwrite it. I.e., if <source> only has one attribute
    // defined, the other one and aspect-ratio shouldn't be set to auto.
    if (widthAttrFromSource.isNull() && heightAttrFromSource.isNull())
        return;

    if (!widthAttrFromSource.isNull())
        addHTMLLengthToStyle(style, CSSPropertyWidth, widthAttrFromSource);
    else
        addPropertyToPresentationalHintStyle(style, CSSPropertyWidth, CSSValueAuto);

    if (!heightAttrFromSource.isNull())
        addHTMLLengthToStyle(style, CSSPropertyHeight, heightAttrFromSource);
    else
        addPropertyToPresentationalHintStyle(style, CSSPropertyHeight, CSSValueAuto);

    if (!widthAttrFromSource.isNull() && !heightAttrFromSource.isNull())
        applyAspectRatioFromWidthAndHeightAttributesToStyle(widthAttrFromSource, heightAttrFromSource, style);
    else
        addPropertyToPresentationalHintStyle(style, CSSPropertyAspectRatio, CSSValueAuto);
}

const AtomString& HTMLImageElement::imageSourceURL() const
{
    return m_bestFitImageURL.isEmpty() ? attributeWithoutSynchronization(srcAttr) : m_bestFitImageURL;
}

const AtomString& HTMLImageElement::currentSrc()
{
    if (m_currentSrc.isNull()) {
        if (!m_currentURL.isEmpty())
            m_currentSrc = AtomString(m_currentURL.string());
    }
    return m_currentSrc;
}

void HTMLImageElement::setBestFitURLAndDPRFromImageCandidate(const ImageCandidate& candidate)
{
    m_bestFitImageURL = candidate.string.toAtomString();
    m_currentURL = document().completeURL(imageSourceURL());
    m_currentSrc = { };
    if (candidate.density >= 0)
        m_imageDevicePixelRatio = 1 / candidate.density;
    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer()))
        renderImage->setImageDevicePixelRatio(m_imageDevicePixelRatio);
}

static String extractMIMETypeFromTypeAttributeForLookup(const String& typeAttribute)
{
    auto semicolonIndex = typeAttribute.find(';');
    if (semicolonIndex == notFound)
        return typeAttribute.trim(isASCIIWhitespace);
    return StringView(typeAttribute).left(semicolonIndex).trim(isASCIIWhitespace<UChar>).toStringWithoutCopying();
}

ImageCandidate HTMLImageElement::bestFitSourceFromPictureElement()
{
    RefPtr picture = pictureElement();
    if (!picture)
        return { };

    ImageCandidate candidate;

    for (RefPtr<Node> child = picture->firstChild(); child && child != this; child = child->nextSibling()) {
        auto* source = dynamicDowncast<HTMLSourceElement>(*child);
        if (!source)
            continue;

        auto& srcset = source->attributeWithoutSynchronization(srcsetAttr);
        if (srcset.isEmpty())
            continue;

        auto& typeAttribute = source->attributeWithoutSynchronization(typeAttr);
        if (!typeAttribute.isNull()) {
            auto type = extractMIMETypeFromTypeAttributeForLookup(typeAttribute);
            if (!type.isEmpty() && !MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(type))
                continue;
        }

        RefPtr documentElement = document().documentElement();
        MQ::MediaQueryEvaluator evaluator { document().printing() ? printAtom() : screenAtom(), document(), documentElement ? documentElement->computedStyle() : nullptr };
        auto& queries = source->parsedMediaAttribute(document());
        LOG(MediaQueries, "HTMLImageElement %p bestFitSourceFromPictureElement evaluating media queries", this);

        auto result = evaluator.evaluate(queries);

        if (!evaluator.collectDynamicDependencies(queries).isEmpty())
            m_dynamicMediaQueryResults.append({ queries, result });

        if (!result)
            continue;

        SizesAttributeParser sizesParser(source->attributeWithoutSynchronization(sizesAttr).string(), document());

        m_dynamicMediaQueryResults.appendVector(sizesParser.dynamicMediaQueryResults());

        auto sourceSize = sizesParser.length();

        candidate = bestFitSourceForImageAttributes(document().deviceScaleFactor(), nullAtom(), srcset, sourceSize, [&](auto& candidate) {
            return m_imageLoader->shouldIgnoreCandidateWhenLoadingFromArchive(candidate);
        });

        if (!candidate.isEmpty()) {
            setSourceElement(source);
            break;
        }
    }

    return candidate;
}

void HTMLImageElement::evaluateDynamicMediaQueryDependencies()
{
    RefPtr documentElement = document().documentElement();
    MQ::MediaQueryEvaluator evaluator { document().printing() ? printAtom() : screenAtom(), document(), documentElement ? documentElement->computedStyle() : nullptr };

    auto hasChanges = [&] {
        for (auto& results : m_dynamicMediaQueryResults) {
            if (results.result != evaluator.evaluate(results.mediaQueryList))
                return true;
        }
        return false;
    }();

    if (!hasChanges)
        return;

    selectImageSource(RelevantMutation::No);
}

void HTMLImageElement::selectImageSource(RelevantMutation relevantMutation)
{
    m_dynamicMediaQueryResults = { };
    document().removeDynamicMediaQueryDependentImage(*this);

    // First look for the best fit source from our <picture> parent if we have one.
    ImageCandidate candidate = bestFitSourceFromPictureElement();
    if (candidate.isEmpty()) {
        setSourceElement(nullptr);
        auto srcAttribute = attributeWithoutSynchronization(srcAttr);
        auto srcsetAttribute = attributeWithoutSynchronization(srcsetAttr);
        // This is extremely common case. We should not invoke SizesAttributeParser at all.
        if (srcsetAttribute.isNull()) {
            if (srcAttribute.isNull())
                candidate = { };
            else
                candidate = ImageCandidate(StringViewWithUnderlyingString(srcAttribute, srcAttribute), DescriptorParsingResult(), ImageCandidate::SrcOrigin);
        } else {
            // If we don't have a <picture> or didn't find a source, then we use our own attributes.
            SizesAttributeParser sizesParser(attributeWithoutSynchronization(sizesAttr).string(), document());
            m_dynamicMediaQueryResults.appendVector(sizesParser.dynamicMediaQueryResults());
            auto sourceSize = sizesParser.length();
            candidate = bestFitSourceForImageAttributes(document().deviceScaleFactor(), srcAttribute, srcsetAttribute, sourceSize, [&](auto& candidate) {
                return m_imageLoader->shouldIgnoreCandidateWhenLoadingFromArchive(candidate);
            });
        }
    }
    setBestFitURLAndDPRFromImageCandidate(candidate);
    m_imageLoader->updateFromElementIgnoringPreviousError(relevantMutation);

    if (!m_dynamicMediaQueryResults.isEmpty())
        document().addDynamicMediaQueryDependentImage(*this);
}

bool HTMLImageElement::hasLazyLoadableAttributeValue(StringView attributeValue)
{
    return equalLettersIgnoringASCIICase(attributeValue, "lazy"_s);
}

void HTMLImageElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    switch (name.nodeName()) {
    case AttributeNames::altAttr:
        if (auto* renderImage = dynamicDowncast<RenderImage>(renderer()))
            renderImage->updateAltText();
        break;
    case AttributeNames::srcAttr:
    case AttributeNames::srcsetAttr:
    case AttributeNames::sizesAttr:
        if (oldValue != newValue)
            selectImageSource(RelevantMutation::Yes);
        else
            m_imageLoader->updateFromElementIgnoringPreviousErrorToSameValue();
        break;
    case AttributeNames::usemapAttr:
        if (isInTreeScope() && !m_parsedUsemap.isNull())
            treeScope().removeImageElementByUsemap(m_parsedUsemap, *this);
        m_parsedUsemap = parseHTMLHashNameReference(newValue);
        if (isInTreeScope() && !m_parsedUsemap.isNull())
            treeScope().addImageElementByUsemap(m_parsedUsemap, *this);
        break;
    case AttributeNames::compositeAttr: {
        // FIXME: images don't support blend modes in their compositing attribute.
        BlendMode blendOp = BlendMode::Normal;
        if (!parseCompositeAndBlendOperator(newValue, m_compositeOperator, blendOp))
            m_compositeOperator = CompositeOperator::SourceOver;
        break;
    }
    case AttributeNames::loadingAttr:
        // No action needed for eager to lazy transition.
        if (!hasLazyLoadableAttributeValue(newValue))
            loadDeferredImage();
        break;
    case AttributeNames::referrerpolicyAttr: {
        auto oldReferrerPolicy = parseReferrerPolicy(oldValue, ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
        auto newReferrerPolicy = parseReferrerPolicy(newValue, ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
        if (oldReferrerPolicy != newReferrerPolicy)
            m_imageLoader->updateFromElementIgnoringPreviousError(RelevantMutation::Yes);
        break;
    }
    case AttributeNames::crossoriginAttr:
        if (parseCORSSettingsAttribute(oldValue) != parseCORSSettingsAttribute(newValue))
            m_imageLoader->updateFromElementIgnoringPreviousError(RelevantMutation::Yes);
        break;
    case AttributeNames::nameAttr: {
        bool willHaveName = !newValue.isEmpty();
        if (auto* document = dynamicDowncast<HTMLDocument>(this->document()); m_hadNameBeforeAttributeChanged != willHaveName && isConnected() && !isInShadowTree() && document) {
            const AtomString& id = getIdAttribute();
            if (!id.isEmpty() && id != getNameAttribute()) {
                if (willHaveName)
                    document->addDocumentNamedItem(id, *this);
                else
                    document->removeDocumentNamedItem(id, *this);
            }
        }
        m_hadNameBeforeAttributeChanged = willHaveName;
        break;
    }
#if ENABLE(SPATIAL_IMAGE_CONTROLS)
    case AttributeNames::controlsAttr:
        SpatialImageControls::updateSpatialImageControls(*this);
        break;
#endif
    default:
        break;
    }

#if ENABLE(SERVICE_CONTROLS)
    if (isImageMenuEnabled())
        ImageControlsMac::updateImageControls(*this);
#endif
}

void HTMLImageElement::loadDeferredImage()
{
    m_imageLoader->loadDeferredImage();
}

const AtomString& HTMLImageElement::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    const AtomString& alt = attributeWithoutSynchronization(altAttr);
    if (!alt.isNull())
        return alt;
    // fall back to title attribute
    return attributeWithoutSynchronization(titleAttr);
}

RenderPtr<RenderElement> HTMLImageElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (style.hasContent())
        return RenderElement::createFor(*this, WTFMove(style));

    return createRenderer<RenderImage>(RenderObject::Type::Image, *this, WTFMove(style), nullptr, m_imageDevicePixelRatio);
}

bool HTMLImageElement::isReplaced(const RenderStyle& style) const
{
    return !style.hasContent();
}

bool HTMLImageElement::canStartSelection() const
{
    if (shadowRoot())
        return HTMLElement::canStartSelection();

    return false;
}

bool HTMLImageElement::isInteractiveContent() const
{
    return hasAttributeWithoutSynchronization(usemapAttr);
}

void HTMLImageElement::didAttachRenderers()
{
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer());
    if (!renderImage)
        return;
    if (m_imageLoader->hasPendingBeforeLoadEvent())
        return;

#if ENABLE(SERVICE_CONTROLS)
    ImageControlsMac::updateImageControls(*this);
#endif

    RenderImageResource& renderImageResource = renderImage->imageResource();
    if (renderImageResource.cachedImage())
        return;
    renderImageResource.setCachedImage(m_imageLoader->protectedImage());

    // If we have no image at all because we have no src attribute, set
    // image height and width for the alt text instead.
    if (!m_imageLoader->image() && !renderImageResource.cachedImage())
        renderImage->setImageSizeForAltText();
}

Node::InsertedIntoAncestorResult HTMLImageElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    FormAssociatedElement::elementInsertedIntoAncestor(*this, insertionType);
    if (!form())
        resetFormOwner();

    // Insert needs to complete first, before we start updating the loader. Loader dispatches events which could result
    // in callbacks back to this node.
    Node::InsertedIntoAncestorResult insertNotificationRequest = HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (insertionType.treeScopeChanged && !m_parsedUsemap.isNull())
        treeScope().addImageElementByUsemap(m_parsedUsemap, *this);

    if (auto* parentPicture = dynamicDowncast<HTMLPictureElement>(parentOfInsertedTree); parentPicture && &parentOfInsertedTree == parentElement()) {
        // FIXME: When the hack in HTMLConstructionSite::createHTMLElementOrFindCustomElementInterface to eagerly call setPictureElement is removed, we can just assert !pictureElement().
        ASSERT(!pictureElement() || pictureElement() == &parentOfInsertedTree);
        setPictureElement(parentPicture);
        selectImageSource(RelevantMutation::Yes);
        return insertNotificationRequest;
    }

    // If we have been inserted from a renderer-less document,
    // our loader may have not fetched the image, so do it now.
    if (insertionType.connectedToDocument && !m_imageLoader->image())
        m_imageLoader->updateFromElement();

    return insertNotificationRequest;
}

void HTMLImageElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.treeScopeChanged && !m_parsedUsemap.isNull())
        oldParentOfRemovedTree.treeScope().removeImageElementByUsemap(m_parsedUsemap, *this);

    if (is<HTMLPictureElement>(oldParentOfRemovedTree) && !parentElement()) {
        ASSERT(pictureElement() == &oldParentOfRemovedTree);
        setPictureElement(nullptr);
        selectImageSource(RelevantMutation::Yes);
    }

    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    FormAssociatedElement::elementRemovedFromAncestor(*this, removalType);
}

HTMLPictureElement* HTMLImageElement::pictureElement() const
{
    return m_pictureElement.get();
}
    
void HTMLImageElement::setPictureElement(HTMLPictureElement* pictureElement)
{
    m_pictureElement = pictureElement;
}
    
unsigned HTMLImageElement::width()
{
    if (inRenderedDocument())
        protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        auto optionalWidth = parseHTMLNonNegativeInteger(attributeWithoutSynchronization(widthAttr));
        if (optionalWidth)
            return optionalWidth.value();

        // if the image is available, use its width
        if (m_imageLoader->image())
            return m_imageLoader->image()->imageSizeForRenderer(renderer(), 1.0f).width().toUnsigned();
    }

    RenderBox* box = renderBox();
    if (!box)
        return 0;
    LayoutRect contentRect = box->contentBoxRect();
    return adjustForAbsoluteZoom(snappedIntRect(contentRect).width(), *box);
}

unsigned HTMLImageElement::height()
{
    if (inRenderedDocument())
        protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        auto optionalHeight = parseHTMLNonNegativeInteger(attributeWithoutSynchronization(heightAttr));
        if (optionalHeight)
            return optionalHeight.value();

        // if the image is available, use its height
        if (m_imageLoader->image())
            return m_imageLoader->image()->imageSizeForRenderer(renderer(), 1.0f).height().toUnsigned();
    }

    RenderBox* box = renderBox();
    if (!box)
        return 0;
    LayoutRect contentRect = box->contentBoxRect();
    return adjustForAbsoluteZoom(snappedIntRect(contentRect).height(), *box);
}

float HTMLImageElement::effectiveImageDevicePixelRatio() const
{
    if (!m_imageLoader->image())
        return 1.0f;

    auto* image = m_imageLoader->image()->image();

    if (image && image->drawsSVGImage())
        return 1.0f;

    return m_imageDevicePixelRatio;
}

unsigned HTMLImageElement::naturalWidth() const
{
    if (!m_imageLoader->image())
        return 0;

    return m_imageLoader->image()->unclampedImageSizeForRenderer(renderer(), effectiveImageDevicePixelRatio()).width().toUnsigned();
}

unsigned HTMLImageElement::naturalHeight() const
{
    if (!m_imageLoader->image())
        return 0;

    return m_imageLoader->image()->unclampedImageSizeForRenderer(renderer(), effectiveImageDevicePixelRatio()).height().toUnsigned();
}

bool HTMLImageElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr
        || attribute.name() == lowsrcAttr
        || attribute.name() == longdescAttr
        || (attribute.name() == usemapAttr && attribute.value().string()[0] != '#')
        || HTMLElement::isURLAttribute(attribute);
}

bool HTMLImageElement::attributeContainsURL(const Attribute& attribute) const
{
    return attribute.name() == srcsetAttr
        || HTMLElement::attributeContainsURL(attribute);
}

String HTMLImageElement::completeURLsInAttributeValue(const URL& base, const Attribute& attribute, ResolveURLs resolveURLs) const
{
    if (attribute.name() == srcsetAttr) {
        if (resolveURLs == ResolveURLs::No)
            return attribute.value();

        Vector<ImageCandidate> imageCandidates = parseImageCandidatesFromSrcsetAttribute(StringView(attribute.value()));

        if (resolveURLs == ResolveURLs::NoExcludingURLsForPrivacy) {
            bool needsToResolveURLs = false;
            for (const auto& candidate : imageCandidates) {
                auto urlString = candidate.string.toString();
                auto completeURL = base.isNull() ? document().completeURL(urlString) : URL(base, urlString);
                if (document().shouldMaskURLForBindings(completeURL)) {
                    needsToResolveURLs = true;
                    break;
                }
            }

            if (!needsToResolveURLs)
                return attribute.value();
        }

        StringBuilder result;
        for (const auto& candidate : imageCandidates) {
            if (&candidate != &imageCandidates[0])
                result.append(", "_s);
            result.append(resolveURLStringIfNeeded(candidate.string.toString(), resolveURLs, base));
            if (candidate.density != UninitializedDescriptor)
                result.append(' ', candidate.density, 'x');
            if (candidate.resourceWidth != UninitializedDescriptor)
                result.append(' ', candidate.resourceWidth, 'w');
        }

        return result.toString();
    }

    return HTMLElement::completeURLsInAttributeValue(base, attribute, resolveURLs);
}

Attribute HTMLImageElement::replaceURLsInAttributeValue(const Attribute& attribute, const UncheckedKeyHashMap<String, String>& replacementURLStrings) const
{
    if (attribute.name() != srcsetAttr)
        return attribute;

    if (replacementURLStrings.isEmpty())
        return attribute;

    return Attribute { srcsetAttr, AtomString { replaceURLsInSrcsetAttribute(*this, StringView(attribute.value()), replacementURLStrings) } };
}

bool HTMLImageElement::matchesUsemap(const AtomString& name) const
{
    return m_parsedUsemap == name;
}

RefPtr<HTMLMapElement> HTMLImageElement::associatedMapElement() const
{
    return treeScope().getImageMap(m_parsedUsemap);
}

const AtomString& HTMLImageElement::alt() const
{
    return attributeWithoutSynchronization(altAttr);
}

void HTMLImageElement::setHeight(unsigned value)
{
    setUnsignedIntegralAttribute(heightAttr, value);
}

URL HTMLImageElement::src() const
{
    return document().completeURL(attributeWithoutSynchronization(srcAttr));
}

void HTMLImageElement::setSrc(const AtomString& value)
{
    setAttributeWithoutSynchronization(srcAttr, value);
}

void HTMLImageElement::setWidth(unsigned value)
{
    setUnsignedIntegralAttribute(widthAttr, value);
}

int HTMLImageElement::x() const
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);
    auto renderer = this->renderer();
    if (!renderer)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    return renderer->localToAbsolute().x();
}

int HTMLImageElement::y() const
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);
    auto renderer = this->renderer();
    if (!renderer)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    return renderer->localToAbsolute().y();
}

bool HTMLImageElement::complete() const
{
    return m_imageLoader->imageComplete();
}

void HTMLImageElement::setDecoding(AtomString&& decodingMode)
{
    setAttributeWithoutSynchronization(decodingAttr, WTFMove(decodingMode));
}

String HTMLImageElement::decoding() const
{
    switch (decodingMode()) {
    case DecodingMode::Auto:
        break;
    case DecodingMode::Synchronous:
        return "sync"_s;
    case DecodingMode::Asynchronous:
        return "async"_s;
    }
    return autoAtom();
}

DecodingMode HTMLImageElement::decodingMode() const
{
    const AtomString& decodingMode = attributeWithoutSynchronization(decodingAttr);
    if (equalLettersIgnoringASCIICase(decodingMode, "sync"_s))
        return DecodingMode::Synchronous;
    if (equalLettersIgnoringASCIICase(decodingMode, "async"_s))
        return DecodingMode::Asynchronous;
    return DecodingMode::Auto;
}
    
void HTMLImageElement::decode(Ref<DeferredPromise>&& promise)
{
    return m_imageLoader->decode(WTFMove(promise));
}

void HTMLImageElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(imageSourceURL()));
    // FIXME: What about when the usemap attribute begins with "#"?
    addSubresourceURL(urls, document().completeURL(attributeWithoutSynchronization(usemapAttr)));
}

void HTMLImageElement::addCandidateSubresourceURLs(ListHashSet<URL>& urls) const
{
    auto src = attributeWithoutSynchronization(srcAttr);
    if (!src.isEmpty()) {
        URL url { resolveURLStringIfNeeded(src) };
        if (!url.isNull())
            urls.add(url);
    }

    getURLsFromSrcsetAttribute(*this, attributeWithoutSynchronization(srcsetAttr), urls);
}

void HTMLImageElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ActiveDOMObject::didMoveToNewDocument(newDocument);
    oldDocument.removeDynamicMediaQueryDependentImage(*this);

    selectImageSource(RelevantMutation::No);
    m_imageLoader->elementDidMoveToNewDocument(oldDocument);
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    if (RefPtr element = pictureElement())
        element->sourcesChanged();
}

bool HTMLImageElement::isServerMap() const
{
    if (!hasAttributeWithoutSynchronization(ismapAttr))
        return false;

    const AtomString& usemap = attributeWithoutSynchronization(usemapAttr);

    // If the usemap attribute starts with '#', it refers to a map element in the document.
    if (usemap.string()[0] == '#')
        return false;

    return document().completeURL(usemap).isEmpty();
}

void HTMLImageElement::setCrossOrigin(const AtomString& value)
{
    setAttributeWithoutSynchronization(crossoriginAttr, value);
}

String HTMLImageElement::crossOrigin() const
{
    return parseCORSSettingsAttribute(attributeWithoutSynchronization(crossoriginAttr));
}

bool HTMLImageElement::allowsOrientationOverride() const
{
    if (auto* cachedImage = this->cachedImage())
        return cachedImage->allowsOrientationOverride();
    return true;
}

Image* HTMLImageElement::image() const
{
    if (auto* cachedImage = this->cachedImage())
        return cachedImage->image();
    return nullptr;
}

bool HTMLImageElement::allowsAnimation() const
{
    if (auto* image = this->image())
        return image->allowsAnimation().value_or(document().page() ? document().page()->imageAnimationEnabled() : false);
    return false;
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void HTMLImageElement::setAllowsAnimation(std::optional<bool> allowsAnimation)
{
    if (!document().settings().imageAnimationControlEnabled())
        return;

    if (auto* image = this->image()) {
        image->setAllowsAnimation(allowsAnimation);
        if (auto* renderer = this->renderer())
            renderer->repaint();

        if (RefPtr page = document().page()) {
            if (allowsAnimation.value_or(false))
                page->addIndividuallyPlayingAnimationElement(*this);
            else
                page->removeIndividuallyPlayingAnimationElement(*this);
        }
    }
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)

void HTMLImageElement::setAttachmentElement(Ref<HTMLAttachmentElement>&& attachment)
{
    AttachmentAssociatedElement::setAttachmentElement(WTFMove(attachment));

#if ENABLE(SERVICE_CONTROLS)
    bool shouldEnableImageMenu = true;
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    shouldEnableImageMenu = !isMultiRepresentationHEIC();
#endif
    setImageMenuEnabled(shouldEnableImageMenu);
#endif // ENABLE(SERVICE_CONTROLS)
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(SERVICE_CONTROLS)
bool HTMLImageElement::childShouldCreateRenderer(const Node& child) const
{
    return hasShadowRootParent(child) && HTMLElement::childShouldCreateRenderer(child);
}
#endif

#if PLATFORM(IOS_FAMILY)
// FIXME: We should find a better place for the touch callout logic. See rdar://problem/48937767.
bool HTMLImageElement::willRespondToMouseClickEventsWithEditability(Editability editability, IgnoreTouchCallout ignoreTouchCallout) const
{
    auto renderer = this->renderer();
    if (ignoreTouchCallout == IgnoreTouchCallout::No && (!renderer || renderer->style().touchCalloutEnabled()))
        return true;
    return HTMLElement::willRespondToMouseClickEventsWithEditability(editability);
}

bool HTMLImageElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    return willRespondToMouseClickEventsWithEditability(editability, IgnoreTouchCallout::No);
}
#endif

#if USE(SYSTEM_PREVIEW)
bool HTMLImageElement::isSystemPreviewImage() const
{
    if (!document().settings().systemPreviewEnabled())
        return false;

    auto* parent = parentElement();
    if (auto* anchorElement = dynamicDowncast<HTMLAnchorElement>(parent))
        return anchorElement->isSystemPreviewLink();
    if (auto* pictureElement = dynamicDowncast<HTMLPictureElement>(parent))
        return pictureElement->isSystemPreviewImage();
    return false;
}
#endif

#if ENABLE(MULTI_REPRESENTATION_HEIC)
bool HTMLImageElement::isMultiRepresentationHEIC() const
{
    if (!m_sourceElement)
        return false;

    auto& typeAttribute = m_sourceElement->attributeWithoutSynchronization(typeAttr);
    return typeAttribute == "image/x-apple-adaptive-glyph"_s;
}
#endif

void HTMLImageElement::copyNonAttributePropertiesFromElement(const Element& source)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    auto& sourceImage = downcast<HTMLImageElement>(source);
    copyAttachmentAssociatedPropertiesFromElement(sourceImage);
#endif
    Element::copyNonAttributePropertiesFromElement(source);
}

CachedImage* HTMLImageElement::cachedImage() const
{
    return m_imageLoader->image();
}

void HTMLImageElement::setLoadManually(bool loadManually)
{
    m_imageLoader->setLoadManually(loadManually);
}

bool HTMLImageElement::virtualHasPendingActivity() const
{
    return m_imageLoader->hasPendingActivity();
}

size_t HTMLImageElement::pendingDecodePromisesCountForTesting() const
{
    return m_imageLoader->pendingDecodePromisesCountForTesting();
}

bool HTMLImageElement::usesSrcsetOrPicture() const
{
    return !attributeWithoutSynchronization(srcsetAttr).isNull() || !!pictureElement();
}

AtomString HTMLImageElement::srcsetForBindings() const
{
    return getAttributeForBindings(srcsetAttr);
}

void HTMLImageElement::setSrcsetForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(srcsetAttr, value);
}

const AtomString& HTMLImageElement::loadingForBindings() const
{
    auto& attributeValue = attributeWithoutSynchronization(HTMLNames::loadingAttr);
    return hasLazyLoadableAttributeValue(attributeValue) ? lazyAtom() : eagerAtom();
}

void HTMLImageElement::setLoadingForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(loadingAttr, value);
}

bool HTMLImageElement::isDeferred() const
{
    return m_imageLoader->isDeferred();
}

bool HTMLImageElement::isLazyLoadable() const
{
    if (!document().frame() || !document().frame()->script().canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return false;
    return hasLazyLoadableAttributeValue(attributeWithoutSynchronization(HTMLNames::loadingAttr));
}

void HTMLImageElement::setReferrerPolicyForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(referrerpolicyAttr, value);
}

String HTMLImageElement::referrerPolicyForBindings() const
{
    return referrerPolicyToString(referrerPolicy());
}

ReferrerPolicy HTMLImageElement::referrerPolicy() const
{
    return parseReferrerPolicy(attributeWithoutSynchronization(referrerpolicyAttr), ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
}

HTMLSourceElement* HTMLImageElement::sourceElement() const
{
    return m_sourceElement.get();
}

void HTMLImageElement::setSourceElement(HTMLSourceElement* sourceElement)
{
    if (m_sourceElement == sourceElement)
        return;
    m_sourceElement = sourceElement;
    invalidateAttributeMapping();
}

void HTMLImageElement::invalidateAttributeMapping()
{
    ensureUniqueElementData().setPresentationalHintStyleIsDirty(true);
    invalidateStyle();
}

Ref<Element> HTMLImageElement::cloneElementWithoutAttributesAndChildren(TreeScope& treeScope)
{
    Ref document = treeScope.documentScope();
    auto clone = create(document);
#if ENABLE(ATTACHMENT_ELEMENT)
    cloneAttachmentAssociatedElementWithoutAttributesAndChildren(clone, document);
#endif
    return clone;
}

void HTMLImageElement::setFetchPriorityForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(fetchpriorityAttr, value);
}

String HTMLImageElement::fetchPriorityForBindings() const
{
    return convertEnumerationToString(fetchPriorityHint());
}

RequestPriority HTMLImageElement::fetchPriorityHint() const
{
    if (document().settings().fetchPriorityEnabled())
        return parseEnumerationFromString<RequestPriority>(attributeWithoutSynchronization(fetchpriorityAttr)).value_or(RequestPriority::Auto);
    return RequestPriority::Auto;
}

bool HTMLImageElement::originClean(const SecurityOrigin& origin) const
{
    UNUSED_PARAM(origin);

    auto* cachedImage = this->cachedImage();
    if (!cachedImage)
        return true;

    RefPtr image = cachedImage->image();
    if (!image)
        return true;

    if (image->renderingTaintsOrigin())
        return false;

    if (image->sourceURL().protocolIsData())
        return true;

    if (cachedImage->isCORSCrossOrigin())
        return false;

    ASSERT(cachedImage->origin());
    ASSERT(origin.toString() == cachedImage->origin()->toString());
    return true;
}

IntersectionObserverData& HTMLImageElement::ensureIntersectionObserverData()
{
    if (!m_intersectionObserverData)
        m_intersectionObserverData = makeUnique<IntersectionObserverData>();
    return *m_intersectionObserverData;
}

IntersectionObserverData* HTMLImageElement::intersectionObserverDataIfExists()
{
    return m_intersectionObserverData.get();
}

}

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameView.h"
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
#include "LazyLoadImageObserver.h"
#include "LegacyMediaQueryEvaluator.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaList.h"
#include "MouseEvent.h"
#include "NodeTraversal.h"
#include "PlatformMouseEvent.h"
#include "RenderImage.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SizesAttributeParser.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLImageElement);

using namespace HTMLNames;

HTMLImageElement::HTMLImageElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLElement(tagName, document)
    , ActiveDOMObject(document)
    , m_imageLoader(makeUnique<HTMLImageLoader>(*this))
    , m_form(nullptr)
    , m_formSetByParser(form)
    , m_compositeOperator(CompositeOperator::SourceOver)
    , m_imageDevicePixelRatio(1.0f)
{
    ASSERT(hasTagName(imgTag));
    setHasCustomStyleResolveCallbacks();
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
    document().removeDynamicMediaQueryDependentImage(*this);

    if (m_form)
        m_form->removeImgElement(this);
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
    if (name == widthAttr || name == heightAttr || name == borderAttr || name == vspaceAttr || name == hspaceAttr || name == valignAttr)
        return true;
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLImageElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr) {
        addHTMLMultiLengthToStyle(style, CSSPropertyWidth, value);
        applyAspectRatioFromWidthAndHeightAttributesToStyle(value, attributeWithoutSynchronization(heightAttr), style);
    } else if (name == heightAttr) {
        addHTMLMultiLengthToStyle(style, CSSPropertyHeight, value);
        applyAspectRatioFromWidthAndHeightAttributesToStyle(attributeWithoutSynchronization(widthAttr), value, style);
    } else if (name == borderAttr)
        applyBorderAttributeToStyle(value, style);
    else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else if (name == valignAttr)
        addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, value);
    else
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
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

void HTMLImageElement::setBestFitURLAndDPRFromImageCandidate(const ImageCandidate& candidate)
{
    m_bestFitImageURL = candidate.string.toAtomString();
    m_currentSrc = AtomString(document().completeURL(imageSourceURL()).string());
    if (candidate.density >= 0)
        m_imageDevicePixelRatio = 1 / candidate.density;
    if (is<RenderImage>(renderer()))
        downcast<RenderImage>(*renderer()).setImageDevicePixelRatio(m_imageDevicePixelRatio);
}

static String extractMIMETypeFromTypeAttributeForLookup(const String& typeAttribute)
{
    auto semicolonIndex = typeAttribute.find(';');
    if (semicolonIndex == notFound)
        return stripLeadingAndTrailingHTMLSpaces(typeAttribute);
    return StringView(typeAttribute).left(semicolonIndex).stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>).toStringWithoutCopying();
}

ImageCandidate HTMLImageElement::bestFitSourceFromPictureElement()
{
    RefPtr picture = pictureElement();
    if (!picture)
        return { };

    ImageCandidate candidate;

    for (RefPtr<Node> child = picture->firstChild(); child && child != this; child = child->nextSibling()) {
        if (!is<HTMLSourceElement>(*child))
            continue;
        auto& source = downcast<HTMLSourceElement>(*child);

        auto& srcset = source.attributeWithoutSynchronization(srcsetAttr);
        if (srcset.isEmpty())
            continue;

        auto& typeAttribute = source.attributeWithoutSynchronization(typeAttr);
        if (!typeAttribute.isNull()) {
            auto type = extractMIMETypeFromTypeAttributeForLookup(typeAttribute);
            if (!type.isEmpty() && !MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(type))
                continue;
        }

        RefPtr documentElement = document().documentElement();
        LegacyMediaQueryEvaluator evaluator { document().printing() ? "print"_s : "screen"_s, document(), documentElement ? documentElement->computedStyle() : nullptr };
        auto* queries = source.parsedMediaAttribute(document());
        LOG(MediaQueries, "HTMLImageElement %p bestFitSourceFromPictureElement evaluating media queries", this);

        auto evaluation = !queries || evaluator.evaluate(*queries, &m_mediaQueryDynamicResults);
        if (!evaluation)
            continue;

        SizesAttributeParser sizesParser(source.attributeWithoutSynchronization(sizesAttr).string(), document(), &m_mediaQueryDynamicResults);
        auto sourceSize = sizesParser.length();

        candidate = bestFitSourceForImageAttributes(document().deviceScaleFactor(), nullAtom(), srcset, sourceSize);
        if (!candidate.isEmpty()) {
            setSourceElement(&source);
            break;
        }
    }

    return candidate;
}

void HTMLImageElement::evaluateDynamicMediaQueryDependencies()
{
    RefPtr documentElement = document().documentElement();
    LegacyMediaQueryEvaluator evaluator { document().printing() ? "print"_s : "screen"_s, document(), documentElement ? documentElement->computedStyle() : nullptr };

    if (!evaluator.evaluateForChanges(m_mediaQueryDynamicResults))
        return;

    selectImageSource(RelevantMutation::No);
}

void HTMLImageElement::selectImageSource(RelevantMutation relevantMutation)
{
    m_mediaQueryDynamicResults = { };
    document().removeDynamicMediaQueryDependentImage(*this);

    // First look for the best fit source from our <picture> parent if we have one.
    ImageCandidate candidate = bestFitSourceFromPictureElement();
    if (candidate.isEmpty()) {
        setSourceElement(nullptr);
        // If we don't have a <picture> or didn't find a source, then we use our own attributes.
        SizesAttributeParser sizesParser(attributeWithoutSynchronization(sizesAttr).string(), document(), &m_mediaQueryDynamicResults);
        auto sourceSize = sizesParser.length();
        candidate = bestFitSourceForImageAttributes(document().deviceScaleFactor(), attributeWithoutSynchronization(srcAttr), attributeWithoutSynchronization(srcsetAttr), sourceSize);
    }
    setBestFitURLAndDPRFromImageCandidate(candidate);
    m_imageLoader->updateFromElementIgnoringPreviousError(relevantMutation);

    if (!m_mediaQueryDynamicResults.isEmpty())
        document().addDynamicMediaQueryDependentImage(*this);
}

bool HTMLImageElement::hasLazyLoadableAttributeValue(StringView attributeValue)
{
    return equalLettersIgnoringASCIICase(attributeValue, "lazy"_s);
}

enum CrossOriginState { NotSet, UseCredentials, Anonymous };
static CrossOriginState parseCrossoriginState(const AtomString& crossoriginValue)
{
    if (crossoriginValue.isNull())
        return NotSet;
    return equalLettersIgnoringASCIICase(crossoriginValue, "use-credentials"_s) ? UseCredentials : Anonymous;
}

void HTMLImageElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, reason);

    if (name == referrerpolicyAttr && document().settings().referrerPolicyAttributeEnabled()) {
        auto oldReferrerPolicy = parseReferrerPolicy(oldValue, ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
        auto newReferrerPolicy = parseReferrerPolicy(newValue, ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
        if (oldReferrerPolicy != newReferrerPolicy)
            m_imageLoader->updateFromElementIgnoringPreviousError(RelevantMutation::Yes);
    } else if (name == crossoriginAttr) {
        if (parseCrossoriginState(oldValue) != parseCrossoriginState(newValue))
            m_imageLoader->updateFromElementIgnoringPreviousError(RelevantMutation::Yes);
    }
}

void HTMLImageElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == altAttr) {
        if (is<RenderImage>(renderer()))
            downcast<RenderImage>(*renderer()).updateAltText();
    } else if (name == srcAttr || name == srcsetAttr || name == sizesAttr)
        selectImageSource(RelevantMutation::Yes);
    else if (name == usemapAttr) {
        if (isInTreeScope() && !m_parsedUsemap.isNull())
            treeScope().removeImageElementByUsemap(*m_parsedUsemap.impl(), *this);

        m_parsedUsemap = parseHTMLHashNameReference(value);

        if (isInTreeScope() && !m_parsedUsemap.isNull())
            treeScope().addImageElementByUsemap(*m_parsedUsemap.impl(), *this);
    } else if (name == compositeAttr) {
        // FIXME: images don't support blend modes in their compositing attribute.
        BlendMode blendOp = BlendMode::Normal;
        if (!parseCompositeAndBlendOperator(value, m_compositeOperator, blendOp))
            m_compositeOperator = CompositeOperator::SourceOver;
#if ENABLE(SERVICE_CONTROLS)
    } else if (isImageMenuEnabled()) {
        ImageControlsMac::updateImageControls(*this);
#endif
    } else if (name == loadingAttr) {
        // No action needed for eager to lazy transition.
        if (!hasLazyLoadableAttributeValue(value))
            loadDeferredImage();
    } else {
        if (name == nameAttr) {
            bool willHaveName = !value.isEmpty();
            if (m_hadNameBeforeAttributeChanged != willHaveName && isConnected() && !isInShadowTree() && is<HTMLDocument>(document())) {
                HTMLDocument& document = downcast<HTMLDocument>(this->document());
                const AtomString& id = getIdAttribute();
                if (!id.isEmpty() && id != getNameAttribute()) {
                    if (willHaveName)
                        document.addDocumentNamedItem(*id.impl(), *this);
                    else
                        document.removeDocumentNamedItem(*id.impl(), *this);
                }
            }
            m_hadNameBeforeAttributeChanged = willHaveName;
        }
        HTMLElement::parseAttribute(name, value);
    }
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

    return createRenderer<RenderImage>(*this, WTFMove(style), nullptr, m_imageDevicePixelRatio);
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
    if (!is<RenderImage>(renderer()))
        return;
    if (m_imageLoader->hasPendingBeforeLoadEvent())
        return;

#if ENABLE(SERVICE_CONTROLS)
    ImageControlsMac::updateImageControls(*this);
#endif

    auto& renderImage = downcast<RenderImage>(*renderer());
    RenderImageResource& renderImageResource = renderImage.imageResource();
    if (renderImageResource.cachedImage())
        return;
    renderImageResource.setCachedImage(m_imageLoader->image());

    // If we have no image at all because we have no src attribute, set
    // image height and width for the alt text instead.
    if (!m_imageLoader->image() && !renderImageResource.cachedImage())
        renderImage.setImageSizeForAltText();
}

Node::InsertedIntoAncestorResult HTMLImageElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    if (m_formSetByParser) {
        m_form = WTFMove(m_formSetByParser);
        m_form->registerImgElement(this);
    }

    if (m_form && rootElement() != m_form->rootElement()) {
        m_form->removeImgElement(this);
        m_form = nullptr;
    }

    if (!m_form) {
        if (auto* newForm = HTMLFormElement::findClosestFormAncestor(*this)) {
            m_form = newForm;
            newForm->registerImgElement(this);
        }
    }

    // Insert needs to complete first, before we start updating the loader. Loader dispatches events which could result
    // in callbacks back to this node.
    Node::InsertedIntoAncestorResult insertNotificationRequest = HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (insertionType.treeScopeChanged && !m_parsedUsemap.isNull())
        treeScope().addImageElementByUsemap(*m_parsedUsemap.impl(), *this);

    if (is<HTMLPictureElement>(&parentOfInsertedTree) && &parentOfInsertedTree == parentElement()) {
        // FIXME: When the hack in HTMLConstructionSite::createHTMLElementOrFindCustomElementInterface to eagerly call setPictureElement is removed, we can just assert !pictureElement().
        ASSERT(!pictureElement() || pictureElement() == &parentOfInsertedTree);
        setPictureElement(&downcast<HTMLPictureElement>(parentOfInsertedTree));
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
    if (m_form)
        m_form->removeImgElement(this);

    if (removalType.treeScopeChanged && !m_parsedUsemap.isNull())
        oldParentOfRemovedTree.treeScope().removeImageElementByUsemap(*m_parsedUsemap.impl(), *this);

    if (is<HTMLPictureElement>(oldParentOfRemovedTree) && !parentElement()) {
        ASSERT(pictureElement() == &oldParentOfRemovedTree);
        setPictureElement(nullptr);
        selectImageSource(RelevantMutation::Yes);
    }

    m_form = nullptr;
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

HTMLPictureElement* HTMLImageElement::pictureElement() const
{
    return m_pictureElement.get();
}
    
void HTMLImageElement::setPictureElement(HTMLPictureElement* pictureElement)
{
    m_pictureElement = pictureElement;
}
    
unsigned HTMLImageElement::width(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        auto optionalWidth = parseHTMLNonNegativeInteger(attributeWithoutSynchronization(widthAttr));
        if (optionalWidth)
            return optionalWidth.value();

        // if the image is available, use its width
        if (m_imageLoader->image())
            return m_imageLoader->image()->imageSizeForRenderer(renderer(), 1.0f).width().toUnsigned();
    }

    if (ignorePendingStylesheets)
        document().updateLayoutIgnorePendingStylesheets();
    else
        document().updateLayout();

    RenderBox* box = renderBox();
    if (!box)
        return 0;
    LayoutRect contentRect = box->contentBoxRect();
    return adjustForAbsoluteZoom(snappedIntRect(contentRect).width(), *box);
}

unsigned HTMLImageElement::height(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        auto optionalHeight = parseHTMLNonNegativeInteger(attributeWithoutSynchronization(heightAttr));
        if (optionalHeight)
            return optionalHeight.value();

        // if the image is available, use its height
        if (m_imageLoader->image())
            return m_imageLoader->image()->imageSizeForRenderer(renderer(), 1.0f).height().toUnsigned();
    }

    if (ignorePendingStylesheets)
        document().updateLayoutIgnorePendingStylesheets();
    else
        document().updateLayout();

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

int HTMLImageElement::naturalWidth() const
{
    if (!m_imageLoader->image())
        return 0;

    return m_imageLoader->image()->unclampedImageSizeForRenderer(renderer(), effectiveImageDevicePixelRatio()).width();
}

int HTMLImageElement::naturalHeight() const
{
    if (!m_imageLoader->image())
        return 0;

    return m_imageLoader->image()->unclampedImageSizeForRenderer(renderer(), effectiveImageDevicePixelRatio()).height();
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
                result.append(", ");
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

bool HTMLImageElement::matchesUsemap(const AtomStringImpl& name) const
{
    return m_parsedUsemap.impl() == &name;
}

HTMLMapElement* HTMLImageElement::associatedMapElement() const
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
    document().updateLayoutIgnorePendingStylesheets();
    auto renderer = this->renderer();
    if (!renderer)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    return renderer->localToAbsolute().x();
}

int HTMLImageElement::y() const
{
    document().updateLayoutIgnorePendingStylesheets();
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
    case DecodingMode::Synchronous:
        return "sync"_s;
    case DecodingMode::Asynchronous:
        return "async"_s;
    case DecodingMode::Auto:
        break;
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

void HTMLImageElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
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

    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(usemap)).isEmpty();
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
    auto* cachedImage = this->cachedImage();
    if (!cachedImage)
        return true;

    auto image = cachedImage->image();
    return !image || image->sourceURL().protocolIsData() || cachedImage->isCORSSameOrigin();
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

void HTMLImageElement::setAllowsAnimation(bool allowsAnimation)
{
    if (auto* image = this->image())
        return image->setAllowsAnimation(allowsAnimation);
}

void HTMLImageElement::resetAllowsAnimation()
{
    if (auto* image = this->image())
        return image->setAllowsAnimation(std::nullopt);
}

#if ENABLE(ATTACHMENT_ELEMENT)

void HTMLImageElement::didUpdateAttachmentIdentifier()
{
    m_pendingClonedAttachmentID = { };
}

void HTMLImageElement::setAttachmentElement(Ref<HTMLAttachmentElement>&& attachment)
{
    if (auto existingAttachment = attachmentElement())
        existingAttachment->remove();

    attachment->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);
    ensureUserAgentShadowRoot().appendChild(WTFMove(attachment));
#if ENABLE(SERVICE_CONTROLS)
    setImageMenuEnabled(true);
#endif
}

RefPtr<HTMLAttachmentElement> HTMLImageElement::attachmentElement() const
{
    if (auto shadowRoot = userAgentShadowRoot())
        return childrenOfType<HTMLAttachmentElement>(*shadowRoot).first();

    return nullptr;
}

const String& HTMLImageElement::attachmentIdentifier() const
{
    if (!m_pendingClonedAttachmentID.isEmpty())
        return m_pendingClonedAttachmentID;

    if (auto attachment = attachmentElement())
        return attachment->uniqueIdentifier();

    return nullAtom();
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
    if (is<HTMLAnchorElement>(parent))
        return downcast<HTMLAnchorElement>(parent)->isSystemPreviewLink();
    if (is<HTMLPictureElement>(parent))
        return downcast<HTMLPictureElement>(parent)->isSystemPreviewImage();
    return false;
}
#endif

void HTMLImageElement::copyNonAttributePropertiesFromElement(const Element& source)
{
    auto& sourceImage = static_cast<const HTMLImageElement&>(source);
#if ENABLE(ATTACHMENT_ELEMENT)
    m_pendingClonedAttachmentID = !sourceImage.m_pendingClonedAttachmentID.isEmpty() ? sourceImage.m_pendingClonedAttachmentID : sourceImage.attachmentIdentifier();
#else
    UNUSED_PARAM(sourceImage);
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

const char* HTMLImageElement::activeDOMObjectName() const
{
    return "HTMLImageElement";
}

bool HTMLImageElement::virtualHasPendingActivity() const
{
    return m_imageLoader->hasPendingActivity();
}

size_t HTMLImageElement::pendingDecodePromisesCountForTesting() const
{
    return m_imageLoader->pendingDecodePromisesCountForTesting();
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
    if (!document().frame() || !document().frame()->script().canExecuteScripts(NotAboutToExecuteScript))
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
    if (document().settings().referrerPolicyAttributeEnabled())
        return parseReferrerPolicy(attributeWithoutSynchronization(referrerpolicyAttr), ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
    return ReferrerPolicy::EmptyString;
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

Ref<Element> HTMLImageElement::cloneElementWithoutAttributesAndChildren(Document& targetDocument)
{
    auto clone = create(targetDocument);
#if ENABLE(ATTACHMENT_ELEMENT)
    if (auto attachment = attachmentElement()) {
        auto attachmentClone = attachment->cloneElementWithoutChildren(targetDocument);
        RELEASE_ASSERT(is<HTMLAttachmentElement>(attachmentClone));
        clone->setAttachmentElement(downcast<HTMLAttachmentElement>(attachmentClone.get()));
    }
#endif
    return clone;
}

}

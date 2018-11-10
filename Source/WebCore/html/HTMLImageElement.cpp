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
#include "ElementIterator.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLPictureElement.h"
#include "HTMLMapElement.h"
#include "HTMLSourceElement.h"
#include "HTMLSrcsetParser.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "NodeTraversal.h"
#include "RenderImage.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SizesAttributeParser.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsRootElement.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLImageElement);

using namespace HTMLNames;

typedef HashMap<const HTMLImageElement*, WeakPtr<HTMLPictureElement>> PictureOwnerMap;
static PictureOwnerMap* gPictureOwnerMap = nullptr;

HTMLImageElement::HTMLImageElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLElement(tagName, document)
    , m_imageLoader(*this)
    , m_form(nullptr)
    , m_formSetByParser(form)
    , m_compositeOperator(CompositeSourceOver)
    , m_imageDevicePixelRatio(1.0f)
#if ENABLE(SERVICE_CONTROLS)
    , m_experimentalImageMenuEnabled(false)
#endif
{
    ASSERT(hasTagName(imgTag));
    setHasCustomStyleResolveCallbacks();
}

Ref<HTMLImageElement> HTMLImageElement::create(Document& document)
{
    return adoptRef(*new HTMLImageElement(imgTag, document));
}

Ref<HTMLImageElement> HTMLImageElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    return adoptRef(*new HTMLImageElement(tagName, document, form));
}

HTMLImageElement::~HTMLImageElement()
{
    if (m_form)
        m_form->removeImgElement(this);
    setPictureElement(nullptr);
}

Ref<HTMLImageElement> HTMLImageElement::createForJSConstructor(Document& document, std::optional<unsigned> width, std::optional<unsigned> height)
{
    auto image = adoptRef(*new HTMLImageElement(imgTag, document));
    if (width)
        image->setWidth(width.value());
    if (height)
        image->setHeight(height.value());
    return image;
}

bool HTMLImageElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == borderAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr || name == valignAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLImageElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == borderAttr)
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
        addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign, value);
    else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

const AtomicString& HTMLImageElement::imageSourceURL() const
{
    return m_bestFitImageURL.isEmpty() ? attributeWithoutSynchronization(srcAttr) : m_bestFitImageURL;
}

void HTMLImageElement::setBestFitURLAndDPRFromImageCandidate(const ImageCandidate& candidate)
{
    m_bestFitImageURL = candidate.string.toAtomicString();
    m_currentSrc = AtomicString(document().completeURL(imageSourceURL()).string());
    if (candidate.density >= 0)
        m_imageDevicePixelRatio = 1 / candidate.density;
    if (is<RenderImage>(renderer()))
        downcast<RenderImage>(*renderer()).setImageDevicePixelRatio(m_imageDevicePixelRatio);
}

ImageCandidate HTMLImageElement::bestFitSourceFromPictureElement()
{
    auto picture = makeRefPtr(pictureElement());
    if (!picture)
        return { };

    picture->clearViewportDependentResults();
    document().removeViewportDependentPicture(*picture);

    picture->clearAppearanceDependentResults();
    document().removeAppearanceDependentPicture(*picture);

    for (RefPtr<Node> child = picture->firstChild(); child && child != this; child = child->nextSibling()) {
        if (!is<HTMLSourceElement>(*child))
            continue;
        auto& source = downcast<HTMLSourceElement>(*child);

        auto& srcset = source.attributeWithoutSynchronization(srcsetAttr);
        if (srcset.isEmpty())
            continue;

        auto& typeAttribute = source.attributeWithoutSynchronization(typeAttr);
        if (!typeAttribute.isNull()) {
            String type = typeAttribute.string();
            type.truncate(type.find(';'));
            type = stripLeadingAndTrailingHTMLSpaces(type);
            if (!type.isEmpty() && !MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(type))
                continue;
        }

        auto documentElement = makeRefPtr(document().documentElement());
        MediaQueryEvaluator evaluator { document().printing() ? "print" : "screen", document(), documentElement ? documentElement->computedStyle() : nullptr };
        auto* queries = source.parsedMediaAttribute(document());
        LOG(MediaQueries, "HTMLImageElement %p bestFitSourceFromPictureElement evaluating media queries", this);
        auto evaluation = !queries || evaluator.evaluate(*queries, picture->viewportDependentResults(), picture->appearanceDependentResults());
        if (picture->hasViewportDependentResults())
            document().addViewportDependentPicture(*picture);
        if (picture->hasAppearanceDependentResults())
            document().addAppearanceDependentPicture(*picture);
        if (!evaluation)
            continue;

        auto sourceSize = SizesAttributeParser(source.attributeWithoutSynchronization(sizesAttr).string(), document()).length();
        auto candidate = bestFitSourceForImageAttributes(document().deviceScaleFactor(), nullAtom(), srcset, sourceSize);
        if (!candidate.isEmpty())
            return candidate;
    }
    return { };
}

void HTMLImageElement::selectImageSource()
{
    // First look for the best fit source from our <picture> parent if we have one.
    ImageCandidate candidate = bestFitSourceFromPictureElement();
    if (candidate.isEmpty()) {
        // If we don't have a <picture> or didn't find a source, then we use our own attributes.
        auto sourceSize = SizesAttributeParser(attributeWithoutSynchronization(sizesAttr).string(), document()).length();
        candidate = bestFitSourceForImageAttributes(document().deviceScaleFactor(), attributeWithoutSynchronization(srcAttr), attributeWithoutSynchronization(srcsetAttr), sourceSize);
    }
    setBestFitURLAndDPRFromImageCandidate(candidate);
    m_imageLoader.updateFromElementIgnoringPreviousError();
}

void HTMLImageElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == altAttr) {
        if (is<RenderImage>(renderer()))
            downcast<RenderImage>(*renderer()).updateAltText();
    } else if (name == srcAttr || name == srcsetAttr || name == sizesAttr)
        selectImageSource();
    else if (name == usemapAttr) {
        if (isConnected() && !m_parsedUsemap.isNull())
            treeScope().removeImageElementByUsemap(*m_parsedUsemap.impl(), *this);

        m_parsedUsemap = parseHTMLHashNameReference(value);

        if (isConnected() && !m_parsedUsemap.isNull())
            treeScope().addImageElementByUsemap(*m_parsedUsemap.impl(), *this);
    } else if (name == compositeAttr) {
        // FIXME: images don't support blend modes in their compositing attribute.
        BlendMode blendOp = BlendMode::Normal;
        if (!parseCompositeAndBlendOperator(value, m_compositeOperator, blendOp))
            m_compositeOperator = CompositeSourceOver;
#if ENABLE(SERVICE_CONTROLS)
    } else if (name == webkitimagemenuAttr) {
        m_experimentalImageMenuEnabled = !value.isNull();
        updateImageControls();
#endif
    } else {
        if (name == nameAttr) {
            bool willHaveName = !value.isNull();
            if (m_hadNameBeforeAttributeChanged != willHaveName && isConnected() && !isInShadowTree() && is<HTMLDocument>(document())) {
                HTMLDocument& document = downcast<HTMLDocument>(this->document());
                const AtomicString& id = getIdAttribute();
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

const AtomicString& HTMLImageElement::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    const AtomicString& alt = attributeWithoutSynchronization(altAttr);
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

void HTMLImageElement::didAttachRenderers()
{
    if (!is<RenderImage>(renderer()))
        return;
    if (m_imageLoader.hasPendingBeforeLoadEvent())
        return;

#if ENABLE(SERVICE_CONTROLS)
    updateImageControls();
#endif

    auto& renderImage = downcast<RenderImage>(*renderer());
    RenderImageResource& renderImageResource = renderImage.imageResource();
    if (renderImageResource.cachedImage())
        return;
    renderImageResource.setCachedImage(m_imageLoader.image());

    // If we have no image at all because we have no src attribute, set
    // image height and width for the alt text instead.
    if (!m_imageLoader.image() && !renderImageResource.cachedImage())
        renderImage.setImageSizeForAltText();
}

Node::InsertedIntoAncestorResult HTMLImageElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    if (m_formSetByParser) {
        m_form = m_formSetByParser;
        m_formSetByParser = nullptr;
        m_form->registerImgElement(this);
    }

    if (m_form && rootElement() != m_form->rootElement()) {
        m_form->removeImgElement(this);
        m_form = nullptr;
    }

    if (!m_form) {
        m_form = HTMLFormElement::findClosestFormAncestor(*this);
        if (m_form)
            m_form->registerImgElement(this);
    }
    // Insert needs to complete first, before we start updating the loader. Loader dispatches events which could result
    // in callbacks back to this node.
    Node::InsertedIntoAncestorResult insertNotificationRequest = HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (insertionType.connectedToDocument && !m_parsedUsemap.isNull())
        treeScope().addImageElementByUsemap(*m_parsedUsemap.impl(), *this);

    if (is<HTMLPictureElement>(parentNode())) {
        setPictureElement(&downcast<HTMLPictureElement>(*parentNode()));
        selectImageSource();
    }

    // If we have been inserted from a renderer-less document,
    // our loader may have not fetched the image, so do it now.
    if (insertionType.connectedToDocument && !m_imageLoader.image())
        m_imageLoader.updateFromElement();

    return insertNotificationRequest;
}

void HTMLImageElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (m_form)
        m_form->removeImgElement(this);

    if (removalType.disconnectedFromDocument && !m_parsedUsemap.isNull())
        oldParentOfRemovedTree.treeScope().removeImageElementByUsemap(*m_parsedUsemap.impl(), *this);

    if (is<HTMLPictureElement>(parentNode()))
        setPictureElement(nullptr);

    m_form = nullptr;
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

HTMLPictureElement* HTMLImageElement::pictureElement() const
{
    if (!gPictureOwnerMap || !gPictureOwnerMap->contains(this))
        return nullptr;
    auto result = gPictureOwnerMap->get(this);
    if (!result)
        gPictureOwnerMap->remove(this);
    return result.get();
}
    
void HTMLImageElement::setPictureElement(HTMLPictureElement* pictureElement)
{
    if (!pictureElement) {
        if (gPictureOwnerMap)
            gPictureOwnerMap->remove(this);
        return;
    }
    
    if (!gPictureOwnerMap)
        gPictureOwnerMap = new PictureOwnerMap();
    gPictureOwnerMap->add(this, makeWeakPtr(*pictureElement));
}
    
unsigned HTMLImageElement::width(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        auto optionalWidth = parseHTMLNonNegativeInteger(attributeWithoutSynchronization(widthAttr));
        if (optionalWidth)
            return optionalWidth.value();

        // if the image is available, use its width
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).width().toUnsigned();
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
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).height().toUnsigned();
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

int HTMLImageElement::naturalWidth() const
{
    if (!m_imageLoader.image())
        return 0;

    return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).width();
}

int HTMLImageElement::naturalHeight() const
{
    if (!m_imageLoader.image())
        return 0;

    return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).height();
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

String HTMLImageElement::completeURLsInAttributeValue(const URL& base, const Attribute& attribute) const
{
    if (attribute.name() == srcsetAttr) {
        Vector<ImageCandidate> imageCandidates = parseImageCandidatesFromSrcsetAttribute(StringView(attribute.value()));
        StringBuilder result;
        for (const auto& candidate : imageCandidates) {
            if (&candidate != &imageCandidates[0])
                result.appendLiteral(", ");
            result.append(URL(base, candidate.string.toString()).string());
            if (candidate.density != UninitializedDescriptor) {
                result.append(' ');
                result.appendNumber(candidate.density);
                result.append('x');
            }
            if (candidate.resourceWidth != UninitializedDescriptor) {
                result.append(' ');
                result.appendNumber(candidate.resourceWidth);
                result.append('w');
            }
        }
        return result.toString();
    }
    return HTMLElement::completeURLsInAttributeValue(base, attribute);
}

bool HTMLImageElement::matchesUsemap(const AtomicStringImpl& name) const
{
    return m_parsedUsemap.impl() == &name;
}

HTMLMapElement* HTMLImageElement::associatedMapElement() const
{
    return treeScope().getImageMap(m_parsedUsemap);
}

const AtomicString& HTMLImageElement::alt() const
{
    return attributeWithoutSynchronization(altAttr);
}

bool HTMLImageElement::draggable() const
{
    // Image elements are draggable by default.
    return !equalLettersIgnoringASCIICase(attributeWithoutSynchronization(draggableAttr), "false");
}

void HTMLImageElement::setHeight(unsigned value)
{
    setUnsignedIntegralAttribute(heightAttr, value);
}

URL HTMLImageElement::src() const
{
    return document().completeURL(attributeWithoutSynchronization(srcAttr));
}

void HTMLImageElement::setSrc(const String& value)
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
    return m_imageLoader.imageComplete();
}

DecodingMode HTMLImageElement::decodingMode() const
{
    const AtomicString& decodingMode = attributeWithoutSynchronization(decodingAttr);
    if (equalLettersIgnoringASCIICase(decodingMode, "sync"))
        return DecodingMode::Synchronous;
    if (equalLettersIgnoringASCIICase(decodingMode, "async"))
        return DecodingMode::Asynchronous;
    return DecodingMode::Auto;
}
    
void HTMLImageElement::decode(Ref<DeferredPromise>&& promise)
{
    return m_imageLoader.decode(WTFMove(promise));
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
    m_imageLoader.elementDidMoveToNewDocument();
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
}

bool HTMLImageElement::isServerMap() const
{
    if (!hasAttributeWithoutSynchronization(ismapAttr))
        return false;

    const AtomicString& usemap = attributeWithoutSynchronization(usemapAttr);

    // If the usemap attribute starts with '#', it refers to a map element in the document.
    if (usemap.string()[0] == '#')
        return false;

    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(usemap)).isEmpty();
}

void HTMLImageElement::setCrossOrigin(const AtomicString& value)
{
    setAttributeWithoutSynchronization(crossoriginAttr, value);
}

String HTMLImageElement::crossOrigin() const
{
    return parseCORSSettingsAttribute(attributeWithoutSynchronization(crossoriginAttr));
}

#if ENABLE(ATTACHMENT_ELEMENT)

void HTMLImageElement::setAttachmentElement(Ref<HTMLAttachmentElement>&& attachment)
{
    if (auto existingAttachment = attachmentElement())
        existingAttachment->remove();

    attachment->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);
    ensureUserAgentShadowRoot().appendChild(WTFMove(attachment));
}

RefPtr<HTMLAttachmentElement> HTMLImageElement::attachmentElement() const
{
    if (auto shadowRoot = userAgentShadowRoot())
        return childrenOfType<HTMLAttachmentElement>(*shadowRoot).first();

    return nullptr;
}

const String& HTMLImageElement::attachmentIdentifier() const
{
    if (auto attachment = attachmentElement())
        return attachment->uniqueIdentifier();

    return nullAtom();
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(SERVICE_CONTROLS)
void HTMLImageElement::updateImageControls()
{
    // If this image element is inside a shadow tree then it is part of an image control.
    if (isInShadowTree())
        return;

    if (!document().settings().imageControlsEnabled())
        return;

    bool hasControls = hasImageControls();
    if (!m_experimentalImageMenuEnabled && hasControls)
        destroyImageControls();
    else if (m_experimentalImageMenuEnabled && !hasControls)
        tryCreateImageControls();
}

void HTMLImageElement::tryCreateImageControls()
{
    ASSERT(m_experimentalImageMenuEnabled);
    ASSERT(!hasImageControls());

    auto imageControls = ImageControlsRootElement::tryCreate(document());
    if (!imageControls)
        return;

    ensureUserAgentShadowRoot().appendChild(*imageControls);

    auto* renderObject = renderer();
    if (!renderObject)
        return;

    downcast<RenderImage>(*renderObject).setHasShadowControls(true);
}

void HTMLImageElement::destroyImageControls()
{
    auto shadowRoot = userAgentShadowRoot();
    if (!shadowRoot)
        return;

    if (RefPtr<Node> node = shadowRoot->firstChild()) {
        ASSERT_WITH_SECURITY_IMPLICATION(node->isImageControlsRootElement());
        shadowRoot->removeChild(*node);
    }

    auto* renderObject = renderer();
    if (!renderObject)
        return;

    downcast<RenderImage>(*renderObject).setHasShadowControls(false);
}

bool HTMLImageElement::hasImageControls() const
{
    if (auto shadowRoot = userAgentShadowRoot()) {
        RefPtr<Node> node = shadowRoot->firstChild();
        ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isImageControlsRootElement());
        return node;
    }

    return false;
}

bool HTMLImageElement::childShouldCreateRenderer(const Node& child) const
{
    return hasShadowRootParent(child) && HTMLElement::childShouldCreateRenderer(child);
}
#endif // ENABLE(SERVICE_CONTROLS)

#if PLATFORM(IOS_FAMILY)
// FIXME: This is a workaround for <rdar://problem/7725158>. We should find a better place for the touchCalloutEnabled() logic.
bool HTMLImageElement::willRespondToMouseClickEvents()
{
    auto renderer = this->renderer();
    if (!renderer || renderer->style().touchCalloutEnabled())
        return true;
    return HTMLElement::willRespondToMouseClickEvents();
}
#endif

#if USE(SYSTEM_PREVIEW)
bool HTMLImageElement::isSystemPreviewImage() const
{
    if (!RuntimeEnabledFeatures::sharedFeatures().systemPreviewEnabled())
        return false;

    const auto* parent = parentElement();
    if (is<HTMLAnchorElement>(parent))
        return downcast<HTMLAnchorElement>(parent)->isSystemPreviewLink();
    if (is<HTMLPictureElement>(parent))
        return downcast<HTMLPictureElement>(parent)->isSystemPreviewImage();
    return false;
}
#endif

}

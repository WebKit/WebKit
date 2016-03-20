/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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
#include "EventNames.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLPictureElement.h"
#include "HTMLSourceElement.h"
#include "HTMLSrcsetParser.h"
#include "MIMETypeRegistry.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "RenderImage.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SourceSizeList.h"
#include <wtf/text/StringBuilder.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsRootElement.h"
#endif

namespace WebCore {

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

Ref<HTMLImageElement> HTMLImageElement::createForJSConstructor(Document& document, const int* optionalWidth, const int* optionalHeight)
{
    Ref<HTMLImageElement> image = adoptRef(*new HTMLImageElement(imgTag, document));
    if (optionalWidth)
        image->setWidth(*optionalWidth);
    if (optionalHeight)
        image->setHeight(*optionalHeight);
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
    return m_bestFitImageURL.isEmpty() ? fastGetAttribute(srcAttr) : m_bestFitImageURL;
}

void HTMLImageElement::setBestFitURLAndDPRFromImageCandidate(const ImageCandidate& candidate)
{
    m_bestFitImageURL = candidate.string.toString();
    m_currentSrc = AtomicString(document().completeURL(imageSourceURL()).string());
    if (candidate.density >= 0)
        m_imageDevicePixelRatio = 1 / candidate.density;
    if (is<RenderImage>(renderer()))
        downcast<RenderImage>(*renderer()).setImageDevicePixelRatio(m_imageDevicePixelRatio);
}

ImageCandidate HTMLImageElement::bestFitSourceFromPictureElement()
{
    auto* picture = pictureElement();
    if (!picture)
        return { };
    picture->clearViewportDependentResults();
    document().removeViewportDependentPicture(*picture);
    for (Node* child = picture->firstChild(); child && child != this; child = child->nextSibling()) {
        if (!is<HTMLSourceElement>(*child))
            continue;
        auto& source = downcast<HTMLSourceElement>(*child);
        auto& srcset = source.fastGetAttribute(srcsetAttr);
        if (srcset.isEmpty())
            continue;
        if (source.hasAttribute(typeAttr)) {
            String type = source.fastGetAttribute(typeAttr).string();
            int indexOfSemicolon = type.find(';');
            if (indexOfSemicolon >= 0)
                type.truncate(indexOfSemicolon);
            type = stripLeadingAndTrailingHTMLSpaces(type);
            if (!type.isEmpty() && !MIMETypeRegistry::isSupportedImageMIMEType(type) && !equalLettersIgnoringASCIICase(type, "image/svg+xml"))
                continue;
        }
        MediaQueryEvaluator evaluator(document().printing() ? "print" : "screen", document().frame(), document().documentElement() ? document().documentElement()->computedStyle() : nullptr);
        bool evaluation = evaluator.evalCheckingViewportDependentResults(source.mediaQuerySet(), picture->viewportDependentResults());
        if (picture->hasViewportDependentResults())
            document().addViewportDependentPicture(*picture);
        if (!evaluation)
            continue;

        float sourceSize = parseSizesAttribute(source.fastGetAttribute(sizesAttr).string(), document().renderView(), document().frame());
        ImageCandidate candidate = bestFitSourceForImageAttributes(document().deviceScaleFactor(), nullAtom, source.fastGetAttribute(srcsetAttr), sourceSize);
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
        float sourceSize = parseSizesAttribute(fastGetAttribute(sizesAttr).string(), document().renderView(), document().frame());
        candidate = bestFitSourceForImageAttributes(document().deviceScaleFactor(), fastGetAttribute(srcAttr), fastGetAttribute(srcsetAttr), sourceSize);
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
        if (inDocument() && !m_caseFoldedUsemap.isNull())
            document().removeImageElementByCaseFoldedUsemap(*m_caseFoldedUsemap.impl(), *this);

        // The HTMLImageElement's useMap() value includes the '#' symbol at the beginning, which has to be stripped off.
        // FIXME: We should check that the first character is '#'.
        // FIXME: HTML specification says we should strip any leading string before '#'.
        // FIXME: HTML specification says we should ignore usemap attributes without '#'.
        if (value.length() > 1)
            m_caseFoldedUsemap = value.string().substring(1).foldCase();
        else
            m_caseFoldedUsemap = nullAtom;

        if (inDocument() && !m_caseFoldedUsemap.isNull())
            document().addImageElementByCaseFoldedUsemap(*m_caseFoldedUsemap.impl(), *this);
    } else if (name == compositeAttr) {
        // FIXME: images don't support blend modes in their compositing attribute.
        BlendMode blendOp = BlendModeNormal;
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
            if (m_hadNameBeforeAttributeChanged != willHaveName && inDocument() && is<HTMLDocument>(document())) {
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
    const AtomicString& alt = fastGetAttribute(altAttr);
    if (!alt.isNull())
        return alt;
    // fall back to title attribute
    return fastGetAttribute(titleAttr);
}

RenderPtr<RenderElement> HTMLImageElement::createElementRenderer(Ref<RenderStyle>&& style, const RenderTreePosition&)
{
    if (style.get().hasContent())
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
    if (renderImageResource.hasImage())
        return;
    renderImageResource.setCachedImage(m_imageLoader.image());

    // If we have no image at all because we have no src attribute, set
    // image height and width for the alt text instead.
    if (!m_imageLoader.image() && !renderImageResource.cachedImage())
        renderImage.setImageSizeForAltText();
}

Node::InsertionNotificationRequest HTMLImageElement::insertedInto(ContainerNode& insertionPoint)
{
    if (m_formSetByParser) {
        m_form = m_formSetByParser;
        m_formSetByParser = nullptr;
        m_form->registerImgElement(this);
    }

    if (!m_form) {
        m_form = HTMLFormElement::findClosestFormAncestor(*this);
        if (m_form)
            m_form->registerImgElement(this);
    }
    // Insert needs to complete first, before we start updating the loader. Loader dispatches events which could result
    // in callbacks back to this node.
    Node::InsertionNotificationRequest insertNotificationRequest = HTMLElement::insertedInto(insertionPoint);

    if (insertionPoint.inDocument() && !m_caseFoldedUsemap.isNull())
        document().addImageElementByCaseFoldedUsemap(*m_caseFoldedUsemap.impl(), *this);

    if (is<HTMLPictureElement>(parentNode())) {
        setPictureElement(&downcast<HTMLPictureElement>(*parentNode()));
        selectImageSource();
    }

    // If we have been inserted from a renderer-less document,
    // our loader may have not fetched the image, so do it now.
    if (insertionPoint.inDocument() && !m_imageLoader.image())
        m_imageLoader.updateFromElement();

    return insertNotificationRequest;
}

void HTMLImageElement::removedFrom(ContainerNode& insertionPoint)
{
    if (m_form)
        m_form->removeImgElement(this);

    if (insertionPoint.inDocument() && !m_caseFoldedUsemap.isNull())
        document().removeImageElementByCaseFoldedUsemap(*m_caseFoldedUsemap.impl(), *this);
    
    if (is<HTMLPictureElement>(parentNode()))
        setPictureElement(nullptr);
    
    m_form = nullptr;
    HTMLElement::removedFrom(insertionPoint);
}

HTMLPictureElement* HTMLImageElement::pictureElement() const
{
    if (!gPictureOwnerMap || !gPictureOwnerMap->contains(this))
        return nullptr;
    HTMLPictureElement* result = gPictureOwnerMap->get(this).get();
    if (!result)
        gPictureOwnerMap->remove(this);
    return result;
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
    gPictureOwnerMap->add(this, pictureElement->createWeakPtr());
}
    
int HTMLImageElement::width(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int width = getAttribute(widthAttr).toInt(&ok);
        if (ok)
            return width;

        // if the image is available, use its width
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).width();
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

int HTMLImageElement::height(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int height = getAttribute(heightAttr).toInt(&ok);
        if (ok)
            return height;

        // if the image is available, use its height
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).height();
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
                result.append('x');
            }
        }
        return result.toString();
    }
    return HTMLElement::completeURLsInAttributeValue(base, attribute);
}

bool HTMLImageElement::matchesCaseFoldedUsemap(const AtomicStringImpl& name) const
{
    ASSERT(String(&const_cast<AtomicStringImpl&>(name)).foldCase().impl() == &name);
    return m_caseFoldedUsemap.impl() == &name;
}

const AtomicString& HTMLImageElement::alt() const
{
    return fastGetAttribute(altAttr);
}

bool HTMLImageElement::draggable() const
{
    // Image elements are draggable by default.
    return !equalLettersIgnoringASCIICase(fastGetAttribute(draggableAttr), "false");
}

void HTMLImageElement::setHeight(int value)
{
    setIntegralAttribute(heightAttr, value);
}

URL HTMLImageElement::src() const
{
    return document().completeURL(fastGetAttribute(srcAttr));
}

void HTMLImageElement::setSrc(const String& value)
{
    setAttribute(srcAttr, value);
}

void HTMLImageElement::setWidth(int value)
{
    setIntegralAttribute(widthAttr, value);
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

void HTMLImageElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, src());
    // FIXME: What about when the usemap attribute begins with "#"?
    addSubresourceURL(urls, document().completeURL(fastGetAttribute(usemapAttr)));
}

void HTMLImageElement::didMoveToNewDocument(Document* oldDocument)
{
    m_imageLoader.elementDidMoveToNewDocument();
    HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLImageElement::isServerMap() const
{
    if (!fastHasAttribute(ismapAttr))
        return false;

    const AtomicString& usemap = fastGetAttribute(usemapAttr);

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
    return parseCORSSettingsAttribute(fastGetAttribute(crossoriginAttr));
}

#if ENABLE(SERVICE_CONTROLS)
void HTMLImageElement::updateImageControls()
{
    // If this image element is inside a shadow tree then it is part of an image control.
    if (isInShadowTree())
        return;

    Settings* settings = document().settings();
    if (!settings || !settings->imageControlsEnabled())
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

    ensureUserAgentShadowRoot().appendChild(imageControls.releaseNonNull());

    auto* renderObject = renderer();
    if (!renderObject)
        return;

    downcast<RenderImage>(*renderObject).setHasShadowControls(true);
}

void HTMLImageElement::destroyImageControls()
{
    ShadowRoot* shadowRoot = userAgentShadowRoot();
    if (!shadowRoot)
        return;

    if (Node* node = shadowRoot->firstChild()) {
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
    if (ShadowRoot* shadowRoot = userAgentShadowRoot()) {
        Node* node = shadowRoot->firstChild();
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

#if PLATFORM(IOS)
// FIXME: This is a workaround for <rdar://problem/7725158>. We should find a better place for the touchCalloutEnabled() logic.
bool HTMLImageElement::willRespondToMouseClickEvents()
{
    auto renderer = this->renderer();
    if (!renderer || renderer->style().touchCalloutEnabled())
        return true;
    return HTMLElement::willRespondToMouseClickEvents();
}
#endif

}

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

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "ElementShadow.h"
#include "EventNames.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ImageInnerElement.h"
#include "RenderImage.h"
#include "ScriptEventListener.h"
#include "ShadowRoot.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

void ImageElement::setImageIfNecessary(RenderObject* renderObject, ImageLoader* imageLoader)
{
    if (renderObject && renderObject->isImage() && !imageLoader->hasPendingBeforeLoadEvent()) {
        RenderImage* renderImage = toRenderImage(renderObject);
        RenderImageResource* renderImageResource = renderImage->imageResource();
        if (renderImageResource->hasImage())
            return;
        renderImageResource->setCachedImage(imageLoader->image());

        // If we have no image at all because we have no src attribute, set
        // image height and width for the alt text instead.
        if (!imageLoader->image() && !renderImageResource->cachedImage())
            renderImage->setImageSizeForAltText();
    }
}

RenderObject* ImageElement::createRendererForImage(HTMLElement* element, RenderArena* arena)
{
    RenderImage* image = new (arena) RenderImage(element);
    image->setImageResource(RenderImageResource::create());
    return image;
}

HTMLImageElement::HTMLImageElement(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
    : HTMLElement(tagName, document)
    , m_imageLoader(this)
    , m_form(form)
    , m_compositeOperator(CompositeSourceOver)
{
    ASSERT(hasTagName(imgTag));
    if (form)
        form->registerImgElement(this);
}

PassRefPtr<HTMLImageElement> HTMLImageElement::create(Document* document)
{
    return adoptRef(new HTMLImageElement(imgTag, document));
}

PassRefPtr<HTMLImageElement> HTMLImageElement::create(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
{
    return adoptRef(new HTMLImageElement(tagName, document, form));
}

HTMLImageElement::~HTMLImageElement()
{
    if (m_form)
        m_form->removeImgElement(this);
}

void HTMLImageElement::willAddAuthorShadowRoot()
{
    if (userAgentShadowRoot())
        return;

    createShadowSubtree();
}

void HTMLImageElement::createShadowSubtree()
{
    RefPtr<ImageInnerElement> innerElement = ImageInnerElement::create(document());
    
    RefPtr<ShadowRoot> root = ShadowRoot::create(this, ShadowRoot::UserAgentShadowRoot);
    root->appendChild(innerElement);
}

Element* HTMLImageElement::imageElement()
{
    if (ShadowRoot* root = userAgentShadowRoot()) {
        ASSERT(root->firstChild()->hasTagName(webkitInnerImageTag));
        return toElement(root->firstChild());
    }

    return this;
}

PassRefPtr<HTMLImageElement> HTMLImageElement::createForJSConstructor(Document* document, const int* optionalWidth, const int* optionalHeight)
{
    RefPtr<HTMLImageElement> image = adoptRef(new HTMLImageElement(imgTag, document));
    if (optionalWidth)
        image->setWidth(*optionalWidth);
    if (optionalHeight)
        image->setHeight(*optionalHeight);
    return image.release();
}

bool HTMLImageElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == borderAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr || name == valignAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLImageElement::collectStyleForAttribute(const Attribute& attribute, StylePropertySet* style)
{
    if (attribute.name() == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, attribute.value());
    else if (attribute.name() == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, attribute.value());
    else if (attribute.name() == borderAttr)
        applyBorderAttributeToStyle(attribute, style);
    else if (attribute.name() == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, attribute.value());
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, attribute.value());
    } else if (attribute.name() == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, attribute.value());
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, attribute.value());
    } else if (attribute.name() == alignAttr)
        applyAlignmentAttributeToStyle(attribute, style);
    else if (attribute.name() == valignAttr)
        addPropertyToAttributeStyle(style, CSSPropertyVerticalAlign, attribute.value());
    else
        HTMLElement::collectStyleForAttribute(attribute, style);
}

void HTMLImageElement::parseAttribute(const Attribute& attribute)
{
    if (attribute.name() == altAttr) {
        RenderObject* renderObject = shadow() ? innerElement()->renderer() : renderer();
        if (renderObject && renderObject->isImage())
            toRenderImage(renderObject)->updateAltText();
    } else if (attribute.name() == srcAttr) {
        m_imageLoader.updateFromElementIgnoringPreviousError();
        if (ElementShadow* elementShadow = shadow())
            elementShadow->invalidateDistribution();
    } else if (attribute.name() == usemapAttr)
        setIsLink(!attribute.isNull());
    else if (attribute.name() == onloadAttr)
        setAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(this, attribute));
    else if (attribute.name() == onbeforeloadAttr)
        setAttributeEventListener(eventNames().beforeloadEvent, createAttributeEventListener(this, attribute));
    else if (attribute.name() == compositeAttr) {
        if (!parseCompositeOperator(attribute.value(), m_compositeOperator))
            m_compositeOperator = CompositeSourceOver;
    } else
        HTMLElement::parseAttribute(attribute);
}

String HTMLImageElement::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    String alt = getAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
    return alt;
}

RenderObject* HTMLImageElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (style->hasContent() || shadow())
        return RenderObject::createObject(this, style);

    return createRendererForImage(this, arena);
}

bool HTMLImageElement::canStartSelection() const
{
    if (shadow())
        return HTMLElement::canStartSelection();

    return false;
}

void HTMLImageElement::attach()
{
    HTMLElement::attach();
    setImageIfNecessary(renderer(), imageLoader());
}

Node::InsertionNotificationRequest HTMLImageElement::insertedInto(ContainerNode* insertionPoint)
{
    if (!m_form) {
        // m_form can be non-null if it was set in constructor.
        for (ContainerNode* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
            if (ancestor->hasTagName(formTag)) {
                m_form = static_cast<HTMLFormElement*>(ancestor);
                m_form->registerImgElement(this);
                break;
            }
        }
    }

    // If we have been inserted from a renderer-less document,
    // our loader may have not fetched the image, so do it now.
    if (insertionPoint->inDocument() && !m_imageLoader.image())
        m_imageLoader.updateFromElement();

    return HTMLElement::insertedInto(insertionPoint);
}

void HTMLImageElement::removedFrom(ContainerNode* insertionPoint)
{
    if (m_form)
        m_form->removeImgElement(this);
    m_form = 0;
    HTMLElement::removedFrom(insertionPoint);
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
        document()->updateLayoutIgnorePendingStylesheets();
    else
        document()->updateLayout();

    RenderBox* box = renderBox();
    return box ? adjustForAbsoluteZoom(box->contentBoxRect().pixelSnappedWidth(), box) : 0;
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
        document()->updateLayoutIgnorePendingStylesheets();
    else
        document()->updateLayout();

    RenderBox* box = renderBox();
    return box ? adjustForAbsoluteZoom(box->contentBoxRect().pixelSnappedHeight(), box) : 0;
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

const AtomicString& HTMLImageElement::alt() const
{
    return getAttribute(altAttr);
}

bool HTMLImageElement::draggable() const
{
    // Image elements are draggable by default.
    return !equalIgnoringCase(getAttribute(draggableAttr), "false");
}

void HTMLImageElement::setHeight(int value)
{
    setAttribute(heightAttr, String::number(value));
}

KURL HTMLImageElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLImageElement::setSrc(const String& value)
{
    setAttribute(srcAttr, value);
}

void HTMLImageElement::setWidth(int value)
{
    setAttribute(widthAttr, String::number(value));
}

int HTMLImageElement::x() const
{
    RenderObject* r = renderer();
    if (!r)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = r->localToAbsolute();
    return absPos.x();
}

int HTMLImageElement::y() const
{
    RenderObject* r = renderer();
    if (!r)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = r->localToAbsolute();
    return absPos.y();
}

bool HTMLImageElement::complete() const
{
    return m_imageLoader.imageComplete();
}

void HTMLImageElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, src());
    // FIXME: What about when the usemap attribute begins with "#"?
    addSubresourceURL(urls, document()->completeURL(getAttribute(usemapAttr)));
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

    return document()->completeURL(stripLeadingAndTrailingHTMLSpaces(usemap)).isEmpty();
}

#if ENABLE(MICRODATA)
String HTMLImageElement::itemValueText() const
{
    return getURLAttribute(srcAttr);
}

void HTMLImageElement::setItemValueText(const String& value, ExceptionCode&)
{
    setAttribute(srcAttr, value);
}
#endif

inline ImageInnerElement* HTMLImageElement::innerElement() const
{
    ASSERT(userAgentShadowRoot());
    return toImageInnerElement(userAgentShadowRoot()->firstChild());
}

}

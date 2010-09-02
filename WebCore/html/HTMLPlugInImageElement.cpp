/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "HTMLPlugInImageElement.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLImageLoader.h"
#include "Image.h"
#include "RenderEmbeddedObject.h"
#include "RenderImage.h"

namespace WebCore {

HTMLPlugInImageElement::HTMLPlugInImageElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLPlugInElement(tagName, document)
    // m_needsWidgetUpdate(!createdByParser) allows HTMLObjectElement to delay
    // widget updates until after all children are parsed.  For HTMLEmbedElement
    // this delay is unnecessary, but it is simpler to make both classes share
    // the same codepath in this class.
    , m_needsWidgetUpdate(!createdByParser)
{
}

RenderEmbeddedObject* HTMLPlugInImageElement::renderEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers
    // when using fallback content.
    if (!renderer() || !renderer()->isEmbeddedObject())
        return 0;
    return toRenderEmbeddedObject(renderer());
}

bool HTMLPlugInImageElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"))
        m_serviceType = mimeTypeFromDataURL(m_url);

    if (Frame* frame = document()->frame()) {
        KURL completedURL = frame->loader()->completeURL(m_url);
        return frame->loader()->client()->objectContentType(completedURL, m_serviceType) == ObjectContentImage;
    }

    return Image::supportsType(m_serviceType);
}

RenderObject* HTMLPlugInImageElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    // Fallback content breaks the DOM->Renderer class relationship of this
    // class and all superclasses because createObject won't necessarily
    // return a RenderEmbeddedObject, RenderPart or even RenderWidget.
    if (useFallbackContent())
        return RenderObject::createObject(this, style);
    if (isImageType()) {
        RenderImage* image = new (arena) RenderImage(this);
        image->setImageResource(RenderImageResource::create());
        return image;
    }
    return new (arena) RenderEmbeddedObject(this);
}

void HTMLPlugInImageElement::recalcStyle(StyleChange ch)
{
    // FIXME: Why is this necessary?  Manual re-attach is almost always wrong.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType()) {
        detach();
        attach();
    }
    HTMLPlugInElement::recalcStyle(ch);
}

void HTMLPlugInImageElement::attach()
{
    bool isImage = isImageType();
    
    if (!isImage)
        queuePostAttachCallback(&HTMLPlugInImageElement::updateWidgetCallback, this);

    HTMLPlugInElement::attach();

    if (isImage && renderer() && !useFallbackContent()) {
        if (!m_imageLoader)
            m_imageLoader = adoptPtr(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
    }
}
    
void HTMLPlugInImageElement::detach()
{
    // FIXME: Because of the insanity that is HTMLObjectElement::recalcStyle,
    // we can end up detaching during an attach() call, before we even have a
    // renderer.  In that case, don't mark the widget for update.
    if (attached() && renderer() && !useFallbackContent())
        // Update the widget the next time we attach (detaching destroys the plugin).
        setNeedsWidgetUpdate(true);
    HTMLPlugInElement::detach();
}

void HTMLPlugInImageElement::updateWidget()
{
    document()->updateStyleIfNeeded();
    if (needsWidgetUpdate() && renderEmbeddedObject() && !useFallbackContent() && !isImageType())
        renderEmbeddedObject()->updateWidget(true);
}

void HTMLPlugInImageElement::finishParsingChildren()
{
    HTMLPlugInElement::finishParsingChildren();
    if (useFallbackContent())
        return;
    
    setNeedsWidgetUpdate(true);
    if (inDocument())
        setNeedsStyleRecalc();    
}

void HTMLPlugInImageElement::willMoveToNewOwnerDocument()
{
    if (m_imageLoader)
        m_imageLoader->elementWillMoveToNewOwnerDocument();
    HTMLPlugInElement::willMoveToNewOwnerDocument();
}

void HTMLPlugInImageElement::updateWidgetCallback(Node* n)
{
    static_cast<HTMLPlugInImageElement*>(n)->updateWidget();
}

} // namespace WebCore

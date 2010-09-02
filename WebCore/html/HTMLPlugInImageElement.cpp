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

HTMLPlugInImageElement::HTMLPlugInImageElement(const QualifiedName& tagName, Document* document)
    : HTMLPlugInElement(tagName, document)
    , m_needsWidgetUpdate(false)
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

void HTMLPlugInImageElement::updateWidget()
{
    document()->updateStyleIfNeeded();
    if (needsWidgetUpdate() && renderEmbeddedObject() && !useFallbackContent() && !isImageType())
        renderEmbeddedObject()->updateWidget(true);
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

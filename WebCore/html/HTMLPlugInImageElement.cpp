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

namespace WebCore {

HTMLPlugInImageElement::HTMLPlugInImageElement(const QualifiedName& tagName, Document* document)
    : HTMLPlugInElement(tagName, document)
{
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

void HTMLPlugInImageElement::willMoveToNewOwnerDocument()
{
    if (m_imageLoader)
        m_imageLoader->elementWillMoveToNewOwnerDocument();
    HTMLPlugInElement::willMoveToNewOwnerDocument();
}

} // namespace WebCore

/*
 * Copyright (C) 2012 Company 100, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedImageBacking.h"

#include "GraphicsContext.h"

using namespace WebCore;

namespace WebKit {

CoordinatedImageBackingID CoordinatedImageBacking::getCoordinatedImageBackingID(Image* image)
{
    // CoordinatedImageBacking keeps a RefPtr<Image> member, so the same Image pointer can not refer two different instances until CoordinatedImageBacking releases the member.
    return reinterpret_cast<CoordinatedImageBackingID>(image);
}

PassRefPtr<CoordinatedImageBacking> CoordinatedImageBacking::create(Coordinator* client, PassRefPtr<Image> image)
{
    return adoptRef(new CoordinatedImageBacking(client, image));
}

CoordinatedImageBacking::CoordinatedImageBacking(Coordinator* client, PassRefPtr<Image> image)
    : m_coordinator(client)
    , m_image(image)
    , m_id(getCoordinatedImageBackingID(m_image.get()))
    , m_isDirty(false)
{
    // FIXME: We would need to decode a small image directly into a GraphicsSurface.
    // http://webkit.org/b/101426

    m_coordinator->createImageBacking(id());
}

CoordinatedImageBacking::~CoordinatedImageBacking()
{
}

void CoordinatedImageBacking::addLayerClient(CoordinatedGraphicsLayer* layerClient)
{
    ASSERT(!m_layerClients.contains(layerClient));
    m_layerClients.append(layerClient);
}

void CoordinatedImageBacking::removeLayerClient(CoordinatedGraphicsLayer* layerClient)
{
    size_t position = m_layerClients.find(layerClient);
    ASSERT(position != notFound);
    m_layerClients.remove(position);

    if (m_layerClients.isEmpty())
        m_coordinator->removeImageBacking(id());
}

void CoordinatedImageBacking::markDirty()
{
    m_isDirty = true;
}

void CoordinatedImageBacking::update()
{
    releaseSurfaceIfNeeded();

    if (!m_isDirty)
        return;

    m_surface = ShareableSurface::create(m_image->size(), m_image->currentFrameHasAlpha() ? ShareableBitmap::SupportsAlpha : ShareableBitmap::NoFlags, ShareableSurface::SupportsGraphicsSurface);
    m_handle = adoptPtr(new ShareableSurface::Handle());

    if (!m_surface->createHandle(*m_handle)) {
        releaseSurfaceIfNeeded();
        m_isDirty = false;
        return;
    }

    IntRect rect(IntPoint::zero(), m_image->size());
    OwnPtr<GraphicsContext> context = m_surface->createGraphicsContext(rect);
    context->drawImage(m_image.get(), ColorSpaceDeviceRGB, rect, rect);

    m_coordinator->updateImageBacking(id(), *m_handle);
    m_isDirty = false;
}

void CoordinatedImageBacking::releaseSurfaceIfNeeded()
{
    // We must keep m_surface until UI Process reads m_surface.
    // If m_surface exists, it was created in the previous update.
    m_handle.clear();
    m_surface.clear();
}

}
#endif

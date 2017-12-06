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
#include "CoordinatedImageBacking.h"

#if USE(COORDINATED_GRAPHICS)

#include "CoordinatedGraphicsState.h"
#include "GraphicsContext.h"
#include "NicosiaBuffer.h"
#include "NicosiaPaintingContext.h"

namespace WebCore {

CoordinatedImageBackingID CoordinatedImageBacking::getCoordinatedImageBackingID(Image* image)
{
    // CoordinatedImageBacking keeps a RefPtr<Image> member, so the same Image pointer can not refer two different instances until CoordinatedImageBacking releases the member.
    return reinterpret_cast<CoordinatedImageBackingID>(image);
}

Ref<CoordinatedImageBacking> CoordinatedImageBacking::create(Client& client, Ref<Image>&& image)
{
    return adoptRef(*new CoordinatedImageBacking(client, WTFMove(image)));
}

CoordinatedImageBacking::CoordinatedImageBacking(Client& client, Ref<Image>&& image)
    : m_client(&client)
    , m_image(WTFMove(image))
    , m_id(getCoordinatedImageBackingID(m_image.get()))
    , m_clearContentsTimer(*this, &CoordinatedImageBacking::clearContentsTimerFired)
    , m_isDirty(false)
    , m_isVisible(false)
{
    // FIXME: We would need to decode a small image directly into a GraphicsSurface.
    // http://webkit.org/b/101426

    m_client->createImageBacking(id());
}

CoordinatedImageBacking::~CoordinatedImageBacking() = default;

void CoordinatedImageBacking::addHost(Host* host)
{
    ASSERT(!m_hosts.contains(host));
    m_hosts.append(host);
}

void CoordinatedImageBacking::removeHost(Host* host)
{
    size_t position = m_hosts.find(host);
    ASSERT(position != notFound);
    m_hosts.remove(position);

    if (m_hosts.isEmpty())
        m_client->removeImageBacking(id());
}

void CoordinatedImageBacking::markDirty()
{
    m_isDirty = true;
}

void CoordinatedImageBacking::update()
{
    releaseSurfaceIfNeeded();

    bool changedToVisible;
    updateVisibilityIfNeeded(changedToVisible);
    if (!m_isVisible)
        return;

    if (!changedToVisible) {
        if (!m_isDirty)
            return;

        if (m_nativeImagePtr == m_image->nativeImageForCurrentFrame()) {
            m_isDirty = false;
            return;
        }
    }

    m_buffer = Nicosia::Buffer::create(IntSize(m_image->size()), !m_image->currentFrameKnownToBeOpaque() ? Nicosia::Buffer::SupportsAlpha : Nicosia::Buffer::NoFlags);
    ASSERT(m_buffer);

    Nicosia::PaintingContext::paint(*m_buffer,
        [this](GraphicsContext& context)
        {
            IntRect rect(IntPoint::zero(), IntSize(m_image->size()));
            context.save();
            context.clip(rect);
            context.drawImage(*m_image, rect, rect);
            context.restore();
        });

    m_nativeImagePtr = m_image->nativeImageForCurrentFrame();

    m_client->updateImageBacking(id(), m_buffer.copyRef());
    m_isDirty = false;
}

void CoordinatedImageBacking::releaseSurfaceIfNeeded()
{
    m_buffer = nullptr;
}

static const Seconds clearContentsTimerInterval { 3_s };

void CoordinatedImageBacking::updateVisibilityIfNeeded(bool& changedToVisible)
{
    bool previousIsVisible = m_isVisible;

    m_isVisible = false;
    for (auto& host : m_hosts) {
        if (host->imageBackingVisible()) {
            m_isVisible = true;
            break;
        }
    }

    bool changedToInvisible = previousIsVisible && !m_isVisible;
    if (changedToInvisible) {
        ASSERT(!m_clearContentsTimer.isActive());
        m_clearContentsTimer.startOneShot(clearContentsTimerInterval);
    }

    changedToVisible = !previousIsVisible && m_isVisible;

    if (m_isVisible && m_clearContentsTimer.isActive()) {
        m_clearContentsTimer.stop();
        // We don't want to update the texture if we didn't remove the texture.
        changedToVisible = false;
    }
}

void CoordinatedImageBacking::clearContentsTimerFired()
{
    m_client->clearImageBackingContents(id());
}

} // namespace WebCore
#endif

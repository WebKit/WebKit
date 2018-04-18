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

CoordinatedImageBackingID CoordinatedImageBacking::getCoordinatedImageBackingID(Image& image)
{
    // CoordinatedImageBacking keeps a RefPtr<Image> member, so the same Image pointer can not refer two different instances until CoordinatedImageBacking releases the member.
    return reinterpret_cast<CoordinatedImageBackingID>(&image);
}

CoordinatedImageBacking::CoordinatedImageBacking(Client& client, Ref<Image>&& image)
    : m_client(client)
    , m_id(getCoordinatedImageBackingID(image))
    , m_image(WTFMove(image))
    , m_clearContentsTimer(*this, &CoordinatedImageBacking::clearContentsTimerFired)
{
    m_client.createImageBacking(m_id);
}

CoordinatedImageBacking::~CoordinatedImageBacking() = default;

void CoordinatedImageBacking::addHost(Host& host)
{
    ASSERT(!m_hosts.contains(&host));
    m_hosts.add(&host);
}

void CoordinatedImageBacking::removeHost(Host& host)
{
    m_hosts.remove(&host);

    if (m_hosts.isEmpty())
        m_client.removeImageBacking(m_id);
}

static const Seconds clearContentsTimerInterval { 3_s };

void CoordinatedImageBacking::update()
{
    bool previousIsVisible = m_isVisible;
    m_isVisible = std::any_of(m_hosts.begin(), m_hosts.end(),
        [](auto* host)
        {
            return host->imageBackingVisible();
        });

    if (!m_isVisible) {
        if (previousIsVisible) {
            ASSERT(!m_clearContentsTimer.isActive());
            m_clearContentsTimer.startOneShot(clearContentsTimerInterval);
        }
        return;
    }

    bool changedToVisible = !previousIsVisible;
    if (m_clearContentsTimer.isActive()) {
        m_clearContentsTimer.stop();
        // We don't want to update the texture if we didn't remove the texture.
        changedToVisible = false;
    }

    auto nativeImagePtr = m_image->nativeImageForCurrentFrame();
    if (!changedToVisible) {
        if (!m_isDirty)
            return;

        if (m_nativeImagePtr == nativeImagePtr) {
            m_isDirty = false;
            return;
        }
    }

    m_nativeImagePtr = WTFMove(nativeImagePtr);

    auto buffer = Nicosia::Buffer::create(IntSize(m_image->size()), !m_image->currentFrameKnownToBeOpaque() ? Nicosia::Buffer::SupportsAlpha : Nicosia::Buffer::NoFlags);
    Nicosia::PaintingContext::paint(buffer,
        [this](GraphicsContext& context)
        {
            IntRect rect { { }, IntSize { m_image->size() } };
            context.drawImage(m_image, rect, rect, ImagePaintingOptions(CompositeCopy));
        });

    m_client.updateImageBacking(m_id, WTFMove(buffer));
    m_isDirty = false;
}

void CoordinatedImageBacking::clearContentsTimerFired()
{
    m_client.clearImageBackingContents(m_id);
}

} // namespace WebCore

#endif

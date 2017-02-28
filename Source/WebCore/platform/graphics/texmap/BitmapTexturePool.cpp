/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2015 Igalia S.L.
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
#include "BitmapTexturePool.h"

#if USE(TEXTURE_MAPPER_GL)
#include "BitmapTextureGL.h"
#endif

namespace WebCore {

static const double s_releaseUnusedSecondsTolerance = 3;
static const double s_releaseUnusedTexturesTimerInterval = 0.5;

#if USE(TEXTURE_MAPPER_GL)
BitmapTexturePool::BitmapTexturePool(RefPtr<GraphicsContext3D>&& context3D)
    : m_context3D(WTFMove(context3D))
    , m_releaseUnusedTexturesTimer(*this, &BitmapTexturePool::releaseUnusedTexturesTimerFired)
{
}
#endif

RefPtr<BitmapTexture> BitmapTexturePool::acquireTexture(const IntSize& size, const BitmapTexture::Flags flags)
{
    Vector<Entry>& list = flags & BitmapTexture::FBOAttachment ? m_attachmentTextures : m_textures;

    Entry* selectedEntry = std::find_if(list.begin(), list.end(),
        [&size](Entry& entry) { return entry.m_texture->refCount() == 1 && entry.m_texture->size() == size; });

    if (selectedEntry == list.end()) {
        list.append(Entry(createTexture(flags)));
        selectedEntry = &list.last();
    }

    scheduleReleaseUnusedTextures();
    selectedEntry->markIsInUse();
    return selectedEntry->m_texture.copyRef();
}

void BitmapTexturePool::scheduleReleaseUnusedTextures()
{
    if (m_releaseUnusedTexturesTimer.isActive())
        return;

    m_releaseUnusedTexturesTimer.startOneShot(s_releaseUnusedTexturesTimerInterval);
}

void BitmapTexturePool::releaseUnusedTexturesTimerFired()
{
    // Delete entries, which have been unused in s_releaseUnusedSecondsTolerance.
    double minUsedTime = monotonicallyIncreasingTime() - s_releaseUnusedSecondsTolerance;

    if (!m_textures.isEmpty()) {
        std::sort(m_textures.begin(), m_textures.end(),
            [](const Entry& a, const Entry& b) { return a.m_lastUsedTime > b.m_lastUsedTime; });

        for (size_t i = 0; i < m_textures.size(); ++i) {
            if (m_textures[i].m_lastUsedTime < minUsedTime) {
                m_textures.remove(i, m_textures.size() - i);
                break;
            }
        }
    }

    if (!m_attachmentTextures.isEmpty()) {
        std::sort(m_attachmentTextures.begin(), m_attachmentTextures.end(),
            [](const Entry& a, const Entry& b) { return a.m_lastUsedTime > b.m_lastUsedTime; });

        for (size_t i = 0; i < m_attachmentTextures.size(); ++i) {
            if (m_attachmentTextures[i].m_lastUsedTime < minUsedTime) {
                m_attachmentTextures.remove(i, m_attachmentTextures.size() - i);
                break;
            }
        }
    }

    if (!m_textures.isEmpty() || !m_attachmentTextures.isEmpty())
        scheduleReleaseUnusedTextures();
}

RefPtr<BitmapTexture> BitmapTexturePool::createTexture(const BitmapTexture::Flags flags)
{
#if USE(TEXTURE_MAPPER_GL)
    return BitmapTextureGL::create(*m_context3D, flags);
#else
    return nullptr;
#endif
}

} // namespace WebCore

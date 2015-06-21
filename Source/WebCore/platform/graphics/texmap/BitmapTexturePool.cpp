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
#include "GLContext.h"
#endif

namespace WebCore {

const double s_releaseUnusedSecondsTolerance = 3;
const double s_releaseUnusedTexturesTimerInterval = 0.5;

BitmapTexturePool::BitmapTexturePool()
    : m_releaseUnusedTexturesTimer(*this, &BitmapTexturePool::releaseUnusedTexturesTimerFired)
{
}

#if USE(TEXTURE_MAPPER_GL)
BitmapTexturePool::BitmapTexturePool(PassRefPtr<GraphicsContext3D> context)
    : m_context3D(context)
    , m_releaseUnusedTexturesTimer(*this, &BitmapTexturePool::releaseUnusedTexturesTimerFired)
{
}
#endif

void BitmapTexturePool::scheduleReleaseUnusedTextures()
{
    if (m_releaseUnusedTexturesTimer.isActive())
        m_releaseUnusedTexturesTimer.stop();

    m_releaseUnusedTexturesTimer.startOneShot(s_releaseUnusedTexturesTimerInterval);
}

void BitmapTexturePool::releaseUnusedTexturesTimerFired()
{
    if (m_textures.isEmpty())
        return;

    // Delete entries, which have been unused in s_releaseUnusedSecondsTolerance.
    std::sort(m_textures.begin(), m_textures.end(), BitmapTexturePoolEntry::compareTimeLastUsed);

    double minUsedTime = monotonicallyIncreasingTime() - s_releaseUnusedSecondsTolerance;
    for (size_t i = 0; i < m_textures.size(); ++i) {
        if (m_textures[i].m_timeLastUsed < minUsedTime) {
            m_textures.remove(i, m_textures.size() - i);
            break;
        }
    }
}

PassRefPtr<BitmapTexture> BitmapTexturePool::acquireTexture(const IntSize& size)
{
    BitmapTexturePoolEntry* selectedEntry = 0;
    for (auto& entry : m_textures) {
        // If the surface has only one reference (the one in m_textures), we can safely reuse it.
        if (entry.m_texture->refCount() > 1)
            continue;

        if (entry.m_texture->canReuseWith(size)) {
            selectedEntry = &entry;
            break;
        }
    }

    if (!selectedEntry) {
        m_textures.append(BitmapTexturePoolEntry(createTexture()));
        selectedEntry = &m_textures.last();
    }

    scheduleReleaseUnusedTextures();
    selectedEntry->markUsed();
    return selectedEntry->m_texture;
}

PassRefPtr<BitmapTexture> BitmapTexturePool::createTexture()
{
#if USE(TEXTURE_MAPPER_GL)
    BitmapTextureGL* texture = new BitmapTextureGL(m_context3D);
    return adoptRef(texture);
#endif
}

} // namespace WebCore

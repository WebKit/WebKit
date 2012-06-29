/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CCPrioritizedTexture.h"

#include "CCPrioritizedTextureManager.h"
#include "CCPriorityCalculator.h"
#include "LayerRendererChromium.h"
#include <algorithm>

using namespace std;

namespace WebCore {

CCPrioritizedTexture::CCPrioritizedTexture(CCPrioritizedTextureManager* manager, IntSize size, GC3Denum format)
    : m_size(size)
    , m_format(format)
    , m_memorySizeBytes(0)
    , m_priority(CCPriorityCalculator::lowestPriority())
    , m_isAbovePriorityCutoff(false)
    , m_currentBacking(0)
    , m_manager(0)
{
    // m_manager is set in registerTexture() so validity can be checked.
    ASSERT(format || size.isEmpty());
    if (format)
        m_memorySizeBytes = TextureManager::memoryUseBytes(size, format);
    if (manager)
        manager->registerTexture(this);
}

CCPrioritizedTexture::~CCPrioritizedTexture()
{
    if (m_manager)
        m_manager->unregisterTexture(this);
}

void CCPrioritizedTexture::setTextureManager(CCPrioritizedTextureManager* manager)
{
    if (m_manager == manager)
        return;
    if (m_manager)
        m_manager->unregisterTexture(this);
    if (manager)
        manager->registerTexture(this);
}

void CCPrioritizedTexture::setDimensions(IntSize size, GC3Denum format)
{
    if (m_format != format || m_size != size) {
        m_isAbovePriorityCutoff = false;
        m_format = format;
        m_size = size;
        m_memorySizeBytes = TextureManager::memoryUseBytes(size, format);
        ASSERT(m_manager || !m_currentBacking);
        if (m_manager)
            m_manager->returnBackingTexture(this);
    }
}

bool CCPrioritizedTexture::requestLate()
{
    if (!m_manager)
        return false;
    return m_manager->requestLate(this);
}

void CCPrioritizedTexture::acquireBackingTexture(TextureAllocator* allocator)
{
    ASSERT(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        m_manager->acquireBackingTextureIfNeeded(this, allocator);
}

unsigned CCPrioritizedTexture::textureId()
{
    if (m_currentBacking)
        return m_currentBacking->textureId();
    return 0;
}

void CCPrioritizedTexture::bindTexture(CCGraphicsContext* context, TextureAllocator* allocator)
{
    ASSERT(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        acquireBackingTexture(allocator);
    ASSERT(m_currentBacking);
    WebKit::WebGraphicsContext3D* context3d = context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId());
}

void CCPrioritizedTexture::framebufferTexture2D(CCGraphicsContext* context, TextureAllocator* allocator)
{
    ASSERT(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        acquireBackingTexture(allocator);
    ASSERT(m_currentBacking);
    WebKit::WebGraphicsContext3D* context3d = context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    context3d->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, textureId(), 0);
}

void CCPrioritizedTexture::setCurrentBacking(CCPrioritizedTexture::Backing* backing)
{
    if (m_currentBacking == backing)
        return;
    m_currentBacking = backing;
}

} // namespace WebCore


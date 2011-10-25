/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCTextureUpdater.h"

#include "GraphicsContext3D.h"
#include "LayerTextureUpdater.h"
#include "ManagedTexture.h"

using namespace std;

namespace WebCore {

CCTextureUpdater::CCTextureUpdater(TextureAllocator* allocator)
    : m_allocator(allocator)
    , m_entryIndex(0)
{
    ASSERT(m_allocator);
}

CCTextureUpdater::~CCTextureUpdater()
{
}

void CCTextureUpdater::append(ManagedTexture* texture, LayerTextureUpdater* updater, const IntRect& sourceRect, const IntRect& destRect)
{
    ASSERT(texture);
    ASSERT(updater);

    UpdateEntry entry;
    entry.m_texture = texture;
    entry.m_updater = updater;
    entry.m_sourceRect = sourceRect;
    entry.m_destRect = destRect;
    m_entries.append(entry);
}

bool CCTextureUpdater::update(GraphicsContext3D* context, size_t count)
{
    size_t maxIndex = min(m_entryIndex + count, m_entries.size());
    for (; m_entryIndex < maxIndex; ++m_entryIndex) {
        UpdateEntry& entry = m_entries[m_entryIndex];
        entry.m_updater->updateTextureRect(context, m_allocator, entry.m_texture, entry.m_sourceRect, entry.m_destRect);
    }

    if (maxIndex < m_entries.size())
        return true;

    // If no entries left to process, auto-clear.
    clear();
    return false;
}

void CCTextureUpdater::clear()
{
    m_entryIndex = 0;
    m_entries.clear();
}

}

#endif // USE(ACCELERATED_COMPOSITING)

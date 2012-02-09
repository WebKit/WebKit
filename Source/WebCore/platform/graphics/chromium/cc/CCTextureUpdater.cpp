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

void CCTextureUpdater::append(LayerTextureUpdater::Texture* texture, const IntRect& sourceRect, const IntRect& destRect, Vector<UpdateEntry>& entries)
{
    ASSERT(texture);

    UpdateEntry entry;
    entry.m_texture = texture;
    entry.m_sourceRect = sourceRect;
    entry.m_destRect = destRect;
    entries.append(entry);
}

void CCTextureUpdater::append(LayerTextureUpdater::Texture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    append(texture, sourceRect, destRect, m_entries);
}

void CCTextureUpdater::appendPartial(LayerTextureUpdater::Texture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    append(texture, sourceRect, destRect, m_partialEntries);
}

bool CCTextureUpdater::hasMoreUpdates() const
{
    return m_entries.size() || m_partialEntries.size();
}

bool CCTextureUpdater::update(GraphicsContext3D* context, size_t count)
{
    size_t index;
    size_t maxIndex = min(m_entryIndex + count, m_entries.size());
    for (index = m_entryIndex; index < maxIndex; ++index) {
        UpdateEntry& entry = m_entries[index];
        entry.m_texture->updateRect(context, m_allocator, entry.m_sourceRect, entry.m_destRect);
    }

    bool moreUpdates = maxIndex < m_entries.size();

    ASSERT(m_partialEntries.size() <= count);
    // Make sure the number of updates including partial updates are not more
    // than |count|.
    if ((count - (index - m_entryIndex)) < m_partialEntries.size())
        moreUpdates = true;

    if (moreUpdates) {
        m_entryIndex = index;
        return true;
    }

    for (index = 0; index < m_partialEntries.size(); ++index) {
        UpdateEntry& entry = m_partialEntries[index];
        entry.m_texture->updateRect(context, m_allocator, entry.m_sourceRect, entry.m_destRect);
    }

    // If no entries left to process, auto-clear.
    clear();
    return false;
}

void CCTextureUpdater::clear()
{
    m_entryIndex = 0;
    m_entries.clear();
    m_partialEntries.clear();
}

}

#endif // USE(ACCELERATED_COMPOSITING)

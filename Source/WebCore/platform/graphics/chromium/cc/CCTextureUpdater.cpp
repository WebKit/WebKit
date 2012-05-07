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
#include "TextureCopier.h"
#include "TextureUploader.h"

using namespace std;

namespace WebCore {

CCTextureUpdater::CCTextureUpdater()
    : m_entryIndex(0)
{
}

CCTextureUpdater::~CCTextureUpdater()
{
}

void CCTextureUpdater::appendUpdate(LayerTextureUpdater::Texture* texture, const IntRect& sourceRect, const IntRect& destRect, Vector<UpdateEntry>& entries)
{
    ASSERT(texture);

    UpdateEntry entry;
    entry.texture = texture;
    entry.sourceRect = sourceRect;
    entry.destRect = destRect;
    entries.append(entry);
}

void CCTextureUpdater::appendUpdate(LayerTextureUpdater::Texture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    appendUpdate(texture, sourceRect, destRect, m_entries);
}

void CCTextureUpdater::appendPartialUpdate(LayerTextureUpdater::Texture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    appendUpdate(texture, sourceRect, destRect, m_partialEntries);
}

void CCTextureUpdater::appendCopy(unsigned sourceTexture, unsigned destTexture, const IntSize& size)
{
    CopyEntry copy;
    copy.sourceTexture = sourceTexture;
    copy.destTexture = destTexture;
    copy.size = size;
    m_copyEntries.append(copy);
}

void CCTextureUpdater::appendManagedCopy(unsigned sourceTexture, ManagedTexture* destTexture, const IntSize& size)
{
    ManagedCopyEntry copy;
    copy.sourceTexture = sourceTexture;
    copy.destTexture = destTexture;
    copy.size = size;
    m_managedCopyEntries.append(copy);
}

bool CCTextureUpdater::hasMoreUpdates() const
{
    return m_entries.size() || m_partialEntries.size() || m_copyEntries.size() || m_managedCopyEntries.size();
}

void CCTextureUpdater::update(GraphicsContext3D* context, TextureAllocator* allocator, TextureCopier* copier, TextureUploader* uploader, size_t count)
{
    size_t index;

    if (m_entries.size() || m_partialEntries.size()) {
        if (uploader->isBusy())
            return;

        uploader->beginUploads();

        size_t maxIndex = min(m_entryIndex + count, m_entries.size());
        for (index = m_entryIndex; index < maxIndex; ++index) {
            UpdateEntry& entry = m_entries[index];
            uploader->uploadTexture(context, entry.texture, allocator, entry.sourceRect, entry.destRect);
        }

        bool moreUploads = maxIndex < m_entries.size();

        ASSERT(m_partialEntries.size() <= count);
        // We need another update batch if the number of updates remaining
        // in |count| is greater than the remaining partial entries.
        if ((count - (index - m_entryIndex)) < m_partialEntries.size())
            moreUploads = true;

        if (moreUploads) {
            m_entryIndex = index;
            uploader->endUploads();
            return;
        }

        for (index = 0; index < m_partialEntries.size(); ++index) {
            UpdateEntry& entry = m_partialEntries[index];
            uploader->uploadTexture(context, entry.texture, allocator, entry.sourceRect, entry.destRect);
        }

        uploader->endUploads();
    }

    for (index = 0; index < m_copyEntries.size(); ++index) {
        CopyEntry& copyEntry = m_copyEntries[index];
        copier->copyTexture(context, copyEntry.sourceTexture, copyEntry.destTexture, copyEntry.size);
    }
    for (index = 0; index < m_managedCopyEntries.size(); ++index) {
        ManagedCopyEntry& managedCopyEntry = m_managedCopyEntries[index];
        managedCopyEntry.destTexture->allocate(allocator);
        copier->copyTexture(context, managedCopyEntry.sourceTexture, managedCopyEntry.destTexture->textureId(), managedCopyEntry.size);
    }
    // If we've performed any texture copies, we need to insert a flush here into the compositor context
    // before letting the main thread proceed as it may make draw calls to the source texture of one of
    // our copy operations.
    if (m_copyEntries.size() || m_managedCopyEntries.size())
        context->flush();

    // If no entries left to process, auto-clear.
    clear();
}

void CCTextureUpdater::clear()
{
    m_entryIndex = 0;
    m_entries.clear();
    m_partialEntries.clear();
    m_copyEntries.clear();
    m_managedCopyEntries.clear();
}

}

#endif // USE(ACCELERATED_COMPOSITING)

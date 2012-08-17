/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "cc/CCTextureUpdateController.h"

#include "GraphicsContext3D.h"
#include "TextureCopier.h"
#include "TextureUploader.h"
#include <wtf/CurrentTime.h>

namespace {

// Number of textures to update with each call to updateMoreTextures().
static const size_t textureUpdatesPerFrame = 48;

// Flush interval when performing texture uploads.
static const int textureUploadFlushPeriod = 4;

} // anonymous namespace

namespace WebCore {

size_t CCTextureUpdateController::maxPartialTextureUpdates()
{
    return textureUpdatesPerFrame;
}

void CCTextureUpdateController::updateTextures(CCResourceProvider* resourceProvider, TextureCopier* copier, TextureUploader* uploader, CCTextureUpdateQueue* queue, size_t count)
{
    if (queue->fullUploadSize() || queue->partialUploadSize()) {
        if (uploader->isBusy())
            return;

        uploader->beginUploads();

        size_t fullUploadCount = 0;
        while (queue->fullUploadSize() && fullUploadCount < count) {
            uploader->uploadTexture(resourceProvider, queue->takeFirstFullUpload());
            fullUploadCount++;
            if (!(fullUploadCount % textureUploadFlushPeriod))
                resourceProvider->shallowFlushIfSupported();
        }

        // Make sure there are no dangling uploads without a flush.
        if (fullUploadCount % textureUploadFlushPeriod)
            resourceProvider->shallowFlushIfSupported();

        bool moreUploads = queue->fullUploadSize();

        ASSERT(queue->partialUploadSize() <= count);
        // We need another update batch if the number of updates remaining
        // in |count| is greater than the remaining partial entries.
        if ((count - fullUploadCount) < queue->partialUploadSize())
            moreUploads = true;

        if (moreUploads) {
            uploader->endUploads();
            return;
        }

        size_t partialUploadCount = 0;
        while (queue->partialUploadSize()) {
            uploader->uploadTexture(resourceProvider, queue->takeFirstPartialUpload());
            partialUploadCount++;
            if (!(partialUploadCount % textureUploadFlushPeriod))
                resourceProvider->shallowFlushIfSupported();
        }

        // Make sure there are no dangling partial uploads without a flush.
        if (partialUploadCount % textureUploadFlushPeriod)
            resourceProvider->shallowFlushIfSupported();

        uploader->endUploads();
    }

    size_t copyCount = 0;
    while (queue->copySize()) {
        copier->copyTexture(queue->takeFirstCopy());
        copyCount++;
    }

    // If we've performed any texture copies, we need to insert a flush here into the compositor context
    // before letting the main thread proceed as it may make draw calls to the source texture of one of
    // our copy operations.
    if (copyCount)
        copier->flush();
}

CCTextureUpdateController::CCTextureUpdateController(PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureCopier* copier, TextureUploader* uploader)
    : m_queue(queue)
    , m_resourceProvider(resourceProvider)
    , m_copier(copier)
    , m_uploader(uploader)
{
}

CCTextureUpdateController::~CCTextureUpdateController()
{
}

bool CCTextureUpdateController::hasMoreUpdates() const
{
    return m_queue->hasMoreUpdates();
}

void CCTextureUpdateController::updateMoreTextures()
{
    if (!m_queue->hasMoreUpdates())
        return;

    updateTextures(m_resourceProvider, m_copier, m_uploader, m_queue.get(), updateMoreTexturesSize());
}

size_t CCTextureUpdateController::updateMoreTexturesSize() const
{
    return textureUpdatesPerFrame;
}

}

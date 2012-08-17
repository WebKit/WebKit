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

#if USE(ACCELERATED_COMPOSITING)

#include "CCTextureUpdateQueue.h"

namespace WebCore {

CCTextureUpdateQueue::CCTextureUpdateQueue()
{
}

CCTextureUpdateQueue::~CCTextureUpdateQueue()
{
}

void CCTextureUpdateQueue::appendFullUpload(TextureUploader::Parameters upload)
{
    m_fullEntries.append(upload);
}

void CCTextureUpdateQueue::appendPartialUpload(TextureUploader::Parameters upload)
{
    m_partialEntries.append(upload);
}

void CCTextureUpdateQueue::appendCopy(TextureCopier::Parameters copy)
{
    m_copyEntries.append(copy);
}

void CCTextureUpdateQueue::clearUploads()
{
    m_fullEntries.clear();
    m_partialEntries.clear();
}

TextureUploader::Parameters CCTextureUpdateQueue::takeFirstFullUpload()
{
    return m_fullEntries.takeFirst();
}

TextureUploader::Parameters CCTextureUpdateQueue::takeFirstPartialUpload()
{
    return m_partialEntries.takeFirst();
}

TextureCopier::Parameters CCTextureUpdateQueue::takeFirstCopy()
{
    return m_copyEntries.takeFirst();
}

bool CCTextureUpdateQueue::hasMoreUpdates() const
{
    return m_fullEntries.size() || m_partialEntries.size() || m_copyEntries.size();
}

}

#endif // USE(ACCELERATED_COMPOSITING)

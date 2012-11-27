/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageDecodingStore.h"

#include "ScaledImageFragment.h"

#include <wtf/MainThread.h>

namespace WebCore {

namespace {

ImageDecodingStore* s_instanceOnMainThread = 0;

static void setInstanceOnMainThread(ImageDecodingStore* imageDecodingStore)
{
    delete s_instanceOnMainThread;
    s_instanceOnMainThread = imageDecodingStore;
}
} // namespace

ImageDecodingStore::ImageDecodingStore()
{
}

ImageDecodingStore::~ImageDecodingStore()
{
}

ImageDecodingStore* ImageDecodingStore::instanceOnMainThread()
{
    ASSERT(isMainThread());
    return s_instanceOnMainThread;
}

void ImageDecodingStore::initializeOnMainThread()
{
    ASSERT(isMainThread());
    setInstanceOnMainThread(ImageDecodingStore::create().leakPtr());
}

void ImageDecodingStore::shutdown()
{
    ASSERT(isMainThread());
    setInstanceOnMainThread(0);
}

bool ImageDecodingStore::calledOnValidThread() const
{
    return this == instanceOnMainThread() && isMainThread();
}

ScaledImageFragment* ImageDecodingStore::lookupFrameCache(int imageId, const SkISize& scaledSize) const
{
    for (size_t i = 0; i < m_frameCache.size(); ++i) {
        if (m_frameCache[i]->isEqual(imageId, scaledSize))
            return m_frameCache[i].get();
    }
    return 0;
}

void ImageDecodingStore::deleteFrameCache(int imageId)
{
    for (size_t i = 0; i < m_frameCache.size(); ++i) {
        if (m_frameCache[i]->isEqual(imageId)) {
            m_frameCache.remove(i);
            return;
        }
    }
}

} // namespace WebCore

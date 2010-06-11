/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(IMAGE_RESIZER)

#include "AsyncImageResizer.h"

#include "CachedImage.h"
#include "ImageResizerThread.h"
#include "SharedBuffer.h"
#include <utility>

namespace WebCore {

PassRefPtr<AsyncImageResizer> AsyncImageResizer::create(CachedImage* cachedImage, OutputType outputType, IntSize desiredBounds, ScriptValue successCallback, ScriptValue errorCallback, float quality, AspectRatioOption aspectRatioOption, OrientationOption orientationOption)
{
    return adoptRef(new AsyncImageResizer(cachedImage, outputType, desiredBounds, successCallback, errorCallback, quality, aspectRatioOption, orientationOption));
}

AsyncImageResizer::AsyncImageResizer(CachedImage* cachedImage, OutputType outputType, IntSize desiredBounds, ScriptValue successCallback, ScriptValue errorCallback, float quality, AspectRatioOption aspectRatioOption, OrientationOption orientationOption)
    : m_cachedImage(cachedImage)
    , m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
    , m_outputType(outputType)
    , m_desiredBounds(desiredBounds)
    , m_quality(quality)
    , m_aspectRatioOption(aspectRatioOption)
    , m_orientationOption(orientationOption)
{
    ASSERT(m_successCallback.isObject());
    m_cachedImage->addClient(this);
}

AsyncImageResizer::~AsyncImageResizer()
{
}

void AsyncImageResizer::notifyFinished(CachedResource* cachedResource)
{
    RefPtr<SharedBuffer> imageData = cachedResource->data()->copy();
    cachedResource->removeClient(this);
    CallbackInfo* callbackInfo = new CallbackInfo(this);
    if (!ImageResizerThread::start(imageData, callbackInfo, m_outputType, m_desiredBounds, m_quality, m_aspectRatioOption, m_orientationOption))
        resizeError();
}

} // namespace WebCore

#endif // ENABLE(IMAGE_RESIZER)

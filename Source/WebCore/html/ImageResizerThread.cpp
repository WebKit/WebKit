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

#include "ImageResizerThread.h"

#include "BitmapImage.h"
#include "Image.h"
#include "SharedBuffer.h"
#include <utility>

namespace WebCore {

static void returnBlobOrError(void* callbackInformation)
{
    AsyncImageResizer::CallbackInfo* callbackInfo = static_cast<AsyncImageResizer::CallbackInfo*>(callbackInformation);
    if (!callbackInfo->blob)
        callbackInfo->asyncImageResizer->resizeError();
    else
        callbackInfo->asyncImageResizer->resizeComplete(callbackInfo->blob);
    delete callbackInfo;
}

bool ImageResizerThread::start(PassRefPtr<SharedBuffer> imageData, AsyncImageResizer::CallbackInfo* callbackInfo, AsyncImageResizer::OutputType outputType, IntSize desiredBounds, float quality, AsyncImageResizer::AspectRatioOption aspectRatioOption, AsyncImageResizer::OrientationOption orientationOption)
{
    ImageResizerThread* imageResizerThread = new ImageResizerThread(callbackInfo, outputType, desiredBounds, quality, aspectRatioOption, orientationOption);

    RefPtr<SharedBuffer> copiedImageData = imageData->copy();
    imageResizerThread->m_image = BitmapImage::create();
    imageResizerThread->m_image->setData(copiedImageData, true);

    imageResizerThread->m_threadID = createThread(ImageResizerThread::imageResizerThreadStart, imageResizerThread, "ImageResizerThread");
    return imageResizerThread->m_threadID;
}

ImageResizerThread::ImageResizerThread(AsyncImageResizer::CallbackInfo* callbackInfo, AsyncImageResizer::OutputType outputType, IntSize desiredBounds, float quality, AsyncImageResizer::AspectRatioOption aspectRatioOption, AsyncImageResizer::OrientationOption orientationOption)
    : m_threadID(0)
    , m_callbackInfo(callbackInfo)
    , m_outputType(outputType)
    , m_desiredBounds(desiredBounds)
    , m_quality(quality)
    , m_aspectRatioOption(aspectRatioOption)
    , m_orientationOption(orientationOption)
{
}

ImageResizerThread::~ImageResizerThread()
{
}

void* ImageResizerThread::imageResizerThreadStart(void* thread)
{
    return static_cast<ImageResizerThread*>(thread)->imageResizerThread();
}

void* ImageResizerThread::imageResizerThread()
{
    // FIXME: Do resizing, create blob, and catch any errors.

    callOnMainThread(returnBlobOrError, static_cast<void*>(m_callbackInfo));
    detachThread(m_threadID);
    delete this;
    return 0;
}

} // namespace WebCore

#endif // ENABLE(IMAGE_RESIZER)

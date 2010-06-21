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

#ifndef AsyncImageResizer_h
#define AsyncImageResizer_h

#if ENABLE(IMAGE_RESIZER)

#include "Blob.h"
#include "CachedResourceClient.h"
#include "IntSize.h"
#include "ScriptValue.h"

namespace WebCore {

class CachedImage;
class ImageResizerThread;

// AsyncImageResizer waits for the CachedImage that is passed in to load completely,
// then starts an ImageResizerThread to resize the image. Once created, ImageResizerThread
// becomes in charge of its own lifetime and that of AsyncImageResizer. After the callbacks
// occur, both objects are destroyed. If the document is destroyed during resizing,
// AsyncImageResizer will receive a notification and subsequently block the callbacks from
// occurring.
class AsyncImageResizer : public CachedResourceClient, public RefCounted<AsyncImageResizer> {
public:
    struct CallbackInfo {
        CallbackInfo(PassRefPtr<AsyncImageResizer> asyncImageResizer)
            : asyncImageResizer(asyncImageResizer)
            , blob(0)
        {
        }

        RefPtr<AsyncImageResizer> asyncImageResizer;
        RefPtr<Blob> blob;
    };
    
    enum OutputType {
        JPEG,
        PNG
    };

    enum AspectRatioOption {
        PreserveAspectRatio,
        IgnoreAspectRatio
    };

    enum OrientationOption {
        CorrectOrientation,
        IgnoreOrientation
    };

    static PassRefPtr<AsyncImageResizer> create(CachedImage*, OutputType, IntSize desiredBounds, ScriptValue successCallback, ScriptValue errorCallback, float quality, AspectRatioOption, OrientationOption);
    ~AsyncImageResizer();

    // FIXME: Insert override function for notification of document destruction (change m_callbacksOk).

    void resizeComplete(RefPtr<Blob>) { /* FIXME: Not yet implemented. */ }
    void resizeError() { /* FIXME: Not yet implemented. */ }

private:
    AsyncImageResizer(CachedImage*, OutputType, IntSize desiredBounds, ScriptValue successCallback, ScriptValue errorCallback, float quality, AspectRatioOption, OrientationOption);
    virtual void notifyFinished(CachedResource*);

    CachedImage* m_cachedImage;
    ScriptValue m_successCallback;
    ScriptValue m_errorCallback;
    
    // Parameters to pass into ImageResizerThread.
    OutputType m_outputType;
    IntSize m_desiredBounds;
    float m_quality;
    AspectRatioOption m_aspectRatioOption;
    OrientationOption m_orientationOption;
};

} // namespace WebCore

#endif // ENABLE(IMAGE_RESIZER)

#endif // AsyncImageResizer_h

/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ImageBufferBackendHandle.h"
#include <WebCore/AffineTransform.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/RenderingMode.h>
#include <optional>
#include <wtf/WeakPtr.h>

namespace IPC {
class Decoder;
class Encoder;
template<typename> struct ArgumentCoder;
}

namespace WebCore {
class DetachedImageBitmap;
}

namespace WebKit {

class RemoteRenderingBackendProxy;

struct DetachedImageBitmapIPCData {
    WebCore::ImageBuffer::Parameters parameters;
    WebCore::RenderingMode renderingMode;
    WebCore::AffineTransform baseTransform;
    uint64_t memoryCost { 0 };
    ImageBufferBackendHandle handle;
    bool originClean { false };
    bool premultiplyAlpha { false };
    bool forciblyPremultiplyAlpha { false };
};

// Provides thread_local acccess to RemoteRenderingBackendProxy*.
class ScopedCrossProcessImageBitmapEncoding {
    WTF_MAKE_NONCOPYABLE(ScopedCrossProcessImageBitmapEncoding);
public:
    explicit ScopedCrossProcessImageBitmapEncoding(RemoteRenderingBackendProxy*);
    ~ScopedCrossProcessImageBitmapEncoding();

    static RefPtr<RemoteRenderingBackendProxy> currentForThread();

private:
    WeakPtr<RemoteRenderingBackendProxy> m_previous;
};

} // namespace WebKit

namespace IPC {
template<> struct ArgumentCoder<std::optional<WebCore::DetachedImageBitmap>> {
    static void encode(Encoder&, const std::optional<WebCore::DetachedImageBitmap>&);
    static std::optional<std::optional<WebCore::DetachedImageBitmap>> decode(Decoder&);
};
} // namespace IPC

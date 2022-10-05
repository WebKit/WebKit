/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)

#include "MessageReceiver.h"
#include "ShareableBitmap.h"
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/ImageDecoderAVFObjC.h>
#include <WebCore/ImageDecoderIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class SharedBufferReference;
}

namespace WebKit {
class GPUConnectionToWebProcess;

class RemoteImageDecoderAVFProxy : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteImageDecoderAVFProxy(GPUConnectionToWebProcess&);
    virtual ~RemoteImageDecoderAVFProxy() = default;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    bool allowsExitUnderMemoryPressure() const;

private:
    void createDecoder(const IPC::SharedBufferReference&, const String& mimeType, CompletionHandler<void(std::optional<WebCore::ImageDecoderIdentifier>&&)>&&);
    void deleteDecoder(WebCore::ImageDecoderIdentifier);
    void setExpectedContentSize(WebCore::ImageDecoderIdentifier, long long expectedContentSize);
    void setData(WebCore::ImageDecoderIdentifier, const IPC::SharedBufferReference&, bool allDataReceived, CompletionHandler<void(size_t frameCount, const WebCore::IntSize& size, bool hasTrack, std::optional<Vector<WebCore::ImageDecoder::FrameInfo>>&&)>&&);
    void createFrameImageAtIndex(WebCore::ImageDecoderIdentifier, size_t index, CompletionHandler<void(std::optional<WebKit::ShareableBitmapHandle>&&)>&&);
    void clearFrameBufferCache(WebCore::ImageDecoderIdentifier, size_t index);

    void encodedDataStatusChanged(WebCore::ImageDecoderIdentifier);

    WeakPtr<GPUConnectionToWebProcess> m_connectionToWebProcess;
    HashMap<WebCore::ImageDecoderIdentifier, RefPtr<WebCore::ImageDecoderAVFObjC>> m_imageDecoders;
};

}

#endif

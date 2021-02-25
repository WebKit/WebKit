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

#include "DataReference.h"
#include "MessageReceiver.h"
#include <WebCore/ImageDecoderAVFObjC.h>
#include <WebCore/ImageDecoderIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/MachSendRight.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class GPUConnectionToWebProcess;

class RemoteImageDecoderAVFProxy : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteImageDecoderAVFProxy(GPUConnectionToWebProcess&);
    virtual ~RemoteImageDecoderAVFProxy() = default;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

private:
    void createDecoder(const IPC::DataReference&, const String& mimeType, CompletionHandler<void(Optional<WebCore::ImageDecoderIdentifier>&&)>&&);
    void deleteDecoder(const WebCore::ImageDecoderIdentifier&);
    void setExpectedContentSize(const WebCore::ImageDecoderIdentifier&, long long expectedContentSize);
    void setData(const WebCore::ImageDecoderIdentifier&, const IPC::DataReference&, bool allDataReceived, CompletionHandler<void(size_t frameCount, const WebCore::IntSize& size, bool hasTrack, Optional<Vector<WebCore::ImageDecoder::FrameInfo>>&&)>&&);
    void createFrameImageAtIndex(const WebCore::ImageDecoderIdentifier&, size_t index, CompletionHandler<void(Optional<WTF::MachSendRight>&&)>&&);

    void encodedDataStatusChanged(const WebCore::ImageDecoderIdentifier&);

    WeakPtr<GPUConnectionToWebProcess> m_connectionToWebProcess;
    HashMap<WebCore::ImageDecoderIdentifier, RefPtr<WebCore::ImageDecoderAVFObjC>> m_imageDecoders;
};

}

#endif

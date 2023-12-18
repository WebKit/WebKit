/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS)

#include "PrepareBackingStoreBuffersData.h"
#include "RemoteImageBufferSetIdentifier.h"
#include "ThreadSafeObjectHeap.h"
#include "WorkQueueMessageReceiver.h"

namespace WebKit {

// A helper for RemoteRenderingBackendProxy that runs on a background thread.
class RemoteRenderingBackendProxyBackground : public IPC::WorkQueueMessageReceiver {
public:
    void imageBufferSetDidPrepareForDisplay(RemoteImageBufferSetIdentifier, ImageBufferSetPrepareBufferForDisplayOutputData);

    ImageBufferSetPrepareBufferForDisplayOutputData takeImageBufferSetPrepareForDisplayOutputData(RemoteImageBufferSetIdentifier);

private:
    // IPC::MessageReceiver (implemented by generated code)
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    class ImageBufferSetPrepareBufferForDisplayOutputDataWrapper : public ThreadSafeRefCounted< ImageBufferSetPrepareBufferForDisplayOutputDataWrapper > {
    public:
        ImageBufferSetPrepareBufferForDisplayOutputDataWrapper(ImageBufferSetPrepareBufferForDisplayOutputData&& data)
            : m_outputData(WTFMove(data))
        { }

        ImageBufferSetPrepareBufferForDisplayOutputData m_outputData;
    };

    IPC::ThreadSafeObjectHeap<RemoteImageBufferSetIdentifier, RefPtr< ImageBufferSetPrepareBufferForDisplayOutputDataWrapper>> m_imageBufferSetsOutputData;
};

} // namespace WebKit

#endif

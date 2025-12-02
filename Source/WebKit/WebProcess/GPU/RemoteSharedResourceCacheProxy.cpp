/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(GPU_PROCESS)
#include "RemoteSharedResourceCacheProxy.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeapProxy.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSharedResourceCacheProxy);

Ref<RemoteSharedResourceCacheProxy> RemoteSharedResourceCacheProxy::create()
{
    return adoptRef(*new RemoteSharedResourceCacheProxy());
}

RemoteSharedResourceCacheProxy::RemoteSharedResourceCacheProxy() = default;

RemoteSharedResourceCacheProxy::~RemoteSharedResourceCacheProxy() = default;

void RemoteSharedResourceCacheProxy::gpuProcessConnectionDidBecomeAvailable(GPUProcessConnection& gpuProcessConnection)
{
    UNUSED_PARAM(gpuProcessConnection);
#if ENABLE(VIDEO)
    if (RefPtr proxy = m_videoFrameObjectHeapProxy)
        proxy->gpuProcessConnectionDidBecomeAvailable(gpuProcessConnection);
#endif
}

#if ENABLE(VIDEO)
RemoteVideoFrameObjectHeapProxy& RemoteSharedResourceCacheProxy::videoFrameObjectHeapProxy()
{
    if (!m_videoFrameObjectHeapProxy) {
        lazyInitialize(m_videoFrameObjectHeapProxy, RemoteVideoFrameObjectHeapProxy::create());
        if (RefPtr gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
            m_videoFrameObjectHeapProxy->gpuProcessConnectionDidBecomeAvailable(*gpuProcessConnection);
    }
    return *m_videoFrameObjectHeapProxy;
}

Ref<RemoteVideoFrameObjectHeapProxy> RemoteSharedResourceCacheProxy::protectedVideoFrameObjectHeapProxy()
{
    return videoFrameObjectHeapProxy();
}
#endif

}

#endif

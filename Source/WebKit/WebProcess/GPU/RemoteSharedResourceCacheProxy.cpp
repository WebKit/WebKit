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

#include "RemoteNativeImageProxy.h"
#include "RemoteSharedResourceCacheMessages.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSharedResourceCacheProxy);

Ref<RemoteSharedResourceCacheProxy> RemoteSharedResourceCacheProxy::create(IPC::Connection& connection)
{
    return adoptRef(*new RemoteSharedResourceCacheProxy(connection));
}

RemoteSharedResourceCacheProxy::RemoteSharedResourceCacheProxy(IPC::Connection& connection)
    : m_connection(connection)
{
}

RemoteSharedResourceCacheProxy::~RemoteSharedResourceCacheProxy() = default;

void RemoteSharedResourceCacheProxy::releaseNativeImage(const RemoteNativeImageProxy& image)
{
    // The write reference carries the number of reads handed out (pendingReads), so the GPU process
    // removes the entry once that many reads have been retired.
    m_connection->send(Messages::RemoteSharedResourceCache::ReleaseNativeImage(image.writeReference()), 0);
}

}

#endif

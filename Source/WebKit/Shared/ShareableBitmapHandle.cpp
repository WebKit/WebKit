/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ShareableBitmapHandle.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {
using namespace WebCore;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ShareableBitmapHandle);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ShareableBitmapHandle);

ShareableBitmapHandle::ShareableBitmapHandle()
{
}

ShareableBitmapHandle::ShareableBitmapHandle(SharedMemory::Handle&& handle, const WebCore::IntSize& size, const ShareableBitmapConfiguration& config)
    : m_handle(WTFMove(handle))
    , m_size(size)
    , m_configuration(config)
{
}

void ShareableBitmapHandle::takeOwnershipOfMemory(MemoryLedger ledger) const
{
    m_handle.takeOwnershipOfMemory(ledger);
}

void ShareableBitmapHandle::clear()
{
    m_handle.clear();
    m_size = IntSize();
    m_configuration = { };
}

} // namespace WebKit

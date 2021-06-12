/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayListWriterHandle.h"

#include <wtf/CheckedArithmetic.h>

namespace WebKit {
using namespace WebCore;

size_t DisplayListWriterHandle::advance(size_t amount)
{
    m_writableOffset = CheckedSize { m_writableOffset } + amount;
    return CheckedSize { header().unreadBytes.exchangeAdd(amount) } + amount;
}

size_t DisplayListWriterHandle::availableCapacity() const
{
    if (UNLIKELY(sharedMemory().size() < writableOffset()))
        RELEASE_ASSERT_NOT_REACHED();

    return sharedMemory().size() - writableOffset();
}

DisplayList::ItemBufferHandle DisplayListWriterHandle::createHandle() const
{
    return {
        identifier(),
        data() + writableOffset(),
        availableCapacity()
    };
}

bool DisplayListWriterHandle::moveWritableOffsetToStartIfPossible()
{
    if (m_writableOffset <= SharedDisplayListHandle::headerSize()) {
        RELEASE_ASSERT(m_writableOffset == SharedDisplayListHandle::headerSize());
        return true;
    }

    if (unreadBytes())
        return false;

    m_writableOffset = SharedDisplayListHandle::headerSize();
    return true;
}

} // namespace WebKit

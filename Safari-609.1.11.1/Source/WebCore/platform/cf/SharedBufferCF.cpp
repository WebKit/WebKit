/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "SharedBuffer.h"

#include <wtf/OSAllocator.h>
#include <wtf/cf/TypeCastsCF.h>

namespace WebCore {

SharedBuffer::SharedBuffer(CFDataRef data)
{
    append(data);
}

// Using Foundation allows for an even more efficient implementation of this function,
// so only use this version for non-Foundation.
#if !USE(FOUNDATION)
RetainPtr<CFDataRef> SharedBuffer::createCFData() const
{
    if (m_segments.size() == 1) {
        if (auto data = WTF::get_if<RetainPtr<CFDataRef>>(m_segments[0].segment->m_immutableData))
            return *data;
    }
    return adoptCF(CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(data()), size()));
}
#endif

Ref<SharedBuffer> SharedBuffer::create(CFDataRef data)
{
    return adoptRef(*new SharedBuffer(data));
}

void SharedBuffer::hintMemoryNotNeededSoon() const
{
    for (const auto& entry : m_segments) {
        if (entry.segment->hasOneRef()) {
            if (auto data = WTF::get_if<RetainPtr<CFDataRef>>(entry.segment->m_immutableData))
                OSAllocator::hintMemoryNotNeededSoon(const_cast<UInt8*>(CFDataGetBytePtr(data->get())), CFDataGetLength(data->get()));
        }
    }
}

void SharedBuffer::append(CFDataRef data)
{
    ASSERT(!m_hasBeenCombinedIntoOneSegment);
    if (data) {
        m_segments.append({m_size, DataSegment::create(data)});
        m_size += CFDataGetLength(data);
    }
    ASSERT(internallyConsistent());
}

}

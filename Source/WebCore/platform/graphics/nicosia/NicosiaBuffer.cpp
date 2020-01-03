/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaBuffer.h"

#include <wtf/FastMalloc.h>

namespace Nicosia {

Ref<Buffer> Buffer::create(const WebCore::IntSize& size, Flags flags)
{
    return adoptRef(*new Buffer(size, flags));
}

Buffer::Buffer(const WebCore::IntSize& size, Flags flags)
    : m_size(size)
    , m_flags(flags)
{
    auto checkedArea = size.area() * 4;
    m_data = MallocPtr<unsigned char>::tryZeroedMalloc(checkedArea.unsafeGet());
}

Buffer::~Buffer() = default;

void Buffer::beginPainting()
{
    LockHolder locker(m_painting.lock);
    ASSERT(m_painting.state == PaintingState::Complete);
    m_painting.state = PaintingState::InProgress;
}

void Buffer::completePainting()
{
    LockHolder locker(m_painting.lock);
    ASSERT(m_painting.state == PaintingState::InProgress);
    m_painting.state = PaintingState::Complete;
    m_painting.condition.notifyOne();
}

void Buffer::waitUntilPaintingComplete()
{
    LockHolder locker(m_painting.lock);
    m_painting.condition.wait(m_painting.lock,
        [this] { return m_painting.state == PaintingState::Complete; });
}

} // namespace Nicosia

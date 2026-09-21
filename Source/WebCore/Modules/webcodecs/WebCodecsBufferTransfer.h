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

#if ENABLE(WEB_CODECS)

#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>

namespace JSC {
class VM;
}

namespace WebCore {

class BufferSource;
class SharedBuffer;
template<typename> class ExceptionOr;

WEBCORE_EXPORT Ref<SharedBuffer> adoptArrayBufferContents(JSC::ArrayBufferContents&&);

// The WebCodecs `transfer` init member. validate() must return no exception before any other
// member is called; until then nothing is detached and the caller may still throw.
class WebCodecsTransferList {
    WTF_MAKE_NONCOPYABLE(WebCodecsTransferList);
public:
    explicit WebCodecsTransferList(Vector<Ref<JSC::ArrayBuffer>>&& list)
        : m_list(WTF::move(list))
    {
    }

    bool isEmpty() const { return m_list.isEmpty(); }

    ExceptionOr<void> validate();

    void detachAll(JSC::VM&) const;

    // Detaches every buffer in the list and returns the bytes of `data`, adopting them without
    // copying when `data` spans the whole of a buffer in the list.
    Ref<SharedBuffer> takeData(JSC::VM&, const BufferSource& data) const;
    Ref<SharedBuffer> takeData(JSC::VM&, JSC::ArrayBuffer* dataBuffer, std::span<const uint8_t> data) const;

private:
    bool contains(const JSC::ArrayBuffer& buffer) const { return m_buffers.contains(&buffer); }

    const Vector<Ref<JSC::ArrayBuffer>> m_list;
    HashSet<const JSC::ArrayBuffer*> m_buffers;
};

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)

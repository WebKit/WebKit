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

#include "config.h"
#include "WebCodecsBufferTransfer.h"

#if ENABLE(WEB_CODECS)

#include "BufferSource.h"
#include "ExceptionOr.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/ArrayBufferView.h>

namespace WebCore {

Ref<SharedBuffer> adoptArrayBufferContents(JSC::ArrayBufferContents&& content)
{
    Ref<const DataSegment> segment = DataSegment::create(makeUniqueWithoutFastMallocCheck<JSC::ArrayBufferContents>(WTF::move(content)));
    return SharedBuffer::create(WTF::move(segment));
}

ExceptionOr<void> WebCodecsTransferList::validate()
{
    for (Ref buffer : m_list) {
        // A SharedArrayBuffer never reaches the list: the IDL sequence<ArrayBuffer> conversion goes
        // through JSArrayBuffer::toWrapped(), which rejects shared buffers.
        ASSERT(!buffer->isShared());

        if (!m_buffers.add(buffer.ptr()).isNewEntry)
            return Exception { ExceptionCode::DataCloneError, "Duplicate ArrayBuffer in transfer list"_s };

        if (buffer->isDetached())
            return Exception { ExceptionCode::DataCloneError, "ArrayBuffer in transfer list is detached"_s };

        // A pinned buffer, such as one backing a WebAssembly.Memory, is silently copied rather
        // than detached by transferTo(), so it cannot be transferred.
        if (!buffer->isDetachable())
            return Exception { ExceptionCode::TypeError, JSC::errorMessageForTransfer(buffer.ptr()) };
    }
    return { };
}

void WebCodecsTransferList::detachAll(JSC::VM& vm) const
{
    for (Ref buffer : m_list) {
        // takeData() detaches the whole list, so it may already have run.
        if (buffer->isDetached())
            continue;
        JSC::ArrayBufferContents content;
        bool transferred = buffer->transferTo(vm, content);
        ASSERT_UNUSED(transferred, transferred);
    }
}

Ref<SharedBuffer> WebCodecsTransferList::takeData(JSC::VM& vm, const BufferSource& data) const
{
    return data.switchOn(
        [&](const Ref<JSC::ArrayBufferView>& view) {
            return takeData(vm, view->possiblySharedBuffer().get(), view->span());
        },
        [&](const Ref<JSC::ArrayBuffer>& buffer) {
            return takeData(vm, buffer.ptr(), buffer->span());
        });
}

Ref<SharedBuffer> WebCodecsTransferList::takeData(JSC::VM& vm, JSC::ArrayBuffer* dataBuffer, std::span<const uint8_t> data) const
{
    // Adopting a view that covers only part of its buffer would retain the whole allocation
    // while reporting the view's length as the memory cost.
    // FIXME: adopt partial views whose retained overhead is small enough.
    bool adoptable = dataBuffer && !dataBuffer->isDetached() && data.data() == dataBuffer->span().data()
        && data.size() == dataBuffer->byteLength() && contains(*dataBuffer);

    RefPtr<SharedBuffer> copy;
    if (!adoptable)
        copy = SharedBuffer::create(data);

    RefPtr<SharedBuffer> adopted;
    for (Ref buffer : m_list) {
        JSC::ArrayBufferContents content;
        bool transferred = buffer->transferTo(vm, content);
        ASSERT_UNUSED(transferred, transferred);
        if (adoptable && buffer.ptr() == dataBuffer)
            adopted = adoptArrayBufferContents(WTF::move(content));
    }

    if (adopted)
        return adopted.releaseNonNull();
    return copy.releaseNonNull();
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)

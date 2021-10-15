/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include "ArrayBufferView.h"
#include "JSArrayBufferView.h"
#include "JSDataView.h"

namespace JSC {

inline bool JSArrayBufferView::isShared()
{
    switch (m_mode) {
    case WastefulTypedArray:
        return existingBufferInButterfly()->isShared();
    case DataViewMode:
        return jsCast<JSDataView*>(this)->possiblySharedBuffer()->isShared();
    default:
        return false;
    }
}

template<JSArrayBufferView::Requester requester>
inline ArrayBuffer* JSArrayBufferView::possiblySharedBufferImpl()
{
    if (requester == ConcurrentThread)
        ASSERT(m_mode != FastTypedArray && m_mode != OversizeTypedArray);

    switch (m_mode) {
    case WastefulTypedArray:
        return existingBufferInButterfly();
    case DataViewMode:
        return jsCast<JSDataView*>(this)->possiblySharedBuffer();
    case FastTypedArray:
    case OversizeTypedArray:
        return slowDownAndWasteMemory();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

inline ArrayBuffer* JSArrayBufferView::possiblySharedBuffer()
{
    return possiblySharedBufferImpl<Mutator>();
}

inline ArrayBuffer* JSArrayBufferView::existingBufferInButterfly()
{
    ASSERT(m_mode == WastefulTypedArray);
    return butterfly()->indexingHeader()->arrayBuffer();
}

inline RefPtr<ArrayBufferView> JSArrayBufferView::unsharedImpl()
{
    RefPtr<ArrayBufferView> result = possiblySharedImpl();
    RELEASE_ASSERT(!result || !result->isShared());
    return result;
}

template<JSArrayBufferView::Requester requester, typename ResultType>
inline ResultType JSArrayBufferView::byteOffsetImpl()
{
    if (!hasArrayBuffer())
        return 0;

    if (requester == ConcurrentThread)
        WTF::loadLoadFence();

    ArrayBuffer* buffer = possiblySharedBufferImpl<requester>();
    ASSERT(buffer);
    if (requester == Mutator) {
        ASSERT(!isCompilationThread());
        ASSERT(!vector() == !buffer->data());
    }

    ptrdiff_t delta =
        bitwise_cast<uint8_t*>(vectorWithoutPACValidation()) - static_cast<uint8_t*>(buffer->data());

    unsigned result = static_cast<unsigned>(delta);
    if (requester == Mutator)
        ASSERT(static_cast<ptrdiff_t>(result) == delta);
    else {
        if (static_cast<ptrdiff_t>(result) != delta)
            return { };
    }

    return result;
}

inline unsigned JSArrayBufferView::byteOffset()
{
    return byteOffsetImpl<Mutator, unsigned>();
}

inline std::optional<unsigned> JSArrayBufferView::byteOffsetConcurrently()
{
    return byteOffsetImpl<ConcurrentThread, std::optional<unsigned>>();
}

inline RefPtr<ArrayBufferView> JSArrayBufferView::toWrapped(VM& vm, JSValue value)
{
    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(vm, value)) {
        if (!view->isShared())
            return view->unsharedImpl();
    }
    return nullptr;
}

inline RefPtr<ArrayBufferView> JSArrayBufferView::toWrappedAllowShared(VM& vm, JSValue value)
{
    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(vm, value))
        return view->possiblySharedImpl();
    return nullptr;
}


} // namespace JSC

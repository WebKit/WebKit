/*
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSDataView.h>

namespace JSC {

inline bool JSArrayBufferView::isShared()
{
    switch (m_mode) {
    case WastefulTypedArray:
    case ResizableNonSharedWastefulTypedArray:
    case ResizableNonSharedAutoLengthWastefulTypedArray:
    case GrowableSharedWastefulTypedArray:
    case GrowableSharedAutoLengthWastefulTypedArray:
        return existingBufferInButterfly()->isShared();
    case DataViewMode:
    case ResizableNonSharedDataViewMode:
    case ResizableNonSharedAutoLengthDataViewMode:
    case GrowableSharedDataViewMode:
    case GrowableSharedAutoLengthDataViewMode:
        return uncheckedDowncast<JSDataView>(this)->possiblySharedBuffer()->isShared();
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
    case ResizableNonSharedWastefulTypedArray:
    case ResizableNonSharedAutoLengthWastefulTypedArray:
    case GrowableSharedWastefulTypedArray:
    case GrowableSharedAutoLengthWastefulTypedArray:
        return existingBufferInButterfly();
    case DataViewMode:
    case ResizableNonSharedDataViewMode:
    case ResizableNonSharedAutoLengthDataViewMode:
    case GrowableSharedDataViewMode:
    case GrowableSharedAutoLengthDataViewMode:
        return uncheckedDowncast<JSDataView>(this)->possiblySharedBuffer();
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

inline RefPtr<ArrayBufferView> JSArrayBufferView::unsharedImpl()
{
    RefPtr<ArrayBufferView> result = possiblySharedImpl();
    RELEASE_ASSERT(!result || !result->isShared());
    return result;
}

inline RefPtr<ArrayBufferView> JSArrayBufferView::toWrapped(VM&, JSValue value)
{
    if (JSArrayBufferView* view = dynamicDowncast<JSArrayBufferView>(value)) {
        if (!view->isShared() && !view->isResizableOrGrowableShared())
            return view->unsharedImpl();
    }
    return nullptr;
}

inline RefPtr<ArrayBufferView> JSArrayBufferView::toWrappedAllowResizable(VM&, JSValue value)
{
    if (JSArrayBufferView* view = dynamicDowncast<JSArrayBufferView>(value)) {
        if (view->isShared())
            return nullptr;
        return view->unsharedImpl();
    }
    return nullptr;
}

inline RefPtr<ArrayBufferView> JSArrayBufferView::toWrappedAllowShared(VM&, JSValue value)
{
    if (JSArrayBufferView* view = dynamicDowncast<JSArrayBufferView>(value)) {
        if (!view->isResizableOrGrowableShared())
            return view->possiblySharedImpl();
    }
    return nullptr;
}

inline RefPtr<ArrayBufferView> JSArrayBufferView::toWrappedAllowSharedAndResizable(VM&, JSValue value)
{
    if (JSArrayBufferView* view = dynamicDowncast<JSArrayBufferView>(value))
        return view->possiblySharedImpl();
    return nullptr;
}

} // namespace JSC

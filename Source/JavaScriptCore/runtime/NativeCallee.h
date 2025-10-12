/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/CalleeBits.h>
#include <JavaScriptCore/ImplementationVisibility.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace JSC {

class LLIntOffsetsExtractor;

class NativeCallee : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<NativeCallee> {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(NativeCallee);
public:
    enum class Category : uint8_t {
        InlineCache,
        Wasm,
    };

    Category category() const { return m_category; }
    ImplementationVisibility implementationVisibility() const { return m_implementationVisibility; }

    void dump(PrintStream&) const;

    JS_EXPORT_PRIVATE void operator delete(NativeCallee*, std::destroying_delete_t);

protected:
    JS_EXPORT_PRIVATE NativeCallee(Category, ImplementationVisibility);

private:
    Category m_category;
    ImplementationVisibility m_implementationVisibility { ImplementationVisibility::Public };
};

// This lets you do a RefPtr<NativeCallee, BoxedNativeCalleePtrTraits<NativeCallee>>
template<typename T>
class BoxedNativeCalleePtrTraits {
public:
    using StorageType = CalleeBits;
    // Use an intermediate cast to uintptr_t to silence unsafe casting warning. It's locally "obvious" (other than the fact that RefPtr uses StorageType's constructor instead of a wrap) the
    // return value is of type T.
    static T* unwrap(const CalleeBits& calleeBits) { return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(calleeBits.asNativeCallee())); }
    static T* exchange(CalleeBits& calleeBits, T* newCallee)
    {
        T* result = unwrap(calleeBits);
        calleeBits = newCallee;
        return result;
    }

    // FIXME: This isn't hashable since we don't have hashTableDeletedValue() or isHashTableDeletedValue()
    // but those probably shouldn't be hard to add if needed.
};

} // namespace JSC

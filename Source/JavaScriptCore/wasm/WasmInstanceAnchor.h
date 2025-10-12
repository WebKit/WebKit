/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/WasmBaselineData.h>
#include <wtf/Expected.h>
#include <wtf/text/WTFString.h>

namespace JSC::Wasm {

class Module;

class InstanceAnchor final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<InstanceAnchor> {
    WTF_MAKE_TZONE_ALLOCATED(InstanceAnchor);
public:
    static Ref<InstanceAnchor> create(Module&, JSWebAssemblyInstance*);

    JSWebAssemblyInstance* instance() const WTF_REQUIRES_LOCK(m_lock) { return m_instance; }
    Lock& lock() const { return m_lock; }

    void tearDown()
    {
        Locker locker { m_lock };
        m_instance = nullptr;
    }

private:
    InstanceAnchor(JSWebAssemblyInstance* instance)
        : m_instance(instance)
    {
    }

    JSWebAssemblyInstance* m_instance WTF_GUARDED_BY_LOCK(m_lock) { nullptr }; // Intentionally non-WriteBarrier<>. This field will be read by the concurrent compilers.
public:
    mutable Lock m_lock;
};

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

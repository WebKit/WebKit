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

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include "WasmInstanceAnchor.h"
#include "WasmModule.h"
#include "WasmVirtualAddress.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSWebAssemblyModule;
class JSWebAssemblyInstance;
class VM;
class WebAssemblyGCStructure;

namespace Wasm {

class IPIntCallee;
class FunctionCodeIndex;

class JS_EXPORT_PRIVATE ModuleManager {
    WTF_MAKE_TZONE_ALLOCATED(ModuleManager);

public:
    ModuleManager() = default;
    ~ModuleManager() = default;

    uint32_t registerModule(Module&);
    void unregisterModule(Module&);

    uint32_t registerInstance(JSWebAssemblyInstance*);
    JSWebAssemblyInstance* jsInstance(uint32_t instanceId);
    uint32_t nextInstanceId() const;

    String generateLibrariesXML() const;

    bool needsLibraryRequery() const;
    void notifyLibraryRequeryComplete();
    Vector<uint32_t> unnotifiedInstanceIds() const;

private:
    using IdToInstance = UncheckedKeyHashMap<uint32_t, ThreadSafeWeakPtr<Wasm::InstanceAnchor>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using IdToId = UncheckedKeyHashMap<uint32_t, uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using InstanceIdSet = UncheckedKeyHashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

    // Amortized cleanup mechanism (matches ThreadSafeWeakHashSet behavior).
    void amortizedCleanupIfNeeded() WTF_REQUIRES_LOCK(m_lock);
    void cleanupHappened() WTF_REQUIRES_LOCK(m_lock);

    mutable Lock m_lock;
    IdToInstance m_instanceIdToInstance WTF_GUARDED_BY_LOCK(m_lock);
    // LLDB merges libraries that share a name, so only one instance per module may carry the
    // module's plain name; the rest are suffixed with their instance id. The first instance
    // registered wins and keeps the name even after it dies, so no library is ever renamed.
    IdToId m_moduleIdToPrimaryInstanceId WTF_GUARDED_BY_LOCK(m_lock);
    InstanceIdSet m_unnotifiedInstanceIds WTF_GUARDED_BY_LOCK(m_lock); // Instance IDs not yet seen by LLDB; cleared by notifyLibraryRequeryComplete() after each qXfer:libraries:read reply
    bool m_hasPendingLibraryRemovals WTF_GUARDED_BY_LOCK(m_lock) { false }; // True when an instance or module went away but LLDB hasn't been notified yet

    uint32_t m_nextModuleId WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    uint32_t m_nextInstanceId WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable unsigned m_operationCountSinceLastCleanup WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable unsigned m_maxOperationCountWithoutCleanup WTF_GUARDED_BY_LOCK(m_lock) { 0 };
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)

/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Integrity.h"

#include "HeapCellInlines.h"
#include "IntegrityInlines.h"
#include "JSCellInlines.h"
#include "Options.h"
#include "VMInspectorInlines.h"

namespace JSC {
namespace Integrity {

namespace {
constexpr bool verbose = false;
}

Random::Random(VM& vm)
{
    reloadAndCheckShouldAuditSlow(vm);
}

bool Random::reloadAndCheckShouldAuditSlow(VM& vm)
{
    auto locker = holdLock(m_lock);

    if (!Options::randomIntegrityAuditRate()) {
        m_triggerBits = 0; // Never trigger, and don't bother reloading.
        if (verbose)
            dataLogLn("disabled Integrity audits: trigger bits ", RawPointer(reinterpret_cast<void*>(m_triggerBits)));
        return false;
    }

    // Reload the trigger bits.
    m_triggerBits = 1ull << 63;

    uint32_t threshold = UINT_MAX * Options::randomIntegrityAuditRate();
    for (int i = 0; i < numberOfTriggerBits; ++i) {
        bool trigger = vm.random().getUint32() <= threshold;
        m_triggerBits = m_triggerBits | (static_cast<uint64_t>(trigger) << i);
    }
    if (verbose)
        dataLogLn("reloaded Integrity trigger bits ", RawPointer(reinterpret_cast<void*>(m_triggerBits)));
    ASSERT(m_triggerBits >= (1ull << 63));
    return vm.random().getUint32() <= threshold;
}

void auditCellFully(VM& vm, JSCell* cell)
{
    VMInspector::verifyCell<VMInspector::ReleaseAssert>(vm, cell);
}

void auditCellMinimallySlow(VM&, JSCell* cell)
{
    if (Gigacage::contains(cell)) {
        if (cell->type() != JSImmutableButterflyType) {
            if (verbose)
                dataLogLn("Bad cell ", RawPointer(cell), " ", JSValue(cell));
            CRASH();
        }
    }
}

} // namespace Integrity
} // namespace JSC

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

#pragma once

#include "Integrity.h"
#include "VM.h"

namespace JSC {
namespace Integrity {

ALWAYS_INLINE bool Random::shouldAudit(VM& vm)
{
    // If auditing is enabled, then the top bit of m_triggerBits is always set
    // to 1 on reload. When this top bit reaches the bottom, it does not
    // indicate that we should trigger an audit but rather that we've shifted
    // out all the available trigger bits and hence, need to reload. Instead,
    // reloadAndCheckShouldAuditSlow() will return whether we actually need to
    // trigger an audit this turn.
    //
    // This function can be called concurrently from different threads and can
    // be racy. For that reason, we intentionally do not write back to
    // m_triggerBits if newTriggerBits is null. This ensures that if
    // Options::randomIntegrityAuditRate() is non-zero, then m_triggerBits will
    // always have at least 1 bit to trigger a reload.

    uint64_t newTriggerBits = m_triggerBits;
    bool shouldAudit = newTriggerBits & 1;
    newTriggerBits = newTriggerBits >> 1;
    if (LIKELY(!shouldAudit)) {
        m_triggerBits = newTriggerBits;
        return false;
    }

    if (!newTriggerBits)
        return reloadAndCheckShouldAuditSlow(vm);

    m_triggerBits = newTriggerBits;
    return true;
}

ALWAYS_INLINE void auditCellMinimally(VM& vm, JSCell* cell)
{
    if (UNLIKELY(Gigacage::contains(cell)))
        auditCellMinimallySlow(vm, cell);
}

ALWAYS_INLINE void auditCellRandomly(VM& vm, JSCell* cell)
{
    if (UNLIKELY(vm.integrityRandom().shouldAudit(vm)))
        auditCellFully(vm, cell);
}

} // namespace Integrity
} // namespace JSC

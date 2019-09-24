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

#include "JSCJSValue.h"
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>

namespace JSC {

class JSCell;
class VM;

namespace Integrity {

enum class AuditLevel {
    None,
    Minimal,
    Full,
    Random,
};

#ifdef NDEBUG
static constexpr AuditLevel DefaultAuditLevel = AuditLevel::None;
#else
static constexpr AuditLevel DefaultAuditLevel = AuditLevel::Random;
#endif

class Random {
public:
    Random(VM&);

    ALWAYS_INLINE bool shouldAudit(VM&);

private:
    JS_EXPORT_PRIVATE bool reloadAndCheckShouldAuditSlow(VM&);

    uint64_t m_triggerBits;
    Lock m_lock;

    // The top bit is reserved as a termination bit. Hence, the number of
    // trigger bits is always 1 less than will fit in m_triggerBits.
    static constexpr int numberOfTriggerBits = (sizeof(m_triggerBits) * CHAR_BIT) - 1;
};

ALWAYS_INLINE void auditCellRandomly(VM&, JSCell*);
ALWAYS_INLINE void auditCellMinimally(VM&, JSCell*);
JS_EXPORT_PRIVATE void auditCellMinimallySlow(VM&, JSCell*);
JS_EXPORT_PRIVATE void auditCellFully(VM&, JSCell*);

template<AuditLevel = AuditLevel::Random, typename T>
ALWAYS_INLINE void auditCell(VM&, T) { }

template<AuditLevel auditLevel = DefaultAuditLevel>
ALWAYS_INLINE void auditCell(VM& vm, JSCell* cell)
{
    switch (auditLevel) {
    case AuditLevel::None:
        return;
    case AuditLevel::Minimal:
        return auditCellMinimally(vm, cell);
    case AuditLevel::Full:
        return auditCellFully(vm, cell);
    case AuditLevel::Random:
        return auditCellRandomly(vm, cell);
    }
}

template<AuditLevel auditLevel = DefaultAuditLevel>
ALWAYS_INLINE void auditCell(VM& vm, JSValue value)
{
    if (auditLevel == AuditLevel::None)
        return;

    if (value.isCell())
        auditCell<auditLevel>(vm, value.asCell());
}

} // namespace Integrity

} // namespace JSC

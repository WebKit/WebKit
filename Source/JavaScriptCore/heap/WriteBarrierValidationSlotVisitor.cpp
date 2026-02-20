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
#include "WriteBarrierValidationSlotVisitor.h"

#if ASSERT_ENABLED

#include "AbstractSlotVisitorInlines.h"
#include "Heap.h"
#include "JSCell.h"
#include "VMInspector.h"

namespace JSC {

WriteBarrierValidationSlotVisitor::WriteBarrierValidationSlotVisitor(Heap& heap, const JSCell* from, JSCell* to)
    : Base(heap, "WriteBarrierValidation"_s, m_opaqueRootStorage)
    , m_from(from)
    , m_to(to)
{
}

WriteBarrierValidationSlotVisitor::~WriteBarrierValidationSlotVisitor()
{
    if (m_foundTarget)
        return;

    WTF::dataFile().atomically([&](PrintStream&) {
        dataLogLn("Write barrier validation failed!");
        dataLogLn("  The 'from' object's visitChildren did not visit 'to'.");
        dataLogLn("  from:");
        VMInspector::dumpCellMemoryToStream(const_cast<JSCell*>(m_from), WTF::dataFile());
        dataLogLn("  to:");
        VMInspector::dumpCellMemoryToStream(m_to, WTF::dataFile());
    });
    RELEASE_ASSERT_NOT_REACHED();
}

void WriteBarrierValidationSlotVisitor::appendUnbarriered(JSCell* cell)
{
    if (cell == m_to)
        m_foundTarget = true;
}

void WriteBarrierValidationSlotVisitor::appendHiddenUnbarriered(JSCell* cell)
{
    if (cell == m_to)
        m_foundTarget = true;
}

} // namespace JSC

#endif // ASSERT_ENABLED

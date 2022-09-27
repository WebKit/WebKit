/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
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
#include "JITStubRoutine.h"

#include "AccessCase.h"
#include "CallLinkInfo.h"
#include "GCAwareJITStubRoutine.h"
#include "PolymorphicCallStubRoutine.h"

#if ENABLE(JIT)

namespace JSC {

void JITStubRoutine::observeZeroRefCountImpl()
{
    RELEASE_ASSERT(!m_refCount);
    delete this;
}

template<typename Func>
void JITStubRoutine::runWithDowncast(const Func& function)
{
    switch (m_type) {
    case Type::JITStubRoutineType:
        function(static_cast<JITStubRoutine*>(this));
        break;
    case Type::GCAwareJITStubRoutineType:
        function(static_cast<GCAwareJITStubRoutine*>(this));
        break;
    case Type::PolymorphicAccessJITStubRoutineType:
        function(static_cast<PolymorphicAccessJITStubRoutine*>(this));
        break;
    case Type::PolymorphicCallStubRoutineType:
        function(static_cast<PolymorphicCallStubRoutine*>(this));
        break;
    case Type::MarkingGCAwareJITStubRoutineType:
        function(static_cast<MarkingGCAwareJITStubRoutine*>(this));
        break;
    case Type::GCAwareJITStubRoutineWithExceptionHandlerType:
        function(static_cast<GCAwareJITStubRoutineWithExceptionHandler*>(this));
        break;
    }
}

void JITStubRoutine::aboutToDie()
{
    runWithDowncast([&](auto* derived) {
        derived->aboutToDieImpl();
    });
}

void JITStubRoutine::observeZeroRefCount()
{
    runWithDowncast([&](auto* derived) {
        derived->observeZeroRefCountImpl();
    });
}

bool JITStubRoutine::visitWeak(VM& vm)
{
    bool result = true;
    runWithDowncast([&](auto* derived) {
        result = derived->visitWeakImpl(vm);
    });
    return result;
}

void JITStubRoutine::markRequiredObjects(AbstractSlotVisitor& visitor)
{
    runWithDowncast([&](auto* derived) {
        derived->markRequiredObjectsImpl(visitor);
    });
}

void JITStubRoutine::markRequiredObjects(SlotVisitor& visitor)
{
    runWithDowncast([&](auto* derived) {
        derived->markRequiredObjectsImpl(visitor);
    });
}

void JITStubRoutine::operator delete(JITStubRoutine* stubRoutine, std::destroying_delete_t)
{
    stubRoutine->runWithDowncast([&](auto* derived) {
        std::destroy_at(derived);
        std::decay_t<decltype(*derived)>::freeAfterDestruction(derived);
    });
}

} // namespace JSC

#endif // ENABLE(JIT)


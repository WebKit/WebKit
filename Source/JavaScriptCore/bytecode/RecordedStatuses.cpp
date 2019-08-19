/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "RecordedStatuses.h"

namespace JSC {

RecordedStatuses& RecordedStatuses::operator=(RecordedStatuses&& other)
{
    calls = WTFMove(other.calls);
    gets = WTFMove(other.gets);
    puts = WTFMove(other.puts);
    ins = WTFMove(other.ins);
    shrinkToFit();
    return *this;
}

RecordedStatuses::RecordedStatuses(RecordedStatuses&& other)
{
    *this = WTFMove(other);
}

CallLinkStatus* RecordedStatuses::addCallLinkStatus(const CodeOrigin& codeOrigin, const CallLinkStatus& status)
{
    auto statusPtr = makeUnique<CallLinkStatus>(status);
    CallLinkStatus* result = statusPtr.get();
    calls.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}

GetByIdStatus* RecordedStatuses::addGetByIdStatus(const CodeOrigin& codeOrigin, const GetByIdStatus& status)
{
    auto statusPtr = makeUnique<GetByIdStatus>(status);
    GetByIdStatus* result = statusPtr.get();
    gets.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}
    
PutByIdStatus* RecordedStatuses::addPutByIdStatus(const CodeOrigin& codeOrigin, const PutByIdStatus& status)
{
    auto statusPtr = makeUnique<PutByIdStatus>(status);
    PutByIdStatus* result = statusPtr.get();
    puts.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}

InByIdStatus* RecordedStatuses::addInByIdStatus(const CodeOrigin& codeOrigin, const InByIdStatus& status)
{
    auto statusPtr = makeUnique<InByIdStatus>(status);
    InByIdStatus* result = statusPtr.get();
    ins.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}

void RecordedStatuses::markIfCheap(SlotVisitor& slotVisitor)
{
    for (auto& pair : gets)
        pair.second->markIfCheap(slotVisitor);
    for (auto& pair : puts)
        pair.second->markIfCheap(slotVisitor);
    for (auto& pair : ins)
        pair.second->markIfCheap(slotVisitor);
}

void RecordedStatuses::finalizeWithoutDeleting(VM& vm)
{
    // This variant of finalize gets called from within graph safepoints -- so there may be DFG IR in
    // some compiler thread that points to the statuses. That thread is stopped at a safepoint so
    // it's OK to edit its data structure, but it's not OK to delete them. Hence we don't remove
    // anything from the vector or delete the unique_ptrs.
    
    auto finalize = [&] (auto& vector) {
        for (auto& pair : vector) {
            if (!pair.second->finalize(vm))
                *pair.second = { };
        }
    };
    forEachVector(finalize);
}

void RecordedStatuses::finalize(VM& vm)
{
    auto finalize = [&] (auto& vector) {
        vector.removeAllMatching(
            [&] (auto& pair) -> bool {
                return !*pair.second || !pair.second->finalize(vm);
            });
        vector.shrinkToFit();
    };
    forEachVector(finalize);
}

void RecordedStatuses::shrinkToFit()
{
    forEachVector([] (auto& vector) { vector.shrinkToFit(); });
}

} // namespace JSC


/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
    deletes = WTFMove(other.deletes);
    checkPrivateBrands = WTFMove(other.checkPrivateBrands);
    setPrivateBrands = WTFMove(other.setPrivateBrands);
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

GetByStatus* RecordedStatuses::addGetByStatus(const CodeOrigin& codeOrigin, const GetByStatus& status)
{
    auto statusPtr = makeUnique<GetByStatus>(status);
    GetByStatus* result = statusPtr.get();
    gets.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}
    
PutByStatus* RecordedStatuses::addPutByStatus(const CodeOrigin& codeOrigin, const PutByStatus& status)
{
    auto statusPtr = makeUnique<PutByStatus>(status);
    PutByStatus* result = statusPtr.get();
    puts.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}

InByStatus* RecordedStatuses::addInByStatus(const CodeOrigin& codeOrigin, const InByStatus& status)
{
    auto statusPtr = makeUnique<InByStatus>(status);
    InByStatus* result = statusPtr.get();
    ins.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}

DeleteByStatus* RecordedStatuses::addDeleteByStatus(const CodeOrigin& codeOrigin, const DeleteByStatus& status)
{
    auto statusPtr = makeUnique<DeleteByStatus>(status);
    DeleteByStatus* result = statusPtr.get();
    deletes.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}

CheckPrivateBrandStatus* RecordedStatuses::addCheckPrivateBrandStatus(const CodeOrigin& codeOrigin, const CheckPrivateBrandStatus& status)
{
    auto statusPtr = makeUnique<CheckPrivateBrandStatus>(status);
    CheckPrivateBrandStatus* result = statusPtr.get();
    checkPrivateBrands.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}

SetPrivateBrandStatus* RecordedStatuses::addSetPrivateBrandStatus(const CodeOrigin& codeOrigin, const SetPrivateBrandStatus& status)
{
    auto statusPtr = makeUnique<SetPrivateBrandStatus>(status);
    SetPrivateBrandStatus* result = statusPtr.get();
    setPrivateBrands.append(std::make_pair(codeOrigin, WTFMove(statusPtr)));
    return result;
}

template<typename Visitor>
void RecordedStatuses::visitAggregateImpl(Visitor& visitor)
{
    for (auto& pair : gets)
        pair.second->visitAggregate(visitor);
    for (auto& pair : puts)
        pair.second->visitAggregate(visitor);
    for (auto& pair : ins)
        pair.second->visitAggregate(visitor);
    for (auto& pair : deletes)
        pair.second->visitAggregate(visitor);
    for (auto& pair : checkPrivateBrands)
        pair.second->visitAggregate(visitor);
    for (auto& pair : setPrivateBrands)
        pair.second->visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(RecordedStatuses);

template<typename Visitor>
void RecordedStatuses::markIfCheap(Visitor& visitor)
{
    for (auto& pair : gets)
        pair.second->markIfCheap(visitor);
    for (auto& pair : puts)
        pair.second->markIfCheap(visitor);
    for (auto& pair : ins)
        pair.second->markIfCheap(visitor);
    for (auto& pair : deletes)
        pair.second->markIfCheap(visitor);
    for (auto& pair : checkPrivateBrands)
        pair.second->markIfCheap(visitor);
    for (auto& pair : setPrivateBrands)
        pair.second->markIfCheap(visitor);
}

template void RecordedStatuses::markIfCheap(AbstractSlotVisitor&);
template void RecordedStatuses::markIfCheap(SlotVisitor&);

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


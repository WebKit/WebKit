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

#pragma once

#include "CallLinkStatus.h"
#include "CheckPrivateBrandStatus.h"
#include "DeleteByStatus.h"
#include "GetByStatus.h"
#include "InByStatus.h"
#include "PutByStatus.h"
#include "SetPrivateBrandStatus.h"

namespace JSC {

struct RecordedStatuses {
    RecordedStatuses() { }
    
    RecordedStatuses& operator=(const RecordedStatuses& other) = delete;
    
    RecordedStatuses& operator=(RecordedStatuses&& other);
    
    RecordedStatuses(const RecordedStatuses& other) = delete;
    
    RecordedStatuses(RecordedStatuses&& other);
    
    CallLinkStatus* addCallLinkStatus(const CodeOrigin&, const CallLinkStatus&);
    GetByStatus* addGetByStatus(const CodeOrigin&, const GetByStatus&);
    PutByStatus* addPutByStatus(const CodeOrigin&, const PutByStatus&);
    InByStatus* addInByStatus(const CodeOrigin&, const InByStatus&);
    DeleteByStatus* addDeleteByStatus(const CodeOrigin&, const DeleteByStatus&);
    CheckPrivateBrandStatus* addCheckPrivateBrandStatus(const CodeOrigin&, const CheckPrivateBrandStatus&);
    SetPrivateBrandStatus* addSetPrivateBrandStatus(const CodeOrigin&, const SetPrivateBrandStatus&);
    
    DECLARE_VISIT_AGGREGATE;
    template<typename Visitor> void markIfCheap(Visitor&);
    
    void finalizeWithoutDeleting(VM&);
    void finalize(VM&);
    
    void shrinkToFit();
    
    template<typename Func>
    void forEachVector(const Func& func)
    {
        func(calls);
        func(gets);
        func(puts);
        func(ins);
        func(deletes);
        func(checkPrivateBrands);
        func(setPrivateBrands);
    }
    
    Vector<std::pair<CodeOrigin, std::unique_ptr<CallLinkStatus>>> calls;
    Vector<std::pair<CodeOrigin, std::unique_ptr<GetByStatus>>> gets;
    Vector<std::pair<CodeOrigin, std::unique_ptr<PutByStatus>>> puts;
    Vector<std::pair<CodeOrigin, std::unique_ptr<InByStatus>>> ins;
    Vector<std::pair<CodeOrigin, std::unique_ptr<DeleteByStatus>>> deletes;
    Vector<std::pair<CodeOrigin, std::unique_ptr<CheckPrivateBrandStatus>>> checkPrivateBrands;
    Vector<std::pair<CodeOrigin, std::unique_ptr<SetPrivateBrandStatus>>> setPrivateBrands;
};

} // namespace JSC


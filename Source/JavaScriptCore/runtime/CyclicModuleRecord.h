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

#include <JavaScriptCore/AbstractModuleRecord.h>

namespace JSC {

class ErrorInstance;

class CyclicModuleRecord : public AbstractModuleRecord {
    friend class LLIntOffsetsExtractor;
public:
    using Base = AbstractModuleRecord;

    enum class Status : uint8_t {
        New,
        Unlinked,
        Linking,
        Linked,
        Evaluating,
        EvaluatingAsync,
        Evaluated,
    };

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    template<typename CellType, SubspaceAccess>
    static void subspaceFor(VM&)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    void initializeEnvironment(JSGlobalObject*, RefPtr<ScriptFetcher>);
    void link(JSGlobalObject*, RefPtr<ScriptFetcher>);
    JSPromise* evaluate(JSGlobalObject*);
    void execute(JSGlobalObject*, JSPromise* = nullptr);
    void executeAsync(JSGlobalObject*);
    void asyncExecutionFulfilled(JSGlobalObject*);
    void asyncExecutionRejected(JSGlobalObject*, JSValue);

    Status status() const { return m_status; }
    JSValue evaluationError() const { return m_evaluationError.get(); }
    unsigned dfsAncestorIndex() const { return m_dfsAncestorIndex; }

    void setStatus(Status newStatus) { m_status = newStatus; }
    void setEvaluationError(VM& vm, JSValue error) { m_evaluationError.set(vm, this, error); }
    void setDFSAncestorIndex(unsigned newIndex) { m_dfsAncestorIndex = newIndex; }

protected:
    CyclicModuleRecord(VM&, Structure*, const Identifier&);
    void finishCreation(JSGlobalObject*, VM&);

    WriteBarrier<Unknown> m_evaluationError;
    unsigned m_dfsAncestorIndex { 0 };
    Status m_status { Status::New };
    bool m_initialized { false };
};

} // namespace JSC

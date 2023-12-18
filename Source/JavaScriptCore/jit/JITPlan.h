/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include "CompilationResult.h"
#include "JITCode.h"
#include "JITCompilationKey.h"
#include "JITCompilationMode.h"
#include "JITPlanStage.h"
#include "ReleaseHeapAccessScope.h"
#include <wtf/MonotonicTime.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class AbstractSlotVisitor;
class CodeBlock;
class JITWorklistThread;
class VM;

class JITPlan : public ThreadSafeRefCounted<JITPlan> {
protected:
    JITPlan(JITCompilationMode, CodeBlock*);

public:
    virtual ~JITPlan() { }

    VM* vm() const { return m_vm; }
    CodeBlock* codeBlock() const { return m_codeBlock; }
    JITWorklistThread* thread() const { return m_thread; }

    JITCompilationMode mode() const { return m_mode; }

    JITPlanStage stage() const { return m_stage; }
    bool isDFG() const { return ::JSC::isDFG(m_mode); }
    bool isFTL() const { return ::JSC::isFTL(m_mode); }
    bool isUnlinked() const { return ::JSC::isUnlinked(m_mode); }

    enum class Tier { Baseline = 0, DFG = 1, FTL = 2, Count = 3 };
    Tier tier() const;
    JITType jitType() const
    {
        switch (tier()) {
        case Tier::Baseline:
            return JITType::BaselineJIT;
        case Tier::DFG:
            return JITType::DFGJIT;
        case Tier::FTL:
            return JITType::FTLJIT;
        default:
            return JITType::None;
        }
    }

    JITCompilationKey key();

    void compileInThread(JITWorklistThread*);

    virtual size_t codeSize() const = 0;

    virtual CompilationResult finalize() = 0;

    virtual void finalizeInGC() { }

    void notifyCompiling();
    virtual void notifyReady();
    virtual void cancel();

    virtual bool isKnownToBeLiveAfterGC();
    virtual bool isKnownToBeLiveDuringGC(AbstractSlotVisitor&);
    virtual bool iterateCodeBlocksForGC(AbstractSlotVisitor&, const Function<void(CodeBlock*)>&);
    virtual bool checkLivenessAndVisitChildren(AbstractSlotVisitor&);

protected:
    bool computeCompileTimes() const;
    bool reportCompileTimes() const;

    enum CompilationPath { FailPath, BaselinePath, DFGPath, FTLPath, CancelPath };
    virtual CompilationPath compileInThreadImpl() = 0;

    JITPlanStage m_stage { JITPlanStage::Preparing };
    JITCompilationMode m_mode;
    MonotonicTime m_timeBeforeFTL;
    VM* m_vm;
    CodeBlock* m_codeBlock;
    JITWorklistThread* m_thread { nullptr };
};

} // namespace JSC

#endif // ENABLE(JIT)

/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "CCallHelpers.h"
#include "JITAddGenerator.h"
#include "JITMathICInlineResult.h"
#include "LinkBuffer.h"
#include "Repatch.h"
#include "SnippetOperand.h"

namespace JSC {

class LinkBuffer;

struct MathICGenerationState {
    MacroAssembler::Label fastPathStart;
    MacroAssembler::Label fastPathEnd;
    MacroAssembler::Label slowPathStart;
    MacroAssembler::Call slowPathCall;
    MacroAssembler::JumpList slowPathJumps;
    bool shouldSlowPathRepatch;
};

template <typename GeneratorType>
class JITMathIC {
public:
    CodeLocationLabel doneLocation() { return m_inlineStart.labelAtOffset(m_inlineSize); }
    CodeLocationLabel slowPathStartLocation() { return m_inlineStart.labelAtOffset(m_deltaFromStartToSlowPathStart); }
    CodeLocationCall slowPathCallLocation() { return m_inlineStart.callAtOffset(m_deltaFromStartToSlowPathCallLocation); }

    bool generateInline(CCallHelpers& jit, MathICGenerationState& state)
    {
        state.fastPathStart = jit.label();
        size_t startSize = jit.m_assembler.buffer().codeSize();
        JITMathICInlineResult result = m_generator.generateInline(jit, state);

        switch (result) {
        case JITMathICInlineResult::GeneratedFastPath: {
            size_t inlineSize = jit.m_assembler.buffer().codeSize() - startSize;
            if (static_cast<ptrdiff_t>(inlineSize) < MacroAssembler::maxJumpReplacementSize()) {
                size_t nopsToEmitInBytes = MacroAssembler::maxJumpReplacementSize() - inlineSize;
                jit.emitNops(nopsToEmitInBytes);
            }
            state.shouldSlowPathRepatch = true;
            state.fastPathEnd = jit.label();
            return true;
        }
        case JITMathICInlineResult::GenerateFullSnippet: {
            MacroAssembler::JumpList endJumpList;
            bool result = m_generator.generateFastPath(jit, endJumpList, state.slowPathJumps);
            if (result) {
                state.fastPathEnd = jit.label();
                state.shouldSlowPathRepatch = false;
                endJumpList.link(&jit);
                return true;
            }
            return false;
        }
        case JITMathICInlineResult::DontGenerate: {
            return false;
        }
        default:
            ASSERT_NOT_REACHED();
        }

        return false;
    }

    void generateOutOfLine(VM& vm, CodeBlock* codeBlock, FunctionPtr callReplacement)
    {
        // We rewire to the alternate regardless of whether or not we can allocate the out of line path
        // because if we fail allocating the out of line path, we don't want to waste time trying to
        // allocate it in the future.
        ftlThunkAwareRepatchCall(codeBlock, slowPathCallLocation(), callReplacement);

        {
            CCallHelpers jit(&vm, codeBlock);

            MacroAssembler::JumpList endJumpList; 
            MacroAssembler::JumpList slowPathJumpList; 
            bool emittedFastPath = m_generator.generateFastPath(jit, endJumpList, slowPathJumpList);
            if (!emittedFastPath)
                return;
            endJumpList.append(jit.jump());

            LinkBuffer linkBuffer(vm, jit, codeBlock, JITCompilationCanFail);
            if (linkBuffer.didFailToAllocate())
                return;

            linkBuffer.link(endJumpList, doneLocation());
            linkBuffer.link(slowPathJumpList, slowPathStartLocation());

            m_code = FINALIZE_CODE_FOR(
                codeBlock, linkBuffer, ("JITMathIC: generating out of line IC snippet"));
        }

        {
            CCallHelpers jit(&vm, codeBlock);
            auto jump = jit.jump();
            // We don't need a nop sled here because nobody should be jumping into the middle of an IC.
            bool needsBranchCompaction = false;
            RELEASE_ASSERT(jit.m_assembler.buffer().codeSize() <= static_cast<size_t>(m_inlineSize));
            LinkBuffer linkBuffer(jit, m_inlineStart.dataLocation(), jit.m_assembler.buffer().codeSize(), JITCompilationMustSucceed, needsBranchCompaction);
            RELEASE_ASSERT(linkBuffer.isValid());
            linkBuffer.link(jump, CodeLocationLabel(m_code.code()));
            FINALIZE_CODE(linkBuffer, ("JITMathIC: linking constant jump to out of line stub"));
        }
    }

    void finalizeInlineCode(const MathICGenerationState& state, LinkBuffer& linkBuffer)
    {
        CodeLocationLabel start = linkBuffer.locationOf(state.fastPathStart);
        m_inlineStart = start;

        m_inlineSize = MacroAssembler::differenceBetweenCodePtr(
            start, linkBuffer.locationOf(state.fastPathEnd));
        ASSERT(m_inlineSize > 0);

        m_deltaFromStartToSlowPathCallLocation = MacroAssembler::differenceBetweenCodePtr(
            start, linkBuffer.locationOf(state.slowPathCall));
        m_deltaFromStartToSlowPathStart = MacroAssembler::differenceBetweenCodePtr(
            start, linkBuffer.locationOf(state.slowPathStart));
    }

    MacroAssemblerCodeRef m_code;
    CodeLocationLabel m_inlineStart;
    int32_t m_inlineSize;
    int32_t m_deltaFromStartToSlowPathCallLocation;
    int32_t m_deltaFromStartToSlowPathStart;
    GeneratorType m_generator;
};

typedef JITMathIC<JITAddGenerator> JITAddIC;

} // namespace JSC

#endif // ENABLE(JIT)

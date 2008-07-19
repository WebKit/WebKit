/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CodeBlock_h
#define CodeBlock_h

#include "Instruction.h"
#include "JSGlobalObject.h"
#include "nodes.h"
#include "SourceRange.h"
#include "ustring.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace KJS {

    class ExecState;

    static ALWAYS_INLINE int missingThisObjectMarker() { return std::numeric_limits<int>::max(); }

    struct HandlerInfo {
        uint32_t start;
        uint32_t end;
        uint32_t target;
        uint32_t scopeDepth;
    };

    struct ExpressionRangeInfo {
        enum { MaxOffset = (1 << 7) - 1, 
               MaxDivot = (1 << 25) - 1
        };
        uint32_t instructionOffset : 25;
        uint32_t divotPoint : 25;
        uint32_t startOffset : 7;
        uint32_t endOffset : 7;
    };

    struct LineInfo {
        uint32_t instructionOffset;
        int32_t lineNumber;
    };

    struct CodeBlock {
        CodeBlock(ScopeNode* ownerNode_, CodeType codeType_, PassRefPtr<SourceProvider> source_, unsigned sourceOffset_)
            : ownerNode(ownerNode_)
            , numTemporaries(0)
            , numVars(0)
            , numParameters(0)
            , numLocals(0)
            , needsFullScopeChain(ownerNode_->usesEval() || ownerNode_->needsClosure())
            , usesEval(ownerNode_->usesEval())
            , codeType(codeType_)
            , source(source_)
            , sourceOffset(sourceOffset_)
        {
        }

        void dump(ExecState*) const;
        int expressionRangeForVPC(const Instruction*, int& divot, int& startOffset, int& endOffset);
        int lineNumberForVPC(const Instruction* vPC);
        bool getHandlerForVPC(const Instruction* vPC, Instruction*& target, int& scopeDepth);
        void mark();

        ScopeNode* ownerNode;

        int numTemporaries;
        int numVars;
        int numParameters;
        int numLocals;
        int thisRegister;
        bool needsFullScopeChain;
        bool usesEval;
        CodeType codeType;
        RefPtr<SourceProvider> source;
        unsigned sourceOffset;

        Vector<Instruction> instructions;

        // Constant pool
        Vector<Identifier> identifiers;
        Vector<RefPtr<FuncDeclNode> > functions;
        Vector<RefPtr<FuncExprNode> > functionExpressions;
        Vector<Register> registers;
        Vector<RefPtr<RegExp> > regexps;
        Vector<HandlerInfo> exceptionHandlers;
        Vector<ExpressionRangeInfo> expressionInfo;
        Vector<LineInfo> lineInfo;

    private:
        void dump(ExecState*, const Vector<Instruction>::const_iterator& begin, Vector<Instruction>::const_iterator&) const;
    };

    // Program code is not marked by any function, so we make the global object
    // responsible for marking it.

    struct ProgramCodeBlock : public CodeBlock {
        ProgramCodeBlock(ScopeNode* ownerNode_, CodeType codeType_, JSGlobalObject* globalObject_, PassRefPtr<SourceProvider> source_)
            : CodeBlock(ownerNode_, codeType_, source_, 0)
            , globalObject(globalObject_)
        {
            globalObject->codeBlocks().add(this);
        }

        ~ProgramCodeBlock()
        {
            if (globalObject)
                globalObject->codeBlocks().remove(this);
        }

        JSGlobalObject* globalObject; // For program and eval nodes, the global object that marks the constant pool.
    };

    struct EvalCodeBlock : public ProgramCodeBlock {
        EvalCodeBlock(ScopeNode* ownerNode_, JSGlobalObject* globalObject_, PassRefPtr<SourceProvider> source_)
            : ProgramCodeBlock(ownerNode_, EvalCode, globalObject_, source_)
        {
        }
    };

} // namespace KJS

#endif // CodeBlock_h

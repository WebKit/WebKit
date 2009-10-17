/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef BytecodeGenerator_h
#define BytecodeGenerator_h

#include "CodeBlock.h"
#include "HashTraits.h"
#include "Instruction.h"
#include "Label.h"
#include "LabelScope.h"
#include "Interpreter.h"
#include "RegisterID.h"
#include "SymbolTable.h"
#include "Debugger.h"
#include "Nodes.h"
#include <wtf/FastAllocBase.h>
#include <wtf/PassRefPtr.h>
#include <wtf/SegmentedVector.h>
#include <wtf/Vector.h>

namespace JSC {

    class Identifier;
    class ScopeChain;
    class ScopeNode;

    struct FinallyContext {
        Label* finallyAddr;
        RegisterID* retAddrDst;
    };

    struct ControlFlowContext {
        bool isFinallyBlock;
        FinallyContext finallyContext;
    };

    class BytecodeGenerator : public FastAllocBase {
    public:
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        static void setDumpsGeneratedCode(bool dumpsGeneratedCode);
        static bool dumpsGeneratedCode();

        BytecodeGenerator(ProgramNode*, const Debugger*, const ScopeChain&, SymbolTable*, ProgramCodeBlock*);
        BytecodeGenerator(FunctionBodyNode*, const Debugger*, const ScopeChain&, SymbolTable*, CodeBlock*);
        BytecodeGenerator(EvalNode*, const Debugger*, const ScopeChain&, SymbolTable*, EvalCodeBlock*);

        JSGlobalData* globalData() const { return m_globalData; }
        const CommonIdentifiers& propertyNames() const { return *m_globalData->propertyNames; }

        void generate();

        // Returns the register corresponding to a local variable, or 0 if no
        // such register exists. Registers returned by registerFor do not
        // require explicit reference counting.
        RegisterID* registerFor(const Identifier&);
        
        bool willResolveToArguments(const Identifier&);
        RegisterID* uncheckedRegisterForArguments();

        // Behaves as registerFor does, but ignores dynamic scope as
        // dynamic scope should not interfere with const initialisation
        RegisterID* constRegisterFor(const Identifier&);

        // Searches the scope chain in an attempt to  statically locate the requested
        // property.  Returns false if for any reason the property cannot be safely
        // optimised at all.  Otherwise it will return the index and depth of the
        // VariableObject that defines the property.  If the property cannot be found
        // statically, depth will contain the depth of the scope chain where dynamic
        // lookup must begin.
        //
        // NB: depth does _not_ include the local scope.  eg. a depth of 0 refers
        // to the scope containing this codeblock.
        bool findScopedProperty(const Identifier&, int& index, size_t& depth, bool forWriting, JSObject*& globalObject);

        // Returns the register storing "this"
        RegisterID* thisRegister() { return &m_thisRegister; }

        bool isLocal(const Identifier&);
        bool isLocalConstant(const Identifier&);

        // Returns the next available temporary register. Registers returned by
        // newTemporary require a modified form of reference counting: any
        // register with a refcount of 0 is considered "available", meaning that
        // the next instruction may overwrite it.
        RegisterID* newTemporary();

        RegisterID* highestUsedRegister();

        // The same as newTemporary(), but this function returns "suggestion" if
        // "suggestion" is a temporary. This function is helpful in situations
        // where you've put "suggestion" in a RefPtr, but you'd like to allow
        // the next instruction to overwrite it anyway.
        RegisterID* newTemporaryOr(RegisterID* suggestion) { return suggestion->isTemporary() ? suggestion : newTemporary(); }

        // Functions for handling of dst register

        RegisterID* ignoredResult() { return &m_ignoredResultRegister; }

        // Returns a place to write intermediate values of an operation
        // which reuses dst if it is safe to do so.
        RegisterID* tempDestination(RegisterID* dst)
        {
            return (dst && dst != ignoredResult() && dst->isTemporary()) ? dst : newTemporary();
        }

        // Returns the place to write the final output of an operation.
        RegisterID* finalDestination(RegisterID* originalDst, RegisterID* tempDst = 0)
        {
            if (originalDst && originalDst != ignoredResult())
                return originalDst;
            ASSERT(tempDst != ignoredResult());
            if (tempDst && tempDst->isTemporary())
                return tempDst;
            return newTemporary();
        }

        RegisterID* destinationForAssignResult(RegisterID* dst)
        {
            if (dst && dst != ignoredResult() && m_codeBlock->needsFullScopeChain())
                return dst->isTemporary() ? dst : newTemporary();
            return 0;
        }

        // Moves src to dst if dst is not null and is different from src, otherwise just returns src.
        RegisterID* moveToDestinationIfNeeded(RegisterID* dst, RegisterID* src)
        {
            return dst == ignoredResult() ? 0 : (dst && dst != src) ? emitMove(dst, src) : src;
        }

        PassRefPtr<LabelScope> newLabelScope(LabelScope::Type, const Identifier* = 0);
        PassRefPtr<Label> newLabel();

        // The emitNode functions are just syntactic sugar for calling
        // Node::emitCode. These functions accept a 0 for the register,
        // meaning that the node should allocate a register, or ignoredResult(),
        // meaning that the node need not put the result in a register.
        // Other emit functions do not accept 0 or ignoredResult().
        RegisterID* emitNode(RegisterID* dst, Node* n)
        {
            // Node::emitCode assumes that dst, if provided, is either a local or a referenced temporary.
            ASSERT(!dst || dst == ignoredResult() || !dst->isTemporary() || dst->refCount());
            if (!m_codeBlock->numberOfLineInfos() || m_codeBlock->lastLineInfo().lineNumber != n->lineNo()) {
                LineInfo info = { instructions().size(), n->lineNo() };
                m_codeBlock->addLineInfo(info);
            }
            if (m_emitNodeDepth >= s_maxEmitNodeDepth)
                return emitThrowExpressionTooDeepException();
            ++m_emitNodeDepth;
            RegisterID* r = n->emitBytecode(*this, dst);
            --m_emitNodeDepth;
            return r;
        }

        RegisterID* emitNode(Node* n)
        {
            return emitNode(0, n);
        }

        void emitExpressionInfo(unsigned divot, unsigned startOffset, unsigned endOffset)
        { 
            divot -= m_codeBlock->sourceOffset();
            if (divot > ExpressionRangeInfo::MaxDivot) {
                // Overflow has occurred, we can only give line number info for errors for this region
                divot = 0;
                startOffset = 0;
                endOffset = 0;
            } else if (startOffset > ExpressionRangeInfo::MaxOffset) {
                // If the start offset is out of bounds we clear both offsets
                // so we only get the divot marker.  Error message will have to be reduced
                // to line and column number.
                startOffset = 0;
                endOffset = 0;
            } else if (endOffset > ExpressionRangeInfo::MaxOffset) {
                // The end offset is only used for additional context, and is much more likely
                // to overflow (eg. function call arguments) so we are willing to drop it without
                // dropping the rest of the range.
                endOffset = 0;
            }
            
            ExpressionRangeInfo info;
            info.instructionOffset = instructions().size();
            info.divotPoint = divot;
            info.startOffset = startOffset;
            info.endOffset = endOffset;
            m_codeBlock->addExpressionInfo(info);
        }

        void emitGetByIdExceptionInfo(OpcodeID opcodeID)
        {
            // Only op_construct and op_instanceof need exception info for
            // a preceding op_get_by_id.
            ASSERT(opcodeID == op_construct || opcodeID == op_instanceof);
            GetByIdExceptionInfo info;
            info.bytecodeOffset = instructions().size();
            info.isOpConstruct = (opcodeID == op_construct);
            m_codeBlock->addGetByIdExceptionInfo(info);
        }
        
        ALWAYS_INLINE bool leftHandSideNeedsCopy(bool rightHasAssignments, bool rightIsPure)
        {
            return (m_codeType != FunctionCode || m_codeBlock->needsFullScopeChain() || rightHasAssignments) && !rightIsPure;
        }

        ALWAYS_INLINE PassRefPtr<RegisterID> emitNodeForLeftHandSide(ExpressionNode* n, bool rightHasAssignments, bool rightIsPure)
        {
            if (leftHandSideNeedsCopy(rightHasAssignments, rightIsPure)) {
                PassRefPtr<RegisterID> dst = newTemporary();
                emitNode(dst.get(), n);
                return dst;
            }

            return PassRefPtr<RegisterID>(emitNode(n));
        }

        RegisterID* emitLoad(RegisterID* dst, bool);
        RegisterID* emitLoad(RegisterID* dst, double);
        RegisterID* emitLoad(RegisterID* dst, const Identifier&);
        RegisterID* emitLoad(RegisterID* dst, JSValue);

        RegisterID* emitUnaryOp(OpcodeID, RegisterID* dst, RegisterID* src);
        RegisterID* emitBinaryOp(OpcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2, OperandTypes);
        RegisterID* emitEqualityOp(OpcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2);
        RegisterID* emitUnaryNoDstOp(OpcodeID, RegisterID* src);

        RegisterID* emitNewObject(RegisterID* dst);
        RegisterID* emitNewArray(RegisterID* dst, ElementNode*); // stops at first elision

        RegisterID* emitNewFunction(RegisterID* dst, FunctionBodyNode* body);
        RegisterID* emitNewFunctionExpression(RegisterID* dst, FuncExprNode* func);
        RegisterID* emitNewRegExp(RegisterID* dst, RegExp* regExp);

        RegisterID* emitMove(RegisterID* dst, RegisterID* src);

        RegisterID* emitToJSNumber(RegisterID* dst, RegisterID* src) { return emitUnaryOp(op_to_jsnumber, dst, src); }
        RegisterID* emitPreInc(RegisterID* srcDst);
        RegisterID* emitPreDec(RegisterID* srcDst);
        RegisterID* emitPostInc(RegisterID* dst, RegisterID* srcDst);
        RegisterID* emitPostDec(RegisterID* dst, RegisterID* srcDst);

        RegisterID* emitInstanceOf(RegisterID* dst, RegisterID* value, RegisterID* base, RegisterID* basePrototype);
        RegisterID* emitTypeOf(RegisterID* dst, RegisterID* src) { return emitUnaryOp(op_typeof, dst, src); }
        RegisterID* emitIn(RegisterID* dst, RegisterID* property, RegisterID* base) { return emitBinaryOp(op_in, dst, property, base, OperandTypes()); }

        RegisterID* emitResolve(RegisterID* dst, const Identifier& property);
        RegisterID* emitGetScopedVar(RegisterID* dst, size_t skip, int index, JSValue globalObject);
        RegisterID* emitPutScopedVar(size_t skip, int index, RegisterID* value, JSValue globalObject);

        RegisterID* emitResolveBase(RegisterID* dst, const Identifier& property);
        RegisterID* emitResolveWithBase(RegisterID* baseDst, RegisterID* propDst, const Identifier& property);

        void emitMethodCheck();

        RegisterID* emitGetById(RegisterID* dst, RegisterID* base, const Identifier& property);
        RegisterID* emitPutById(RegisterID* base, const Identifier& property, RegisterID* value);
        RegisterID* emitDeleteById(RegisterID* dst, RegisterID* base, const Identifier&);
        RegisterID* emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitPutByVal(RegisterID* base, RegisterID* property, RegisterID* value);
        RegisterID* emitDeleteByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitPutByIndex(RegisterID* base, unsigned index, RegisterID* value);
        RegisterID* emitPutGetter(RegisterID* base, const Identifier& property, RegisterID* value);
        RegisterID* emitPutSetter(RegisterID* base, const Identifier& property, RegisterID* value);

        RegisterID* emitCall(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);
        RegisterID* emitCallEval(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);
        RegisterID* emitCallVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* argCount, unsigned divot, unsigned startOffset, unsigned endOffset);
        RegisterID* emitLoadVarargs(RegisterID* argCountDst, RegisterID* args);

        RegisterID* emitReturn(RegisterID* src);
        RegisterID* emitEnd(RegisterID* src) { return emitUnaryNoDstOp(op_end, src); }

        RegisterID* emitConstruct(RegisterID* dst, RegisterID* func, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);
        RegisterID* emitStrcat(RegisterID* dst, RegisterID* src, int count);
        void emitToPrimitive(RegisterID* dst, RegisterID* src);

        PassRefPtr<Label> emitLabel(Label*);
        PassRefPtr<Label> emitJump(Label* target);
        PassRefPtr<Label> emitJumpIfTrue(RegisterID* cond, Label* target);
        PassRefPtr<Label> emitJumpIfFalse(RegisterID* cond, Label* target);
        PassRefPtr<Label> emitJumpIfNotFunctionCall(RegisterID* cond, Label* target);
        PassRefPtr<Label> emitJumpIfNotFunctionApply(RegisterID* cond, Label* target);
        PassRefPtr<Label> emitJumpScopes(Label* target, int targetScopeDepth);

        PassRefPtr<Label> emitJumpSubroutine(RegisterID* retAddrDst, Label*);
        void emitSubroutineReturn(RegisterID* retAddrSrc);

        RegisterID* emitGetPropertyNames(RegisterID* dst, RegisterID* base, RegisterID* i, RegisterID* size, Label* breakTarget);
        RegisterID* emitNextPropertyName(RegisterID* dst, RegisterID* base, RegisterID* i, RegisterID* size, RegisterID* iter, Label* target);

        RegisterID* emitCatch(RegisterID*, Label* start, Label* end);
        void emitThrow(RegisterID* exc) { emitUnaryNoDstOp(op_throw, exc); }
        RegisterID* emitNewError(RegisterID* dst, ErrorType type, JSValue message);
        void emitPushNewScope(RegisterID* dst, const Identifier& property, RegisterID* value);

        RegisterID* emitPushScope(RegisterID* scope);
        void emitPopScope();

        void emitDebugHook(DebugHookID, int firstLine, int lastLine);

        int scopeDepth() { return m_dynamicScopeDepth + m_finallyDepth; }
        bool hasFinaliser() { return m_finallyDepth != 0; }

        void pushFinallyContext(Label* target, RegisterID* returnAddrDst);
        void popFinallyContext();

        LabelScope* breakTarget(const Identifier&);
        LabelScope* continueTarget(const Identifier&);

        void beginSwitch(RegisterID*, SwitchInfo::SwitchType);
        void endSwitch(uint32_t clauseCount, RefPtr<Label>*, ExpressionNode**, Label* defaultLabel, int32_t min, int32_t range);

        CodeType codeType() const { return m_codeType; }

        void setRegeneratingForExceptionInfo(CodeBlock* originalCodeBlock)
        {
            m_regeneratingForExceptionInfo = true;
            m_codeBlockBeingRegeneratedFrom = originalCodeBlock;
        }

    private:
        void emitOpcode(OpcodeID);
        void retrieveLastBinaryOp(int& dstIndex, int& src1Index, int& src2Index);
        void retrieveLastUnaryOp(int& dstIndex, int& srcIndex);
        void rewindBinaryOp();
        void rewindUnaryOp();

        PassRefPtr<Label> emitComplexJumpScopes(Label* target, ControlFlowContext* topScope, ControlFlowContext* bottomScope);

        typedef HashMap<EncodedJSValue, unsigned, EncodedJSValueHash, EncodedJSValueHashTraits> JSValueMap;

        struct IdentifierMapIndexHashTraits {
            typedef int TraitType;
            typedef IdentifierMapIndexHashTraits StorageTraits;
            static int emptyValue() { return std::numeric_limits<int>::max(); }
            static const bool emptyValueIsZero = false;
            static const bool needsDestruction = false;
            static const bool needsRef = false;
        };

        typedef HashMap<RefPtr<UString::Rep>, int, IdentifierRepHash, HashTraits<RefPtr<UString::Rep> >, IdentifierMapIndexHashTraits> IdentifierMap;
        typedef HashMap<double, JSValue> NumberMap;
        typedef HashMap<UString::Rep*, JSString*, IdentifierRepHash> IdentifierStringMap;
        
        RegisterID* emitCall(OpcodeID, RegisterID* dst, RegisterID* func, RegisterID* thisRegister, ArgumentsNode*, unsigned divot, unsigned startOffset, unsigned endOffset);
        
        RegisterID* newRegister();

        // Returns the RegisterID corresponding to ident.
        RegisterID* addVar(const Identifier& ident, bool isConstant)
        {
            RegisterID* local;
            addVar(ident, isConstant, local);
            return local;
        }
        // Returns true if a new RegisterID was added, false if a pre-existing RegisterID was re-used.
        bool addVar(const Identifier&, bool isConstant, RegisterID*&);

        // Returns the RegisterID corresponding to ident.
        RegisterID* addGlobalVar(const Identifier& ident, bool isConstant)
        {
            RegisterID* local;
            addGlobalVar(ident, isConstant, local);
            return local;
        }
        // Returns true if a new RegisterID was added, false if a pre-existing RegisterID was re-used.
        bool addGlobalVar(const Identifier&, bool isConstant, RegisterID*&);

        RegisterID* addParameter(const Identifier&);
        
        void preserveLastVar();

        RegisterID& registerFor(int index)
        {
            if (index >= 0)
                return m_calleeRegisters[index];

            if (index == RegisterFile::OptionalCalleeArguments)
                return m_argumentsRegister;

            if (m_parameters.size()) {
                ASSERT(!m_globals.size());
                return m_parameters[index + m_parameters.size() + RegisterFile::CallFrameHeaderSize];
            }

            return m_globals[-index - 1];
        }

        unsigned addConstant(const Identifier&);
        RegisterID* addConstantValue(JSValue);
        unsigned addRegExp(RegExp*);

        PassRefPtr<FunctionExecutable> makeFunction(ExecState* exec, FunctionBodyNode* body)
        {
            return FunctionExecutable::create(exec, body->ident(), body->source(), body->usesArguments(), body->parameters(), body->lineNo(), body->lastLine());
        }

        PassRefPtr<FunctionExecutable> makeFunction(JSGlobalData* globalData, FunctionBodyNode* body)
        {
            return FunctionExecutable::create(globalData, body->ident(), body->source(), body->usesArguments(), body->parameters(), body->lineNo(), body->lastLine());
        }

        Vector<Instruction>& instructions() { return m_codeBlock->instructions(); }
        SymbolTable& symbolTable() { return *m_symbolTable; }

        bool shouldOptimizeLocals() { return (m_codeType != EvalCode) && !m_dynamicScopeDepth; }
        bool canOptimizeNonLocals() { return (m_codeType == FunctionCode) && !m_dynamicScopeDepth && !m_codeBlock->usesEval(); }

        RegisterID* emitThrowExpressionTooDeepException();

        void createArgumentsIfNecessary();

        bool m_shouldEmitDebugHooks;
        bool m_shouldEmitProfileHooks;

        const ScopeChain* m_scopeChain;
        SymbolTable* m_symbolTable;

        ScopeNode* m_scopeNode;
        CodeBlock* m_codeBlock;

        // Some of these objects keep pointers to one another. They are arranged
        // to ensure a sane destruction order that avoids references to freed memory.
        HashSet<RefPtr<UString::Rep>, IdentifierRepHash> m_functions;
        RegisterID m_ignoredResultRegister;
        RegisterID m_thisRegister;
        RegisterID m_argumentsRegister;
        int m_activationRegisterIndex;
        SegmentedVector<RegisterID, 32> m_constantPoolRegisters;
        SegmentedVector<RegisterID, 32> m_calleeRegisters;
        SegmentedVector<RegisterID, 32> m_parameters;
        SegmentedVector<RegisterID, 32> m_globals;
        SegmentedVector<Label, 32> m_labels;
        SegmentedVector<LabelScope, 8> m_labelScopes;
        RefPtr<RegisterID> m_lastVar;
        int m_finallyDepth;
        int m_dynamicScopeDepth;
        int m_baseScopeDepth;
        CodeType m_codeType;

        Vector<ControlFlowContext> m_scopeContextStack;
        Vector<SwitchInfo> m_switchContextStack;

        int m_nextGlobalIndex;
        int m_nextParameterIndex;
        int m_firstConstantIndex;
        int m_nextConstantOffset;
        unsigned m_globalConstantIndex;

        int m_globalVarStorageOffset;

        // Constant pool
        IdentifierMap m_identifierMap;
        JSValueMap m_jsValueMap;
        NumberMap m_numberMap;
        IdentifierStringMap m_stringMap;

        JSGlobalData* m_globalData;

        OpcodeID m_lastOpcodeID;

        unsigned m_emitNodeDepth;

        bool m_regeneratingForExceptionInfo;
        CodeBlock* m_codeBlockBeingRegeneratedFrom;

        static const unsigned s_maxEmitNodeDepth = 5000;
    };

}

#endif // BytecodeGenerator_h

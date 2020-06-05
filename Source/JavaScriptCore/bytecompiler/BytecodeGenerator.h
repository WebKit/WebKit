/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 * Copyright (C) 2012 Igalia, S.L.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#include "BytecodeGeneratorBase.h"
#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "Instruction.h"
#include "Interpreter.h"
#include "JSAsyncGenerator.h"
#include "JSBigInt.h"
#include "JSGenerator.h"
#include "JSTemplateObjectDescriptor.h"
#include "Label.h"
#include "LabelScope.h"
#include "Nodes.h"
#include "ParserError.h"
#include "ProfileTypeBytecodeFlag.h"
#include "RegisterID.h"
#include "StaticPropertyAnalyzer.h"
#include "SymbolTable.h"
#include "UnlinkedCodeBlock.h"
#include "UnlinkedCodeBlockGenerator.h"
#include <functional>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashFunctions.h>
#include <wtf/Optional.h>
#include <wtf/SegmentedVector.h>
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

namespace JSC {

    class JSImmutableButterfly;
    class Identifier;
    class IndexedForInContext;
    class StructureForInContext;

    enum ExpectedFunction {
        NoExpectedFunction,
        ExpectObjectConstructor,
        ExpectArrayConstructor
    };

    enum class EmitAwait { Yes, No };

    enum class DebuggableCall { Yes, No };
    enum class ThisResolutionType { Local, Scoped };
    enum class LinkTimeConstant : int32_t;
    
    class CallArguments {
    public:
        CallArguments(BytecodeGenerator&, ArgumentsNode*, unsigned additionalArguments = 0);

        RegisterID* thisRegister() { return m_argv[0].get(); }
        RegisterID* argumentRegister(unsigned i) { return m_argv[i + 1].get(); }
        unsigned stackOffset() { return -m_argv[0]->index() + CallFrame::headerSizeInRegisters; }
        unsigned argumentCountIncludingThis() { return m_argv.size() - m_padding; }
        ArgumentsNode* argumentsNode() { return m_argumentsNode; }

    private:
        ArgumentsNode* m_argumentsNode;
        Vector<RefPtr<RegisterID>, 8, UnsafeVectorOverflow> m_argv;
        unsigned m_padding;
    };

    class Variable {
    public:
        enum VariableKind { NormalVariable, SpecialVariable };

        Variable() = default;
        
        Variable(const Identifier& ident)
            : m_ident(ident)
            , m_local(nullptr)
            , m_attributes(0)
            , m_kind(NormalVariable) // This is somewhat meaningless here for this kind of Variable.
            , m_symbolTableConstantIndex(0) // This is meaningless here for this kind of Variable.
            , m_isLexicallyScoped(false)
        {
        }

        Variable(const Identifier& ident, VarOffset offset, RegisterID* local, unsigned attributes, VariableKind kind, int symbolTableConstantIndex, bool isLexicallyScoped)
            : m_ident(ident)
            , m_offset(offset)
            , m_local(local)
            , m_attributes(attributes)
            , m_kind(kind)
            , m_symbolTableConstantIndex(symbolTableConstantIndex)
            , m_isLexicallyScoped(isLexicallyScoped)
        {
        }

        // If it's unset, then it is a non-locally-scoped variable. If it is set, then it could be
        // a stack variable, a scoped variable in a local scope, or a variable captured in the
        // direct arguments object.
        bool isResolved() const { return !!m_offset; }
        int symbolTableConstantIndex() const { ASSERT(isResolved() && !isSpecial()); return m_symbolTableConstantIndex; }
        
        const Identifier& ident() const { return m_ident; }
        
        VarOffset offset() const { return m_offset; }
        bool isLocal() const { return m_offset.isStack(); }
        RegisterID* local() const { return m_local; }

        bool isReadOnly() const { return m_attributes & PropertyAttribute::ReadOnly; }
        bool isSpecial() const { return m_kind != NormalVariable; }
        bool isConst() const { return isReadOnly() && m_isLexicallyScoped; }
        void setIsReadOnly() { m_attributes |= PropertyAttribute::ReadOnly; }

        void dump(PrintStream&) const;

        bool operator==(const Variable& other) const
        {
            return m_ident == other.m_ident
                && m_offset == other.m_offset
                && m_local == other.m_local
                && m_attributes == other.m_attributes
                && m_kind == other.m_kind
                && m_symbolTableConstantIndex == other.m_symbolTableConstantIndex
                && m_isLexicallyScoped == other.m_isLexicallyScoped;
        }

    private:
        Identifier m_ident;
        VarOffset m_offset { };
        RegisterID* m_local { nullptr };
        unsigned m_attributes { 0 };
        VariableKind m_kind { NormalVariable };
        int m_symbolTableConstantIndex { 0 }; // This is meaningless here for this default NormalVariable kind of Variable.
        bool m_isLexicallyScoped { false };
    };

    // https://tc39.github.io/ecma262/#sec-completion-record-specification-type
    //
    // For the Break and Continue cases, instead of using the Break and Continue enum values
    // below, we use the unique jumpID of the break and continue statement as the encoding
    // for the CompletionType value. emitFinallyCompletion() uses this jumpID value later
    // to determine the appropriate jump target to jump to after executing the relevant finally
    // blocks. The jumpID is computed as:
    //     jumpID = bytecodeOffset (of the break/continue node) + CompletionType::NumberOfTypes.
    // Hence, there won't be any collision between jumpIDs and CompletionType enums.
    enum class CompletionType : int {
        Normal,
        Throw,
        Return,
        NumberOfTypes
    };

    inline CompletionType bytecodeOffsetToJumpID(unsigned offset)
    {
        int jumpIDAsInt = offset + static_cast<int>(CompletionType::NumberOfTypes);
        ASSERT(jumpIDAsInt >= static_cast<int>(CompletionType::NumberOfTypes));
        return static_cast<CompletionType>(jumpIDAsInt);
    }

    struct FinallyJump {
        FinallyJump(CompletionType jumpID, int targetLexicalScopeIndex, Label& targetLabel)
            : jumpID(jumpID)
            , targetLexicalScopeIndex(targetLexicalScopeIndex)
            , targetLabel(targetLabel)
        { }

        CompletionType jumpID;
        int targetLexicalScopeIndex;
        Ref<Label> targetLabel;
    };

    class FinallyContext {
    public:
        FinallyContext() { }
        FinallyContext(BytecodeGenerator&, Label& finallyLabel);

        FinallyContext* outerContext() const { return m_outerContext; }
        Label* finallyLabel() const { return m_finallyLabel; }

        RegisterID* completionTypeRegister() const { return m_completionRecord.typeRegister.get(); }
        RegisterID* completionValueRegister() const { return m_completionRecord.valueRegister.get(); }

        uint32_t numberOfBreaksOrContinues() const { return m_numberOfBreaksOrContinues.unsafeGet(); }
        void incNumberOfBreaksOrContinues() { m_numberOfBreaksOrContinues++; }

        bool handlesReturns() const { return m_handlesReturns; }
        void setHandlesReturns() { m_handlesReturns = true; }

        void registerJump(CompletionType jumpID, int lexicalScopeIndex, Label& targetLabel)
        {
            m_jumps.append(FinallyJump(jumpID, lexicalScopeIndex, targetLabel));
        }

        size_t numberOfJumps() const { return m_jumps.size(); }
        FinallyJump& jumps(size_t i) { return m_jumps[i]; }

    private:
        FinallyContext* m_outerContext { nullptr };
        Label* m_finallyLabel { nullptr };
        Checked<uint32_t, WTF::CrashOnOverflow> m_numberOfBreaksOrContinues;
        bool m_handlesReturns { false };
        Vector<FinallyJump> m_jumps;
        struct {
            RefPtr<RegisterID> typeRegister;
            RefPtr<RegisterID> valueRegister;
        } m_completionRecord;
    };

    struct ControlFlowScope {
        typedef uint8_t Type;
        enum {
            Label,
            Finally
        };
        ControlFlowScope(Type type, int lexicalScopeIndex, FinallyContext* finallyContext = nullptr)
            : type(type)
            , lexicalScopeIndex(lexicalScopeIndex)
            , finallyContext(finallyContext)
        { }

        bool isLabelScope() const { return type == Label; }
        bool isFinallyScope() const { return type == Finally; }

        Type type;
        int lexicalScopeIndex;
        FinallyContext* finallyContext;
    };

    class ForInContext : public RefCounted<ForInContext> {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(ForInContext);
    public:
        virtual ~ForInContext() = default;

        bool isValid() const { return m_isValid; }
        void invalidate() { m_isValid = false; }

        enum class Type : uint8_t {
            IndexedForIn,
            StructureForIn
        };

        Type type() const { return m_type; }
        bool isIndexedForInContext() const { return m_type == Type::IndexedForIn; }
        bool isStructureForInContext() const { return m_type == Type::StructureForIn; }

        IndexedForInContext& asIndexedForInContext()
        {
            ASSERT(isIndexedForInContext());
            return *reinterpret_cast<IndexedForInContext*>(this);
        }

        StructureForInContext& asStructureForInContext()
        {
            ASSERT(isStructureForInContext());
            return *reinterpret_cast<StructureForInContext*>(this);
        }

        RegisterID* local() const { return m_localRegister.get(); }

    protected:
        ForInContext(RegisterID* localRegister, Type type, unsigned bodyBytecodeStartOffset)
            : m_localRegister(localRegister)
            , m_type(type)
            , m_bodyBytecodeStartOffset(bodyBytecodeStartOffset)
        { }

        unsigned bodyBytecodeStartOffset() const { return m_bodyBytecodeStartOffset; }

        void finalize(BytecodeGenerator&, UnlinkedCodeBlockGenerator*, unsigned bodyBytecodeEndOffset);

    private:
        RefPtr<RegisterID> m_localRegister;
        bool m_isValid { true };
        Type m_type;
        unsigned m_bodyBytecodeStartOffset;
    };

    class StructureForInContext : public ForInContext {
        using Base = ForInContext;
    public:
        using GetInst = std::tuple<unsigned, int>;
        using InInst = GetInst;
        using HasOwnPropertyJumpInst = std::tuple<unsigned, unsigned>;

        StructureForInContext(RegisterID* localRegister, RegisterID* indexRegister, RegisterID* propertyRegister, RegisterID* enumeratorRegister, Optional<Variable> baseVariable, unsigned bodyBytecodeStartOffset)
            : ForInContext(localRegister, Type::StructureForIn, bodyBytecodeStartOffset)
            , m_indexRegister(indexRegister)
            , m_propertyRegister(propertyRegister)
            , m_enumeratorRegister(enumeratorRegister)
            , m_baseVariable(baseVariable)
        {
        }

        RegisterID* index() const { return m_indexRegister.get(); }
        RegisterID* property() const { return m_propertyRegister.get(); }
        RegisterID* enumerator() const { return m_enumeratorRegister.get(); }
        const Optional<Variable>& baseVariable() const { return m_baseVariable; }

        void addGetInst(unsigned instIndex, int propertyRegIndex)
        {
            m_getInsts.append(GetInst { instIndex, propertyRegIndex });
        }

        void addInInst(unsigned instIndex, int propertyRegIndex)
        {
            m_inInsts.append(InInst { instIndex, propertyRegIndex });
        }

        void addHasOwnPropertyJump(unsigned branchInstIndex, unsigned genericPathTarget)
        {
            m_hasOwnPropertyJumpInsts.append(HasOwnPropertyJumpInst { branchInstIndex, genericPathTarget });
        }

        void finalize(BytecodeGenerator&, UnlinkedCodeBlockGenerator*, unsigned bodyBytecodeEndOffset);

    private:
        RefPtr<RegisterID> m_indexRegister;
        RefPtr<RegisterID> m_propertyRegister;
        RefPtr<RegisterID> m_enumeratorRegister;
        Optional<Variable> m_baseVariable;
        Vector<GetInst> m_getInsts;
        Vector<InInst> m_inInsts;
        Vector<HasOwnPropertyJumpInst> m_hasOwnPropertyJumpInsts;
    };

    class IndexedForInContext : public ForInContext {
        using Base = ForInContext;
    public:
        IndexedForInContext(RegisterID* localRegister, RegisterID* indexRegister, unsigned bodyBytecodeStartOffset)
            : ForInContext(localRegister, Type::IndexedForIn, bodyBytecodeStartOffset)
            , m_indexRegister(indexRegister)
        {
        }

        RegisterID* index() const { return m_indexRegister.get(); }

        void finalize(BytecodeGenerator&, UnlinkedCodeBlockGenerator*, unsigned bodyBytecodeEndOffset);
        void addGetInst(unsigned instIndex, int propertyIndex) { m_getInsts.append({ instIndex, propertyIndex }); }

    private:
        RefPtr<RegisterID> m_indexRegister;
        Vector<std::pair<unsigned, int>> m_getInsts;
    };

    struct TryData {
        Ref<Label> target;
        HandlerType handlerType;
    };

    struct TryContext {
        Ref<Label> start;
        TryData* tryData;
    };

    struct TryRange {
        Ref<Label> start;
        Ref<Label> end;
        TryData* tryData;
    };


    struct JSGeneratorTraits {
        using OpcodeTraits = JSOpcodeTraits;
        using OpcodeID = ::JSC::OpcodeID;
        using OpNop = ::JSC::OpNop;
        using CodeBlock = std::unique_ptr<UnlinkedCodeBlockGenerator>;
        static constexpr OpcodeID opcodeForDisablingOptimizations = op_end;
    };

    class BytecodeGenerator : public BytecodeGeneratorBase<JSGeneratorTraits> {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(BytecodeGenerator);

        friend class FinallyContext;
        friend class IndexedForInContext;
        friend class StructureForInContext;
        friend class StrictModeScope;

        template <typename OldOpType, typename TupleType>
        friend void rewriteOp(BytecodeGenerator&, TupleType&);

    public:
        typedef DeclarationStacks::FunctionStack FunctionStack;

        BytecodeGenerator(VM&, ProgramNode*, UnlinkedProgramCodeBlock*, OptionSet<CodeGenerationMode>, const VariableEnvironment*, ECMAMode);
        BytecodeGenerator(VM&, FunctionNode*, UnlinkedFunctionCodeBlock*, OptionSet<CodeGenerationMode>, const VariableEnvironment*, ECMAMode);
        BytecodeGenerator(VM&, EvalNode*, UnlinkedEvalCodeBlock*, OptionSet<CodeGenerationMode>, const VariableEnvironment*, ECMAMode);
        BytecodeGenerator(VM&, ModuleProgramNode*, UnlinkedModuleProgramCodeBlock*, OptionSet<CodeGenerationMode>, const VariableEnvironment*, ECMAMode);

        ~BytecodeGenerator();
        
        VM& vm() const { return m_vm; }
        ParserArena& parserArena() const { return m_scopeNode->parserArena(); }
        const CommonIdentifiers& propertyNames() const { return *m_vm.propertyNames; }

        bool isConstructor() const { return m_codeBlock->isConstructor(); }
        DerivedContextType derivedContextType() const { return m_derivedContextType; }
        bool usesArrowFunction() const { return m_scopeNode->usesArrowFunction(); }
        bool needsToUpdateArrowFunctionContext() const { return m_needsToUpdateArrowFunctionContext; }
        bool usesEval() const { return m_scopeNode->usesEval(); }
        bool usesThis() const { return m_scopeNode->usesThis(); }
        ConstructorKind constructorKind() const { return m_codeBlock->constructorKind(); }
        SuperBinding superBinding() const { return m_codeBlock->superBinding(); }
        JSParserScriptMode scriptMode() const { return m_codeBlock->scriptMode(); }
        NeedsClassFieldInitializer needsClassFieldInitializer() const { return m_codeBlock->needsClassFieldInitializer(); }

        template<typename Node, typename UnlinkedCodeBlock>
        static ParserError generate(VM& vm, Node* node, const SourceCode& sourceCode, UnlinkedCodeBlock* unlinkedCodeBlock, OptionSet<CodeGenerationMode> codeGenerationMode, const VariableEnvironment* environment, ECMAMode ecmaMode)
        {
            MonotonicTime before;
            if (UNLIKELY(Options::reportBytecodeCompileTimes()))
                before = MonotonicTime::now();

            DeferGC deferGC(vm.heap);
            auto bytecodeGenerator = makeUnique<BytecodeGenerator>(vm, node, unlinkedCodeBlock, codeGenerationMode, environment, ecmaMode);
            auto result = bytecodeGenerator->generate();

            if (UNLIKELY(Options::reportBytecodeCompileTimes())) {
                MonotonicTime after = MonotonicTime::now();
                dataLogLn(result.isValid() ? "Failed to compile #" : "Compiled #", CodeBlockHash(sourceCode, unlinkedCodeBlock->isConstructor() ? CodeForConstruct : CodeForCall), " into bytecode ", bytecodeGenerator->instructions().size(), " instructions in ", (after - before).milliseconds(), " ms.");
            }
            return result;
        }

        bool isArgumentNumber(const Identifier&, int);

        Variable variable(const Identifier&, ThisResolutionType = ThisResolutionType::Local);
        
        enum ExistingVariableMode { VerifyExisting, IgnoreExisting };
        void createVariable(const Identifier&, VarKind, SymbolTable*, ExistingVariableMode = VerifyExisting); // Creates the variable, or asserts that the already-created variable is sufficiently compatible.
        
        // Returns the register storing "this"
        RegisterID* thisRegister() { return &m_thisRegister; }
        RegisterID* argumentsRegister() { return m_argumentsRegister; }
        RegisterID* newTarget()
        {
            ASSERT(m_newTargetRegister);
            return m_newTargetRegister;
        }

        RegisterID* scopeRegister() { return m_scopeRegister; }

        RegisterID* generatorRegister() { return m_generatorRegister; }

        RegisterID* promiseRegister() { return m_promiseRegister; }

        // The same as newTemporary(), but this function returns "suggestion" if
        // "suggestion" is a temporary. This function is helpful in situations
        // where you've put "suggestion" in a RefPtr, but you'd like to allow
        // the next instruction to overwrite it anyway.
        RegisterID* newTemporaryOr(RegisterID* suggestion) { return suggestion->isTemporary() ? suggestion : newTemporary(); }

        // Functions for handling of dst register

        RegisterID* ignoredResult() { return &m_ignoredResultRegister; }

        // This will be allocated in the temporary region of registers, but it will
        // not be marked as a temporary. This will ensure that finalDestination() does
        // not overwrite a block scope variable that it mistakes as a temporary. These
        // registers can be (and are) reclaimed when the lexical scope they belong to
        // is no longer on the symbol table stack.
        RegisterID* newBlockScopeVariable();

        // Returns a place to write intermediate values of an operation
        // which reuses dst if it is safe to do so.
        RegisterID* tempDestination(RegisterID* dst)
        {
            return (dst && dst != ignoredResult() && dst->isTemporary()) ? dst : newTemporary();
        }

        // Returns the place to write the final output of an operation.
        RegisterID* finalDestination(RegisterID* originalDst, RegisterID* tempDst = nullptr)
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
            if (dst && dst != ignoredResult())
                return dst->isTemporary() ? dst : newTemporary();
            return nullptr;
        }

        // Moves src to dst if dst is not null and is different from src, otherwise just returns src.
        RegisterID* move(RegisterID* dst, RegisterID* src)
        {
            return dst == ignoredResult() ? nullptr : (dst && dst != src) ? emitMove(dst, src) : src;
        }

        Ref<LabelScope> newLabelScope(LabelScope::Type, const Identifier* = nullptr);

        void emitNode(RegisterID* dst, StatementNode* n)
        {
            SetForScope<bool> tailPositionPoisoner(m_inTailPosition, false);
            return emitNodeInTailPosition(dst, n);
        }

        void emitNodeInTailPosition(RegisterID* dst, StatementNode* n)
        {
            // Node::emitCode assumes that dst, if provided, is either a local or a referenced temporary.
            ASSERT(!dst || dst == ignoredResult() || !dst->isTemporary() || dst->refCount());
            if (UNLIKELY(!m_vm.isSafeToRecurse())) {
                emitThrowExpressionTooDeepException();
                return;
            }
            if (UNLIKELY(n->needsDebugHook()))
                emitDebugHook(n);
            n->emitBytecode(*this, dst);
        }

        ALWAYS_INLINE unsigned addMetadataFor(OpcodeID opcodeID)
        {
            return m_codeBlock->metadata().addEntry(opcodeID);
        }

        void emitNode(StatementNode* n)
        {
            emitNode(nullptr, n);
        }

        void emitNodeInTailPosition(StatementNode* n)
        {
            emitNodeInTailPosition(nullptr, n);
        }

        RegisterID* emitNode(RegisterID* dst, ExpressionNode* n)
        {
            SetForScope<bool> tailPositionPoisoner(m_inTailPosition, false);
            return emitNodeInTailPosition(dst, n);
        }

        RegisterID* emitNodeInTailPosition(RegisterID* dst, ExpressionNode* n)
        {
            // Node::emitCode assumes that dst, if provided, is either a local or a referenced temporary.
            ASSERT(!dst || dst == ignoredResult() || !dst->isTemporary() || dst->refCount());
            if (UNLIKELY(!m_vm.isSafeToRecurse()))
                return emitThrowExpressionTooDeepException();
            if (UNLIKELY(n->needsDebugHook()))
                emitDebugHook(n);
            return n->emitBytecode(*this, dst);
        }

        RegisterID* emitNode(ExpressionNode* n)
        {
            return emitNode(nullptr, n);
        }

        RegisterID* emitNodeInTailPosition(ExpressionNode* n)
        {
            return emitNodeInTailPosition(nullptr, n);
        }

        RegisterID* emitDefineClassElements(PropertyListNode* n, RegisterID* constructor, RegisterID* prototype, Vector<JSTextPosition>& instanceFieldLocations)
        {
            ASSERT(constructor->refCount() && prototype->refCount());
            if (UNLIKELY(!m_vm.isSafeToRecurse()))
                return emitThrowExpressionTooDeepException();
            if (UNLIKELY(n->needsDebugHook()))
                emitDebugHook(n);
            return n->emitBytecode(*this, constructor, prototype, &instanceFieldLocations);
        }

        RegisterID* emitNodeForProperty(RegisterID* dst, ExpressionNode* node)
        {
            if (node->isString()) {
                if (Optional<uint32_t> index = parseIndex(static_cast<StringNode*>(node)->value()))
                    return emitLoad(dst, jsNumber(index.value()));
            }
            return emitNode(dst, node);
        }

        RegisterID* emitNodeForProperty(ExpressionNode* n)
        {
            return emitNodeForProperty(nullptr, n);
        }

        void emitNodeInConditionContext(ExpressionNode* n, Label& trueTarget, Label& falseTarget, FallThroughMode fallThroughMode)
        {
            if (UNLIKELY(!m_vm.isSafeToRecurse())) {
                emitThrowExpressionTooDeepException();
                return;
            }
            n->emitBytecodeInConditionContext(*this, trueTarget, falseTarget, fallThroughMode);
        }

        void emitExpressionInfo(const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
        {            
            ASSERT(divot.offset >= divotStart.offset);
            ASSERT(divotEnd.offset >= divot.offset);

            if (m_isBuiltinFunction)
                return;

            int sourceOffset = m_scopeNode->source().startOffset();
            unsigned firstLine = m_scopeNode->source().firstLine().oneBasedInt();

            int divotOffset = divot.offset - sourceOffset;
            int startOffset = divot.offset - divotStart.offset;
            int endOffset = divotEnd.offset - divot.offset;

            unsigned line = divot.line;
            ASSERT(line >= firstLine);
            line -= firstLine;

            int lineStart = divot.lineStartOffset;
            if (lineStart > sourceOffset)
                lineStart -= sourceOffset;
            else
                lineStart = 0;

            if (divotOffset < lineStart)
                return;

            unsigned column = divotOffset - lineStart;

            unsigned instructionOffset = instructions().size();
            m_codeBlock->addExpressionInfo(instructionOffset, divotOffset, startOffset, endOffset, line, column);
        }


        ALWAYS_INLINE bool leftHandSideNeedsCopy(bool rightHasAssignments, bool rightIsPure)
        {
            return (m_codeType != FunctionCode || rightHasAssignments) && !rightIsPure;
        }

        ALWAYS_INLINE RefPtr<RegisterID> emitNodeForLeftHandSide(ExpressionNode* n, bool rightHasAssignments, bool rightIsPure)
        {
            if (leftHandSideNeedsCopy(rightHasAssignments, rightIsPure)) {
                RefPtr<RegisterID> dst = newTemporary();
                emitNode(dst.get(), n);
                return dst;
            }

            return emitNode(n);
        }

        ALWAYS_INLINE RefPtr<RegisterID> emitNodeForLeftHandSideForProperty(ExpressionNode* n, bool rightHasAssignments, bool rightIsPure)
        {
            if (leftHandSideNeedsCopy(rightHasAssignments, rightIsPure)) {
                RefPtr<RegisterID> dst = newTemporary();
                emitNodeForProperty(dst.get(), n);
                return dst;
            }

            return emitNodeForProperty(n);
        }

        void hoistSloppyModeFunctionIfNecessary(const Identifier& functionName);

        StructureForInContext* findStructureForInContext(RegisterID* property);

    private:
        void emitTypeProfilerExpressionInfo(const JSTextPosition& startDivot, const JSTextPosition& endDivot);
    public:

        // This doesn't emit expression info. If using this, make sure you shouldn't be emitting text offset.
        void emitProfileType(RegisterID* registerToProfile, ProfileTypeBytecodeFlag); 
        // These variables are associated with variables in a program. They could be Locals, LocalClosureVar, or ClosureVar.
        void emitProfileType(RegisterID* registerToProfile, const Variable&, const JSTextPosition& startDivot, const JSTextPosition& endDivot);

        void emitProfileType(RegisterID* registerToProfile, ProfileTypeBytecodeFlag, const JSTextPosition& startDivot, const JSTextPosition& endDivot);
        // These are not associated with variables and don't have a global id.
        void emitProfileType(RegisterID* registerToProfile, const JSTextPosition& startDivot, const JSTextPosition& endDivot);

        void emitProfileControlFlow(int);
        
        RegisterID* emitLoadArrowFunctionLexicalEnvironment(const Identifier&);
        RegisterID* ensureThis();
        void emitLoadThisFromArrowFunctionLexicalEnvironment();
        RegisterID* emitLoadNewTargetFromArrowFunctionLexicalEnvironment();

        unsigned addConstantIndex();
        RegisterID* emitLoad(RegisterID* dst, bool);
        RegisterID* emitLoad(RegisterID* dst, const Identifier&);
        RegisterID* emitLoad(RegisterID* dst, JSValue, SourceCodeRepresentation = SourceCodeRepresentation::Other);
        RegisterID* emitLoad(RegisterID* dst, IdentifierSet& excludedList);

        template<typename UnaryOp, typename = std::enable_if_t<UnaryOp::opcodeID != op_negate>>
        RegisterID* emitUnaryOp(RegisterID* dst, RegisterID* src)
        {
            UnaryOp::emit(this, dst, src);
            return dst;
        }

        RegisterID* emitUnaryOp(OpcodeID, RegisterID* dst, RegisterID* src, ResultType);

        template<typename BinaryOp>
        std::enable_if_t<
            BinaryOp::opcodeID != op_add
            && BinaryOp::opcodeID != op_mul
            && BinaryOp::opcodeID != op_sub
            && BinaryOp::opcodeID != op_div,
            RegisterID*>
        emitBinaryOp(RegisterID* dst, RegisterID* src1, RegisterID* src2, OperandTypes)
        {
            BinaryOp::emit(this, dst, src1, src2);
            return dst;
        }

        template<typename BinaryOp>
        std::enable_if_t<
            BinaryOp::opcodeID == op_add
            || BinaryOp::opcodeID == op_mul
            || BinaryOp::opcodeID == op_sub
            || BinaryOp::opcodeID == op_div,
            RegisterID*>
        emitBinaryOp(RegisterID* dst, RegisterID* src1, RegisterID* src2, OperandTypes types)
        {
            BinaryOp::emit(this, dst, src1, src2, types);
            return dst;
        }

        RegisterID* emitBinaryOp(OpcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2, OperandTypes);

        template<typename EqOp>
        RegisterID* emitEqualityOp(RegisterID* dst, RegisterID* src1, RegisterID* src2)
        {
            if (!emitEqualityOpImpl(dst, src1, src2))
                EqOp::emit(this, dst, src1, src2);
            return dst;
        }

        bool emitEqualityOpImpl(RegisterID* dst, RegisterID* src1, RegisterID* src2);

        RegisterID* emitCreateThis(RegisterID* dst);
        RegisterID* emitCreatePromise(RegisterID* dst, RegisterID* newTarget, bool isInternalPromise);
        RegisterID* emitCreateGenerator(RegisterID* dst, RegisterID* newTarget);
        RegisterID* emitCreateAsyncGenerator(RegisterID* dst, RegisterID* newTarget);
        RegisterID* emitCreateArgumentsButterfly(RegisterID* dst);
        RegisterID* emitInstanceFieldInitializationIfNeeded(RegisterID* dst, RegisterID* constructor, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);
        void emitTDZCheck(RegisterID* target);
        bool needsTDZCheck(const Variable&);
        void emitTDZCheckIfNecessary(const Variable&, RegisterID* target, RegisterID* scope);
        void liftTDZCheckIfPossible(const Variable&);
        RegisterID* emitNewObject(RegisterID* dst);
        RegisterID* emitNewPromise(RegisterID* dst, bool isInternalPromise);
        RegisterID* emitNewGenerator(RegisterID* dst);
        RegisterID* emitNewArray(RegisterID* dst, ElementNode*, unsigned length, IndexingType recommendedIndexingType); // stops at first elision
        RegisterID* emitNewArrayBuffer(RegisterID* dst, JSImmutableButterfly*, IndexingType recommendedIndexingType);
        // FIXME: new_array_with_spread should use an array allocation profile and take a recommendedIndexingType
        RegisterID* emitNewArrayWithSpread(RegisterID* dst, ElementNode*);
        RegisterID* emitNewArrayWithSize(RegisterID* dst, RegisterID* length);

        RegisterID* emitNewFunction(RegisterID* dst, FunctionMetadataNode*);
        RegisterID* emitNewFunctionExpression(RegisterID* dst, FuncExprNode*);
        RegisterID* emitNewDefaultConstructor(RegisterID* dst, ConstructorKind, const Identifier& name, const Identifier& ecmaName, const SourceCode& classSource, NeedsClassFieldInitializer);
        RegisterID* emitNewInstanceFieldInitializerFunction(RegisterID* dst, Vector<JSTextPosition>&& instanceFieldLocations, bool isDerived);
        RegisterID* emitNewArrowFunctionExpression(RegisterID*, ArrowFuncExprNode*);
        RegisterID* emitNewMethodDefinition(RegisterID* dst, MethodDefinitionNode*);
        RegisterID* emitNewRegExp(RegisterID* dst, RegExp*);

        bool shouldSetFunctionName(ExpressionNode*);
        void emitSetFunctionName(RegisterID* value, RegisterID* name);
        void emitSetFunctionName(RegisterID* value, const Identifier&);

        RegisterID* moveLinkTimeConstant(RegisterID* dst, LinkTimeConstant);
        RegisterID* moveEmptyValue(RegisterID* dst);

        RegisterID* emitToNumber(RegisterID* dst, RegisterID* src);
        RegisterID* emitToNumeric(RegisterID* dst, RegisterID* src);
        RegisterID* emitToString(RegisterID* dst, RegisterID* src);
        RegisterID* emitToObject(RegisterID* dst, RegisterID* src, const Identifier& message);
        RegisterID* emitInc(RegisterID* srcDst);
        RegisterID* emitDec(RegisterID* srcDst);

        RegisterID* emitOverridesHasInstance(RegisterID* dst, RegisterID* constructor, RegisterID* hasInstanceValue);
        RegisterID* emitInstanceOf(RegisterID* dst, RegisterID* value, RegisterID* basePrototype);
        RegisterID* emitInstanceOfCustom(RegisterID* dst, RegisterID* value, RegisterID* constructor, RegisterID* hasInstanceValue);
        RegisterID* emitTypeOf(RegisterID* dst, RegisterID* src);
        RegisterID* emitInByVal(RegisterID* dst, RegisterID* property, RegisterID* base);
        RegisterID* emitInById(RegisterID* dst, RegisterID* base, const Identifier& property);

        RegisterID* emitTryGetById(RegisterID* dst, RegisterID* base, const Identifier& property);
        RegisterID* emitGetById(RegisterID* dst, RegisterID* base, const Identifier& property);
        RegisterID* emitGetById(RegisterID* dst, RegisterID* base, RegisterID* thisVal, const Identifier& property);
        RegisterID* emitDirectGetById(RegisterID* dst, RegisterID* base, const Identifier& property);
        RegisterID* emitPutById(RegisterID* base, const Identifier& property, RegisterID* value);
        RegisterID* emitPutById(RegisterID* base, RegisterID* thisValue, const Identifier& property, RegisterID* value);
        RegisterID* emitDirectPutById(RegisterID* base, const Identifier& property, RegisterID* value, PropertyNode::PutType);
        RegisterID* emitDeleteById(RegisterID* dst, RegisterID* base, const Identifier&);
        RegisterID* emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* thisValue, RegisterID* property);
        RegisterID* emitPutByVal(RegisterID* base, RegisterID* property, RegisterID* value);
        RegisterID* emitPutByVal(RegisterID* base, RegisterID* thisValue, RegisterID* property, RegisterID* value);
        RegisterID* emitDirectGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitDirectPutByVal(RegisterID* base, RegisterID* property, RegisterID* value);
        RegisterID* emitDeleteByVal(RegisterID* dst, RegisterID* base, RegisterID* property);

        RegisterID* emitGetInternalField(RegisterID* dst, RegisterID* base, unsigned index);
        RegisterID* emitPutInternalField(RegisterID* base, unsigned index, RegisterID* value);
        RegisterID* emitDefinePrivateField(RegisterID* base, RegisterID* property, RegisterID* value);
        RegisterID* emitPrivateFieldPut(RegisterID* base, RegisterID* property, RegisterID* value);

        void emitSuperSamplerBegin();
        void emitSuperSamplerEnd();

        RegisterID* emitIdWithProfile(RegisterID* src, SpeculatedType profile);
        void emitUnreachable();

        void emitPutGetterById(RegisterID* base, const Identifier& property, unsigned propertyDescriptorOptions, RegisterID* getter);
        void emitPutSetterById(RegisterID* base, const Identifier& property, unsigned propertyDescriptorOptions, RegisterID* setter);
        void emitPutGetterSetter(RegisterID* base, const Identifier& property, unsigned attributes, RegisterID* getter, RegisterID* setter);
        void emitPutGetterByVal(RegisterID* base, RegisterID* property, unsigned propertyDescriptorOptions, RegisterID* getter);
        void emitPutSetterByVal(RegisterID* base, RegisterID* property, unsigned propertyDescriptorOptions, RegisterID* setter);

        RegisterID* emitGetArgument(RegisterID* dst, int32_t index);

        // Initialize object with generator fields (@generatorThis, @generatorNext, @generatorState, @generatorFrame)
        void emitPutGeneratorFields(RegisterID* nextFunction);
        
        void emitPutAsyncGeneratorFields(RegisterID* nextFunction);

        ExpectedFunction expectedFunctionForIdentifier(const Identifier&);
        RegisterID* emitCall(RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);
        RegisterID* emitCallInTailPosition(RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);
        RegisterID* emitCallEval(RegisterID* dst, RegisterID* func, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);
        RegisterID* emitCallVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);
        RegisterID* emitCallVarargsInTailPosition(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);
        RegisterID* emitCallForwardArgumentsInTailPosition(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);

        enum PropertyDescriptorOption {
            PropertyConfigurable = 1,
            PropertyWritable     = 1 << 1,
            PropertyEnumerable   = 1 << 2,
        };
        void emitCallDefineProperty(RegisterID* newObj, RegisterID* propertyNameRegister,
            RegisterID* valueRegister, RegisterID* getterRegister, RegisterID* setterRegister, unsigned options, const JSTextPosition&);

        void emitGenericEnumeration(ThrowableExpressionData* enumerationNode, ExpressionNode* subjectNode, const ScopedLambda<void(BytecodeGenerator&, RegisterID*)>& callBack, ForOfNode* = nullptr, RegisterID* forLoopSymbolTable = nullptr);
        void emitEnumeration(ThrowableExpressionData* enumerationNode, ExpressionNode* subjectNode, const ScopedLambda<void(BytecodeGenerator&, RegisterID*)>& callBack, ForOfNode* = nullptr, RegisterID* forLoopSymbolTable = nullptr);

        RegisterID* emitGetTemplateObject(RegisterID* dst, TaggedTemplateNode*);
        RegisterID* emitGetGlobalPrivate(RegisterID* dst, const Identifier& property);

        enum class ReturnFrom { Normal, Finally };
        RegisterID* emitReturn(RegisterID* src, ReturnFrom = ReturnFrom::Normal);
        RegisterID* emitEnd(RegisterID* src);

        RegisterID* emitConstruct(RegisterID* dst, RegisterID* func, RegisterID* lazyThis, ExpectedFunction, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);
        RegisterID* emitStrcat(RegisterID* dst, RegisterID* src, int count);
        void emitToPrimitive(RegisterID* dst, RegisterID* src);
        RegisterID* emitToPropertyKey(RegisterID* dst, RegisterID* src);

        ResolveType resolveType();
        RegisterID* emitResolveConstantLocal(RegisterID* dst, const Variable&);
        RegisterID* emitResolveScope(RegisterID* dst, const Variable&);
        RegisterID* emitGetFromScope(RegisterID* dst, RegisterID* scope, const Variable&, ResolveMode);
        RegisterID* emitPutToScope(RegisterID* scope, const Variable&, RegisterID* value, ResolveMode, InitializationMode);

        RegisterID* emitResolveScopeForHoistingFuncDeclInEval(RegisterID* dst, const Identifier&);

        RegisterID* initializeVariable(const Variable&, RegisterID* value);

        void emitLoopHint();
        void emitJump(Label& target);
        void emitJumpIfTrue(RegisterID* cond, Label& target);
        void emitJumpIfFalse(RegisterID* cond, Label& target);
        void emitJumpIfNotFunctionCall(RegisterID* cond, Label& target);
        void emitJumpIfNotFunctionApply(RegisterID* cond, Label& target);
        unsigned emitWideJumpIfNotFunctionHasOwnProperty(RegisterID* cond, Label& target);
        void recordHasOwnStructurePropertyInForInLoop(StructureForInContext&, unsigned branchOffset, Label& genericPath);

        template<typename BinOp, typename JmpOp>
        bool fuseCompareAndJump(RegisterID* cond, Label& target, bool swapOperands = false);

        template<typename UnaryOp, typename JmpOp>
        bool fuseTestAndJmp(RegisterID* cond, Label& target);

        void emitEnter();
        void emitCheckTraps();

        RegisterID* emitHasIndexedProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName);
        RegisterID* emitHasStructureProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName, RegisterID* enumerator);
        RegisterID* emitHasOwnStructureProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName, RegisterID* enumerator);
        RegisterID* emitHasGenericProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName);
        RegisterID* emitGetPropertyEnumerator(RegisterID* dst, RegisterID* base);
        RegisterID* emitGetEnumerableLength(RegisterID* dst, RegisterID* base);
        RegisterID* emitGetStructurePropertyEnumerator(RegisterID* dst, RegisterID* base, RegisterID* length);
        RegisterID* emitGetGenericPropertyEnumerator(RegisterID* dst, RegisterID* base, RegisterID* length, RegisterID* structureEnumerator);
        RegisterID* emitEnumeratorStructurePropertyName(RegisterID* dst, RegisterID* enumerator, RegisterID* index);
        RegisterID* emitEnumeratorGenericPropertyName(RegisterID* dst, RegisterID* enumerator, RegisterID* index);
        RegisterID* emitToIndexString(RegisterID* dst, RegisterID* index);

        RegisterID* emitIsCellWithType(RegisterID* dst, RegisterID* src, JSType);
        RegisterID* emitIsGenerator(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSGeneratorType); }
        RegisterID* emitIsAsyncGenerator(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSAsyncGeneratorType); }
        RegisterID* emitIsJSArray(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, ArrayType); }
        RegisterID* emitIsPromise(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSPromiseType); }
        RegisterID* emitIsProxyObject(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, ProxyObjectType); }
        RegisterID* emitIsRegExpObject(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, RegExpObjectType); }
        RegisterID* emitIsMap(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSMapType); }
        RegisterID* emitIsSet(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSSetType); }
        RegisterID* emitIsStringIterator(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSStringIteratorType); }
        RegisterID* emitIsArrayIterator(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSArrayIteratorType); }
        RegisterID* emitIsMapIterator(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSMapIteratorType); }
        RegisterID* emitIsSetIterator(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, JSSetIteratorType); }
        RegisterID* emitIsObject(RegisterID* dst, RegisterID* src);
        RegisterID* emitIsConstructor(RegisterID* dst, RegisterID* src);
        RegisterID* emitIsNumber(RegisterID* dst, RegisterID* src);
        RegisterID* emitIsUndefined(RegisterID* dst, RegisterID* src);
        RegisterID* emitIsUndefinedOrNull(RegisterID* dst, RegisterID* src);
        RegisterID* emitIsEmpty(RegisterID* dst, RegisterID* src);
        RegisterID* emitIsDerivedArray(RegisterID* dst, RegisterID* src) { return emitIsCellWithType(dst, src, DerivedArrayType); }
        void emitRequireObjectCoercible(RegisterID* value, const String& error);

        void emitIteratorOpen(RegisterID* iterator, RegisterID* nextOrIndex, RegisterID* symbolIterator, CallArguments& iterable, const ThrowableExpressionData*);
        void emitIteratorNext(RegisterID* done, RegisterID* value, RegisterID* iterable, RegisterID* nextOrIndex, CallArguments& iterator, const ThrowableExpressionData*);

        RegisterID* emitGetGenericIterator(RegisterID*, ThrowableExpressionData*);
        RegisterID* emitGetAsyncIterator(RegisterID*, ThrowableExpressionData*);

        RegisterID* emitIteratorGenericNext(RegisterID* dst, RegisterID* nextMethod, RegisterID* iterator, const ThrowableExpressionData* node, JSC::EmitAwait = JSC::EmitAwait::No);
        RegisterID* emitIteratorGenericNextWithValue(RegisterID* dst, RegisterID* nextMethod, RegisterID* iterator, RegisterID* value, const ThrowableExpressionData* node);
        void emitIteratorGenericClose(RegisterID* iterator, const ThrowableExpressionData* node, EmitAwait = EmitAwait::No);

        RegisterID* emitRestParameter(RegisterID* result, unsigned numParametersToSkip);

        bool emitReadOnlyExceptionIfNeeded(const Variable&);

        // Start a try block. 'start' must have been emitted.
        TryData* pushTry(Label& start, Label& handlerLabel, HandlerType);
        // End a try block. 'end' must have been emitted.
        void popTry(TryData*, Label& end);

        void emitOutOfLineCatchHandler(RegisterID* thrownValueRegister, RegisterID* completionTypeRegister, TryData*);
        void emitOutOfLineFinallyHandler(RegisterID* exceptionRegister, RegisterID* completionTypeRegister, TryData*);

    private:
        static constexpr int CurrentLexicalScopeIndex = -2;
        static constexpr int OutermostLexicalScopeIndex = -1;

        int currentLexicalScopeIndex() const
        {
            int size = static_cast<int>(m_lexicalScopeStack.size());
            ASSERT(static_cast<size_t>(size) == m_lexicalScopeStack.size());
            ASSERT(size >= 0);
            if (!size)
                return OutermostLexicalScopeIndex;
            return size - 1;
        }

        void emitOutOfLineExceptionHandler(RegisterID* exceptionRegister, RegisterID* thrownValueRegister, RegisterID* completionTypeRegister, TryData*);

    public:
        void restoreScopeRegister();
        void restoreScopeRegister(int lexicalScopeIndex);

        int labelScopeDepthToLexicalScopeIndex(int labelScopeDepth);

        void emitThrow(RegisterID*);
        RegisterID* emitArgumentCount(RegisterID*);

        void emitThrowStaticError(ErrorTypeWithExtension, RegisterID*);
        void emitThrowStaticError(ErrorTypeWithExtension, const Identifier& message);
        void emitThrowReferenceError(const String& message);
        void emitThrowTypeError(const String& message);
        void emitThrowTypeError(const Identifier& message);
        void emitThrowRangeError(const Identifier& message);
        void emitThrowOutOfMemoryError();

        void emitPushCatchScope(VariableEnvironment&);
        void emitPopCatchScope(VariableEnvironment&);

        void emitAwait(RegisterID*);
        void emitGetScope();
        RegisterID* emitPushWithScope(RegisterID* objectScope);
        void emitPopWithScope();
        void emitPutThisToArrowFunctionContextScope();
        void emitPutNewTargetToArrowFunctionContextScope();
        void emitPutDerivedConstructorToArrowFunctionContextScope();
        RegisterID* emitLoadDerivedConstructorFromArrowFunctionLexicalEnvironment();
        RegisterID* emitLoadDerivedConstructor();

        void emitDebugHook(DebugHookType, const JSTextPosition&);
        void emitDebugHook(DebugHookType, unsigned line, unsigned charOffset, unsigned lineStart);
        void emitDebugHook(StatementNode*);
        void emitDebugHook(ExpressionNode*);
        void emitWillLeaveCallFrameDebugHook();

        void emitLoad(RegisterID* completionTypeRegister, CompletionType type)
        {
            emitLoad(completionTypeRegister, JSValue(static_cast<int>(type)));
        }

        template<typename CompareOp>
        void emitJumpIf(RegisterID* completionTypeRegister, CompletionType, Label& jumpTarget);

        bool emitJumpViaFinallyIfNeeded(int targetLabelScopeDepth, Label& jumpTarget);
        bool emitReturnViaFinallyIfNeeded(RegisterID* returnRegister);
        void emitFinallyCompletion(FinallyContext&, Label& normalCompletionLabel);

    public:
        void pushFinallyControlFlowScope(FinallyContext&);
        void popFinallyControlFlowScope();

        void pushOptionalChainTarget();
        void popOptionalChainTarget();
        void popOptionalChainTarget(RegisterID* dst, bool isDelete);
        void emitOptionalCheck(RegisterID* src);

        void pushIndexedForInScope(RegisterID* local, RegisterID* index);
        void popIndexedForInScope(RegisterID* local);
        void pushStructureForInScope(RegisterID* local, RegisterID* index, RegisterID* property, RegisterID* enumerator, Optional<Variable> base);
        void popStructureForInScope(RegisterID* local);

        LabelScope* breakTarget(const Identifier&);
        LabelScope* continueTarget(const Identifier&);

        void beginSwitch(RegisterID*, SwitchInfo::SwitchType);
        void endSwitch(uint32_t clauseCount, const Vector<Ref<Label>, 8>&, ExpressionNode**, Label& defaultLabel, int32_t min, int32_t range);

        void emitYieldPoint(RegisterID*, JSAsyncGenerator::AsyncGeneratorSuspendReason);

        void emitGeneratorStateLabel();
        void emitGeneratorStateChange(int32_t state);
        RegisterID* emitYield(RegisterID* argument, JSAsyncGenerator::AsyncGeneratorSuspendReason = JSAsyncGenerator::AsyncGeneratorSuspendReason::Yield);
        RegisterID* emitDelegateYield(RegisterID* argument, ThrowableExpressionData*);
        RegisterID* generatorStateRegister() { return &m_parameters[static_cast<int32_t>(JSGenerator::GeneratorArgument::State)]; }
        RegisterID* generatorValueRegister() { return &m_parameters[static_cast<int32_t>(JSGenerator::GeneratorArgument::Value)]; }
        RegisterID* generatorResumeModeRegister() { return &m_parameters[static_cast<int32_t>(JSGenerator::GeneratorArgument::ResumeMode)]; }
        RegisterID* generatorFrameRegister() { return &m_parameters[static_cast<int32_t>(JSGenerator::GeneratorArgument::Frame)]; }

        CodeType codeType() const { return m_codeType; }

        bool shouldBeConcernedWithCompletionValue() const { return m_codeType != FunctionCode; }

        bool shouldEmitDebugHooks() const { return m_codeGenerationMode.contains(CodeGenerationMode::Debugger) && !m_isBuiltinFunction; }
        bool shouldEmitTypeProfilerHooks() const { return m_codeGenerationMode.contains(CodeGenerationMode::TypeProfiler); }
        bool shouldEmitControlFlowProfilerHooks() const { return m_codeGenerationMode.contains(CodeGenerationMode::ControlFlowProfiler); }
        
        ECMAMode ecmaMode() const { return m_ecmaMode; }
        void setUsesCheckpoints() { m_codeBlock->setHasCheckpoints(); }

        SourceParseMode parseMode() const { return m_codeBlock->parseMode(); }
        
        bool isBuiltinFunction() const { return m_isBuiltinFunction; }

        OpcodeID lastOpcodeID() const { return m_lastOpcodeID; }
        
        bool isDerivedConstructorContext() { return m_derivedContextType == DerivedContextType::DerivedConstructorContext; }
        bool isDerivedClassContext() { return m_derivedContextType == DerivedContextType::DerivedMethodContext; }
        bool isArrowFunction() { return m_codeBlock->isArrowFunction(); }

        enum class TDZCheckOptimization { Optimize, DoNotOptimize };
        enum class NestedScopeType { IsNested, IsNotNested };
    private:
        enum class TDZRequirement { UnderTDZ, NotUnderTDZ };
        enum class ScopeType { CatchScope, LetConstScope, FunctionNameScope };
        enum class ScopeRegisterType { Var, Block };
        void pushLexicalScopeInternal(VariableEnvironment&, TDZCheckOptimization, NestedScopeType, RegisterID** constantSymbolTableResult, TDZRequirement, ScopeType, ScopeRegisterType);
        void initializeBlockScopedFunctions(VariableEnvironment&, FunctionStack&, RegisterID* constantSymbolTable);
        void popLexicalScopeInternal(VariableEnvironment&);
        template<typename LookUpVarKindFunctor>
        bool instantiateLexicalVariables(const VariableEnvironment&, SymbolTable*, ScopeRegisterType, LookUpVarKindFunctor);
        void emitPrefillStackTDZVariables(const VariableEnvironment&, SymbolTable*);
        RegisterID* emitGetParentScope(RegisterID* dst, RegisterID* scope);
        void emitPushFunctionNameScope(const Identifier& property, RegisterID* value, bool isCaptured);
        void emitNewFunctionExpressionCommon(RegisterID*, FunctionMetadataNode*);
        
        bool isNewTargetUsedInInnerArrowFunction();
        bool isArgumentsUsedInInnerArrowFunction();

        void emitToThis();

        RegisterID* emitMove(RegisterID* dst, RegisterID* src);

    public:
        void disablePeepholeOptimization() { m_lastOpcodeID = op_end; }
    private:
        bool canDoPeepholeOptimization() const { return m_lastOpcodeID != op_end; }

    public:
        bool isSuperUsedInInnerArrowFunction();
        bool isSuperCallUsedInInnerArrowFunction();
        bool isThisUsedInInnerArrowFunction();
        void pushLexicalScope(VariableEnvironmentNode*, TDZCheckOptimization, NestedScopeType = NestedScopeType::IsNotNested, RegisterID** constantSymbolTableResult = nullptr, bool shouldInitializeBlockScopedFunctions = true);
        void popLexicalScope(VariableEnvironmentNode*);
        void prepareLexicalScopeForNextForLoopIteration(VariableEnvironmentNode*, RegisterID* loopSymbolTable);
        int labelScopeDepth() const;
        UnlinkedArrayProfile newArrayProfile();

    private:
        ParserError generate();
        Variable variableForLocalEntry(const Identifier&, const SymbolTableEntry&, int symbolTableConstantIndex, bool isLexicallyScoped);

        RegisterID* kill(RegisterID* dst)
        {
            m_staticPropertyAnalyzer.kill(dst);
            return dst;
        }

        void retrieveLastUnaryOp(int& dstIndex, int& srcIndex);
        ALWAYS_INLINE void rewind();

        void allocateAndEmitScope();

        template<typename JumpOp>
        void setTargetForJumpInstruction(InstructionStream::MutableRef&, int target);

        using BigIntMapEntry = std::tuple<UniquedStringImpl*, uint8_t, bool>;

        using NumberMap = HashMap<double, JSValue>;
        using IdentifierStringMap = HashMap<UniquedStringImpl*, JSString*, IdentifierRepHash>;
        using IdentifierBigIntMap = HashMap<BigIntMapEntry, JSValue>;
        using TemplateObjectDescriptorSet = HashSet<Ref<TemplateObjectDescriptor>>;
        using TemplateDescriptorMap = HashMap<uint64_t, JSTemplateObjectDescriptor*, WTF::IntHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>;

        // Helper for emitCall() and emitConstruct(). This works because the set of
        // expected functions have identical behavior for both call and construct
        // (i.e. "Object()" is identical to "new Object()").
        ExpectedFunction emitExpectedFunctionSnippet(RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, Label& done);
        
        template<typename CallOp>
        RegisterID* emitCall(RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);

        RegisterID* emitCallIterator(RegisterID* iterator, RegisterID* argument, ThrowableExpressionData*);

        // Initializes the stack form the parameter; does nothing for the symbol table.
        RegisterID* initializeNextParameter();
        UniquedStringImpl* visibleNameForParameter(DestructuringPatternNode*);
        
        RegisterID& registerFor(VirtualRegister reg)
        {
            if (reg.isLocal())
                return m_calleeLocals[reg.toLocal()];

            if (reg.offset() == CallFrameSlot::callee)
                return m_calleeRegister;

            ASSERT(m_parameters.size());
            return m_parameters[reg.toArgument()];
        }

        bool hasConstant(const Identifier&) const;
        unsigned addConstant(const Identifier&);
        RegisterID* addConstantValue(JSValue, SourceCodeRepresentation = SourceCodeRepresentation::Other);
        RegisterID* addConstantEmptyValue();

        UnlinkedFunctionExecutable* makeFunction(FunctionMetadataNode* metadata)
        {
            DerivedContextType newDerivedContextType = DerivedContextType::None;

            NeedsClassFieldInitializer needsClassFieldInitializer = metadata->isConstructorAndNeedsClassFieldInitializer() ? NeedsClassFieldInitializer::Yes : NeedsClassFieldInitializer::No;
            if (SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode, SourceParseMode::AsyncArrowFunctionBodyMode).contains(metadata->parseMode())) {
                if (constructorKind() == ConstructorKind::Extends || isDerivedConstructorContext()) {
                    newDerivedContextType = DerivedContextType::DerivedConstructorContext;
                    needsClassFieldInitializer = m_codeBlock->needsClassFieldInitializer();
                }
                else if (m_codeBlock->isClassContext() || isDerivedClassContext())
                    newDerivedContextType = DerivedContextType::DerivedMethodContext;
            }

            Optional<CompactVariableMap::Handle> optionalVariablesUnderTDZ = getVariablesUnderTDZ();

            // FIXME: These flags, ParserModes and propagation to XXXCodeBlocks should be reorganized.
            // https://bugs.webkit.org/show_bug.cgi?id=151547
            SourceParseMode parseMode = metadata->parseMode();
            ConstructAbility constructAbility = constructAbilityForParseMode(parseMode);
            if (parseMode == SourceParseMode::MethodMode && metadata->constructorKind() != ConstructorKind::None)
                constructAbility = ConstructAbility::CanConstruct;

            return UnlinkedFunctionExecutable::create(m_vm, m_scopeNode->source(), metadata, isBuiltinFunction() ? UnlinkedBuiltinFunction : UnlinkedNormalFunction, constructAbility, scriptMode(), WTFMove(optionalVariablesUnderTDZ), newDerivedContextType, needsClassFieldInitializer);
        }

        Optional<CompactVariableMap::Handle> getVariablesUnderTDZ();

        RegisterID* emitConstructVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);
        template<typename CallOp>
        RegisterID* emitCallVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd, DebuggableCall);
        
        void emitLogShadowChickenPrologueIfNecessary();
        void emitLogShadowChickenTailIfNecessary();

        void initializeParameters(FunctionParameters&);
        void initializeVarLexicalEnvironment(int symbolTableConstantIndex, SymbolTable* functionSymbolTable, bool hasCapturedVariables);
        void initializeDefaultParameterValuesAndSetupFunctionScopeStack(FunctionParameters&, bool isSimpleParameterList, FunctionNode*, SymbolTable*, int symbolTableConstantIndex, const ScopedLambda<bool (UniquedStringImpl*)>& captures, bool shouldCreateArgumentsVariableInParameterScope);
        void initializeArrowFunctionContextScopeIfNeeded(SymbolTable* functionSymbolTable = nullptr, bool canReuseLexicalEnvironment = false);
        bool needsDerivedConstructorInArrowFunctionLexicalEnvironment();

        enum class TDZNecessityLevel {
            NotNeeded,
            Optimize,
            DoNotOptimize
        };
        typedef HashMap<RefPtr<UniquedStringImpl>, TDZNecessityLevel, IdentifierRepHash> TDZMap;

    public:
        JSString* addStringConstant(const Identifier&);
        JSValue addBigIntConstant(const Identifier&, uint8_t radix, bool sign);
        RegisterID* addTemplateObjectConstant(Ref<TemplateObjectDescriptor>&&, int);

        const InstructionStream& instructions() const { return m_writer; }

        RegisterID* emitThrowExpressionTooDeepException();

        class PreservedTDZStack {
        private:
            Vector<TDZMap> m_preservedTDZStack;
            friend class BytecodeGenerator;
        };

        void preserveTDZStack(PreservedTDZStack&);
        void restoreTDZStack(const PreservedTDZStack&);

        template<typename Func>
        void withWriter(InstructionStreamWriter& writer, const Func& fn)
        {
            auto prevLastOpcodeID = m_lastOpcodeID;
            auto prevLastInstruction = m_lastInstruction;
            m_writer.swap(writer);
            disablePeepholeOptimization();
            m_lastInstruction = m_writer.ref();
            fn();
            m_writer.swap(writer);
            m_lastOpcodeID = prevLastOpcodeID;
            m_lastInstruction = prevLastInstruction;
        }

    private:
        OptionSet<CodeGenerationMode> m_codeGenerationMode;

        struct LexicalScopeStackEntry {
            SymbolTable* m_symbolTable;
            RegisterID* m_scope;
            bool m_isWithScope;
            int m_symbolTableConstantIndex;
        };
        Vector<LexicalScopeStackEntry> m_lexicalScopeStack;

        Vector<TDZMap> m_TDZStack;
        Optional<size_t> m_varScopeLexicalScopeStackIndex;
        void pushTDZVariables(const VariableEnvironment&, TDZCheckOptimization, TDZRequirement);

        ScopeNode* const m_scopeNode;

        // Some of these objects keep pointers to one another. They are arranged
        // to ensure a sane destruction order that avoids references to freed memory.
        HashSet<RefPtr<UniquedStringImpl>, IdentifierRepHash> m_functions;
        RegisterID m_ignoredResultRegister;
        RegisterID m_thisRegister;
        RegisterID m_calleeRegister;
        RegisterID* m_scopeRegister { nullptr };
        RegisterID* m_topMostScope { nullptr };
        RegisterID* m_argumentsRegister { nullptr };
        RegisterID* m_lexicalEnvironmentRegister { nullptr };
        RegisterID* m_generatorRegister { nullptr };
        RegisterID* m_emptyValueRegister { nullptr };
        RegisterID* m_newTargetRegister { nullptr };
        RegisterID* m_isDerivedConstuctor { nullptr };
        HashMap<LinkTimeConstant, RegisterID*, WTF::IntHash<LinkTimeConstant>, WTF::StrongEnumHashTraits<LinkTimeConstant>> m_linkTimeConstantRegisters;
        RegisterID* m_arrowFunctionContextLexicalEnvironmentRegister { nullptr };
        RegisterID* m_promiseRegister { nullptr };

        FinallyContext* m_currentFinallyContext { nullptr };

        SegmentedVector<RegisterID, 32> m_parameters;
        SegmentedVector<LabelScope, 32> m_labelScopes;
        SegmentedVector<RegisterID, 32> m_constantPoolRegisters;
        unsigned m_finallyDepth { 0 };
        unsigned m_localScopeDepth { 0 };
        const CodeType m_codeType;

        unsigned localScopeDepth() const;
        void pushLocalControlFlowScope();
        void popLocalControlFlowScope();

        // FIXME: Restore overflow checking with UnsafeVectorOverflow once SegmentVector supports it.
        // https://bugs.webkit.org/show_bug.cgi?id=165980
        SegmentedVector<ControlFlowScope, 16> m_controlFlowScopeStack;
        Vector<SwitchInfo> m_switchContextStack;
        Vector<Ref<ForInContext>> m_forInContextStack;
        Vector<TryContext> m_tryContextStack;
        unsigned m_yieldPoints { 0 };

        Strong<SymbolTable> m_generatorFrameSymbolTable;
        int m_generatorFrameSymbolTableIndex { 0 };

        enum FunctionVariableType : uint8_t { NormalFunctionVariable, TopLevelFunctionVariable };
        Vector<std::pair<FunctionMetadataNode*, FunctionVariableType>> m_functionsToInitialize;
        bool m_needToInitializeArguments { false };
        RestParameterNode* m_restParameter { nullptr };
        
        Vector<TryRange> m_tryRanges;
        SegmentedVector<TryData, 8> m_tryData;

        Vector<Ref<Label>> m_optionalChainTargetStack;

        int m_nextConstantOffset { 0 };

        typedef HashMap<FunctionMetadataNode*, unsigned> FunctionOffsetMap;
        FunctionOffsetMap m_functionOffsets;
        
        // Constant pool
        IdentifierMap m_identifierMap;

        typedef HashMap<EncodedJSValueWithRepresentation, unsigned, EncodedJSValueWithRepresentationHash, EncodedJSValueWithRepresentationHashTraits> JSValueMap;
        JSValueMap m_jsValueMap;
        IdentifierStringMap m_stringMap;
        IdentifierBigIntMap m_bigIntMap;
        TemplateObjectDescriptorSet m_templateObjectDescriptorSet;
        TemplateDescriptorMap m_templateDescriptorMap;

        StaticPropertyAnalyzer m_staticPropertyAnalyzer;

        VM& m_vm;

        bool m_usesExceptions { false };
        bool m_expressionTooDeep { false };
        bool m_isBuiltinFunction { false };
        bool m_usesNonStrictEval { false };
        bool m_inTailPosition { false };
        bool m_hasCachedVariablesUnderTDZ { false };
        bool m_needsToUpdateArrowFunctionContext : 1;
        ECMAMode m_ecmaMode;
        DerivedContextType m_derivedContextType { DerivedContextType::None };

        CompactVariableMap::Handle m_cachedVariablesUnderTDZ;

        struct CatchEntry {
            TryData* tryData;
            VirtualRegister exceptionRegister;
            VirtualRegister thrownValueRegister;
            VirtualRegister completionTypeRegister;
        };
        Vector<CatchEntry> m_exceptionHandlersToEmit;

        struct {
            JSTextPosition position;
            DebugHookType type { DidExecuteProgram };
        } m_lastDebugHook;
    };

    class StrictModeScope : private SetForScope<ECMAMode> {
    public:
        StrictModeScope(BytecodeGenerator& generator)
            : SetForScope(generator.m_ecmaMode, ECMAMode::strict())
        {
        }
    };

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::Variable::VariableKind);

} // namespace WTF

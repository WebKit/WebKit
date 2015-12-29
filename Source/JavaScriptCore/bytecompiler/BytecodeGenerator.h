/*
 * Copyright (C) 2008, 2009, 2012-2015 Apple Inc. All rights reserved.
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

#ifndef BytecodeGenerator_h
#define BytecodeGenerator_h

#include "CodeBlock.h"
#include <wtf/HashTraits.h>
#include "Instruction.h"
#include "Label.h"
#include "LabelScope.h"
#include "Interpreter.h"
#include "ParserError.h"
#include "RegisterID.h"
#include "SetForScope.h"
#include "SymbolTable.h"
#include "Debugger.h"
#include "Nodes.h"
#include "StaticPropertyAnalyzer.h"
#include "TemplateRegistryKey.h"
#include "UnlinkedCodeBlock.h"

#include <functional>

#include <wtf/PassRefPtr.h>
#include <wtf/SegmentedVector.h>
#include <wtf/Vector.h>


namespace JSC {

    class Identifier;
    class JSTemplateRegistryKey;

    enum ExpectedFunction {
        NoExpectedFunction,
        ExpectObjectConstructor,
        ExpectArrayConstructor
    };

    enum class ThisResolutionType { Local, Scoped };
    
    class CallArguments {
    public:
        CallArguments(BytecodeGenerator&, ArgumentsNode*, unsigned additionalArguments = 0);

        RegisterID* thisRegister() { return m_argv[0].get(); }
        RegisterID* argumentRegister(unsigned i) { return m_argv[i + 1].get(); }
        unsigned stackOffset() { return -m_argv[0]->index() + JSStack::CallFrameHeaderSize; }
        unsigned argumentCountIncludingThis() { return m_argv.size() - m_padding; }
        RegisterID* profileHookRegister() { return m_profileHookRegister.get(); }
        ArgumentsNode* argumentsNode() { return m_argumentsNode; }

    private:
        RefPtr<RegisterID> m_profileHookRegister;
        ArgumentsNode* m_argumentsNode;
        Vector<RefPtr<RegisterID>, 8, UnsafeVectorOverflow> m_argv;
        unsigned m_padding;
    };

    struct FinallyContext {
        StatementNode* finallyBlock;
        RegisterID* iterator;
        ThrowableExpressionData* enumerationNode;
        unsigned scopeContextStackSize;
        unsigned switchContextStackSize;
        unsigned forInContextStackSize;
        unsigned tryContextStackSize;
        unsigned labelScopesSize;
        unsigned symbolTableStackSize;
        int finallyDepth;
        int dynamicScopeDepth;
    };

    struct ControlFlowContext {
        bool isFinallyBlock;
        FinallyContext finallyContext;
    };

    class ForInContext {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        ForInContext(RegisterID* localRegister)
            : m_localRegister(localRegister)
            , m_isValid(true)
        {
        }

        virtual ~ForInContext()
        {
        }

        bool isValid() const { return m_isValid; }
        void invalidate() { m_isValid = false; }

        enum ForInContextType {
            StructureForInContextType,
            IndexedForInContextType
        };
        virtual ForInContextType type() const = 0;

        RegisterID* local() const { return m_localRegister.get(); }

    private:
        RefPtr<RegisterID> m_localRegister;
        bool m_isValid;
    };

    class StructureForInContext : public ForInContext {
    public:
        StructureForInContext(RegisterID* localRegister, RegisterID* indexRegister, RegisterID* propertyRegister, RegisterID* enumeratorRegister)
            : ForInContext(localRegister)
            , m_indexRegister(indexRegister)
            , m_propertyRegister(propertyRegister)
            , m_enumeratorRegister(enumeratorRegister)
        {
        }

        virtual ForInContextType type() const
        {
            return StructureForInContextType;
        }

        RegisterID* index() const { return m_indexRegister.get(); }
        RegisterID* property() const { return m_propertyRegister.get(); }
        RegisterID* enumerator() const { return m_enumeratorRegister.get(); }

    private:
        RefPtr<RegisterID> m_indexRegister;
        RefPtr<RegisterID> m_propertyRegister;
        RefPtr<RegisterID> m_enumeratorRegister;
    };

    class IndexedForInContext : public ForInContext {
    public:
        IndexedForInContext(RegisterID* localRegister, RegisterID* indexRegister)
            : ForInContext(localRegister)
            , m_indexRegister(indexRegister)
        {
        }

        virtual ForInContextType type() const
        {
            return IndexedForInContextType;
        }

        RegisterID* index() const { return m_indexRegister.get(); }

    private:
        RefPtr<RegisterID> m_indexRegister;
    };

    struct TryData {
        RefPtr<Label> target;
        HandlerType handlerType;
    };

    struct TryContext {
        RefPtr<Label> start;
        TryData* tryData;
    };

    class Variable {
    public:
        enum VariableKind { NormalVariable, SpecialVariable };

        Variable()
            : m_offset()
            , m_local(nullptr)
            , m_attributes(0)
            , m_kind(NormalVariable)
            , m_symbolTableConstantIndex(0) // This is meaningless here for this kind of Variable.
            , m_isLexicallyScoped(false)
        {
        }
        
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

        bool isReadOnly() const { return m_attributes & ReadOnly; }
        bool isSpecial() const { return m_kind != NormalVariable; }
        bool isConst() const { return isReadOnly() && m_isLexicallyScoped; }
        void setIsReadOnly() { m_attributes |= ReadOnly; }

    private:
        Identifier m_ident;
        VarOffset m_offset;
        RegisterID* m_local;
        unsigned m_attributes;
        VariableKind m_kind;
        int m_symbolTableConstantIndex;
        bool m_isLexicallyScoped;
    };

    struct TryRange {
        RefPtr<Label> start;
        RefPtr<Label> end;
        TryData* tryData;
    };

    enum ProfileTypeBytecodeFlag {
        ProfileTypeBytecodeClosureVar,
        ProfileTypeBytecodeLocallyResolved,
        ProfileTypeBytecodeDoesNotHaveGlobalID,
        ProfileTypeBytecodeFunctionArgument,
        ProfileTypeBytecodeFunctionReturnStatement
    };

    class BytecodeGenerator {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(BytecodeGenerator);
    public:
        typedef DeclarationStacks::FunctionStack FunctionStack;

        BytecodeGenerator(VM&, ProgramNode*, UnlinkedProgramCodeBlock*, DebuggerMode, ProfilerMode, const VariableEnvironment*);
        BytecodeGenerator(VM&, FunctionNode*, UnlinkedFunctionCodeBlock*, DebuggerMode, ProfilerMode, const VariableEnvironment*);
        BytecodeGenerator(VM&, EvalNode*, UnlinkedEvalCodeBlock*, DebuggerMode, ProfilerMode, const VariableEnvironment*);
        BytecodeGenerator(VM&, ModuleProgramNode*, UnlinkedModuleProgramCodeBlock*, DebuggerMode, ProfilerMode, const VariableEnvironment*);

        ~BytecodeGenerator();
        
        VM* vm() const { return m_vm; }
        ParserArena& parserArena() const { return m_scopeNode->parserArena(); }
        const CommonIdentifiers& propertyNames() const { return *m_vm->propertyNames; }

        bool isConstructor() const { return m_codeBlock->isConstructor(); }
        bool isDerivedConstructorContext() const { return m_codeBlock->isDerivedConstructorContext(); }
        bool usesArrowFunction() const { return m_scopeNode->usesArrowFunction(); }
        bool needsToUpdateArrowFunctionContext() const { return m_needsToUpdateArrowFunctionContext; }
        bool usesEval() const { return m_scopeNode->usesEval(); }
        bool usesThis() const { return m_scopeNode->usesThis(); }
        ConstructorKind constructorKind() const { return m_codeBlock->constructorKind(); }
        SuperBinding superBinding() const { return m_codeBlock->superBinding(); }

        ParserError generate();

        bool isArgumentNumber(const Identifier&, int);

        Variable variable(const Identifier&, ThisResolutionType = ThisResolutionType::Local);
        
        enum ExistingVariableMode { VerifyExisting, IgnoreExisting };
        void createVariable(const Identifier&, VarKind, SymbolTable*, ExistingVariableMode = VerifyExisting); // Creates the variable, or asserts that the already-created variable is sufficiently compatible.
        
        // Returns the register storing "this"
        RegisterID* thisRegister() { return &m_thisRegister; }
        RegisterID* argumentsRegister() { return m_argumentsRegister; }
        RegisterID* newTarget()
        {
            return !m_codeBlock->isArrowFunction() || m_isNewTargetLoadedInArrowFunction
                ? m_newTargetRegister : emitLoadNewTargetFromArrowFunctionLexicalEnvironment();
        }

        RegisterID* scopeRegister() { return m_scopeRegister; }

        RegisterID* generatorRegister() { return m_generatorRegister; }

        // Returns the next available temporary register. Registers returned by
        // newTemporary require a modified form of reference counting: any
        // register with a refcount of 0 is considered "available", meaning that
        // the next instruction may overwrite it.
        RegisterID* newTemporary();

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

        LabelScopePtr newLabelScope(LabelScope::Type, const Identifier* = 0);
        PassRefPtr<Label> newLabel();

        void emitNode(RegisterID* dst, StatementNode* n)
        {
            SetForScope<bool> tailPositionPoisoner(m_inTailPosition, false);
            return emitNodeInTailPosition(dst, n);
        }

        void emitNodeInTailPosition(RegisterID* dst, StatementNode* n)
        {
            // Node::emitCode assumes that dst, if provided, is either a local or a referenced temporary.
            ASSERT(!dst || dst == ignoredResult() || !dst->isTemporary() || dst->refCount());
            if (!m_vm->isSafeToRecurse()) {
                emitThrowExpressionTooDeepException();
                return;
            }
            n->emitBytecode(*this, dst);
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
            if (!m_vm->isSafeToRecurse())
                return emitThrowExpressionTooDeepException();
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

        void emitNodeInConditionContext(ExpressionNode* n, Label* trueTarget, Label* falseTarget, FallThroughMode fallThroughMode)
        {
            if (!m_vm->isSafeToRecurse()) {
                emitThrowExpressionTooDeepException();
                return;
            }

            n->emitBytecodeInConditionContext(*this, trueTarget, falseTarget, fallThroughMode);
        }

        void emitExpressionInfo(const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
        {            
            ASSERT(divot.offset >= divotStart.offset);
            ASSERT(divotEnd.offset >= divot.offset);

            int sourceOffset = m_scopeNode->source().startOffset();
            unsigned firstLine = m_scopeNode->source().firstLine();

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
            if (!m_isBuiltinFunction)
                m_codeBlock->addExpressionInfo(instructionOffset, divotOffset, startOffset, endOffset, line, column);
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

            return emitNode(n);
        }

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
        
        RegisterID* emitLoadArrowFunctionLexicalEnvironment();
        void emitLoadThisFromArrowFunctionLexicalEnvironment();
        RegisterID* emitLoadNewTargetFromArrowFunctionLexicalEnvironment();

        RegisterID* emitLoad(RegisterID* dst, bool);
        RegisterID* emitLoad(RegisterID* dst, const Identifier&);
        RegisterID* emitLoad(RegisterID* dst, JSValue, SourceCodeRepresentation = SourceCodeRepresentation::Other);
        RegisterID* emitLoadGlobalObject(RegisterID* dst);

        RegisterID* emitUnaryOp(OpcodeID, RegisterID* dst, RegisterID* src);
        RegisterID* emitBinaryOp(OpcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2, OperandTypes);
        RegisterID* emitEqualityOp(OpcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2);
        RegisterID* emitUnaryNoDstOp(OpcodeID, RegisterID* src);

        RegisterID* emitCreateThis(RegisterID* dst);
        void emitTDZCheck(RegisterID* target);
        bool needsTDZCheck(const Variable&);
        void emitTDZCheckIfNecessary(const Variable&, RegisterID* target, RegisterID* scope);
        void liftTDZCheckIfPossible(const Variable&);
        RegisterID* emitNewObject(RegisterID* dst);
        RegisterID* emitNewArray(RegisterID* dst, ElementNode*, unsigned length); // stops at first elision
        RegisterID* emitNewArrayWithSize(RegisterID* dst, RegisterID* length);

        RegisterID* emitNewFunction(RegisterID* dst, FunctionMetadataNode*);
        RegisterID* emitNewFunctionExpression(RegisterID* dst, FuncExprNode* func);
        RegisterID* emitNewDefaultConstructor(RegisterID* dst, ConstructorKind, const Identifier& name);
        RegisterID* emitNewArrowFunctionExpression(RegisterID*, ArrowFuncExprNode*);
        RegisterID* emitNewRegExp(RegisterID* dst, RegExp*);

        RegisterID* emitMoveLinkTimeConstant(RegisterID* dst, LinkTimeConstant);
        RegisterID* emitMoveEmptyValue(RegisterID* dst);
        RegisterID* emitMove(RegisterID* dst, RegisterID* src);

        RegisterID* emitToNumber(RegisterID* dst, RegisterID* src) { return emitUnaryOp(op_to_number, dst, src); }
        RegisterID* emitToString(RegisterID* dst, RegisterID* src) { return emitUnaryOp(op_to_string, dst, src); }
        RegisterID* emitInc(RegisterID* srcDst);
        RegisterID* emitDec(RegisterID* srcDst);

        RegisterID* emitOverridesHasInstance(RegisterID* dst, RegisterID* constructor, RegisterID* hasInstanceValue);
        RegisterID* emitInstanceOf(RegisterID* dst, RegisterID* value, RegisterID* basePrototype);
        RegisterID* emitInstanceOfCustom(RegisterID* dst, RegisterID* value, RegisterID* constructor, RegisterID* hasInstanceValue);
        RegisterID* emitTypeOf(RegisterID* dst, RegisterID* src) { return emitUnaryOp(op_typeof, dst, src); }
        RegisterID* emitIn(RegisterID* dst, RegisterID* property, RegisterID* base) { return emitBinaryOp(op_in, dst, property, base, OperandTypes()); }

        RegisterID* emitGetById(RegisterID* dst, RegisterID* base, const Identifier& property);
        RegisterID* emitPutById(RegisterID* base, const Identifier& property, RegisterID* value);
        RegisterID* emitDirectPutById(RegisterID* base, const Identifier& property, RegisterID* value, PropertyNode::PutType);
        RegisterID* emitDeleteById(RegisterID* dst, RegisterID* base, const Identifier&);
        RegisterID* emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitGetArgumentByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitPutByVal(RegisterID* base, RegisterID* property, RegisterID* value);
        RegisterID* emitDirectPutByVal(RegisterID* base, RegisterID* property, RegisterID* value);
        RegisterID* emitDeleteByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitPutByIndex(RegisterID* base, unsigned index, RegisterID* value);

        RegisterID* emitAssert(RegisterID* condition, int line);

        void emitPutGetterById(RegisterID* base, const Identifier& property, unsigned propertyDescriptorOptions, RegisterID* getter);
        void emitPutSetterById(RegisterID* base, const Identifier& property, unsigned propertyDescriptorOptions, RegisterID* setter);
        void emitPutGetterSetter(RegisterID* base, const Identifier& property, unsigned attributes, RegisterID* getter, RegisterID* setter);
        void emitPutGetterByVal(RegisterID* base, RegisterID* property, unsigned propertyDescriptorOptions, RegisterID* getter);
        void emitPutSetterByVal(RegisterID* base, RegisterID* property, unsigned propertyDescriptorOptions, RegisterID* setter);
        
        ExpectedFunction expectedFunctionForIdentifier(const Identifier&);
        RegisterID* emitCall(RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);
        RegisterID* emitCallInTailPosition(RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);
        RegisterID* emitCallEval(RegisterID* dst, RegisterID* func, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);
        RegisterID* emitCallVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, RegisterID* profileHookRegister, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);
        RegisterID* emitCallVarargsInTailPosition(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, RegisterID* profileHookRegister, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

        enum PropertyDescriptorOption {
            PropertyConfigurable = 1,
            PropertyWritable     = 1 << 1,
            PropertyEnumerable   = 1 << 2,
        };
        void emitCallDefineProperty(RegisterID* newObj, RegisterID* propertyNameRegister,
            RegisterID* valueRegister, RegisterID* getterRegister, RegisterID* setterRegister, unsigned options, const JSTextPosition&);

        void emitEnumeration(ThrowableExpressionData* enumerationNode, ExpressionNode* subjectNode, const std::function<void(BytecodeGenerator&, RegisterID*)>& callBack, VariableEnvironmentNode* = nullptr, RegisterID* forLoopSymbolTable = nullptr);

        RegisterID* emitGetTemplateObject(RegisterID* dst, TaggedTemplateNode*);

        RegisterID* emitReturn(RegisterID* src);
        RegisterID* emitEnd(RegisterID* src) { return emitUnaryNoDstOp(op_end, src); }

        RegisterID* emitConstruct(RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);
        RegisterID* emitStrcat(RegisterID* dst, RegisterID* src, int count);
        void emitToPrimitive(RegisterID* dst, RegisterID* src);

        ResolveType resolveType();
        RegisterID* emitResolveConstantLocal(RegisterID* dst, const Variable&);
        RegisterID* emitResolveScope(RegisterID* dst, const Variable&);
        RegisterID* emitGetFromScope(RegisterID* dst, RegisterID* scope, const Variable&, ResolveMode);
        RegisterID* emitPutToScope(RegisterID* scope, const Variable&, RegisterID* value, ResolveMode, InitializationMode);
        RegisterID* initializeVariable(const Variable&, RegisterID* value);

        PassRefPtr<Label> emitLabel(Label*);
        void emitLoopHint();
        PassRefPtr<Label> emitJump(Label* target);
        PassRefPtr<Label> emitJumpIfTrue(RegisterID* cond, Label* target);
        PassRefPtr<Label> emitJumpIfFalse(RegisterID* cond, Label* target);
        PassRefPtr<Label> emitJumpIfNotFunctionCall(RegisterID* cond, Label* target);
        PassRefPtr<Label> emitJumpIfNotFunctionApply(RegisterID* cond, Label* target);
        void emitPopScopes(RegisterID* srcDst, int targetScopeDepth);

        void emitEnter();
        void emitWatchdog();

        RegisterID* emitHasIndexedProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName);
        RegisterID* emitHasStructureProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName, RegisterID* enumerator);
        RegisterID* emitHasGenericProperty(RegisterID* dst, RegisterID* base, RegisterID* propertyName);
        RegisterID* emitGetPropertyEnumerator(RegisterID* dst, RegisterID* base);
        RegisterID* emitGetEnumerableLength(RegisterID* dst, RegisterID* base);
        RegisterID* emitGetStructurePropertyEnumerator(RegisterID* dst, RegisterID* base, RegisterID* length);
        RegisterID* emitGetGenericPropertyEnumerator(RegisterID* dst, RegisterID* base, RegisterID* length, RegisterID* structureEnumerator);
        RegisterID* emitEnumeratorStructurePropertyName(RegisterID* dst, RegisterID* enumerator, RegisterID* index);
        RegisterID* emitEnumeratorGenericPropertyName(RegisterID* dst, RegisterID* enumerator, RegisterID* index);
        RegisterID* emitToIndexString(RegisterID* dst, RegisterID* index);

        RegisterID* emitIsObject(RegisterID* dst, RegisterID* src);
        RegisterID* emitIsUndefined(RegisterID* dst, RegisterID* src);
        void emitRequireObjectCoercible(RegisterID* value, const String& error);

        RegisterID* emitIteratorNext(RegisterID* dst, RegisterID* iterator, const ThrowableExpressionData* node);
        RegisterID* emitIteratorNextWithValue(RegisterID* dst, RegisterID* iterator, RegisterID* value, const ThrowableExpressionData* node);
        void emitIteratorClose(RegisterID* iterator, const ThrowableExpressionData* node);

        RegisterID* emitRestParameter(RegisterID* result, unsigned numParametersToSkip);

        bool emitReadOnlyExceptionIfNeeded(const Variable&);

        // Start a try block. 'start' must have been emitted.
        TryData* pushTry(Label* start);
        // End a try block. 'end' must have been emitted.
        void popTryAndEmitCatch(TryData*, RegisterID* exceptionRegister, RegisterID* thrownValueRegister, Label* end, HandlerType);

        void emitThrow(RegisterID* exc)
        { 
            m_usesExceptions = true;
            emitUnaryNoDstOp(op_throw, exc);
        }

        void emitThrowReferenceError(const String& message);
        void emitThrowTypeError(const String& message);

        void emitPushCatchScope(const Identifier& property, RegisterID* exceptionValue, VariableEnvironment&);
        void emitPopCatchScope(VariableEnvironment&);

        void emitGetScope();
        RegisterID* emitPushWithScope(RegisterID* objectScope);
        void emitPopWithScope();
        void emitPutThisToArrowFunctionContextScope();
        void emitPutNewTargetToArrowFunctionContextScope();
        void emitPutDerivedConstructorToArrowFunctionContextScope();
        RegisterID* emitLoadDerivedConstructorFromArrowFunctionLexicalEnvironment();

        void emitDebugHook(DebugHookID, unsigned line, unsigned charOffset, unsigned lineStart);

        bool isInFinallyBlock() { return m_finallyDepth > 0; }

        void pushFinallyContext(StatementNode* finallyBlock);
        void popFinallyContext();
        void pushIteratorCloseContext(RegisterID* iterator, ThrowableExpressionData* enumerationNode);
        void popIteratorCloseContext();

        void pushIndexedForInScope(RegisterID* local, RegisterID* index);
        void popIndexedForInScope(RegisterID* local);
        void pushStructureForInScope(RegisterID* local, RegisterID* index, RegisterID* property, RegisterID* enumerator);
        void popStructureForInScope(RegisterID* local);
        void invalidateForInContextForLocal(RegisterID* local);

        LabelScopePtr breakTarget(const Identifier&);
        LabelScopePtr continueTarget(const Identifier&);

        void beginSwitch(RegisterID*, SwitchInfo::SwitchType);
        void endSwitch(uint32_t clauseCount, RefPtr<Label>*, ExpressionNode**, Label* defaultLabel, int32_t min, int32_t range);

        void emitYieldPoint(RegisterID*);
        void emitSave(Label* mergePoint, unsigned liveCalleeLocalsIndex);
        void emitResume(Label* mergePoint, unsigned liveCalleeLocalsIndex);

        void emitGeneratorStateLabel();
        void emitGeneratorStateChange(int32_t state);
        RegisterID* emitYield(RegisterID* argument);
        RegisterID* emitDelegateYield(RegisterID* argument, ThrowableExpressionData*);
        void beginGenerator(RegisterID*);
        void endGenerator(Label* defaultLabel);
        RegisterID* generatorStateRegister() { return &m_parameters[2]; }
        RegisterID* generatorValueRegister() { return &m_parameters[3]; }
        RegisterID* generatorResumeModeRegister() { return &m_parameters[4]; }

        CodeType codeType() const { return m_codeType; }

        bool shouldEmitProfileHooks() { return m_shouldEmitProfileHooks; }
        bool shouldEmitDebugHooks() { return m_shouldEmitDebugHooks; }
        
        bool isStrictMode() const { return m_codeBlock->isStrictMode(); }

        SourceParseMode parseMode() const { return m_codeBlock->parseMode(); }
        
        bool isBuiltinFunction() const { return m_isBuiltinFunction; }

        OpcodeID lastOpcodeID() const { return m_lastOpcodeID; }

        enum class TDZCheckOptimization { Optimize, DoNotOptimize };
        enum class NestedScopeType { IsNested, IsNotNested };
    private:
        enum class TDZRequirement { UnderTDZ, NotUnderTDZ };
        enum class ScopeType { CatchScope, LetConstScope, FunctionNameScope };
        enum class ScopeRegisterType { Var, Block };
        void pushLexicalScopeInternal(VariableEnvironment&, TDZCheckOptimization, NestedScopeType, RegisterID** constantSymbolTableResult, TDZRequirement, ScopeType, ScopeRegisterType);
        void popLexicalScopeInternal(VariableEnvironment&, TDZRequirement);
        template<typename LookUpVarKindFunctor>
        bool instantiateLexicalVariables(const VariableEnvironment&, SymbolTable*, ScopeRegisterType, LookUpVarKindFunctor);
        void emitPrefillStackTDZVariables(const VariableEnvironment&, SymbolTable*);
        void emitPopScope(RegisterID* dst, RegisterID* scope);
        RegisterID* emitGetParentScope(RegisterID* dst, RegisterID* scope);
        void emitPushFunctionNameScope(const Identifier& property, RegisterID* value, bool isCaptured);
        void emitNewFunctionExpressionCommon(RegisterID*, BaseFuncExprNode*);

    public:
        void pushLexicalScope(VariableEnvironmentNode*, TDZCheckOptimization, NestedScopeType = NestedScopeType::IsNotNested, RegisterID** constantSymbolTableResult = nullptr);
        void popLexicalScope(VariableEnvironmentNode*);
        void prepareLexicalScopeForNextForLoopIteration(VariableEnvironmentNode*, RegisterID* loopSymbolTable);
        int labelScopeDepth() const;

    private:
        void reclaimFreeRegisters();
        Variable variableForLocalEntry(const Identifier&, const SymbolTableEntry&, int symbolTableConstantIndex, bool isLexicallyScoped);

        void emitOpcode(OpcodeID);
        UnlinkedArrayAllocationProfile newArrayAllocationProfile();
        UnlinkedObjectAllocationProfile newObjectAllocationProfile();
        UnlinkedArrayProfile newArrayProfile();
        UnlinkedValueProfile emitProfiledOpcode(OpcodeID);
        int kill(RegisterID* dst)
        {
            int index = dst->index();
            m_staticPropertyAnalyzer.kill(index);
            return index;
        }

        void retrieveLastBinaryOp(int& dstIndex, int& src1Index, int& src2Index);
        void retrieveLastUnaryOp(int& dstIndex, int& srcIndex);
        ALWAYS_INLINE void rewindBinaryOp();
        ALWAYS_INLINE void rewindUnaryOp();

        void allocateCalleeSaveSpace();
        void allocateAndEmitScope();
        void emitComplexPopScopes(RegisterID*, ControlFlowContext* topScope, ControlFlowContext* bottomScope);

        typedef HashMap<double, JSValue> NumberMap;
        typedef HashMap<UniquedStringImpl*, JSString*, IdentifierRepHash> IdentifierStringMap;
        typedef HashMap<TemplateRegistryKey, JSTemplateRegistryKey*> TemplateRegistryKeyMap;
        
        // Helper for emitCall() and emitConstruct(). This works because the set of
        // expected functions have identical behavior for both call and construct
        // (i.e. "Object()" is identical to "new Object()").
        ExpectedFunction emitExpectedFunctionSnippet(RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, Label* done);
        
        RegisterID* emitCall(OpcodeID, RegisterID* dst, RegisterID* func, ExpectedFunction, CallArguments&, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

        RegisterID* newRegister();

        // Adds an anonymous local var slot. To give this slot a name, add it to symbolTable().
        RegisterID* addVar()
        {
            ++m_codeBlock->m_numVars;
            RegisterID* result = newRegister();
            ASSERT(VirtualRegister(result->index()).toLocal() == m_codeBlock->m_numVars - 1);
            result->ref(); // We should never free this slot.
            return result;
        }

        // Initializes the stack form the parameter; does nothing for the symbol table.
        RegisterID* initializeNextParameter();
        UniquedStringImpl* visibleNameForParameter(DestructuringPatternNode*);
        
        RegisterID& registerFor(VirtualRegister reg)
        {
            if (reg.isLocal())
                return m_calleeLocals[reg.toLocal()];

            if (reg.offset() == JSStack::Callee)
                return m_calleeRegister;

            ASSERT(m_parameters.size());
            return m_parameters[reg.toArgument()];
        }

        bool hasConstant(const Identifier&) const;
        unsigned addConstant(const Identifier&);
        RegisterID* addConstantValue(JSValue, SourceCodeRepresentation = SourceCodeRepresentation::Other);
        RegisterID* addConstantEmptyValue();
        unsigned addRegExp(RegExp*);

        unsigned addConstantBuffer(unsigned length);
        
        UnlinkedFunctionExecutable* makeFunction(FunctionMetadataNode* metadata)
        {
            bool newisDerivedConstructorContext = constructorKind() == ConstructorKind::Derived || (m_isDerivedConstructorContext && metadata->parseMode() == SourceParseMode::ArrowFunctionMode);

            VariableEnvironment variablesUnderTDZ;
            getVariablesUnderTDZ(variablesUnderTDZ);

            // FIXME: These flags, ParserModes and propagation to XXXCodeBlocks should be reorganized.
            // https://bugs.webkit.org/show_bug.cgi?id=151547
            SourceParseMode parseMode = metadata->parseMode();
            ConstructAbility constructAbility = ConstructAbility::CanConstruct;
            if (parseMode == SourceParseMode::GetterMode || parseMode == SourceParseMode::SetterMode || parseMode == SourceParseMode::ArrowFunctionMode || parseMode == SourceParseMode::GeneratorWrapperFunctionMode)
                constructAbility = ConstructAbility::CannotConstruct;
            else if (parseMode == SourceParseMode::MethodMode && metadata->constructorKind() == ConstructorKind::None)
                constructAbility = ConstructAbility::CannotConstruct;

            return UnlinkedFunctionExecutable::create(m_vm, m_scopeNode->source(), metadata, isBuiltinFunction() ? UnlinkedBuiltinFunction : UnlinkedNormalFunction, constructAbility, variablesUnderTDZ, newisDerivedConstructorContext);
        }

        void getVariablesUnderTDZ(VariableEnvironment&);

        RegisterID* emitConstructVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, RegisterID* profileHookRegister, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);
        RegisterID* emitCallVarargs(OpcodeID, RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, int32_t firstVarArgOffset, RegisterID* profileHookRegister, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd);

        void initializeParameters(FunctionParameters&);
        void initializeVarLexicalEnvironment(int symbolTableConstantIndex);
        void initializeDefaultParameterValuesAndSetupFunctionScopeStack(FunctionParameters&, FunctionNode*, SymbolTable*, int symbolTableConstantIndex, const std::function<bool (UniquedStringImpl*)>& captures);
        void initializeArrowFunctionContextScopeIfNeeded(SymbolTable* = nullptr);

    public:
        JSString* addStringConstant(const Identifier&);
        JSTemplateRegistryKey* addTemplateRegistryKeyConstant(const TemplateRegistryKey&);

        Vector<UnlinkedInstruction, 0, UnsafeVectorOverflow>& instructions() { return m_instructions; }

        RegisterID* emitThrowExpressionTooDeepException();

    private:
        Vector<UnlinkedInstruction, 0, UnsafeVectorOverflow> m_instructions;

        bool m_shouldEmitDebugHooks;
        bool m_shouldEmitProfileHooks;

        struct SymbolTableStackEntry {
            Strong<SymbolTable> m_symbolTable;
            RegisterID* m_scope;
            bool m_isWithScope;
            int m_symbolTableConstantIndex;
        };
        Vector<SymbolTableStackEntry> m_symbolTableStack;
        Vector<std::pair<VariableEnvironment, bool>> m_TDZStack;

        ScopeNode* const m_scopeNode;
        Strong<UnlinkedCodeBlock> m_codeBlock;

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
        RegisterID* m_globalObjectRegister { nullptr };
        RegisterID* m_newTargetRegister { nullptr };
        RegisterID* m_linkTimeConstantRegisters[LinkTimeConstantCount];
        RegisterID* m_arrowFunctionContextLexicalEnvironmentRegister { nullptr };

        SegmentedVector<RegisterID*, 16> m_localRegistersForCalleeSaveRegisters;
        SegmentedVector<RegisterID, 32> m_constantPoolRegisters;
        SegmentedVector<RegisterID, 32> m_calleeLocals;
        SegmentedVector<RegisterID, 32> m_parameters;
        SegmentedVector<Label, 32> m_labels;
        LabelScopeStore m_labelScopes;
        int m_finallyDepth { 0 };
        int m_localScopeDepth { 0 };
        const CodeType m_codeType;

        int localScopeDepth() const;
        void pushScopedControlFlowContext();
        void popScopedControlFlowContext();

        Vector<ControlFlowContext, 0, UnsafeVectorOverflow> m_scopeContextStack;
        Vector<SwitchInfo> m_switchContextStack;
        Vector<std::unique_ptr<ForInContext>> m_forInContextStack;
        Vector<TryContext> m_tryContextStack;
        Vector<RefPtr<Label>> m_generatorResumeLabels;
        enum FunctionVariableType : uint8_t { NormalFunctionVariable, GlobalFunctionVariable };
        Vector<std::pair<FunctionMetadataNode*, FunctionVariableType>> m_functionsToInitialize;
        bool m_needToInitializeArguments { false };
        RestParameterNode* m_restParameter { nullptr };
        
        Vector<TryRange> m_tryRanges;
        SegmentedVector<TryData, 8> m_tryData;

        int m_nextConstantOffset { 0 };

        typedef HashMap<FunctionMetadataNode*, unsigned> FunctionOffsetMap;
        FunctionOffsetMap m_functionOffsets;
        
        // Constant pool
        IdentifierMap m_identifierMap;

        typedef HashMap<EncodedJSValueWithRepresentation, unsigned, EncodedJSValueWithRepresentationHash, EncodedJSValueWithRepresentationHashTraits> JSValueMap;
        JSValueMap m_jsValueMap;
        IdentifierStringMap m_stringMap;
        TemplateRegistryKeyMap m_templateRegistryKeyMap;

        StaticPropertyAnalyzer m_staticPropertyAnalyzer { &m_instructions };

        VM* m_vm;

        OpcodeID m_lastOpcodeID = op_end;
#ifndef NDEBUG
        size_t m_lastOpcodePosition { 0 };
#endif

        bool m_usesExceptions { false };
        bool m_expressionTooDeep { false };
        bool m_isBuiltinFunction { false };
        bool m_usesNonStrictEval { false };
        bool m_inTailPosition { false };
        bool m_isDerivedConstructorContext { false };
        bool m_needsToUpdateArrowFunctionContext;
        bool m_isNewTargetLoadedInArrowFunction { false };
    };

}

#endif // BytecodeGenerator_h

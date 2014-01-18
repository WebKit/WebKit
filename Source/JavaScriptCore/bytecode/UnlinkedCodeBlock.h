/*
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
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

#ifndef UnlinkedCodeBlock_h
#define UnlinkedCodeBlock_h

#include "BytecodeConventions.h"
#include "CodeSpecializationKind.h"
#include "CodeType.h"
#include "ExpressionRangeInfo.h"
#include "Identifier.h"
#include "JSCell.h"
#include "JSString.h"
#include "LineColumnInfo.h"
#include "ParserModes.h"
#include "RegExp.h"
#include "SpecialPointer.h"
#include "SymbolTable.h"
#include "VirtualRegister.h"

#include <wtf/Compression.h>
#include <wtf/RefCountedArray.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace JSC {

class Debugger;
class FunctionBodyNode;
class FunctionExecutable;
class FunctionParameters;
class JSScope;
struct ParserError;
class ScriptExecutable;
class SourceCode;
class SourceProvider;
class SymbolTable;
class UnlinkedCodeBlock;
class UnlinkedFunctionCodeBlock;

typedef unsigned UnlinkedValueProfile;
typedef unsigned UnlinkedArrayProfile;
typedef unsigned UnlinkedArrayAllocationProfile;
typedef unsigned UnlinkedObjectAllocationProfile;
typedef unsigned UnlinkedLLIntCallLinkInfo;

struct ExecutableInfo {
    ExecutableInfo(bool needsActivation, bool usesEval, bool isStrictMode, bool isConstructor)
        : m_needsActivation(needsActivation)
        , m_usesEval(usesEval)
        , m_isStrictMode(isStrictMode)
        , m_isConstructor(isConstructor)
    {
    }
    bool m_needsActivation;
    bool m_usesEval;
    bool m_isStrictMode;
    bool m_isConstructor;
};

class UnlinkedFunctionExecutable : public JSCell {
public:
    friend class CodeCache;
    typedef JSCell Base;
    static UnlinkedFunctionExecutable* create(VM* vm, const SourceCode& source, FunctionBodyNode* node, bool isFromGlobalCode = false)
    {
        UnlinkedFunctionExecutable* instance = new (NotNull, allocateCell<UnlinkedFunctionExecutable>(vm->heap)) UnlinkedFunctionExecutable(vm, vm->unlinkedFunctionExecutableStructure.get(), source, node, isFromGlobalCode);
        instance->finishCreation(*vm);
        return instance;
    }

    const Identifier& name() const { return m_name; }
    const Identifier& inferredName() const { return m_inferredName; }
    JSString* nameValue() const { return m_nameValue.get(); }
    SymbolTable* symbolTable(CodeSpecializationKind kind)
    {
        return (kind == CodeForCall) ? m_symbolTableForCall.get() : m_symbolTableForConstruct.get();
    }
    size_t parameterCount() const;
    bool isInStrictContext() const { return m_isInStrictContext; }
    FunctionNameIsInScopeToggle functionNameIsInScopeToggle() const { return m_functionNameIsInScopeToggle; }

    unsigned firstLineOffset() const { return m_firstLineOffset; }
    unsigned lineCount() const { return m_lineCount; }
    unsigned unlinkedFunctionNameStart() const { return m_unlinkedFunctionNameStart; }
    unsigned unlinkedBodyStartColumn() const { return m_unlinkedBodyStartColumn; }
    unsigned unlinkedBodyEndColumn() const { return m_unlinkedBodyEndColumn; }
    unsigned startOffset() const { return m_startOffset; }
    unsigned sourceLength() { return m_sourceLength; }

    String paramString() const;

    UnlinkedFunctionCodeBlock* codeBlockFor(VM&, const SourceCode&, CodeSpecializationKind, DebuggerMode, ProfilerMode, ParserError&);

    static UnlinkedFunctionExecutable* fromGlobalCode(const Identifier&, ExecState*, Debugger*, const SourceCode&, JSObject** exception);

    FunctionExecutable* link(VM&, const SourceCode&, size_t lineOffset, size_t sourceOffset);

    void clearCodeForRecompilation()
    {
        m_symbolTableForCall.clear();
        m_symbolTableForConstruct.clear();
        m_codeBlockForCall.clear();
        m_codeBlockForConstruct.clear();
    }

    FunctionParameters* parameters() { return m_parameters.get(); }

    void recordParse(CodeFeatures features, bool hasCapturedVariables)
    {
        m_features = features;
        m_hasCapturedVariables = hasCapturedVariables;
    }

    bool forceUsesArguments() const { return m_forceUsesArguments; }

    CodeFeatures features() const { return m_features; }
    bool hasCapturedVariables() const { return m_hasCapturedVariables; }

    static const bool needsDestruction = true;
    static const bool hasImmortalStructure = true;
    static void destroy(JSCell*);

private:
    UnlinkedFunctionExecutable(VM*, Structure*, const SourceCode&, FunctionBodyNode*, bool isFromGlobalCode);
    WriteBarrier<UnlinkedFunctionCodeBlock> m_codeBlockForCall;
    WriteBarrier<UnlinkedFunctionCodeBlock> m_codeBlockForConstruct;

    unsigned m_numCapturedVariables : 29;
    bool m_forceUsesArguments : 1;
    bool m_isInStrictContext : 1;
    bool m_hasCapturedVariables : 1;
    bool m_isFromGlobalCode : 1;

    Identifier m_name;
    Identifier m_inferredName;
    WriteBarrier<JSString> m_nameValue;
    WriteBarrier<SymbolTable> m_symbolTableForCall;
    WriteBarrier<SymbolTable> m_symbolTableForConstruct;
    RefPtr<FunctionParameters> m_parameters;
    unsigned m_firstLineOffset;
    unsigned m_lineCount;
    unsigned m_unlinkedFunctionNameStart;
    unsigned m_unlinkedBodyStartColumn;
    unsigned m_unlinkedBodyEndColumn;
    unsigned m_startOffset;
    unsigned m_sourceLength;

    CodeFeatures m_features;

    FunctionNameIsInScopeToggle m_functionNameIsInScopeToggle;

protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        m_nameValue.set(vm, this, jsString(&vm, name().string()));
    }

    static void visitChildren(JSCell*, SlotVisitor&);

public:
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(UnlinkedFunctionExecutableType, StructureFlags), info());
    }

    static const unsigned StructureFlags = OverridesVisitChildren | JSCell::StructureFlags;

    DECLARE_EXPORT_INFO;
};

struct UnlinkedStringJumpTable {
    typedef HashMap<RefPtr<StringImpl>, int32_t> StringOffsetTable;
    StringOffsetTable offsetTable;

    inline int32_t offsetForValue(StringImpl* value, int32_t defaultOffset)
    {
        StringOffsetTable::const_iterator end = offsetTable.end();
        StringOffsetTable::const_iterator loc = offsetTable.find(value);
        if (loc == end)
            return defaultOffset;
        return loc->value;
    }

};

struct UnlinkedSimpleJumpTable {
    Vector<int32_t> branchOffsets;
    int32_t min;

    int32_t offsetForValue(int32_t value, int32_t defaultOffset);
    void add(int32_t key, int32_t offset)
    {
        if (!branchOffsets[key])
            branchOffsets[key] = offset;
    }
};

struct UnlinkedHandlerInfo {
    uint32_t start;
    uint32_t end;
    uint32_t target;
    uint32_t scopeDepth;
};

struct UnlinkedInstruction {
    UnlinkedInstruction() { u.operand = 0; }
    UnlinkedInstruction(OpcodeID opcode) { u.opcode = opcode; }
    UnlinkedInstruction(int operand) { u.operand = operand; }
    UnlinkedInstruction(StringImpl* uid) { u.uid = uid; }
    union {
        OpcodeID opcode;
        int32_t operand;
        StringImpl* uid;
    } u;
};

class UnlinkedCodeBlock : public JSCell {
public:
    typedef JSCell Base;
    static const bool needsDestruction = true;
    static const bool hasImmortalStructure = true;

    enum { CallFunction, ApplyFunction };

    bool isConstructor() const { return m_isConstructor; }
    bool isStrictMode() const { return m_isStrictMode; }
    bool usesEval() const { return m_usesEval; }

    bool needsFullScopeChain() const { return m_needsFullScopeChain; }
    void setNeedsFullScopeChain(bool needsFullScopeChain) { m_needsFullScopeChain = needsFullScopeChain; }

    void addExpressionInfo(unsigned instructionOffset, int divot,
        int startOffset, int endOffset, unsigned line, unsigned column);

    bool hasExpressionInfo() { return m_expressionInfo.size(); }

    // Special registers
    void setThisRegister(VirtualRegister thisRegister) { m_thisRegister = thisRegister; }
    void setActivationRegister(VirtualRegister activationRegister) { m_activationRegister = activationRegister; }

    void setArgumentsRegister(VirtualRegister argumentsRegister) { m_argumentsRegister = argumentsRegister; }
    bool usesArguments() const { return m_argumentsRegister.isValid(); }
    VirtualRegister argumentsRegister() const { return m_argumentsRegister; }


    bool usesGlobalObject() const { return m_globalObjectRegister.isValid(); }
    void setGlobalObjectRegister(VirtualRegister globalObjectRegister) { m_globalObjectRegister = globalObjectRegister; }
    VirtualRegister globalObjectRegister() const { return m_globalObjectRegister; }

    // Parameter information
    void setNumParameters(int newValue) { m_numParameters = newValue; }
    void addParameter() { m_numParameters++; }
    unsigned numParameters() const { return m_numParameters; }

    unsigned addRegExp(RegExp* r)
    {
        createRareDataIfNecessary();
        unsigned size = m_rareData->m_regexps.size();
        m_rareData->m_regexps.append(WriteBarrier<RegExp>(*m_vm, this, r));
        return size;
    }
    unsigned numberOfRegExps() const
    {
        if (!m_rareData)
            return 0;
        return m_rareData->m_regexps.size();
    }
    RegExp* regexp(int index) const { ASSERT(m_rareData); return m_rareData->m_regexps[index].get(); }

    // Constant Pools

    size_t numberOfIdentifiers() const { return m_identifiers.size(); }
    void addIdentifier(const Identifier& i) { return m_identifiers.append(i); }
    const Identifier& identifier(int index) const { return m_identifiers[index]; }
    const Vector<Identifier>& identifiers() const { return m_identifiers; }

    size_t numberOfConstantRegisters() const { return m_constantRegisters.size(); }
    unsigned addConstant(JSValue v)
    {
        unsigned result = m_constantRegisters.size();
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantRegisters.last().set(*m_vm, this, v);
        return result;
    }
    unsigned addOrFindConstant(JSValue);
    const Vector<WriteBarrier<Unknown>>& constantRegisters() { return m_constantRegisters; }
    const WriteBarrier<Unknown>& constantRegister(int index) const { return m_constantRegisters[index - FirstConstantRegisterIndex]; }
    ALWAYS_INLINE bool isConstantRegisterIndex(int index) const { return index >= FirstConstantRegisterIndex; }
    ALWAYS_INLINE JSValue getConstant(int index) const { return m_constantRegisters[index - FirstConstantRegisterIndex].get(); }

    // Jumps
    size_t numberOfJumpTargets() const { return m_jumpTargets.size(); }
    void addJumpTarget(unsigned jumpTarget) { m_jumpTargets.append(jumpTarget); }
    unsigned jumpTarget(int index) const { return m_jumpTargets[index]; }
    unsigned lastJumpTarget() const { return m_jumpTargets.last(); }

    void setIsNumericCompareFunction(bool isNumericCompareFunction) { m_isNumericCompareFunction = isNumericCompareFunction; }
    bool isNumericCompareFunction() const { return m_isNumericCompareFunction; }

    void shrinkToFit()
    {
        m_jumpTargets.shrinkToFit();
        m_identifiers.shrinkToFit();
        m_constantRegisters.shrinkToFit();
        m_functionDecls.shrinkToFit();
        m_functionExprs.shrinkToFit();
        m_propertyAccessInstructions.shrinkToFit();
        m_expressionInfo.shrinkToFit();

#if ENABLE(BYTECODE_COMMENTS)
        m_bytecodeComments.shrinkToFit();
#endif
        if (m_rareData) {
            m_rareData->m_exceptionHandlers.shrinkToFit();
            m_rareData->m_regexps.shrinkToFit();
            m_rareData->m_constantBuffers.shrinkToFit();
            m_rareData->m_switchJumpTables.shrinkToFit();
            m_rareData->m_stringSwitchJumpTables.shrinkToFit();
            m_rareData->m_expressionInfoFatPositions.shrinkToFit();
        }
    }

    unsigned numberOfInstructions() const { return m_unlinkedInstructions.size(); }
    RefCountedArray<UnlinkedInstruction>& instructions() { return m_unlinkedInstructions; }
    const RefCountedArray<UnlinkedInstruction>& instructions() const { return m_unlinkedInstructions; }

    int m_numVars;
    int m_numCapturedVars;
    int m_numCalleeRegisters;

    // Jump Tables

    size_t numberOfSwitchJumpTables() const { return m_rareData ? m_rareData->m_switchJumpTables.size() : 0; }
    UnlinkedSimpleJumpTable& addSwitchJumpTable() { createRareDataIfNecessary(); m_rareData->m_switchJumpTables.append(UnlinkedSimpleJumpTable()); return m_rareData->m_switchJumpTables.last(); }
    UnlinkedSimpleJumpTable& switchJumpTable(int tableIndex) { ASSERT(m_rareData); return m_rareData->m_switchJumpTables[tableIndex]; }

    size_t numberOfStringSwitchJumpTables() const { return m_rareData ? m_rareData->m_stringSwitchJumpTables.size() : 0; }
    UnlinkedStringJumpTable& addStringSwitchJumpTable() { createRareDataIfNecessary(); m_rareData->m_stringSwitchJumpTables.append(UnlinkedStringJumpTable()); return m_rareData->m_stringSwitchJumpTables.last(); }
    UnlinkedStringJumpTable& stringSwitchJumpTable(int tableIndex) { ASSERT(m_rareData); return m_rareData->m_stringSwitchJumpTables[tableIndex]; }

    unsigned addFunctionDecl(UnlinkedFunctionExecutable* n)
    {
        unsigned size = m_functionDecls.size();
        m_functionDecls.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_functionDecls.last().set(*m_vm, this, n);
        return size;
    }
    UnlinkedFunctionExecutable* functionDecl(int index) { return m_functionDecls[index].get(); }
    size_t numberOfFunctionDecls() { return m_functionDecls.size(); }
    unsigned addFunctionExpr(UnlinkedFunctionExecutable* n)
    {
        unsigned size = m_functionExprs.size();
        m_functionExprs.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_functionExprs.last().set(*m_vm, this, n);
        return size;
    }
    UnlinkedFunctionExecutable* functionExpr(int index) { return m_functionExprs[index].get(); }
    size_t numberOfFunctionExprs() { return m_functionExprs.size(); }

    // Exception handling support
    size_t numberOfExceptionHandlers() const { return m_rareData ? m_rareData->m_exceptionHandlers.size() : 0; }
    void addExceptionHandler(const UnlinkedHandlerInfo& hanler) { createRareDataIfNecessary(); return m_rareData->m_exceptionHandlers.append(hanler); }
    UnlinkedHandlerInfo& exceptionHandler(int index) { ASSERT(m_rareData); return m_rareData->m_exceptionHandlers[index]; }

    SymbolTable* symbolTable() const { return m_symbolTable.get(); }

    VM* vm() const { return m_vm; }

    UnlinkedArrayProfile addArrayProfile() { return m_arrayProfileCount++; }
    unsigned numberOfArrayProfiles() { return m_arrayProfileCount; }
    UnlinkedArrayAllocationProfile addArrayAllocationProfile() { return m_arrayAllocationProfileCount++; }
    unsigned numberOfArrayAllocationProfiles() { return m_arrayAllocationProfileCount; }
    UnlinkedObjectAllocationProfile addObjectAllocationProfile() { return m_objectAllocationProfileCount++; }
    unsigned numberOfObjectAllocationProfiles() { return m_objectAllocationProfileCount; }
    UnlinkedValueProfile addValueProfile() { return m_valueProfileCount++; }
    unsigned numberOfValueProfiles() { return m_valueProfileCount; }

    UnlinkedLLIntCallLinkInfo addLLIntCallLinkInfo() { return m_llintCallLinkInfoCount++; }
    unsigned numberOfLLintCallLinkInfos() { return m_llintCallLinkInfoCount; }

    CodeType codeType() const { return m_codeType; }

    VirtualRegister thisRegister() const { return m_thisRegister; }
    VirtualRegister activationRegister() const { return m_activationRegister; }


    void addPropertyAccessInstruction(unsigned propertyAccessInstruction)
    {
        m_propertyAccessInstructions.append(propertyAccessInstruction);
    }

    size_t numberOfPropertyAccessInstructions() const { return m_propertyAccessInstructions.size(); }
    const Vector<unsigned>& propertyAccessInstructions() const { return m_propertyAccessInstructions; }

    typedef Vector<JSValue> ConstantBuffer;

    size_t constantBufferCount() { ASSERT(m_rareData); return m_rareData->m_constantBuffers.size(); }
    unsigned addConstantBuffer(unsigned length)
    {
        createRareDataIfNecessary();
        unsigned size = m_rareData->m_constantBuffers.size();
        m_rareData->m_constantBuffers.append(Vector<JSValue>(length));
        return size;
    }

    const ConstantBuffer& constantBuffer(unsigned index) const
    {
        ASSERT(m_rareData);
        return m_rareData->m_constantBuffers[index];
    }

    ConstantBuffer& constantBuffer(unsigned index)
    {
        ASSERT(m_rareData);
        return m_rareData->m_constantBuffers[index];
    }

    bool hasRareData() const { return m_rareData; }

    int lineNumberForBytecodeOffset(unsigned bytecodeOffset);

    void expressionRangeForBytecodeOffset(unsigned bytecodeOffset, int& divot,
        int& startOffset, int& endOffset, unsigned& line, unsigned& column);
    unsigned opDebugBytecodeOffsetForLineAndColumn(unsigned& line, unsigned& column);

    void recordParse(CodeFeatures features, bool hasCapturedVariables, unsigned firstLine, unsigned lineCount, unsigned endColumn)
    {
        m_features = features;
        m_hasCapturedVariables = hasCapturedVariables;
        m_firstLine = firstLine;
        m_lineCount = lineCount;
        // For the UnlinkedCodeBlock, startColumn is always 0.
        m_endColumn = endColumn;
    }

    CodeFeatures codeFeatures() const { return m_features; }
    bool hasCapturedVariables() const { return m_hasCapturedVariables; }
    unsigned firstLine() const { return m_firstLine; }
    unsigned lineCount() const { return m_lineCount; }
    ALWAYS_INLINE unsigned startColumn() const { return 0; }
    unsigned endColumn() const { return m_endColumn; }

    void dumpExpressionRangeInfo(); // For debugging purpose only.
    void dumpOpDebugLineColumnInfoList(); // For debugging purpose only.

protected:
    UnlinkedCodeBlock(VM*, Structure*, CodeType, const ExecutableInfo&);
    ~UnlinkedCodeBlock();

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        if (codeType() == GlobalCode)
            return;
        m_symbolTable.set(vm, this, SymbolTable::create(vm));
    }

private:

    void createRareDataIfNecessary()
    {
        if (!m_rareData)
            m_rareData = adoptPtr(new RareData);
    }

    void getLineAndColumn(ExpressionRangeInfo&, unsigned& line, unsigned& column);

    typedef Vector<LineColumnInfo> LineColumnInfoList;
    LineColumnInfoList& opDebugLineColumnInfoList();

    RefCountedArray<UnlinkedInstruction> m_unlinkedInstructions;

    int m_numParameters;
    VM* m_vm;

    VirtualRegister m_thisRegister;
    VirtualRegister m_argumentsRegister;
    VirtualRegister m_activationRegister;
    VirtualRegister m_globalObjectRegister;

    bool m_needsFullScopeChain : 1;
    bool m_usesEval : 1;
    bool m_isNumericCompareFunction : 1;
    bool m_isStrictMode : 1;
    bool m_isConstructor : 1;
    bool m_hasCapturedVariables : 1;
    unsigned m_firstLine;
    unsigned m_lineCount;
    unsigned m_endColumn;

    CodeFeatures m_features;
    CodeType m_codeType;

    Vector<unsigned> m_jumpTargets;

    // Constant Pools
    Vector<Identifier> m_identifiers;
    Vector<WriteBarrier<Unknown>> m_constantRegisters;
    typedef Vector<WriteBarrier<UnlinkedFunctionExecutable>> FunctionExpressionVector;
    FunctionExpressionVector m_functionDecls;
    FunctionExpressionVector m_functionExprs;

    WriteBarrier<SymbolTable> m_symbolTable;

    Vector<unsigned> m_propertyAccessInstructions;

#if ENABLE(BYTECODE_COMMENTS)
    Vector<Comment>  m_bytecodeComments;
    size_t m_bytecodeCommentIterator;
#endif

    unsigned m_arrayProfileCount;
    unsigned m_arrayAllocationProfileCount;
    unsigned m_objectAllocationProfileCount;
    unsigned m_valueProfileCount;
    unsigned m_llintCallLinkInfoCount;

public:
    struct RareData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Vector<UnlinkedHandlerInfo> m_exceptionHandlers;

        // Rare Constants
        Vector<WriteBarrier<RegExp>> m_regexps;

        // Buffers used for large array literals
        Vector<ConstantBuffer> m_constantBuffers;

        // Jump Tables
        Vector<UnlinkedSimpleJumpTable> m_switchJumpTables;
        Vector<UnlinkedStringJumpTable> m_stringSwitchJumpTables;

        Vector<ExpressionRangeInfo::FatPosition> m_expressionInfoFatPositions;
        std::unique_ptr<LineColumnInfoList> m_opDebugLineColumnInfoList;
    };

private:
    OwnPtr<RareData> m_rareData;
    Vector<ExpressionRangeInfo> m_expressionInfo;

protected:

    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;
    static void visitChildren(JSCell*, SlotVisitor&);

public:
    DECLARE_INFO;
};

class UnlinkedGlobalCodeBlock : public UnlinkedCodeBlock {
public:
    typedef UnlinkedCodeBlock Base;

protected:
    UnlinkedGlobalCodeBlock(VM* vm, Structure* structure, CodeType codeType, const ExecutableInfo& info)
        : Base(vm, structure, codeType, info)
    {
    }

    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;

    DECLARE_INFO;
};

class UnlinkedProgramCodeBlock : public UnlinkedGlobalCodeBlock {
private:
    friend class CodeCache;
    static UnlinkedProgramCodeBlock* create(VM* vm, const ExecutableInfo& info)
    {
        UnlinkedProgramCodeBlock* instance = new (NotNull, allocateCell<UnlinkedProgramCodeBlock>(vm->heap)) UnlinkedProgramCodeBlock(vm, vm->unlinkedProgramCodeBlockStructure.get(), info);
        instance->finishCreation(*vm);
        return instance;
    }

public:
    typedef UnlinkedGlobalCodeBlock Base;
    static void destroy(JSCell*);

    void addFunctionDeclaration(VM& vm, const Identifier& name, UnlinkedFunctionExecutable* functionExecutable)
    {
        m_functionDeclarations.append(std::make_pair(name, WriteBarrier<UnlinkedFunctionExecutable>(vm, this, functionExecutable)));
    }

    void addVariableDeclaration(const Identifier& name, bool isConstant)
    {
        m_varDeclarations.append(std::make_pair(name, isConstant));
    }

    typedef Vector<std::pair<Identifier, bool>> VariableDeclations;
    typedef Vector<std::pair<Identifier, WriteBarrier<UnlinkedFunctionExecutable>> > FunctionDeclations;

    const VariableDeclations& variableDeclarations() const { return m_varDeclarations; }
    const FunctionDeclations& functionDeclarations() const { return m_functionDeclarations; }

    static void visitChildren(JSCell*, SlotVisitor&);

private:
    UnlinkedProgramCodeBlock(VM* vm, Structure* structure, const ExecutableInfo& info)
        : Base(vm, structure, GlobalCode, info)
    {
    }

    VariableDeclations m_varDeclarations;
    FunctionDeclations m_functionDeclarations;

public:
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(UnlinkedProgramCodeBlockType, StructureFlags), info());
    }

    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;

    DECLARE_INFO;
};

class UnlinkedEvalCodeBlock : public UnlinkedGlobalCodeBlock {
private:
    friend class CodeCache;

    static UnlinkedEvalCodeBlock* create(VM* vm, const ExecutableInfo& info)
    {
        UnlinkedEvalCodeBlock* instance = new (NotNull, allocateCell<UnlinkedEvalCodeBlock>(vm->heap)) UnlinkedEvalCodeBlock(vm, vm->unlinkedEvalCodeBlockStructure.get(), info);
        instance->finishCreation(*vm);
        return instance;
    }

public:
    typedef UnlinkedGlobalCodeBlock Base;
    static void destroy(JSCell*);

    const Identifier& variable(unsigned index) { return m_variables[index]; }
    unsigned numVariables() { return m_variables.size(); }
    void adoptVariables(Vector<Identifier, 0, UnsafeVectorOverflow>& variables)
    {
        ASSERT(m_variables.isEmpty());
        m_variables.swap(variables);
    }

private:
    UnlinkedEvalCodeBlock(VM* vm, Structure* structure, const ExecutableInfo& info)
        : Base(vm, structure, EvalCode, info)
    {
    }

    Vector<Identifier, 0, UnsafeVectorOverflow> m_variables;

public:
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(UnlinkedEvalCodeBlockType, StructureFlags), info());
    }

    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;

    DECLARE_INFO;
};

class UnlinkedFunctionCodeBlock : public UnlinkedCodeBlock {
public:
    static UnlinkedFunctionCodeBlock* create(VM* vm, CodeType codeType, const ExecutableInfo& info)
    {
        UnlinkedFunctionCodeBlock* instance = new (NotNull, allocateCell<UnlinkedFunctionCodeBlock>(vm->heap)) UnlinkedFunctionCodeBlock(vm, vm->unlinkedFunctionCodeBlockStructure.get(), codeType, info);
        instance->finishCreation(*vm);
        return instance;
    }

    typedef UnlinkedCodeBlock Base;
    static void destroy(JSCell*);

private:
    UnlinkedFunctionCodeBlock(VM* vm, Structure* structure, CodeType codeType, const ExecutableInfo& info)
        : Base(vm, structure, codeType, info)
    {
    }
    
public:
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(UnlinkedFunctionCodeBlockType, StructureFlags), info());
    }

    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;

    DECLARE_INFO;
};

}

#endif // UnlinkedCodeBlock_h

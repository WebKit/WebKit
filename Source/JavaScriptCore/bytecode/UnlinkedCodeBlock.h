/*
 * Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
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
#include "ConstructAbility.h"
#include "ExpressionRangeInfo.h"
#include "GeneratorThisMode.h"
#include "HandlerInfo.h"
#include "Identifier.h"
#include "JSCell.h"
#include "JSString.h"
#include "ParserModes.h"
#include "RegExp.h"
#include "SpecialPointer.h"
#include "UnlinkedFunctionExecutable.h"
#include "VariableEnvironment.h"
#include "VirtualRegister.h"
#include <wtf/FastBitVector.h>
#include <wtf/RefCountedArray.h>
#include <wtf/Vector.h>

namespace JSC {

class Debugger;
class FunctionMetadataNode;
class FunctionExecutable;
class JSScope;
class ParserError;
class ScriptExecutable;
class SourceCode;
class SourceProvider;
class UnlinkedCodeBlock;
class UnlinkedFunctionCodeBlock;
class UnlinkedFunctionExecutable;
class UnlinkedInstructionStream;
struct ExecutableInfo;

typedef unsigned UnlinkedValueProfile;
typedef unsigned UnlinkedArrayProfile;
typedef unsigned UnlinkedArrayAllocationProfile;
typedef unsigned UnlinkedObjectAllocationProfile;
typedef unsigned UnlinkedLLIntCallLinkInfo;

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

struct UnlinkedInstruction {
    UnlinkedInstruction() { u.operand = 0; }
    UnlinkedInstruction(OpcodeID opcode) { u.opcode = opcode; }
    UnlinkedInstruction(int operand) { u.operand = operand; }
    union {
        OpcodeID opcode;
        int32_t operand;
        unsigned index;
    } u;
};

class UnlinkedCodeBlock : public JSCell {
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags;

    static const bool needsDestruction = true;

    enum { CallFunction, ApplyFunction };

    bool isConstructor() const { return m_isConstructor; }
    bool isStrictMode() const { return m_isStrictMode; }
    bool usesEval() const { return m_usesEval; }
    SourceParseMode parseMode() const { return m_parseMode; }

    bool needsFullScopeChain() const { return m_needsFullScopeChain; }

    void addExpressionInfo(unsigned instructionOffset, int divot,
        int startOffset, int endOffset, unsigned line, unsigned column);

    void addTypeProfilerExpressionInfo(unsigned instructionOffset, unsigned startDivot, unsigned endDivot);

    bool hasExpressionInfo() { return m_expressionInfo.size(); }

    // Special registers
    void setThisRegister(VirtualRegister thisRegister) { m_thisRegister = thisRegister; }
    void setScopeRegister(VirtualRegister scopeRegister) { m_scopeRegister = scopeRegister; }
    void setActivationRegister(VirtualRegister activationRegister) { m_lexicalEnvironmentRegister = activationRegister; }

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

    unsigned addConstant(JSValue v, SourceCodeRepresentation sourceCodeRepresentation = SourceCodeRepresentation::Other)
    {
        unsigned result = m_constantRegisters.size();
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantRegisters.last().set(*m_vm, this, v);
        m_constantsSourceCodeRepresentation.append(sourceCodeRepresentation);
        return result;
    }
    unsigned addConstant(LinkTimeConstant type)
    {
        unsigned result = m_constantRegisters.size();
        ASSERT(result);
        unsigned index = static_cast<unsigned>(type);
        ASSERT(index < LinkTimeConstantCount);
        m_linkTimeConstants[index] = result;
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantsSourceCodeRepresentation.append(SourceCodeRepresentation::Other);
        return result;
    }
    unsigned registerIndexForLinkTimeConstant(LinkTimeConstant type)
    {
        unsigned index = static_cast<unsigned>(type);
        ASSERT(index < LinkTimeConstantCount);
        return m_linkTimeConstants[index];
    }
    const Vector<WriteBarrier<Unknown>>& constantRegisters() { return m_constantRegisters; }
    const WriteBarrier<Unknown>& constantRegister(int index) const { return m_constantRegisters[index - FirstConstantRegisterIndex]; }
    ALWAYS_INLINE bool isConstantRegisterIndex(int index) const { return index >= FirstConstantRegisterIndex; }
    const Vector<SourceCodeRepresentation>& constantsSourceCodeRepresentation() { return m_constantsSourceCodeRepresentation; }

    // Jumps
    size_t numberOfJumpTargets() const { return m_jumpTargets.size(); }
    void addJumpTarget(unsigned jumpTarget) { m_jumpTargets.append(jumpTarget); }
    unsigned jumpTarget(int index) const { return m_jumpTargets[index]; }
    unsigned lastJumpTarget() const { return m_jumpTargets.last(); }

    bool isBuiltinFunction() const { return m_isBuiltinFunction; }

    ConstructorKind constructorKind() const { return static_cast<ConstructorKind>(m_constructorKind); }
    GeneratorThisMode generatorThisMode() const { return static_cast<GeneratorThisMode>(m_generatorThisMode); }
    SuperBinding superBinding() const { return static_cast<SuperBinding>(m_superBinding); }

    void shrinkToFit()
    {
        m_jumpTargets.shrinkToFit();
        m_identifiers.shrinkToFit();
        m_constantRegisters.shrinkToFit();
        m_constantsSourceCodeRepresentation.shrinkToFit();
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

    void setInstructions(std::unique_ptr<UnlinkedInstructionStream>);
    const UnlinkedInstructionStream& instructions() const;

    int m_numVars;
    int m_numCapturedVars;
    int m_numCalleeLocals;

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
    void addExceptionHandler(const UnlinkedHandlerInfo& handler) { createRareDataIfNecessary(); return m_rareData->m_exceptionHandlers.append(handler); }
    UnlinkedHandlerInfo& exceptionHandler(int index) { ASSERT(m_rareData); return m_rareData->m_exceptionHandlers[index]; }

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
    VirtualRegister scopeRegister() const { return m_scopeRegister; }
    VirtualRegister activationRegister() const { return m_lexicalEnvironmentRegister; }
    bool hasActivationRegister() const { return m_lexicalEnvironmentRegister.isValid(); }

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

    bool hasRareData() const { return m_rareData.get(); }

    int lineNumberForBytecodeOffset(unsigned bytecodeOffset);

    void expressionRangeForBytecodeOffset(unsigned bytecodeOffset, int& divot,
        int& startOffset, int& endOffset, unsigned& line, unsigned& column);

    bool typeProfilerExpressionInfoForBytecodeOffset(unsigned bytecodeOffset, unsigned& startDivot, unsigned& endDivot);

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

    void addOpProfileControlFlowBytecodeOffset(size_t offset) { m_opProfileControlFlowBytecodeOffsets.append(offset); }
    const Vector<size_t>& opProfileControlFlowBytecodeOffsets() const { return m_opProfileControlFlowBytecodeOffsets; }

    void dumpExpressionRangeInfo(); // For debugging purpose only.

protected:
    UnlinkedCodeBlock(VM*, Structure*, CodeType, const ExecutableInfo&);
    ~UnlinkedCodeBlock();

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
    }

private:

    void createRareDataIfNecessary()
    {
        if (!m_rareData)
            m_rareData = std::make_unique<RareData>();
    }

    void getLineAndColumn(ExpressionRangeInfo&, unsigned& line, unsigned& column);

    std::unique_ptr<UnlinkedInstructionStream> m_unlinkedInstructions;

    int m_numParameters;
    VM* m_vm;

    VirtualRegister m_thisRegister;
    VirtualRegister m_scopeRegister;
    VirtualRegister m_lexicalEnvironmentRegister;
    VirtualRegister m_globalObjectRegister;

    unsigned m_needsFullScopeChain : 1;
    unsigned m_usesEval : 1;
    unsigned m_isStrictMode : 1;
    unsigned m_isConstructor : 1;
    unsigned m_hasCapturedVariables : 1;
    unsigned m_isBuiltinFunction : 1;
    unsigned m_constructorKind : 2;
    unsigned m_generatorThisMode : 1;
    unsigned m_superBinding : 1;

    unsigned m_firstLine;
    unsigned m_lineCount;
    unsigned m_endColumn;

    SourceParseMode m_parseMode;
    CodeFeatures m_features;
    CodeType m_codeType;

    Vector<unsigned> m_jumpTargets;

    // Constant Pools
    Vector<Identifier> m_identifiers;
    Vector<WriteBarrier<Unknown>> m_constantRegisters;
    Vector<SourceCodeRepresentation> m_constantsSourceCodeRepresentation;
    std::array<unsigned, LinkTimeConstantCount> m_linkTimeConstants;
    typedef Vector<WriteBarrier<UnlinkedFunctionExecutable>> FunctionExpressionVector;
    FunctionExpressionVector m_functionDecls;
    FunctionExpressionVector m_functionExprs;

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
    };

private:
    std::unique_ptr<RareData> m_rareData;
    Vector<ExpressionRangeInfo> m_expressionInfo;
    struct TypeProfilerExpressionRange {
        unsigned m_startDivot;
        unsigned m_endDivot;
    };
    HashMap<unsigned, TypeProfilerExpressionRange> m_typeProfilerInfoMap;
    Vector<size_t> m_opProfileControlFlowBytecodeOffsets;

protected:
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

    DECLARE_INFO;
};

class UnlinkedProgramCodeBlock final : public UnlinkedGlobalCodeBlock {
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
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static void destroy(JSCell*);

    void setVariableDeclarations(const VariableEnvironment& environment) { m_varDeclarations = environment; }
    const VariableEnvironment& variableDeclarations() const { return m_varDeclarations; }

    void setLexicalDeclarations(const VariableEnvironment& environment) { m_lexicalDeclarations = environment; }
    const VariableEnvironment& lexicalDeclarations() const { return m_lexicalDeclarations; }

    static void visitChildren(JSCell*, SlotVisitor&);

private:
    UnlinkedProgramCodeBlock(VM* vm, Structure* structure, const ExecutableInfo& info)
        : Base(vm, structure, GlobalCode, info)
    {
    }

    VariableEnvironment m_varDeclarations;
    VariableEnvironment m_lexicalDeclarations;

public:
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(UnlinkedProgramCodeBlockType, StructureFlags), info());
    }

    DECLARE_INFO;
};

class UnlinkedModuleProgramCodeBlock final : public UnlinkedGlobalCodeBlock {
private:
    friend class CodeCache;
    static UnlinkedModuleProgramCodeBlock* create(VM* vm, const ExecutableInfo& info)
    {
        UnlinkedModuleProgramCodeBlock* instance = new (NotNull, allocateCell<UnlinkedModuleProgramCodeBlock>(vm->heap)) UnlinkedModuleProgramCodeBlock(vm, vm->unlinkedModuleProgramCodeBlockStructure.get(), info);
        instance->finishCreation(*vm);
        return instance;
    }

public:
    typedef UnlinkedGlobalCodeBlock Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static void destroy(JSCell*);

    static void visitChildren(JSCell*, SlotVisitor&);

    // This offset represents the constant register offset to the stored symbol table that represents the layout of the
    // module environment. This symbol table is created by the byte code generator since the module environment includes
    // the top-most lexical captured variables inside the module code. This means that, once the module environment is
    // allocated and instantiated from this symbol table, it is titely coupled with the specific unlinked module program
    // code block and the stored symbol table. So before executing the module code, we should not clear the unlinked module
    // program code block in the module executable. This requirement is met because the garbage collector only clears
    // unlinked code in (1) unmarked executables and (2) function executables.
    //
    // Since the function code may be executed repeatedly and the environment of each function execution is different,
    // the function code need to allocate and instantiate the environment in the prologue of the function code. On the
    // other hand, the module code is executed only once. So we can instantiate the module environment outside the module
    // code. At that time, we construct the module environment by using the symbol table that is held by the module executable.
    // The symbol table held by the executable is the cloned one from one in the unlinked code block. Instantiating the module
    // environment before executing and linking the module code is required to link the imported bindings between the modules.
    //
    // The unlinked module program code block only holds the pre-cloned symbol table in its constant register pool. It does
    // not hold the instantiated module environment. So while the module environment requires the specific unlinked module
    // program code block, the unlinked module code block can be used for the module environment instantiated from this
    // unlinked code block. There is 1:N relation between the unlinked module code block and the module environments. So the
    // unlinked module program code block can be cached.
    //
    // On the other hand, the linked code block for the module environment includes the resolved references to the imported
    // bindings. The imported binding references the other module environment, so the linked code block is titly coupled
    // with the specific set of the module environments. Thus, the linked code block should not be cached.
    int moduleEnvironmentSymbolTableConstantRegisterOffset() { return m_moduleEnvironmentSymbolTableConstantRegisterOffset; }
    void setModuleEnvironmentSymbolTableConstantRegisterOffset(int offset)
    {
        m_moduleEnvironmentSymbolTableConstantRegisterOffset = offset;
    }

private:
    UnlinkedModuleProgramCodeBlock(VM* vm, Structure* structure, const ExecutableInfo& info)
        : Base(vm, structure, ModuleCode, info)
    {
    }

    int m_moduleEnvironmentSymbolTableConstantRegisterOffset { 0 };

public:
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(UnlinkedModuleProgramCodeBlockType, StructureFlags), info());
    }

    DECLARE_INFO;
};

class UnlinkedEvalCodeBlock final : public UnlinkedGlobalCodeBlock {
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
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

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

    DECLARE_INFO;
};

class UnlinkedFunctionCodeBlock final : public UnlinkedCodeBlock {
public:
    typedef UnlinkedCodeBlock Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static UnlinkedFunctionCodeBlock* create(VM* vm, CodeType codeType, const ExecutableInfo& info)
    {
        UnlinkedFunctionCodeBlock* instance = new (NotNull, allocateCell<UnlinkedFunctionCodeBlock>(vm->heap)) UnlinkedFunctionCodeBlock(vm, vm->unlinkedFunctionCodeBlockStructure.get(), codeType, info);
        instance->finishCreation(*vm);
        return instance;
    }

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

    DECLARE_INFO;
};

}

#endif // UnlinkedCodeBlock_h

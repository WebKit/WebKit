/*
 * Copyright (C) 2008, 2009, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "BytecodeGenerator.h"

#include "Interpreter.h"
#include "JSActivation.h"
#include "JSFunction.h"
#include "JSNameScope.h"
#include "LowLevelInterpreter.h"
#include "Operations.h"
#include "Options.h"
#include "StackAlignment.h"
#include "StrongInlines.h"
#include "UnlinkedCodeBlock.h"
#include "UnlinkedInstructionStream.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

using namespace std;

namespace JSC {

void Label::setLocation(unsigned location)
{
    m_location = location;
    
    unsigned size = m_unresolvedJumps.size();
    for (unsigned i = 0; i < size; ++i)
        m_generator->m_instructions[m_unresolvedJumps[i].second].u.operand = m_location - m_unresolvedJumps[i].first;
}

ParserError BytecodeGenerator::generate()
{
    SamplingRegion samplingRegion("Bytecode Generation");
    
    m_codeBlock->setThisRegister(m_thisRegister.virtualRegister());

    m_scopeNode->emitBytecode(*this);

    m_staticPropertyAnalyzer.kill();

    for (unsigned i = 0; i < m_tryRanges.size(); ++i) {
        TryRange& range = m_tryRanges[i];
        int start = range.start->bind();
        int end = range.end->bind();
        
        // This will happen for empty try blocks and for some cases of finally blocks:
        //
        // try {
        //    try {
        //    } finally {
        //        return 42;
        //        // *HERE*
        //    }
        // } finally {
        //    print("things");
        // }
        //
        // The return will pop scopes to execute the outer finally block. But this includes
        // popping the try context for the inner try. The try context is live in the fall-through
        // part of the finally block not because we will emit a handler that overlaps the finally,
        // but because we haven't yet had a chance to plant the catch target. Then when we finish
        // emitting code for the outer finally block, we repush the try contex, this time with a
        // new start index. But that means that the start index for the try range corresponding
        // to the inner-finally-following-the-return (marked as "*HERE*" above) will be greater
        // than the end index of the try block. This is harmless since end < start handlers will
        // never get matched in our logic, but we do the runtime a favor and choose to not emit
        // such handlers at all.
        if (end <= start)
            continue;
        
        ASSERT(range.tryData->targetScopeDepth != UINT_MAX);
        UnlinkedHandlerInfo info = {
            static_cast<uint32_t>(start), static_cast<uint32_t>(end),
            static_cast<uint32_t>(range.tryData->target->bind()),
            range.tryData->targetScopeDepth
        };
        m_codeBlock->addExceptionHandler(info);
    }
    
    m_codeBlock->setInstructions(std::make_unique<UnlinkedInstructionStream>(m_instructions));

    m_codeBlock->shrinkToFit();

    if (m_expressionTooDeep)
        return ParserError(ParserError::OutOfMemory);
    return ParserError(ParserError::ErrorNone);
}

bool BytecodeGenerator::addVar(
    const Identifier& ident, ConstantMode constantMode, WatchMode watchMode, RegisterID*& r0)
{
    ASSERT(static_cast<size_t>(m_codeBlock->m_numVars) == m_calleeRegisters.size());
    
    ConcurrentJITLocker locker(symbolTable().m_lock);
    int index = virtualRegisterForLocal(m_calleeRegisters.size()).offset();
    SymbolTableEntry newEntry(index, constantMode == IsConstant ? ReadOnly : 0);
    SymbolTable::Map::AddResult result = symbolTable().add(locker, ident.impl(), newEntry);

    if (!result.isNewEntry) {
        r0 = &registerFor(result.iterator->value.getIndex());
        return false;
    }
    
    if (watchMode == IsWatchable) {
        while (m_watchableVariables.size() < static_cast<size_t>(m_codeBlock->m_numVars))
            m_watchableVariables.append(Identifier());
        m_watchableVariables.append(ident);
    }
    
    r0 = addVar();
    
    ASSERT(watchMode == NotWatchable || static_cast<size_t>(m_codeBlock->m_numVars) == m_watchableVariables.size());
    
    return true;
}

void BytecodeGenerator::preserveLastVar()
{
    if ((m_firstConstantIndex = m_calleeRegisters.size()) != 0)
        m_lastVar = &m_calleeRegisters.last();
}

BytecodeGenerator::BytecodeGenerator(VM& vm, ProgramNode* programNode, UnlinkedProgramCodeBlock* codeBlock, DebuggerMode debuggerMode, ProfilerMode profilerMode)
    : m_shouldEmitDebugHooks(Options::forceDebuggerBytecodeGeneration() || debuggerMode == DebuggerOn)
    , m_shouldEmitProfileHooks(Options::forceProfilerBytecodeGeneration() || profilerMode == ProfilerOn)
    , m_symbolTable(0)
    , m_scopeNode(programNode)
    , m_codeBlock(vm, codeBlock)
    , m_thisRegister(CallFrame::thisArgumentOffset())
    , m_activationRegister(0)
    , m_emptyValueRegister(0)
    , m_globalObjectRegister(0)
    , m_finallyDepth(0)
    , m_localScopeDepth(0)
    , m_codeType(GlobalCode)
    , m_nextConstantOffset(0)
    , m_globalConstantIndex(0)
    , m_firstLazyFunction(0)
    , m_lastLazyFunction(0)
    , m_staticPropertyAnalyzer(&m_instructions)
    , m_vm(&vm)
    , m_lastOpcodeID(op_end)
#ifndef NDEBUG
    , m_lastOpcodePosition(0)
#endif
    , m_usesExceptions(false)
    , m_expressionTooDeep(false)
    , m_isBuiltinFunction(false)
{
    m_codeBlock->setNumParameters(1); // Allocate space for "this"

    emitOpcode(op_enter);

    const VarStack& varStack = programNode->varStack();
    const FunctionStack& functionStack = programNode->functionStack();

    for (size_t i = 0; i < functionStack.size(); ++i) {
        FunctionBodyNode* function = functionStack[i];
        UnlinkedFunctionExecutable* unlinkedFunction = makeFunction(function);
        codeBlock->addFunctionDeclaration(*m_vm, function->ident(), unlinkedFunction);
    }

    for (size_t i = 0; i < varStack.size(); ++i)
        codeBlock->addVariableDeclaration(varStack[i].first, !!(varStack[i].second & DeclarationStacks::IsConstant));

}

BytecodeGenerator::BytecodeGenerator(VM& vm, FunctionBodyNode* functionBody, UnlinkedFunctionCodeBlock* codeBlock, DebuggerMode debuggerMode, ProfilerMode profilerMode)
    : m_shouldEmitDebugHooks(Options::forceDebuggerBytecodeGeneration() || debuggerMode == DebuggerOn)
    , m_shouldEmitProfileHooks(Options::forceProfilerBytecodeGeneration() || profilerMode == ProfilerOn)
    , m_symbolTable(codeBlock->symbolTable())
    , m_scopeNode(functionBody)
    , m_codeBlock(vm, codeBlock)
    , m_activationRegister(0)
    , m_emptyValueRegister(0)
    , m_globalObjectRegister(0)
    , m_finallyDepth(0)
    , m_localScopeDepth(0)
    , m_codeType(FunctionCode)
    , m_nextConstantOffset(0)
    , m_globalConstantIndex(0)
    , m_firstLazyFunction(0)
    , m_lastLazyFunction(0)
    , m_staticPropertyAnalyzer(&m_instructions)
    , m_vm(&vm)
    , m_lastOpcodeID(op_end)
#ifndef NDEBUG
    , m_lastOpcodePosition(0)
#endif
    , m_usesExceptions(false)
    , m_expressionTooDeep(false)
    , m_isBuiltinFunction(codeBlock->isBuiltinFunction())
{
    if (m_isBuiltinFunction) {
        m_shouldEmitProfileHooks = false;
        m_shouldEmitDebugHooks = false;
    }

    m_symbolTable->setUsesNonStrictEval(codeBlock->usesEval() && !codeBlock->isStrictMode());
    Vector<Identifier> boundParameterProperties;
    FunctionParameters& parameters = *functionBody->parameters();
    for (size_t i = 0; i < parameters.size(); i++) {
        auto pattern = parameters.at(i);
        if (pattern->isBindingNode())
            continue;
        pattern->collectBoundIdentifiers(boundParameterProperties);
        continue;
    }
    m_symbolTable->setParameterCountIncludingThis(functionBody->parameters()->size() + 1);

    emitOpcode(op_enter);
    if (m_codeBlock->needsFullScopeChain() || m_shouldEmitDebugHooks) {
        m_activationRegister = addVar();
        emitInitLazyRegister(m_activationRegister);
        m_codeBlock->setActivationRegister(m_activationRegister->virtualRegister());
    }

    m_symbolTable->setCaptureStart(virtualRegisterForLocal(m_codeBlock->m_numVars).offset());

    if (functionBody->usesArguments() || codeBlock->usesEval()) { // May reify arguments object.
        RegisterID* unmodifiedArgumentsRegister = addVar(); // Anonymous, so it can't be modified by user code.
        RegisterID* argumentsRegister = addVar(propertyNames().arguments, IsVariable, NotWatchable); // Can be changed by assigning to 'arguments'.

        // We can save a little space by hard-coding the knowledge that the two
        // 'arguments' values are stored in consecutive registers, and storing
        // only the index of the assignable one.
        codeBlock->setArgumentsRegister(argumentsRegister->virtualRegister());
        ASSERT_UNUSED(unmodifiedArgumentsRegister, unmodifiedArgumentsRegister->virtualRegister() == JSC::unmodifiedArgumentsRegister(codeBlock->argumentsRegister()));

        emitInitLazyRegister(argumentsRegister);
        emitInitLazyRegister(unmodifiedArgumentsRegister);
        
        if (shouldTearOffArgumentsEagerly()) {
            emitOpcode(op_create_arguments);
            instructions().append(argumentsRegister->index());
        }
    }

    bool shouldCaptureAllTheThings = m_shouldEmitDebugHooks || codeBlock->usesEval();

    bool capturesAnyArgumentByName = false;
    Vector<RegisterID*, 0, UnsafeVectorOverflow> capturedArguments;
    if (functionBody->hasCapturedVariables() || shouldCaptureAllTheThings) {
        FunctionParameters& parameters = *functionBody->parameters();
        capturedArguments.resize(parameters.size());
        for (size_t i = 0; i < parameters.size(); ++i) {
            capturedArguments[i] = 0;
            auto pattern = parameters.at(i);
            if (!pattern->isBindingNode())
                continue;
            const Identifier& ident = static_cast<const BindingNode*>(pattern)->boundProperty();
            if (!functionBody->captures(ident) && !shouldCaptureAllTheThings)
                continue;
            capturesAnyArgumentByName = true;
            capturedArguments[i] = addVar();
        }
    }

    if (capturesAnyArgumentByName && !shouldTearOffArgumentsEagerly()) {
        size_t parameterCount = m_symbolTable->parameterCount();
        auto slowArguments = std::make_unique<SlowArgument[]>(parameterCount);
        for (size_t i = 0; i < parameterCount; ++i) {
            if (!capturedArguments[i]) {
                ASSERT(slowArguments[i].status == SlowArgument::Normal);
                slowArguments[i].index = CallFrame::argumentOffset(i);
                continue;
            }
            slowArguments[i].status = SlowArgument::Captured;
            slowArguments[i].index = capturedArguments[i]->index();
        }
        m_symbolTable->setSlowArguments(std::move(slowArguments));
    }

    RegisterID* calleeRegister = resolveCallee(functionBody); // May push to the scope chain and/or add a captured var.

    const DeclarationStacks::FunctionStack& functionStack = functionBody->functionStack();
    const DeclarationStacks::VarStack& varStack = functionBody->varStack();

    // Captured variables and functions go first so that activations don't have
    // to step over the non-captured locals to mark them.
    if (functionBody->hasCapturedVariables()) {
        for (size_t i = 0; i < functionStack.size(); ++i) {
            FunctionBodyNode* function = functionStack[i];
            const Identifier& ident = function->ident();
            if (functionBody->captures(ident)) {
                m_functions.add(ident.impl());
                emitNewFunction(addVar(ident, IsVariable, IsWatchable), IsCaptured, function);
            }
        }
        for (size_t i = 0; i < varStack.size(); ++i) {
            const Identifier& ident = varStack[i].first;
            if (functionBody->captures(ident))
                addVar(ident, (varStack[i].second & DeclarationStacks::IsConstant) ? IsConstant : IsVariable, IsWatchable);
        }
    }

    m_symbolTable->setCaptureEnd(virtualRegisterForLocal(codeBlock->m_numVars).offset());

    bool canLazilyCreateFunctions = !functionBody->needsActivationForMoreThanVariables() && !m_shouldEmitDebugHooks;
    m_firstLazyFunction = codeBlock->m_numVars;
    for (size_t i = 0; i < functionStack.size(); ++i) {
        FunctionBodyNode* function = functionStack[i];
        const Identifier& ident = function->ident();
        if (!functionBody->captures(ident)) {
            m_functions.add(ident.impl());
            RefPtr<RegisterID> reg = addVar(ident, IsVariable, NotWatchable);
            // Don't lazily create functions that override the name 'arguments'
            // as this would complicate lazy instantiation of actual arguments.
            if (!canLazilyCreateFunctions || ident == propertyNames().arguments)
                emitNewFunction(reg.get(), NotCaptured, function);
            else {
                emitInitLazyRegister(reg.get());
                m_lazyFunctions.set(reg->virtualRegister().toLocal(), function);
            }
        }
    }
    m_lastLazyFunction = canLazilyCreateFunctions ? codeBlock->m_numVars : m_firstLazyFunction;
    for (size_t i = 0; i < varStack.size(); ++i) {
        const Identifier& ident = varStack[i].first;
        if (!functionBody->captures(ident))
            addVar(ident, (varStack[i].second & DeclarationStacks::IsConstant) ? IsConstant : IsVariable, NotWatchable);
    }

    if (shouldCaptureAllTheThings)
        m_symbolTable->setCaptureEnd(virtualRegisterForLocal(codeBlock->m_numVars).offset());

    if (m_symbolTable->captureCount())
        emitOpcode(op_touch_entry);
    
    m_parameters.grow(parameters.size() + 1); // reserve space for "this"

    // Add "this" as a parameter
    int nextParameterIndex = CallFrame::thisArgumentOffset();
    m_thisRegister.setIndex(nextParameterIndex++);
    m_codeBlock->addParameter();
    Vector<std::pair<RegisterID*, const DeconstructionPatternNode*>> deconstructedParameters;
    for (size_t i = 0; i < parameters.size(); ++i, ++nextParameterIndex) {
        int index = nextParameterIndex;
        auto pattern = parameters.at(i);
        if (!pattern->isBindingNode()) {
            m_codeBlock->addParameter();
            RegisterID& parameter = registerFor(index);
            parameter.setIndex(index);
            deconstructedParameters.append(std::make_pair(&parameter, pattern));
            continue;
        }
        auto simpleParameter = static_cast<const BindingNode*>(pattern);
        if (capturedArguments.size() && capturedArguments[i]) {
            ASSERT((functionBody->hasCapturedVariables() && functionBody->captures(simpleParameter->boundProperty())) || shouldCaptureAllTheThings);
            index = capturedArguments[i]->index();
            RegisterID original(nextParameterIndex);
            emitMove(capturedArguments[i], &original);
        }
        addParameter(simpleParameter->boundProperty(), index);
    }
    preserveLastVar();

    // We declare the callee's name last because it should lose to a var, function, and/or parameter declaration.
    addCallee(functionBody, calleeRegister);

    if (isConstructor()) {
        emitCreateThis(&m_thisRegister);
    } else if (functionBody->usesThis() || codeBlock->usesEval() || m_shouldEmitDebugHooks) {
        m_codeBlock->addPropertyAccessInstruction(instructions().size());
        emitOpcode(op_to_this);
        instructions().append(kill(&m_thisRegister));
        instructions().append(0);
    }
    for (size_t i = 0; i < deconstructedParameters.size(); i++) {
        auto& entry = deconstructedParameters[i];
        entry.second->bindValue(*this, entry.first);
    }
}

BytecodeGenerator::BytecodeGenerator(VM& vm, EvalNode* evalNode, UnlinkedEvalCodeBlock* codeBlock, DebuggerMode debuggerMode, ProfilerMode profilerMode)
    : m_shouldEmitDebugHooks(Options::forceDebuggerBytecodeGeneration() || debuggerMode == DebuggerOn)
    , m_shouldEmitProfileHooks(Options::forceProfilerBytecodeGeneration() || profilerMode == ProfilerOn)
    , m_symbolTable(codeBlock->symbolTable())
    , m_scopeNode(evalNode)
    , m_codeBlock(vm, codeBlock)
    , m_thisRegister(CallFrame::thisArgumentOffset())
    , m_activationRegister(0)
    , m_emptyValueRegister(0)
    , m_globalObjectRegister(0)
    , m_finallyDepth(0)
    , m_localScopeDepth(0)
    , m_codeType(EvalCode)
    , m_nextConstantOffset(0)
    , m_globalConstantIndex(0)
    , m_firstLazyFunction(0)
    , m_lastLazyFunction(0)
    , m_staticPropertyAnalyzer(&m_instructions)
    , m_vm(&vm)
    , m_lastOpcodeID(op_end)
#ifndef NDEBUG
    , m_lastOpcodePosition(0)
#endif
    , m_usesExceptions(false)
    , m_expressionTooDeep(false)
    , m_isBuiltinFunction(false)
{
    m_symbolTable->setUsesNonStrictEval(codeBlock->usesEval() && !codeBlock->isStrictMode());
    m_codeBlock->setNumParameters(1);

    emitOpcode(op_enter);

    const DeclarationStacks::FunctionStack& functionStack = evalNode->functionStack();
    for (size_t i = 0; i < functionStack.size(); ++i)
        m_codeBlock->addFunctionDecl(makeFunction(functionStack[i]));

    const DeclarationStacks::VarStack& varStack = evalNode->varStack();
    unsigned numVariables = varStack.size();
    Vector<Identifier, 0, UnsafeVectorOverflow> variables;
    variables.reserveCapacity(numVariables);
    for (size_t i = 0; i < numVariables; ++i) {
        ASSERT(varStack[i].first.impl()->isIdentifier());
        variables.append(varStack[i].first);
    }
    codeBlock->adoptVariables(variables);
    preserveLastVar();
}

BytecodeGenerator::~BytecodeGenerator()
{
}

RegisterID* BytecodeGenerator::emitInitLazyRegister(RegisterID* reg)
{
    emitOpcode(op_init_lazy_reg);
    instructions().append(reg->index());
    ASSERT(!hasWatchableVariable(reg->index()));
    return reg;
}

RegisterID* BytecodeGenerator::resolveCallee(FunctionBodyNode* functionBodyNode)
{
    if (functionBodyNode->ident().isNull() || !functionBodyNode->functionNameIsInScope())
        return 0;

    m_calleeRegister.setIndex(JSStack::Callee);

    // If non-strict eval is in play, we use a separate object in the scope chain for the callee's name.
    if (m_codeBlock->usesEval() && !m_codeBlock->isStrictMode())
        emitPushFunctionNameScope(functionBodyNode->ident(), &m_calleeRegister, ReadOnly | DontDelete);

    if (!functionBodyNode->captures(functionBodyNode->ident()))
        return &m_calleeRegister;

    // Move the callee into the captured section of the stack.
    return emitMove(addVar(), IsCaptured, &m_calleeRegister);
}

void BytecodeGenerator::addCallee(FunctionBodyNode* functionBodyNode, RegisterID* calleeRegister)
{
    if (functionBodyNode->ident().isNull() || !functionBodyNode->functionNameIsInScope())
        return;

    // If non-strict eval is in play, we use a separate object in the scope chain for the callee's name.
    if (m_codeBlock->usesEval() && !m_codeBlock->isStrictMode())
        return;

    ASSERT(calleeRegister);
    symbolTable().add(functionBodyNode->ident().impl(), SymbolTableEntry(calleeRegister->index(), ReadOnly));
}

void BytecodeGenerator::addParameter(const Identifier& ident, int parameterIndex)
{
    // Parameters overwrite var declarations, but not function declarations.
    StringImpl* rep = ident.impl();
    if (!m_functions.contains(rep)) {
        symbolTable().set(rep, parameterIndex);
        RegisterID& parameter = registerFor(parameterIndex);
        parameter.setIndex(parameterIndex);
    }

    // To maintain the calling convention, we have to allocate unique space for
    // each parameter, even if the parameter doesn't make it into the symbol table.
    m_codeBlock->addParameter();
}

bool BytecodeGenerator::willResolveToArguments(const Identifier& ident)
{
    if (ident != propertyNames().arguments)
        return false;
    
    if (!shouldOptimizeLocals())
        return false;
    
    SymbolTableEntry entry = symbolTable().get(ident.impl());
    if (entry.isNull())
        return false;

    if (m_codeBlock->usesArguments() && m_codeType == FunctionCode)
        return true;
    
    return false;
}

RegisterID* BytecodeGenerator::uncheckedRegisterForArguments()
{
    ASSERT(willResolveToArguments(propertyNames().arguments));

    SymbolTableEntry entry = symbolTable().get(propertyNames().arguments.impl());
    ASSERT(!entry.isNull());
    return &registerFor(entry.getIndex());
}

RegisterID* BytecodeGenerator::createLazyRegisterIfNecessary(RegisterID* reg)
{
    if (!reg->virtualRegister().isLocal())
        return reg;

    int localVariableNumber = reg->virtualRegister().toLocal();

    if (m_lastLazyFunction <= localVariableNumber || localVariableNumber < m_firstLazyFunction)
        return reg;
    emitLazyNewFunction(reg, m_lazyFunctions.get(localVariableNumber));
    return reg;
}

RegisterID* BytecodeGenerator::newRegister()
{
    m_calleeRegisters.append(virtualRegisterForLocal(m_calleeRegisters.size()));
    int numCalleeRegisters = max<int>(m_codeBlock->m_numCalleeRegisters, m_calleeRegisters.size());
    numCalleeRegisters = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), numCalleeRegisters);
    m_codeBlock->m_numCalleeRegisters = numCalleeRegisters;
    return &m_calleeRegisters.last();
}

RegisterID* BytecodeGenerator::newTemporary()
{
    // Reclaim free register IDs.
    while (m_calleeRegisters.size() && !m_calleeRegisters.last().refCount())
        m_calleeRegisters.removeLast();
        
    RegisterID* result = newRegister();
    result->setTemporary();
    return result;
}

LabelScopePtr BytecodeGenerator::newLabelScope(LabelScope::Type type, const Identifier* name)
{
    // Reclaim free label scopes.
    while (m_labelScopes.size() && !m_labelScopes.last().refCount())
        m_labelScopes.removeLast();

    // Allocate new label scope.
    LabelScope scope(type, name, scopeDepth(), newLabel(), type == LabelScope::Loop ? newLabel() : PassRefPtr<Label>()); // Only loops have continue targets.
    m_labelScopes.append(scope);
    return LabelScopePtr(&m_labelScopes, m_labelScopes.size() - 1);
}

PassRefPtr<Label> BytecodeGenerator::newLabel()
{
    // Reclaim free label IDs.
    while (m_labels.size() && !m_labels.last().refCount())
        m_labels.removeLast();

    // Allocate new label ID.
    m_labels.append(this);
    return &m_labels.last();
}

PassRefPtr<Label> BytecodeGenerator::emitLabel(Label* l0)
{
    unsigned newLabelIndex = instructions().size();
    l0->setLocation(newLabelIndex);

    if (m_codeBlock->numberOfJumpTargets()) {
        unsigned lastLabelIndex = m_codeBlock->lastJumpTarget();
        ASSERT(lastLabelIndex <= newLabelIndex);
        if (newLabelIndex == lastLabelIndex) {
            // Peephole optimizations have already been disabled by emitting the last label
            return l0;
        }
    }

    m_codeBlock->addJumpTarget(newLabelIndex);

    // This disables peephole optimizations when an instruction is a jump target
    m_lastOpcodeID = op_end;
    return l0;
}

void BytecodeGenerator::emitOpcode(OpcodeID opcodeID)
{
#ifndef NDEBUG
    size_t opcodePosition = instructions().size();
    ASSERT(opcodePosition - m_lastOpcodePosition == opcodeLength(m_lastOpcodeID) || m_lastOpcodeID == op_end);
    m_lastOpcodePosition = opcodePosition;
#endif
    instructions().append(opcodeID);
    m_lastOpcodeID = opcodeID;
}

UnlinkedArrayProfile BytecodeGenerator::newArrayProfile()
{
    return m_codeBlock->addArrayProfile();
}

UnlinkedArrayAllocationProfile BytecodeGenerator::newArrayAllocationProfile()
{
    return m_codeBlock->addArrayAllocationProfile();
}

UnlinkedObjectAllocationProfile BytecodeGenerator::newObjectAllocationProfile()
{
    return m_codeBlock->addObjectAllocationProfile();
}

UnlinkedValueProfile BytecodeGenerator::emitProfiledOpcode(OpcodeID opcodeID)
{
    UnlinkedValueProfile result = m_codeBlock->addValueProfile();
    emitOpcode(opcodeID);
    return result;
}

void BytecodeGenerator::emitLoopHint()
{
    emitOpcode(op_loop_hint);
}

void BytecodeGenerator::retrieveLastBinaryOp(int& dstIndex, int& src1Index, int& src2Index)
{
    ASSERT(instructions().size() >= 4);
    size_t size = instructions().size();
    dstIndex = instructions().at(size - 3).u.operand;
    src1Index = instructions().at(size - 2).u.operand;
    src2Index = instructions().at(size - 1).u.operand;
}

void BytecodeGenerator::retrieveLastUnaryOp(int& dstIndex, int& srcIndex)
{
    ASSERT(instructions().size() >= 3);
    size_t size = instructions().size();
    dstIndex = instructions().at(size - 2).u.operand;
    srcIndex = instructions().at(size - 1).u.operand;
}

void ALWAYS_INLINE BytecodeGenerator::rewindBinaryOp()
{
    ASSERT(instructions().size() >= 4);
    instructions().shrink(instructions().size() - 4);
    m_lastOpcodeID = op_end;
}

void ALWAYS_INLINE BytecodeGenerator::rewindUnaryOp()
{
    ASSERT(instructions().size() >= 3);
    instructions().shrink(instructions().size() - 3);
    m_lastOpcodeID = op_end;
}

PassRefPtr<Label> BytecodeGenerator::emitJump(Label* target)
{
    size_t begin = instructions().size();
    emitOpcode(op_jmp);
    instructions().append(target->bind(begin, instructions().size()));
    return target;
}

PassRefPtr<Label> BytecodeGenerator::emitJumpIfTrue(RegisterID* cond, Label* target)
{
    if (m_lastOpcodeID == op_less) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jless);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_lesseq) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jlesseq);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_greater) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jgreater);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_greatereq) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jgreatereq);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_eq_null && target->isForward()) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jeq_null);
            instructions().append(srcIndex);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_neq_null && target->isForward()) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jneq_null);
            instructions().append(srcIndex);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    }

    size_t begin = instructions().size();

    emitOpcode(op_jtrue);
    instructions().append(cond->index());
    instructions().append(target->bind(begin, instructions().size()));
    return target;
}

PassRefPtr<Label> BytecodeGenerator::emitJumpIfFalse(RegisterID* cond, Label* target)
{
    if (m_lastOpcodeID == op_less && target->isForward()) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jnless);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_lesseq && target->isForward()) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jnlesseq);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_greater && target->isForward()) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jngreater);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_greatereq && target->isForward()) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jngreatereq);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_not) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jtrue);
            instructions().append(srcIndex);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_eq_null && target->isForward()) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jneq_null);
            instructions().append(srcIndex);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_neq_null && target->isForward()) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();

            size_t begin = instructions().size();
            emitOpcode(op_jeq_null);
            instructions().append(srcIndex);
            instructions().append(target->bind(begin, instructions().size()));
            return target;
        }
    }

    size_t begin = instructions().size();
    emitOpcode(op_jfalse);
    instructions().append(cond->index());
    instructions().append(target->bind(begin, instructions().size()));
    return target;
}

PassRefPtr<Label> BytecodeGenerator::emitJumpIfNotFunctionCall(RegisterID* cond, Label* target)
{
    size_t begin = instructions().size();

    emitOpcode(op_jneq_ptr);
    instructions().append(cond->index());
    instructions().append(Special::CallFunction);
    instructions().append(target->bind(begin, instructions().size()));
    return target;
}

PassRefPtr<Label> BytecodeGenerator::emitJumpIfNotFunctionApply(RegisterID* cond, Label* target)
{
    size_t begin = instructions().size();

    emitOpcode(op_jneq_ptr);
    instructions().append(cond->index());
    instructions().append(Special::ApplyFunction);
    instructions().append(target->bind(begin, instructions().size()));
    return target;
}

unsigned BytecodeGenerator::addConstant(const Identifier& ident)
{
    StringImpl* rep = ident.impl();
    IdentifierMap::AddResult result = m_identifierMap.add(rep, m_codeBlock->numberOfIdentifiers());
    if (result.isNewEntry)
        m_codeBlock->addIdentifier(ident);

    return result.iterator->value;
}

// We can't hash JSValue(), so we use a dedicated data member to cache it.
RegisterID* BytecodeGenerator::addConstantEmptyValue()
{
    if (!m_emptyValueRegister) {
        int index = m_nextConstantOffset;
        m_constantPoolRegisters.append(FirstConstantRegisterIndex + m_nextConstantOffset);
        ++m_nextConstantOffset;
        m_codeBlock->addConstant(JSValue());
        m_emptyValueRegister = &m_constantPoolRegisters[index];
    }

    return m_emptyValueRegister;
}

RegisterID* BytecodeGenerator::addConstantValue(JSValue v)
{
    if (!v)
        return addConstantEmptyValue();

    int index = m_nextConstantOffset;
    JSValueMap::AddResult result = m_jsValueMap.add(JSValue::encode(v), m_nextConstantOffset);
    if (result.isNewEntry) {
        m_constantPoolRegisters.append(FirstConstantRegisterIndex + m_nextConstantOffset);
        ++m_nextConstantOffset;
        m_codeBlock->addConstant(v);
    } else
        index = result.iterator->value;
    return &m_constantPoolRegisters[index];
}

unsigned BytecodeGenerator::addRegExp(RegExp* r)
{
    return m_codeBlock->addRegExp(r);
}

RegisterID* BytecodeGenerator::emitMove(RegisterID* dst, CaptureMode captureMode, RegisterID* src)
{
    m_staticPropertyAnalyzer.mov(dst->index(), src->index());

    emitOpcode(captureMode == IsCaptured ? op_captured_mov : op_mov);
    instructions().append(dst->index());
    instructions().append(src->index());
    if (captureMode == IsCaptured)
        instructions().append(watchableVariable(dst->index()));
    return dst;
}

RegisterID* BytecodeGenerator::emitMove(RegisterID* dst, RegisterID* src)
{
    return emitMove(dst, captureMode(dst->index()), src);
}

RegisterID* BytecodeGenerator::emitUnaryOp(OpcodeID opcodeID, RegisterID* dst, RegisterID* src)
{
    emitOpcode(opcodeID);
    instructions().append(dst->index());
    instructions().append(src->index());
    return dst;
}

RegisterID* BytecodeGenerator::emitInc(RegisterID* srcDst)
{
    emitOpcode(op_inc);
    instructions().append(srcDst->index());
    return srcDst;
}

RegisterID* BytecodeGenerator::emitDec(RegisterID* srcDst)
{
    emitOpcode(op_dec);
    instructions().append(srcDst->index());
    return srcDst;
}

RegisterID* BytecodeGenerator::emitBinaryOp(OpcodeID opcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2, OperandTypes types)
{
    emitOpcode(opcodeID);
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());

    if (opcodeID == op_bitor || opcodeID == op_bitand || opcodeID == op_bitxor ||
        opcodeID == op_add || opcodeID == op_mul || opcodeID == op_sub || opcodeID == op_div)
        instructions().append(types.toInt());

    return dst;
}

RegisterID* BytecodeGenerator::emitEqualityOp(OpcodeID opcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    if (m_lastOpcodeID == op_typeof) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (src1->index() == dstIndex
            && src1->isTemporary()
            && m_codeBlock->isConstantRegisterIndex(src2->index())
            && m_codeBlock->constantRegister(src2->index()).get().isString()) {
            const String& value = asString(m_codeBlock->constantRegister(src2->index()).get())->tryGetValue();
            if (value == "undefined") {
                rewindUnaryOp();
                emitOpcode(op_is_undefined);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "boolean") {
                rewindUnaryOp();
                emitOpcode(op_is_boolean);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "number") {
                rewindUnaryOp();
                emitOpcode(op_is_number);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "string") {
                rewindUnaryOp();
                emitOpcode(op_is_string);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "object") {
                rewindUnaryOp();
                emitOpcode(op_is_object);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "function") {
                rewindUnaryOp();
                emitOpcode(op_is_function);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
        }
    }

    emitOpcode(opcodeID);
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* BytecodeGenerator::emitLoad(RegisterID* dst, bool b)
{
    return emitLoad(dst, jsBoolean(b));
}

RegisterID* BytecodeGenerator::emitLoad(RegisterID* dst, double number)
{
    // FIXME: Our hash tables won't hold infinity, so we make a new JSValue each time.
    // Later we can do the extra work to handle that like the other cases.  They also don't
    // work correctly with NaN as a key.
    if (std::isnan(number) || number == HashTraits<double>::emptyValue() || HashTraits<double>::isDeletedValue(number))
        return emitLoad(dst, jsNumber(number));
    JSValue& valueInMap = m_numberMap.add(number, JSValue()).iterator->value;
    if (!valueInMap)
        valueInMap = jsNumber(number);
    return emitLoad(dst, valueInMap);
}

RegisterID* BytecodeGenerator::emitLoad(RegisterID* dst, const Identifier& identifier)
{
    JSString*& stringInMap = m_stringMap.add(identifier.impl(), nullptr).iterator->value;
    if (!stringInMap)
        stringInMap = jsOwnedString(vm(), identifier.string());
    return emitLoad(dst, JSValue(stringInMap));
}

RegisterID* BytecodeGenerator::emitLoad(RegisterID* dst, JSValue v)
{
    RegisterID* constantID = addConstantValue(v);
    if (dst)
        return emitMove(dst, constantID);
    return constantID;
}

RegisterID* BytecodeGenerator::emitLoadGlobalObject(RegisterID* dst)
{
    if (!m_globalObjectRegister) {
        int index = m_nextConstantOffset;
        m_constantPoolRegisters.append(FirstConstantRegisterIndex + m_nextConstantOffset);
        ++m_nextConstantOffset;
        m_codeBlock->addConstant(JSValue());
        m_globalObjectRegister = &m_constantPoolRegisters[index];
        m_codeBlock->setGlobalObjectRegister(VirtualRegister(index));
    }
    if (dst)
        emitMove(dst, m_globalObjectRegister);
    return m_globalObjectRegister;
}

bool BytecodeGenerator::isCaptured(int operand)
{
    return m_symbolTable && m_symbolTable->isCaptured(operand);
}

Local BytecodeGenerator::local(const Identifier& property)
{
    if (property == propertyNames().thisIdentifier)
        return Local(thisRegister(), ReadOnly, NotCaptured);
    
    if (property == propertyNames().arguments)
        createArgumentsIfNecessary();

    if (!shouldOptimizeLocals())
        return Local();

    SymbolTableEntry entry = symbolTable().get(property.impl());
    if (entry.isNull())
        return Local();

    RegisterID* local = createLazyRegisterIfNecessary(&registerFor(entry.getIndex()));
    return Local(local, entry.getAttributes(), captureMode(local->index()));
}

Local BytecodeGenerator::constLocal(const Identifier& property)
{
    if (m_codeType != FunctionCode)
        return Local();

    SymbolTableEntry entry = symbolTable().get(property.impl());
    if (entry.isNull())
        return Local();

    RegisterID* local = createLazyRegisterIfNecessary(&registerFor(entry.getIndex()));
    return Local(local, entry.getAttributes(), captureMode(local->index()));
}

void BytecodeGenerator::emitCheckHasInstance(RegisterID* dst, RegisterID* value, RegisterID* base, Label* target)
{
    size_t begin = instructions().size();
    emitOpcode(op_check_has_instance);
    instructions().append(dst->index());
    instructions().append(value->index());
    instructions().append(base->index());
    instructions().append(target->bind(begin, instructions().size()));
}

// Indicates the least upper bound of resolve type based on local scope. The bytecode linker
// will start with this ResolveType and compute the least upper bound including intercepting scopes.
ResolveType BytecodeGenerator::resolveType()
{
    if (m_localScopeDepth)
        return Dynamic;
    if (m_symbolTable && m_symbolTable->usesNonStrictEval())
        return GlobalPropertyWithVarInjectionChecks;
    return GlobalProperty;
}

RegisterID* BytecodeGenerator::emitResolveScope(RegisterID* dst, const Identifier& identifier)
{
    m_codeBlock->addPropertyAccessInstruction(instructions().size());

    ASSERT(!m_symbolTable || !m_symbolTable->contains(identifier.impl()) || resolveType() == Dynamic);

    // resolve_scope dst, id, ResolveType, depth
    emitOpcode(op_resolve_scope);
    instructions().append(kill(dst));
    instructions().append(addConstant(identifier));
    instructions().append(resolveType());
    instructions().append(0);
    instructions().append(0);
    return dst;
}

RegisterID* BytecodeGenerator::emitGetFromScope(RegisterID* dst, RegisterID* scope, const Identifier& identifier, ResolveMode resolveMode)
{
    m_codeBlock->addPropertyAccessInstruction(instructions().size());

    // get_from_scope dst, scope, id, ResolveModeAndType, Structure, Operand
    UnlinkedValueProfile profile = emitProfiledOpcode(op_get_from_scope);
    instructions().append(kill(dst));
    instructions().append(scope->index());
    instructions().append(addConstant(identifier));
    instructions().append(ResolveModeAndType(resolveMode, resolveType()).operand());
    instructions().append(0);
    instructions().append(0);
    instructions().append(profile);
    return dst;
}

RegisterID* BytecodeGenerator::emitPutToScope(RegisterID* scope, const Identifier& identifier, RegisterID* value, ResolveMode resolveMode)
{
    m_codeBlock->addPropertyAccessInstruction(instructions().size());

    // put_to_scope scope, id, value, ResolveModeAndType, Structure, Operand
    emitOpcode(op_put_to_scope);
    instructions().append(scope->index());
    instructions().append(addConstant(identifier));
    instructions().append(value->index());
    instructions().append(ResolveModeAndType(resolveMode, resolveType()).operand());
    instructions().append(0);
    instructions().append(0);
    return value;
}

RegisterID* BytecodeGenerator::emitInstanceOf(RegisterID* dst, RegisterID* value, RegisterID* basePrototype)
{ 
    emitOpcode(op_instanceof);
    instructions().append(dst->index());
    instructions().append(value->index());
    instructions().append(basePrototype->index());
    return dst;
}

RegisterID* BytecodeGenerator::emitInitGlobalConst(const Identifier& identifier, RegisterID* value)
{
    ASSERT(m_codeType == GlobalCode);
    emitOpcode(op_init_global_const_nop);
    instructions().append(0);
    instructions().append(value->index());
    instructions().append(0);
    instructions().append(addConstant(identifier));
    return value;
}

RegisterID* BytecodeGenerator::emitGetById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    m_codeBlock->addPropertyAccessInstruction(instructions().size());

    UnlinkedValueProfile profile = emitProfiledOpcode(op_get_by_id);
    instructions().append(kill(dst));
    instructions().append(base->index());
    instructions().append(addConstant(property));
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    instructions().append(profile);
    return dst;
}

RegisterID* BytecodeGenerator::emitGetArgumentsLength(RegisterID* dst, RegisterID* base)
{
    emitOpcode(op_get_arguments_length);
    instructions().append(dst->index());
    ASSERT(base->virtualRegister() == m_codeBlock->argumentsRegister());
    instructions().append(base->index());
    instructions().append(addConstant(propertyNames().length));
    return dst;
}

RegisterID* BytecodeGenerator::emitPutById(RegisterID* base, const Identifier& property, RegisterID* value)
{
    unsigned propertyIndex = addConstant(property);

    m_staticPropertyAnalyzer.putById(base->index(), propertyIndex);

    m_codeBlock->addPropertyAccessInstruction(instructions().size());

    emitOpcode(op_put_by_id);
    instructions().append(base->index());
    instructions().append(propertyIndex);
    instructions().append(value->index());
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    return value;
}

RegisterID* BytecodeGenerator::emitDirectPutById(RegisterID* base, const Identifier& property, RegisterID* value)
{
    unsigned propertyIndex = addConstant(property);

    m_staticPropertyAnalyzer.putById(base->index(), propertyIndex);

    m_codeBlock->addPropertyAccessInstruction(instructions().size());
    
    emitOpcode(op_put_by_id);
    instructions().append(base->index());
    instructions().append(propertyIndex);
    instructions().append(value->index());
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    instructions().append(
        property != m_vm->propertyNames->underscoreProto
        && PropertyName(property).asIndex() == PropertyName::NotAnIndex);
    return value;
}

void BytecodeGenerator::emitPutGetterSetter(RegisterID* base, const Identifier& property, RegisterID* getter, RegisterID* setter)
{
    unsigned propertyIndex = addConstant(property);

    m_staticPropertyAnalyzer.putById(base->index(), propertyIndex);

    emitOpcode(op_put_getter_setter);
    instructions().append(base->index());
    instructions().append(propertyIndex);
    instructions().append(getter->index());
    instructions().append(setter->index());
}

RegisterID* BytecodeGenerator::emitDeleteById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    emitOpcode(op_del_by_id);
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(addConstant(property));
    return dst;
}

RegisterID* BytecodeGenerator::emitGetArgumentByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    UnlinkedArrayProfile arrayProfile = newArrayProfile();
    UnlinkedValueProfile profile = emitProfiledOpcode(op_get_argument_by_val);
    instructions().append(kill(dst));
    ASSERT(base->virtualRegister() == m_codeBlock->argumentsRegister());
    instructions().append(base->index());
    instructions().append(property->index());
    instructions().append(arrayProfile);
    instructions().append(profile);
    return dst;
}

RegisterID* BytecodeGenerator::emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    for (size_t i = m_forInContextStack.size(); i > 0; i--) {
        ForInContext& context = m_forInContextStack[i - 1];
        if (context.propertyRegister == property) {
            emitOpcode(op_get_by_pname);
            instructions().append(dst->index());
            instructions().append(base->index());
            instructions().append(property->index());
            instructions().append(context.expectedSubscriptRegister->index());
            instructions().append(context.iterRegister->index());
            instructions().append(context.indexRegister->index());
            return dst;
        }
    }
    UnlinkedArrayProfile arrayProfile = newArrayProfile();
    UnlinkedValueProfile profile = emitProfiledOpcode(op_get_by_val);
    instructions().append(kill(dst));
    instructions().append(base->index());
    instructions().append(property->index());
    instructions().append(arrayProfile);
    instructions().append(profile);
    return dst;
}

RegisterID* BytecodeGenerator::emitPutByVal(RegisterID* base, RegisterID* property, RegisterID* value)
{
    UnlinkedArrayProfile arrayProfile = newArrayProfile();
    emitOpcode(op_put_by_val);
    instructions().append(base->index());
    instructions().append(property->index());
    instructions().append(value->index());
    instructions().append(arrayProfile);
    return value;
}

RegisterID* BytecodeGenerator::emitDirectPutByVal(RegisterID* base, RegisterID* property, RegisterID* value)
{
    UnlinkedArrayProfile arrayProfile = newArrayProfile();
    emitOpcode(op_put_by_val_direct);
    instructions().append(base->index());
    instructions().append(property->index());
    instructions().append(value->index());
    instructions().append(arrayProfile);
    return value;
}

RegisterID* BytecodeGenerator::emitDeleteByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    emitOpcode(op_del_by_val);
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(property->index());
    return dst;
}

RegisterID* BytecodeGenerator::emitPutByIndex(RegisterID* base, unsigned index, RegisterID* value)
{
    emitOpcode(op_put_by_index);
    instructions().append(base->index());
    instructions().append(index);
    instructions().append(value->index());
    return value;
}

RegisterID* BytecodeGenerator::emitCreateThis(RegisterID* dst)
{
    RefPtr<RegisterID> func = newTemporary(); 

    m_codeBlock->addPropertyAccessInstruction(instructions().size());
    emitOpcode(op_get_callee);
    instructions().append(func->index());
    instructions().append(0);

    size_t begin = instructions().size();
    m_staticPropertyAnalyzer.createThis(m_thisRegister.index(), begin + 3);

    emitOpcode(op_create_this); 
    instructions().append(m_thisRegister.index()); 
    instructions().append(func->index()); 
    instructions().append(0);
    return dst;
}

RegisterID* BytecodeGenerator::emitNewObject(RegisterID* dst)
{
    size_t begin = instructions().size();
    m_staticPropertyAnalyzer.newObject(dst->index(), begin + 2);

    emitOpcode(op_new_object);
    instructions().append(dst->index());
    instructions().append(0);
    instructions().append(newObjectAllocationProfile());
    return dst;
}

unsigned BytecodeGenerator::addConstantBuffer(unsigned length)
{
    return m_codeBlock->addConstantBuffer(length);
}

JSString* BytecodeGenerator::addStringConstant(const Identifier& identifier)
{
    JSString*& stringInMap = m_stringMap.add(identifier.impl(), nullptr).iterator->value;
    if (!stringInMap) {
        stringInMap = jsString(vm(), identifier.string());
        addConstantValue(stringInMap);
    }
    return stringInMap;
}

RegisterID* BytecodeGenerator::emitNewArray(RegisterID* dst, ElementNode* elements, unsigned length)
{
#if !ASSERT_DISABLED
    unsigned checkLength = 0;
#endif
    bool hadVariableExpression = false;
    if (length) {
        for (ElementNode* n = elements; n; n = n->next()) {
            if (!n->value()->isConstant()) {
                hadVariableExpression = true;
                break;
            }
            if (n->elision())
                break;
#if !ASSERT_DISABLED
            checkLength++;
#endif
        }
        if (!hadVariableExpression) {
            ASSERT(length == checkLength);
            unsigned constantBufferIndex = addConstantBuffer(length);
            JSValue* constantBuffer = m_codeBlock->constantBuffer(constantBufferIndex).data();
            unsigned index = 0;
            for (ElementNode* n = elements; index < length; n = n->next()) {
                ASSERT(n->value()->isConstant());
                constantBuffer[index++] = static_cast<ConstantNode*>(n->value())->jsValue(*this);
            }
            emitOpcode(op_new_array_buffer);
            instructions().append(dst->index());
            instructions().append(constantBufferIndex);
            instructions().append(length);
            instructions().append(newArrayAllocationProfile());
            return dst;
        }
    }

    Vector<RefPtr<RegisterID>, 16, UnsafeVectorOverflow> argv;
    for (ElementNode* n = elements; n; n = n->next()) {
        if (!length)
            break;
        length--;
        ASSERT(!n->value()->isSpreadExpression());
        argv.append(newTemporary());
        // op_new_array requires the initial values to be a sequential range of registers
        ASSERT(argv.size() == 1 || argv[argv.size() - 1]->index() == argv[argv.size() - 2]->index() - 1);
        emitNode(argv.last().get(), n->value());
    }
    ASSERT(!length);
    emitOpcode(op_new_array);
    instructions().append(dst->index());
    instructions().append(argv.size() ? argv[0]->index() : 0); // argv
    instructions().append(argv.size()); // argc
    instructions().append(newArrayAllocationProfile());
    return dst;
}

RegisterID* BytecodeGenerator::emitNewFunction(RegisterID* dst, CaptureMode captureMode, FunctionBodyNode* function)
{
    return emitNewFunctionInternal(dst, captureMode, m_codeBlock->addFunctionDecl(makeFunction(function)), false);
}

RegisterID* BytecodeGenerator::emitLazyNewFunction(RegisterID* dst, FunctionBodyNode* function)
{
    FunctionOffsetMap::AddResult ptr = m_functionOffsets.add(function, 0);
    if (ptr.isNewEntry)
        ptr.iterator->value = m_codeBlock->addFunctionDecl(makeFunction(function));
    return emitNewFunctionInternal(dst, NotCaptured, ptr.iterator->value, true);
}

RegisterID* BytecodeGenerator::emitNewFunctionInternal(RegisterID* dst, CaptureMode captureMode, unsigned index, bool doNullCheck)
{
    createActivationIfNecessary();
    emitOpcode(captureMode == IsCaptured ? op_new_captured_func : op_new_func);
    instructions().append(dst->index());
    instructions().append(index);
    if (captureMode == IsCaptured) {
        ASSERT(!doNullCheck);
        instructions().append(watchableVariable(dst->index()));
    } else
        instructions().append(doNullCheck);
    return dst;
}

RegisterID* BytecodeGenerator::emitNewRegExp(RegisterID* dst, RegExp* regExp)
{
    emitOpcode(op_new_regexp);
    instructions().append(dst->index());
    instructions().append(addRegExp(regExp));
    return dst;
}

RegisterID* BytecodeGenerator::emitNewFunctionExpression(RegisterID* r0, FuncExprNode* n)
{
    FunctionBodyNode* function = n->body();
    unsigned index = m_codeBlock->addFunctionExpr(makeFunction(function));
    
    createActivationIfNecessary();
    emitOpcode(op_new_func_exp);
    instructions().append(r0->index());
    instructions().append(index);
    return r0;
}

RegisterID* BytecodeGenerator::emitCall(RegisterID* dst, RegisterID* func, ExpectedFunction expectedFunction, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
{
    return emitCall(op_call, dst, func, expectedFunction, callArguments, divot, divotStart, divotEnd);
}

void BytecodeGenerator::createArgumentsIfNecessary()
{
    if (m_codeType != FunctionCode)
        return;
    
    if (!m_codeBlock->usesArguments())
        return;

    if (shouldTearOffArgumentsEagerly())
        return;

    emitOpcode(op_create_arguments);
    instructions().append(m_codeBlock->argumentsRegister().offset());
    ASSERT(!hasWatchableVariable(m_codeBlock->argumentsRegister().offset()));
}

void BytecodeGenerator::createActivationIfNecessary()
{
    if (!m_activationRegister)
        return;
    emitOpcode(op_create_activation);
    instructions().append(m_activationRegister->index());
}

RegisterID* BytecodeGenerator::emitCallEval(RegisterID* dst, RegisterID* func, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
{
    createActivationIfNecessary();
    return emitCall(op_call_eval, dst, func, NoExpectedFunction, callArguments, divot, divotStart, divotEnd);
}

ExpectedFunction BytecodeGenerator::expectedFunctionForIdentifier(const Identifier& identifier)
{
    if (identifier == m_vm->propertyNames->Object)
        return ExpectObjectConstructor;
    if (identifier == m_vm->propertyNames->Array)
        return ExpectArrayConstructor;
    return NoExpectedFunction;
}

ExpectedFunction BytecodeGenerator::emitExpectedFunctionSnippet(RegisterID* dst, RegisterID* func, ExpectedFunction expectedFunction, CallArguments& callArguments, Label* done)
{
    RefPtr<Label> realCall = newLabel();
    switch (expectedFunction) {
    case ExpectObjectConstructor: {
        // If the number of arguments is non-zero, then we can't do anything interesting.
        if (callArguments.argumentCountIncludingThis() >= 2)
            return NoExpectedFunction;
        
        size_t begin = instructions().size();
        emitOpcode(op_jneq_ptr);
        instructions().append(func->index());
        instructions().append(Special::ObjectConstructor);
        instructions().append(realCall->bind(begin, instructions().size()));
        
        if (dst != ignoredResult())
            emitNewObject(dst);
        break;
    }
        
    case ExpectArrayConstructor: {
        // If you're doing anything other than "new Array()" or "new Array(foo)" then we
        // don't do inline it, for now. The only reason is that call arguments are in
        // the opposite order of what op_new_array expects, so we'd either need to change
        // how op_new_array works or we'd need an op_new_array_reverse. Neither of these
        // things sounds like it's worth it.
        if (callArguments.argumentCountIncludingThis() > 2)
            return NoExpectedFunction;
        
        size_t begin = instructions().size();
        emitOpcode(op_jneq_ptr);
        instructions().append(func->index());
        instructions().append(Special::ArrayConstructor);
        instructions().append(realCall->bind(begin, instructions().size()));
        
        if (dst != ignoredResult()) {
            if (callArguments.argumentCountIncludingThis() == 2) {
                emitOpcode(op_new_array_with_size);
                instructions().append(dst->index());
                instructions().append(callArguments.argumentRegister(0)->index());
                instructions().append(newArrayAllocationProfile());
            } else {
                ASSERT(callArguments.argumentCountIncludingThis() == 1);
                emitOpcode(op_new_array);
                instructions().append(dst->index());
                instructions().append(0);
                instructions().append(0);
                instructions().append(newArrayAllocationProfile());
            }
        }
        break;
    }
        
    default:
        ASSERT(expectedFunction == NoExpectedFunction);
        return NoExpectedFunction;
    }
    
    size_t begin = instructions().size();
    emitOpcode(op_jmp);
    instructions().append(done->bind(begin, instructions().size()));
    emitLabel(realCall.get());
    
    return expectedFunction;
}

RegisterID* BytecodeGenerator::emitCall(OpcodeID opcodeID, RegisterID* dst, RegisterID* func, ExpectedFunction expectedFunction, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
{
    ASSERT(opcodeID == op_call || opcodeID == op_call_eval);
    ASSERT(func->refCount());

    if (m_shouldEmitProfileHooks)
        emitMove(callArguments.profileHookRegister(), func);

    // Generate code for arguments.
    unsigned argument = 0;
    if (callArguments.argumentsNode()) {
        ArgumentListNode* n = callArguments.argumentsNode()->m_listNode;
        if (n && n->m_expr->isSpreadExpression()) {
            RELEASE_ASSERT(!n->m_next);
            auto expression = static_cast<SpreadExpressionNode*>(n->m_expr)->expression();
            expression->emitBytecode(*this, callArguments.argumentRegister(0));
            return emitCallVarargs(dst, func, callArguments.thisRegister(), callArguments.argumentRegister(0), newTemporary(), callArguments.profileHookRegister(), divot, divotStart, divotEnd);
        }
        for (; n; n = n->m_next)
            emitNode(callArguments.argumentRegister(argument++), n);
    }
    
    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, JSStack::CallFrameHeaderSize, UnsafeVectorOverflow> callFrame;
    for (int i = 0; i < JSStack::CallFrameHeaderSize; ++i)
        callFrame.append(newTemporary());

    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_will_call);
        instructions().append(callArguments.profileHookRegister()->index());
    }

    emitExpressionInfo(divot, divotStart, divotEnd);

    RefPtr<Label> done = newLabel();
    expectedFunction = emitExpectedFunctionSnippet(dst, func, expectedFunction, callArguments, done.get());
    
    // Emit call.
    UnlinkedArrayProfile arrayProfile = newArrayProfile();
    UnlinkedValueProfile profile = emitProfiledOpcode(opcodeID);
    ASSERT(dst);
    ASSERT(dst != ignoredResult());
    instructions().append(dst->index());
    instructions().append(func->index());
    instructions().append(callArguments.argumentCountIncludingThis());
    instructions().append(callArguments.stackOffset());
#if ENABLE(LLINT)
    instructions().append(m_codeBlock->addLLIntCallLinkInfo());
#else
    instructions().append(0);
#endif
    instructions().append(arrayProfile);
    instructions().append(profile);
    
    if (expectedFunction != NoExpectedFunction)
        emitLabel(done.get());

    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_did_call);
        instructions().append(callArguments.profileHookRegister()->index());
    }

    return dst;
}

RegisterID* BytecodeGenerator::emitCallVarargs(RegisterID* dst, RegisterID* func, RegisterID* thisRegister, RegisterID* arguments, RegisterID* firstFreeRegister, RegisterID* profileHookRegister, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
{
    if (m_shouldEmitProfileHooks) {
        emitMove(profileHookRegister, func);
        emitOpcode(op_profile_will_call);
        instructions().append(profileHookRegister->index());
    }
    
    emitExpressionInfo(divot, divotStart, divotEnd);

    // Emit call.
    UnlinkedValueProfile profile = emitProfiledOpcode(op_call_varargs);
    ASSERT(dst != ignoredResult());
    instructions().append(dst->index());
    instructions().append(func->index());
    instructions().append(thisRegister->index());
    instructions().append(arguments->index());
    instructions().append(firstFreeRegister->index());
    instructions().append(0); // Pad to make it as big as an op_call.
    instructions().append(profile);
    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_did_call);
        instructions().append(profileHookRegister->index());
    }
    return dst;
}

RegisterID* BytecodeGenerator::emitReturn(RegisterID* src)
{
    if (m_activationRegister) {
        emitOpcode(op_tear_off_activation);
        instructions().append(m_activationRegister->index());
    }

    if (m_codeBlock->usesArguments() && m_codeBlock->numParameters() != 1 && !isStrictMode()) {
        emitOpcode(op_tear_off_arguments);
        instructions().append(m_codeBlock->argumentsRegister().offset());
        instructions().append(m_activationRegister ? m_activationRegister->index() : emitLoad(0, JSValue())->index());
    }

    // Constructors use op_ret_object_or_this to check the result is an
    // object, unless we can trivially determine the check is not
    // necessary (currently, if the return value is 'this').
    if (isConstructor() && (src->index() != m_thisRegister.index())) {
        emitOpcode(op_ret_object_or_this);
        instructions().append(src->index());
        instructions().append(m_thisRegister.index());
        return src;
    }
    return emitUnaryNoDstOp(op_ret, src);
}

RegisterID* BytecodeGenerator::emitUnaryNoDstOp(OpcodeID opcodeID, RegisterID* src)
{
    emitOpcode(opcodeID);
    instructions().append(src->index());
    return src;
}

RegisterID* BytecodeGenerator::emitConstruct(RegisterID* dst, RegisterID* func, ExpectedFunction expectedFunction, CallArguments& callArguments, const JSTextPosition& divot, const JSTextPosition& divotStart, const JSTextPosition& divotEnd)
{
    ASSERT(func->refCount());

    if (m_shouldEmitProfileHooks)
        emitMove(callArguments.profileHookRegister(), func);

    // Generate code for arguments.
    unsigned argument = 0;
    if (ArgumentsNode* argumentsNode = callArguments.argumentsNode()) {
        for (ArgumentListNode* n = argumentsNode->m_listNode; n; n = n->m_next)
            emitNode(callArguments.argumentRegister(argument++), n);
    }

    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_will_call);
        instructions().append(callArguments.profileHookRegister()->index());
    }

    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, JSStack::CallFrameHeaderSize, UnsafeVectorOverflow> callFrame;
    for (int i = 0; i < JSStack::CallFrameHeaderSize; ++i)
        callFrame.append(newTemporary());

    emitExpressionInfo(divot, divotStart, divotEnd);
    
    RefPtr<Label> done = newLabel();
    expectedFunction = emitExpectedFunctionSnippet(dst, func, expectedFunction, callArguments, done.get());

    UnlinkedValueProfile profile = emitProfiledOpcode(op_construct);
    ASSERT(dst != ignoredResult());
    instructions().append(dst->index());
    instructions().append(func->index());
    instructions().append(callArguments.argumentCountIncludingThis());
    instructions().append(callArguments.stackOffset());
#if ENABLE(LLINT)
    instructions().append(m_codeBlock->addLLIntCallLinkInfo());
#else
    instructions().append(0);
#endif
    instructions().append(0);
    instructions().append(profile);

    if (expectedFunction != NoExpectedFunction)
        emitLabel(done.get());

    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_did_call);
        instructions().append(callArguments.profileHookRegister()->index());
    }

    return dst;
}

RegisterID* BytecodeGenerator::emitStrcat(RegisterID* dst, RegisterID* src, int count)
{
    emitOpcode(op_strcat);
    instructions().append(dst->index());
    instructions().append(src->index());
    instructions().append(count);

    return dst;
}

void BytecodeGenerator::emitToPrimitive(RegisterID* dst, RegisterID* src)
{
    emitOpcode(op_to_primitive);
    instructions().append(dst->index());
    instructions().append(src->index());
}

RegisterID* BytecodeGenerator::emitPushWithScope(RegisterID* scope)
{
    ControlFlowContext context;
    context.isFinallyBlock = false;
    m_scopeContextStack.append(context);
    m_localScopeDepth++;

    createActivationIfNecessary();
    return emitUnaryNoDstOp(op_push_with_scope, scope);
}

void BytecodeGenerator::emitPopScope()
{
    ASSERT(m_scopeContextStack.size());
    ASSERT(!m_scopeContextStack.last().isFinallyBlock);

    emitOpcode(op_pop_scope);

    m_scopeContextStack.removeLast();
    m_localScopeDepth--;
}

void BytecodeGenerator::emitDebugHook(DebugHookID debugHookID, unsigned line, unsigned charOffset, unsigned lineStart)
{
#if ENABLE(DEBUG_WITH_BREAKPOINT)
    if (debugHookID != DidReachBreakpoint)
        return;
#else
    if (!m_shouldEmitDebugHooks)
        return;
#endif
    JSTextPosition divot(line, charOffset, lineStart);
    emitExpressionInfo(divot, divot, divot);
    emitOpcode(op_debug);
    instructions().append(debugHookID);
    instructions().append(false);
}

void BytecodeGenerator::pushFinallyContext(StatementNode* finallyBlock)
{
    // Reclaim free label scopes.
    while (m_labelScopes.size() && !m_labelScopes.last().refCount())
        m_labelScopes.removeLast();

    ControlFlowContext scope;
    scope.isFinallyBlock = true;
    FinallyContext context = {
        finallyBlock,
        static_cast<unsigned>(m_scopeContextStack.size()),
        static_cast<unsigned>(m_switchContextStack.size()),
        static_cast<unsigned>(m_forInContextStack.size()),
        static_cast<unsigned>(m_tryContextStack.size()),
        static_cast<unsigned>(m_labelScopes.size()),
        m_finallyDepth,
        m_localScopeDepth
    };
    scope.finallyContext = context;
    m_scopeContextStack.append(scope);
    m_finallyDepth++;
}

void BytecodeGenerator::popFinallyContext()
{
    ASSERT(m_scopeContextStack.size());
    ASSERT(m_scopeContextStack.last().isFinallyBlock);
    ASSERT(m_finallyDepth > 0);
    m_scopeContextStack.removeLast();
    m_finallyDepth--;
}

LabelScope* BytecodeGenerator::breakTarget(const Identifier& name)
{
    // Reclaim free label scopes.
    //
    // The condition was previously coded as 'm_labelScopes.size() && !m_labelScopes.last().refCount()',
    // however sometimes this appears to lead to GCC going a little haywire and entering the loop with
    // size 0, leading to segfaulty badness.  We are yet to identify a valid cause within our code to
    // cause the GCC codegen to misbehave in this fashion, and as such the following refactoring of the
    // loop condition is a workaround.
    while (m_labelScopes.size()) {
        if  (m_labelScopes.last().refCount())
            break;
        m_labelScopes.removeLast();
    }

    if (!m_labelScopes.size())
        return 0;

    // We special-case the following, which is a syntax error in Firefox:
    // label:
    //     break;
    if (name.isEmpty()) {
        for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
            LabelScope* scope = &m_labelScopes[i];
            if (scope->type() != LabelScope::NamedLabel) {
                ASSERT(scope->breakTarget());
                return scope;
            }
        }
        return 0;
    }

    for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
        LabelScope* scope = &m_labelScopes[i];
        if (scope->name() && *scope->name() == name) {
            ASSERT(scope->breakTarget());
            return scope;
        }
    }
    return 0;
}

LabelScope* BytecodeGenerator::continueTarget(const Identifier& name)
{
    // Reclaim free label scopes.
    while (m_labelScopes.size() && !m_labelScopes.last().refCount())
        m_labelScopes.removeLast();

    if (!m_labelScopes.size())
        return 0;

    if (name.isEmpty()) {
        for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
            LabelScope* scope = &m_labelScopes[i];
            if (scope->type() == LabelScope::Loop) {
                ASSERT(scope->continueTarget());
                return scope;
            }
        }
        return 0;
    }

    // Continue to the loop nested nearest to the label scope that matches
    // 'name'.
    LabelScope* result = 0;
    for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
        LabelScope* scope = &m_labelScopes[i];
        if (scope->type() == LabelScope::Loop) {
            ASSERT(scope->continueTarget());
            result = scope;
        }
        if (scope->name() && *scope->name() == name)
            return result; // may be 0
    }
    return 0;
}

void BytecodeGenerator::emitComplexPopScopes(ControlFlowContext* topScope, ControlFlowContext* bottomScope)
{
    while (topScope > bottomScope) {
        // First we count the number of dynamic scopes we need to remove to get
        // to a finally block.
        int nNormalScopes = 0;
        while (topScope > bottomScope) {
            if (topScope->isFinallyBlock)
                break;
            ++nNormalScopes;
            --topScope;
        }

        if (nNormalScopes) {
            // We need to remove a number of dynamic scopes to get to the next
            // finally block
            while (nNormalScopes--)
                emitOpcode(op_pop_scope);

            // If topScope == bottomScope then there isn't a finally block left to emit.
            if (topScope == bottomScope)
                return;
        }
        
        Vector<ControlFlowContext> savedScopeContextStack;
        Vector<SwitchInfo> savedSwitchContextStack;
        Vector<ForInContext> savedForInContextStack;
        Vector<TryContext> poppedTryContexts;
        LabelScopeStore savedLabelScopes;
        while (topScope > bottomScope && topScope->isFinallyBlock) {
            RefPtr<Label> beforeFinally = emitLabel(newLabel().get());
            
            // Save the current state of the world while instating the state of the world
            // for the finally block.
            FinallyContext finallyContext = topScope->finallyContext;
            bool flipScopes = finallyContext.scopeContextStackSize != m_scopeContextStack.size();
            bool flipSwitches = finallyContext.switchContextStackSize != m_switchContextStack.size();
            bool flipForIns = finallyContext.forInContextStackSize != m_forInContextStack.size();
            bool flipTries = finallyContext.tryContextStackSize != m_tryContextStack.size();
            bool flipLabelScopes = finallyContext.labelScopesSize != m_labelScopes.size();
            int topScopeIndex = -1;
            int bottomScopeIndex = -1;
            if (flipScopes) {
                topScopeIndex = topScope - m_scopeContextStack.begin();
                bottomScopeIndex = bottomScope - m_scopeContextStack.begin();
                savedScopeContextStack = m_scopeContextStack;
                m_scopeContextStack.shrink(finallyContext.scopeContextStackSize);
            }
            if (flipSwitches) {
                savedSwitchContextStack = m_switchContextStack;
                m_switchContextStack.shrink(finallyContext.switchContextStackSize);
            }
            if (flipForIns) {
                savedForInContextStack = m_forInContextStack;
                m_forInContextStack.shrink(finallyContext.forInContextStackSize);
            }
            if (flipTries) {
                while (m_tryContextStack.size() != finallyContext.tryContextStackSize) {
                    ASSERT(m_tryContextStack.size() > finallyContext.tryContextStackSize);
                    TryContext context = m_tryContextStack.last();
                    m_tryContextStack.removeLast();
                    TryRange range;
                    range.start = context.start;
                    range.end = beforeFinally;
                    range.tryData = context.tryData;
                    m_tryRanges.append(range);
                    poppedTryContexts.append(context);
                }
            }
            if (flipLabelScopes) {
                savedLabelScopes = m_labelScopes;
                while (m_labelScopes.size() > finallyContext.labelScopesSize)
                    m_labelScopes.removeLast();
            }
            int savedFinallyDepth = m_finallyDepth;
            m_finallyDepth = finallyContext.finallyDepth;
            int savedDynamicScopeDepth = m_localScopeDepth;
            m_localScopeDepth = finallyContext.dynamicScopeDepth;
            
            // Emit the finally block.
            emitNode(finallyContext.finallyBlock);
            
            RefPtr<Label> afterFinally = emitLabel(newLabel().get());
            
            // Restore the state of the world.
            if (flipScopes) {
                m_scopeContextStack = savedScopeContextStack;
                topScope = &m_scopeContextStack[topScopeIndex]; // assert it's within bounds
                bottomScope = m_scopeContextStack.begin() + bottomScopeIndex; // don't assert, since it the index might be -1.
            }
            if (flipSwitches)
                m_switchContextStack = savedSwitchContextStack;
            if (flipForIns)
                m_forInContextStack = savedForInContextStack;
            if (flipTries) {
                ASSERT(m_tryContextStack.size() == finallyContext.tryContextStackSize);
                for (unsigned i = poppedTryContexts.size(); i--;) {
                    TryContext context = poppedTryContexts[i];
                    context.start = afterFinally;
                    m_tryContextStack.append(context);
                }
                poppedTryContexts.clear();
            }
            if (flipLabelScopes)
                m_labelScopes = savedLabelScopes;
            m_finallyDepth = savedFinallyDepth;
            m_localScopeDepth = savedDynamicScopeDepth;
            
            --topScope;
        }
    }
}

void BytecodeGenerator::emitPopScopes(int targetScopeDepth)
{
    ASSERT(scopeDepth() - targetScopeDepth >= 0);

    size_t scopeDelta = scopeDepth() - targetScopeDepth;
    ASSERT(scopeDelta <= m_scopeContextStack.size());
    if (!scopeDelta)
        return;

    if (!m_finallyDepth) {
        while (scopeDelta--)
            emitOpcode(op_pop_scope);
        return;
    }

    emitComplexPopScopes(&m_scopeContextStack.last(), &m_scopeContextStack.last() - scopeDelta);
}

RegisterID* BytecodeGenerator::emitGetPropertyNames(RegisterID* dst, RegisterID* base, RegisterID* i, RegisterID* size, Label* breakTarget)
{
    size_t begin = instructions().size();

    emitOpcode(op_get_pnames);
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(i->index());
    instructions().append(size->index());
    instructions().append(breakTarget->bind(begin, instructions().size()));
    return dst;
}

RegisterID* BytecodeGenerator::emitNextPropertyName(RegisterID* dst, RegisterID* base, RegisterID* i, RegisterID* size, RegisterID* iter, Label* target)
{
    size_t begin = instructions().size();

    emitOpcode(op_next_pname);
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(i->index());
    instructions().append(size->index());
    instructions().append(iter->index());
    instructions().append(target->bind(begin, instructions().size()));
    return dst;
}

TryData* BytecodeGenerator::pushTry(Label* start)
{
    TryData tryData;
    tryData.target = newLabel();
    tryData.targetScopeDepth = UINT_MAX;
    m_tryData.append(tryData);
    TryData* result = &m_tryData.last();
    
    TryContext tryContext;
    tryContext.start = start;
    tryContext.tryData = result;
    
    m_tryContextStack.append(tryContext);
    
    return result;
}

RegisterID* BytecodeGenerator::popTryAndEmitCatch(TryData* tryData, RegisterID* targetRegister, Label* end)
{
    m_usesExceptions = true;
    
    ASSERT_UNUSED(tryData, m_tryContextStack.last().tryData == tryData);
    
    TryRange tryRange;
    tryRange.start = m_tryContextStack.last().start;
    tryRange.end = end;
    tryRange.tryData = m_tryContextStack.last().tryData;
    m_tryRanges.append(tryRange);
    m_tryContextStack.removeLast();
    
    emitLabel(tryRange.tryData->target.get());
    tryRange.tryData->targetScopeDepth = m_localScopeDepth;

    emitOpcode(op_catch);
    instructions().append(targetRegister->index());
    return targetRegister;
}

void BytecodeGenerator::emitThrowReferenceError(const String& message)
{
    emitOpcode(op_throw_static_error);
    instructions().append(addConstantValue(addStringConstant(Identifier(m_vm, message)))->index());
    instructions().append(true);
}

void BytecodeGenerator::emitPushFunctionNameScope(const Identifier& property, RegisterID* value, unsigned attributes)
{
    emitOpcode(op_push_name_scope);
    instructions().append(addConstant(property));
    instructions().append(value->index());
    instructions().append(attributes);
}

void BytecodeGenerator::emitPushCatchScope(const Identifier& property, RegisterID* value, unsigned attributes)
{
    createActivationIfNecessary();

    ControlFlowContext context;
    context.isFinallyBlock = false;
    m_scopeContextStack.append(context);
    m_localScopeDepth++;

    emitOpcode(op_push_name_scope);
    instructions().append(addConstant(property));
    instructions().append(value->index());
    instructions().append(attributes);
}

void BytecodeGenerator::beginSwitch(RegisterID* scrutineeRegister, SwitchInfo::SwitchType type)
{
    SwitchInfo info = { static_cast<uint32_t>(instructions().size()), type };
    switch (type) {
        case SwitchInfo::SwitchImmediate:
            emitOpcode(op_switch_imm);
            break;
        case SwitchInfo::SwitchCharacter:
            emitOpcode(op_switch_char);
            break;
        case SwitchInfo::SwitchString:
            emitOpcode(op_switch_string);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
    }

    instructions().append(0); // place holder for table index
    instructions().append(0); // place holder for default target    
    instructions().append(scrutineeRegister->index());
    m_switchContextStack.append(info);
}

static int32_t keyForImmediateSwitch(ExpressionNode* node, int32_t min, int32_t max)
{
    UNUSED_PARAM(max);
    ASSERT(node->isNumber());
    double value = static_cast<NumberNode*>(node)->value();
    int32_t key = static_cast<int32_t>(value);
    ASSERT(key == value);
    ASSERT(key >= min);
    ASSERT(key <= max);
    return key - min;
}

static int32_t keyForCharacterSwitch(ExpressionNode* node, int32_t min, int32_t max)
{
    UNUSED_PARAM(max);
    ASSERT(node->isString());
    StringImpl* clause = static_cast<StringNode*>(node)->value().impl();
    ASSERT(clause->length() == 1);
    
    int32_t key = (*clause)[0];
    ASSERT(key >= min);
    ASSERT(key <= max);
    return key - min;
}

static void prepareJumpTableForSwitch(
    UnlinkedSimpleJumpTable& jumpTable, int32_t switchAddress, uint32_t clauseCount,
    RefPtr<Label>* labels, ExpressionNode** nodes, int32_t min, int32_t max,
    int32_t (*keyGetter)(ExpressionNode*, int32_t min, int32_t max))
{
    jumpTable.min = min;
    jumpTable.branchOffsets.resize(max - min + 1);
    jumpTable.branchOffsets.fill(0);
    for (uint32_t i = 0; i < clauseCount; ++i) {
        // We're emitting this after the clause labels should have been fixed, so 
        // the labels should not be "forward" references
        ASSERT(!labels[i]->isForward());
        jumpTable.add(keyGetter(nodes[i], min, max), labels[i]->bind(switchAddress, switchAddress + 3)); 
    }
}

static void prepareJumpTableForStringSwitch(UnlinkedStringJumpTable& jumpTable, int32_t switchAddress, uint32_t clauseCount, RefPtr<Label>* labels, ExpressionNode** nodes)
{
    for (uint32_t i = 0; i < clauseCount; ++i) {
        // We're emitting this after the clause labels should have been fixed, so 
        // the labels should not be "forward" references
        ASSERT(!labels[i]->isForward());
        
        ASSERT(nodes[i]->isString());
        StringImpl* clause = static_cast<StringNode*>(nodes[i])->value().impl();
        jumpTable.offsetTable.add(clause, labels[i]->bind(switchAddress, switchAddress + 3));
    }
}

void BytecodeGenerator::endSwitch(uint32_t clauseCount, RefPtr<Label>* labels, ExpressionNode** nodes, Label* defaultLabel, int32_t min, int32_t max)
{
    SwitchInfo switchInfo = m_switchContextStack.last();
    m_switchContextStack.removeLast();
    
    switch (switchInfo.switchType) {
    case SwitchInfo::SwitchImmediate:
    case SwitchInfo::SwitchCharacter: {
        instructions()[switchInfo.bytecodeOffset + 1] = m_codeBlock->numberOfSwitchJumpTables();
        instructions()[switchInfo.bytecodeOffset + 2] = defaultLabel->bind(switchInfo.bytecodeOffset, switchInfo.bytecodeOffset + 3);

        UnlinkedSimpleJumpTable& jumpTable = m_codeBlock->addSwitchJumpTable();
        prepareJumpTableForSwitch(
            jumpTable, switchInfo.bytecodeOffset, clauseCount, labels, nodes, min, max,
            switchInfo.switchType == SwitchInfo::SwitchImmediate
                ? keyForImmediateSwitch
                : keyForCharacterSwitch); 
        break;
    }
        
    case SwitchInfo::SwitchString: {
        instructions()[switchInfo.bytecodeOffset + 1] = m_codeBlock->numberOfStringSwitchJumpTables();
        instructions()[switchInfo.bytecodeOffset + 2] = defaultLabel->bind(switchInfo.bytecodeOffset, switchInfo.bytecodeOffset + 3);

        UnlinkedStringJumpTable& jumpTable = m_codeBlock->addStringSwitchJumpTable();
        prepareJumpTableForStringSwitch(jumpTable, switchInfo.bytecodeOffset, clauseCount, labels, nodes);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

RegisterID* BytecodeGenerator::emitThrowExpressionTooDeepException()
{
    // It would be nice to do an even better job of identifying exactly where the expression is.
    // And we could make the caller pass the node pointer in, if there was some way of getting
    // that from an arbitrary node. However, calling emitExpressionInfo without any useful data
    // is still good enough to get us an accurate line number.
    m_expressionTooDeep = true;
    return newTemporary();
}

void BytecodeGenerator::setIsNumericCompareFunction(bool isNumericCompareFunction)
{
    m_codeBlock->setIsNumericCompareFunction(isNumericCompareFunction);
}

bool BytecodeGenerator::isArgumentNumber(const Identifier& ident, int argumentNumber)
{
    RegisterID* registerID = local(ident).get();
    if (!registerID || registerID->index() >= 0)
         return 0;
    return registerID->index() == CallFrame::argumentOffset(argumentNumber);
}

void BytecodeGenerator::emitReadOnlyExceptionIfNeeded()
{
    if (!isStrictMode())
        return;
    emitOpcode(op_throw_static_error);
    instructions().append(addConstantValue(addStringConstant(Identifier(m_vm, StrictModeReadonlyPropertyWriteError)))->index());
    instructions().append(false);
}
    
void BytecodeGenerator::emitEnumeration(ThrowableExpressionData* node, ExpressionNode* subjectNode, const std::function<void(BytecodeGenerator&, RegisterID*)>& callBack)
{
    if (subjectNode->isResolveNode()
        && willResolveToArguments(static_cast<ResolveNode*>(subjectNode)->identifier())
        && !symbolTable().slowArguments()) {
        RefPtr<RegisterID> index = emitLoad(newTemporary(), jsNumber(0));

        LabelScopePtr scope = newLabelScope(LabelScope::Loop);
        RefPtr<RegisterID> value = emitLoad(newTemporary(), jsUndefined());
        
        emitJump(scope->continueTarget());
        
        RefPtr<Label> loopStart = newLabel();
        emitLabel(loopStart.get());
        emitLoopHint();
        emitGetArgumentByVal(value.get(), uncheckedRegisterForArguments(), index.get());
        callBack(*this, value.get());
        emitInc(index.get());
        emitLabel(scope->continueTarget());

        RefPtr<RegisterID> length = emitGetArgumentsLength(newTemporary(), uncheckedRegisterForArguments());
        emitJumpIfTrue(emitEqualityOp(op_less, newTemporary(), index.get(), length.get()), loopStart.get());
        emitLabel(scope->breakTarget());
        return;
    }

    LabelScopePtr scope = newLabelScope(LabelScope::Loop);
    RefPtr<RegisterID> subject = newTemporary();
    emitNode(subject.get(), subjectNode);
    RefPtr<RegisterID> iterator = emitGetById(newTemporary(), subject.get(), propertyNames().iteratorPrivateName);
    {
        CallArguments args(*this, 0);
        emitMove(args.thisRegister(), subject.get());
        emitCall(iterator.get(), iterator.get(), NoExpectedFunction, args, node->divot(), node->divotStart(), node->divotEnd());
    }
    RefPtr<RegisterID> iteratorNext = emitGetById(newTemporary(), iterator.get(), propertyNames().iteratorNextPrivateName);
    RefPtr<RegisterID> value = newTemporary();
    emitLoad(value.get(), jsUndefined());
    
    emitJump(scope->continueTarget());
    
    RefPtr<Label> loopStart = newLabel();
    emitLabel(loopStart.get());
    emitLoopHint();
    callBack(*this, value.get());
    emitLabel(scope->continueTarget());
    CallArguments nextArguments(*this, 0, 1);
    emitMove(nextArguments.thisRegister(), iterator.get());
    emitMove(nextArguments.argumentRegister(0), value.get());
    emitCall(value.get(), iteratorNext.get(), NoExpectedFunction, nextArguments, node->divot(), node->divotStart(), node->divotEnd());
    RefPtr<RegisterID> result = newTemporary();
    emitJumpIfFalse(emitEqualityOp(op_stricteq, result.get(), value.get(), emitLoad(0, JSValue(vm()->iterationTerminator.get()))), loopStart.get());
    emitLabel(scope->breakTarget());
}

} // namespace JSC

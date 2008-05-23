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

#include "config.h"
#include "CodeGenerator.h"

#include "Machine.h"
#include "function.h"

using namespace std;

namespace KJS {

/*
    The layout of a register frame looks like this:
    
    For 
    
    function f(x, y) {
        var v1;
        function g() { }
        var v2;
        return (x) * (y);
    }
    
    assuming (x) and (y) generated temporaries t1 and t2, you would have

    ------------------------------------
    |  x |  y |  g | v2 | v1 | t1 | t2 | <-- value held
    ------------------------------------
    | -5 | -4 | -3 | -2 | -1 | +0 | +1 | <-- register index
    ------------------------------------
    | params->|<-locals      | temps->
    
    Because temporary registers are allocated in a stack-like fashion, we
    can reclaim them with a simple popping algorithm. The same goes for labels.
    (We never reclaim parameter or local registers, because parameters and
    locals are DontDelete.)
    
    The register layout before a function call looks like this:
    
    For
    
    function f(x, y)
    {
    }
    
    f(1);
    
    >                        <------------------------------
    <                        >  reserved: call frame  |  1 | <-- value held
    >         >snip<         <------------------------------
    <                        > +0 | +1 | +2 | +3 | +4 | +5 | <-- register index
    >                        <------------------------------
    | params->|<-locals      | temps->    
    
    The call instruction fills in the "call frame" registers. It also pads
    missing arguments at the end of the call:
    
    >                        <-----------------------------------
    <                        >  reserved: call frame  |  1 |  ? | <-- value held ("?" stands for "undefined")
    >         >snip<         <-----------------------------------
    <                        > +0 | +1 | +2 | +3 | +4 | +5 | +6 | <-- register index
    >                        <-----------------------------------
    | params->|<-locals      | temps->
    
    After filling in missing arguments, the call instruction sets up the new
    stack frame to overlap the end of the old stack frame:

                             |---------------------------------->                        <
                             |  reserved: call frame  |  1 |  ? <                        > <-- value held ("?" stands for "undefined")
                             |---------------------------------->         >snip<         <
                             | -7 | -6 | -5 | -4 | -3 | -2 | -1 <                        > <-- register index
                             |---------------------------------->                        <
                             |                        | params->|<-locals       | temps->
    
    That way, arguments are "copied" into the callee's stack frame for free.
    
    If the caller supplies too many arguments, this trick doesn't work. The
    extra arguments protrude into space reserved for locals and temporaries.
    In that case, the call instruction makes a real copy of the call frame header,
    along with just the arguments expected by the callee, leaving the original
    call frame header and arguments behind. (The call instruction can't just discard
    extra arguments, because the "arguments" object may access them later.)
    This copying strategy ensures that all named values will be at the indices
    expected by the callee.

*/

#ifndef NDEBUG
bool CodeGenerator::s_dumpsGeneratedCode = false;
#endif

void CodeGenerator::setDumpsGeneratedCode(bool dumpsGeneratedCode)
{
#ifndef NDEBUG
    s_dumpsGeneratedCode = dumpsGeneratedCode;
#else
    UNUSED_PARAM(dumpsGeneratedCode);
#endif
}

void CodeGenerator::generate()
{
    m_codeBlock->numLocals = m_codeBlock->numVars + m_codeBlock->numParameters;
    m_codeBlock->thisRegister = m_codeType == FunctionCode ? -m_codeBlock->numLocals : Machine::ProgramCodeThisRegister;
    if (m_shouldEmitDebugHooks)
        m_codeBlock->needsFullScopeChain = true;
    
    m_scopeNode->emitCode(*this);

#ifndef NDEBUG
    if (s_dumpsGeneratedCode) {
        JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(m_scopeChain->bottom());
        m_codeBlock->dump(globalObject->globalExec());
    }
#endif

    // Remove "this" from symbol table so it does not appear as a global object property at runtime.
    symbolTable().remove(m_propertyNames->thisIdentifier.ustring().rep());
}

bool CodeGenerator::addVar(const Identifier& ident, RegisterID*& r0, bool isConstant)
{
    int index = m_nextVar;
    SymbolTableEntry newEntry(index, isConstant ? ReadOnly : 0);
    pair<SymbolTable::iterator, bool> result = symbolTable().add(ident.ustring().rep(), newEntry);

    if (!result.second)
        index = result.first->second.getIndex();
    else {
        --m_nextVar;
        ++m_codeBlock->numVars;

        m_locals.append(index);
    }

    r0 = &m_locals[localsIndex(index)];
    return result.second;
}

RegisterID* CodeGenerator::programCodeThis()
{
    static RegisterID programThis(Machine::ProgramCodeThisRegister);
    return &programThis;
}

CodeGenerator::CodeGenerator(ProgramNode* programNode, const Debugger* debugger, const ScopeChain& scopeChain, SymbolTable* symbolTable, CodeBlock* codeBlock, VarStack& varStack, FunctionStack& functionStack, bool canCreateVariables)
    : m_shouldEmitDebugHooks(!!debugger)
    , m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(programNode)
    , m_codeBlock(codeBlock)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_codeType(GlobalCode)
    , m_continueDepth(0)
    , m_nextVar(-1)
    , m_propertyNames(CommonIdentifiers::shared())

{
    // Global code can inherit previously defined symbols.
    int size = symbolTable->size() + 1; // + 1 slot for  "this"
    m_thisRegister = programCodeThis();

    // Add previously defined symbols to bookkeeping.
    m_locals.resize(size);
    SymbolTable::iterator end = symbolTable->end();
    for (SymbolTable::iterator it = symbolTable->begin(); it != end; ++it)
        m_locals[localsIndex(it->second.getIndex())].setIndex(it->second.getIndex());

    // Shift new symbols so they get stored prior to previously defined symbols.
    m_nextVar -= size;

    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(scopeChain.bottom());
    ASSERT(globalObject->isGlobalObject());
    
    ExecState* exec = globalObject->globalExec();

    if (canCreateVariables) {
        for (size_t i = 0; i < functionStack.size(); ++i) {
            FuncDeclNode* funcDecl = functionStack[i];
            globalObject->removeDirect(funcDecl->m_ident); // Make sure our new function is not shadowed by an old property.
            emitNewFunction(addVar(funcDecl->m_ident, false), funcDecl);
        }
        
        for (size_t i = 0; i < varStack.size(); ++i) {
            if (!globalObject->hasProperty(exec, varStack[i].first))
                emitLoad(addVar(varStack[i].first, varStack[i].second & DeclarationStacks::IsConstant), jsUndefined());
        }
    } else {
        for (size_t i = 0; i < functionStack.size(); ++i) {
            FuncDeclNode* funcDecl = functionStack[i];
            globalObject->putWithAttributes(exec, funcDecl->m_ident, funcDecl->makeFunction(exec, scopeChain.node()), DontDelete);
        }
        for (size_t i = 0; i < varStack.size(); ++i) {
            if (globalObject->hasProperty(exec, varStack[i].first))
                continue;
            int attributes = DontDelete;
            if (varStack[i].second & DeclarationStacks::IsConstant)
                attributes |= ReadOnly;
            globalObject->putWithAttributes(exec, varStack[i].first, jsUndefined(), attributes);
        }
    }
}

CodeGenerator::CodeGenerator(FunctionBodyNode* functionBody, const Debugger* debugger, const ScopeChain& scopeChain, SymbolTable* symbolTable, CodeBlock* codeBlock, VarStack& varStack, FunctionStack& functionStack, Vector<Identifier>& parameters)
    : m_shouldEmitDebugHooks(!!debugger)
    , m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(functionBody)
    , m_codeBlock(codeBlock)
    , m_thisRegister(0)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_codeType(FunctionCode)
    , m_continueDepth(0)
    , m_nextVar(-1)
    , m_propertyNames(CommonIdentifiers::shared())
{
    for (size_t i = 0; i < functionStack.size(); ++i) {
        FuncDeclNode* funcDecl = functionStack[i];
        const Identifier& ident = funcDecl->m_ident;
        
        m_functions.add(ident.ustring().rep());
        emitNewFunction(addVar(ident, false), funcDecl);
    }

    for (size_t i = 0; i < varStack.size(); ++i) {
        const Identifier& ident = varStack[i].first;
        if (ident == m_propertyNames->arguments)
            continue;
        
        RegisterID* r0;
        if (addVar(ident, r0, varStack[i].second & DeclarationStacks::IsConstant))
            emitLoad(r0, jsUndefined());
    }

    m_nextParameter = m_nextVar - parameters.size();
    m_locals.resize(localsIndex(m_nextParameter) + 1);

    m_thisRegister = addParameter(m_propertyNames->thisIdentifier);
    for (size_t i = 0; i < parameters.size(); ++i) {
        addParameter(parameters[i]);
    }
}

CodeGenerator::CodeGenerator(EvalNode* evalNode, const Debugger* debugger, const ScopeChain& scopeChain, SymbolTable* symbolTable, EvalCodeBlock* codeBlock, VarStack& varStack, FunctionStack& functionStack)
    : m_shouldEmitDebugHooks(!!debugger)
    , m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(evalNode)
    , m_codeBlock(codeBlock)
    , m_thisRegister(0)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_codeType(EvalCode)
    , m_continueDepth(0)
    , m_nextVar(-1)
    , m_propertyNames(CommonIdentifiers::shared())
{
    addVar(m_propertyNames->thisIdentifier, m_thisRegister, false);
    
    for (size_t i = 0; i < varStack.size(); ++i)
        codeBlock->declaredVariableNames.append(varStack[i].first);
    
    for (size_t i = 0; i < functionStack.size(); ++i) {
        FuncDeclNode* funcDecl = functionStack[i];
        codeBlock->declaredFunctionNames.append(funcDecl->m_ident);
        addConstant(funcDecl);
    }
}

CodeGenerator::~CodeGenerator()
{
}

RegisterID* CodeGenerator::addParameter(const Identifier& ident)
{
    // Parameters overwrite var declarations, but not function declarations,
    // in the symbol table.
    RegisterID* result = 0;
    UString::Rep* rep = ident.ustring().rep();
    if (!m_functions.contains(rep)) {
        symbolTable().set(rep, m_nextParameter);
        m_locals[localsIndex(m_nextParameter)].setIndex(m_nextParameter);
        result = &(m_locals[localsIndex(m_nextParameter)]);
    }
    
    // To maintain the calling convention, we have to allocate unique space for
    // each parameter, even if the parameter doesn't make it into the symbol table.
    ++m_nextParameter;
    ++m_codeBlock->numParameters;
    return result;
}

RegisterID* CodeGenerator::registerForLocal(const Identifier& ident)
{
    if (m_codeType == FunctionCode && ident == m_propertyNames->arguments)
        m_codeBlock->needsFullScopeChain = true;

    if (!shouldOptimizeLocals() && ident != m_propertyNames->thisIdentifier)
        return 0;

    SymbolTableEntry entry = symbolTable().get(ident.ustring().rep());
    if (entry.isEmpty())
        return 0;

    return &m_locals[localsIndex(entry.getIndex())];
}

RegisterID* CodeGenerator::registerForLocalConstInit(const Identifier& ident)
{
    if (m_codeType == EvalCode)
        return 0;
    
    SymbolTableEntry entry = symbolTable().get(ident.ustring().rep());
    ASSERT(!entry.isEmpty());
    
    return &m_locals[localsIndex(entry.getIndex())];
}

bool CodeGenerator::isLocalConstant(const Identifier& ident)
{
    return symbolTable().get(ident.ustring().rep()).isReadOnly();
}

RegisterID* CodeGenerator::newTemporary()
{
    // Reclaim free register IDs.
    while (m_temporaries.size() && !m_temporaries.last().refCount())
        m_temporaries.removeLast();

    // Allocate new register ID.
    m_temporaries.append(m_temporaries.size());
    m_codeBlock->numTemporaries = max<int>(m_codeBlock->numTemporaries, m_temporaries.size());
    return &m_temporaries.last();
}

PassRefPtr<LabelID> CodeGenerator::newLabel()
{
    // Reclaim free label IDs.
    while (m_labels.size() && !m_labels.last().refCount())
        m_labels.removeLast();

    // Allocate new label ID.
    m_labels.append(m_codeBlock);
    return &m_labels.last();
}

PassRefPtr<LabelID> CodeGenerator::emitLabel(LabelID* l0)
{
    l0->setLocation(instructions().size());
    return l0;
}

PassRefPtr<LabelID> CodeGenerator::emitJump(LabelID* target)
{
    instructions().append(machine().getOpcode(op_jmp));
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpIfTrue(RegisterID* cond, LabelID* target)
{
    instructions().append(machine().getOpcode(op_jtrue));
    instructions().append(cond->index());
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpIfFalse(RegisterID* cond, LabelID* target)
{
    instructions().append(machine().getOpcode(op_jfalse));
    instructions().append(cond->index());
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

unsigned CodeGenerator::addConstant(FuncDeclNode* n)
{
    // No need to explicitly unique function body nodes -- they're unique already.
    int index = m_codeBlock->functions.size();
    m_codeBlock->functions.append(n);
    return index;
}

unsigned CodeGenerator::addConstant(FuncExprNode* n)
{
    // No need to explicitly unique function expression nodes -- they're unique already.
    int index = m_codeBlock->functionExpressions.size();
    m_codeBlock->functionExpressions.append(n);
    return index;
}

unsigned CodeGenerator::addConstant(const Identifier& ident)
{
    UString::Rep* rep = ident.ustring().rep();
    pair<IdentifierMap::iterator, bool> result = m_identifierMap.add(rep, m_codeBlock->identifiers.size());
    if (result.second) // new entry
        m_codeBlock->identifiers.append(rep);
    
    return result.first->second;
}

unsigned CodeGenerator::addConstant(JSValue* v)
{
    pair<JSValueMap::iterator, bool> result = m_jsValueMap.add(v, m_codeBlock->jsValues.size());
    if (result.second) // new entry
        m_codeBlock->jsValues.append(v);
    
    return result.first->second;
}

unsigned CodeGenerator::addRegExp(RegExp* r)
{
    int index = m_codeBlock->regexps.size();
    m_codeBlock->regexps.append(r);
    return index;
}

RegisterID* CodeGenerator::emitMove(RegisterID* dst, RegisterID* src)
{
    instructions().append(machine().getOpcode(op_mov));
    instructions().append(dst->index());
    instructions().append(src->index());
    return dst;
}

RegisterID* CodeGenerator::emitNot(RegisterID* dst, RegisterID* src)
{
    instructions().append(machine().getOpcode(op_not));
    instructions().append(dst->index());
    instructions().append(src->index());
    return dst;
}

RegisterID* CodeGenerator::emitEqual(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_eq));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitNotEqual(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_neq));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitStrictEqual(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_stricteq));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitNotStrictEqual(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_nstricteq));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitLess(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_less));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitLessEq(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_lesseq));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitPreInc(RegisterID* srcDst)
{
    instructions().append(machine().getOpcode(op_pre_inc));
    instructions().append(srcDst->index());
    return srcDst;
}

RegisterID* CodeGenerator::emitPreDec(RegisterID* srcDst)
{
    instructions().append(machine().getOpcode(op_pre_dec));
    instructions().append(srcDst->index());
    return srcDst;
}

RegisterID* CodeGenerator::emitPostInc(RegisterID* dst, RegisterID* srcDst)
{
    instructions().append(machine().getOpcode(op_post_inc));
    instructions().append(dst->index());
    instructions().append(srcDst->index());
    return dst;
}

RegisterID* CodeGenerator::emitPostDec(RegisterID* dst, RegisterID* srcDst)
{
    instructions().append(machine().getOpcode(op_post_dec));
    instructions().append(dst->index());
    instructions().append(srcDst->index());
    return dst;
}

RegisterID* CodeGenerator::emitToJSNumber(RegisterID* dst, RegisterID* src)
{
    instructions().append(machine().getOpcode(op_to_jsnumber));
    instructions().append(dst->index());
    instructions().append(src->index());
    return dst;
}

RegisterID* CodeGenerator::emitNegate(RegisterID* dst, RegisterID* src)
{
    instructions().append(machine().getOpcode(op_negate));
    instructions().append(dst->index());
    instructions().append(src->index());
    return dst;
}

RegisterID* CodeGenerator::emitAdd(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_add));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitMul(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_mul));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitDiv(RegisterID* dst, RegisterID* dividend, RegisterID* divisor)
{
    instructions().append(machine().getOpcode(op_div));
    instructions().append(dst->index());
    instructions().append(dividend->index());
    instructions().append(divisor->index());
    return dst;
}

RegisterID* CodeGenerator::emitMod(RegisterID* dst, RegisterID* dividend, RegisterID* divisor)
{
    instructions().append(machine().getOpcode(op_mod));
    instructions().append(dst->index());
    instructions().append(dividend->index());
    instructions().append(divisor->index());
    return dst;
}

RegisterID* CodeGenerator::emitSub(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_sub));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitLeftShift(RegisterID* dst, RegisterID* val, RegisterID* shift)
{
    instructions().append(machine().getOpcode(op_lshift));
    instructions().append(dst->index());
    instructions().append(val->index());
    instructions().append(shift->index());
    return dst;
}

RegisterID* CodeGenerator::emitRightShift(RegisterID* dst, RegisterID* val, RegisterID* shift)
{
    instructions().append(machine().getOpcode(op_rshift));
    instructions().append(dst->index());
    instructions().append(val->index());
    instructions().append(shift->index());
    return dst;
}

RegisterID* CodeGenerator::emitUnsignedRightShift(RegisterID* dst, RegisterID* val, RegisterID* shift)
{
    instructions().append(machine().getOpcode(op_urshift));
    instructions().append(dst->index());
    instructions().append(val->index());
    instructions().append(shift->index());
    return dst;
}

RegisterID* CodeGenerator::emitBitAnd(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_bitand));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitBitXOr(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_bitxor));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitBitOr(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_bitor));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitBitNot(RegisterID* dst, RegisterID* src)
{
    instructions().append(machine().getOpcode(op_bitnot));
    instructions().append(dst->index());
    instructions().append(src->index());
    return dst;
}

RegisterID* CodeGenerator::emitInstanceOf(RegisterID* dst, RegisterID* value, RegisterID* base)
{
    instructions().append(machine().getOpcode(op_instanceof));
    instructions().append(dst->index());
    instructions().append(value->index());
    instructions().append(base->index());
    return dst;
}

RegisterID* CodeGenerator::emitTypeOf(RegisterID* dst, RegisterID* src)
{
    instructions().append(machine().getOpcode(op_typeof));
    instructions().append(dst->index());
    instructions().append(src->index());
    return dst;
}

RegisterID* CodeGenerator::emitIn(RegisterID* dst, RegisterID* property, RegisterID* base)
{
    instructions().append(machine().getOpcode(op_in));
    instructions().append(dst->index());
    instructions().append(property->index());
    instructions().append(base->index());
    return dst;
}

RegisterID* CodeGenerator::emitLoad(RegisterID* dst, bool b)
{
    instructions().append(machine().getOpcode(op_load));
    instructions().append(dst->index());
    instructions().append(addConstant(jsBoolean(b)));
    return dst;
}

RegisterID* CodeGenerator::emitLoad(RegisterID* dst, double d)
{
    instructions().append(machine().getOpcode(op_load));
    instructions().append(dst->index());
    instructions().append(addConstant(jsNumber(d)));
    return dst;
}

RegisterID* CodeGenerator::emitLoad(RegisterID* dst, JSValue* v)
{
    instructions().append(machine().getOpcode(op_load));
    instructions().append(dst->index());
    instructions().append(addConstant(v));
    return dst;
}

RegisterID* CodeGenerator::emitNewObject(RegisterID* dst)
{
    instructions().append(machine().getOpcode(op_new_object));
    instructions().append(dst->index());
    return dst;
}

RegisterID* CodeGenerator::emitNewArray(RegisterID* dst)
{
    instructions().append(machine().getOpcode(op_new_array));
    instructions().append(dst->index());
    return dst;
}

bool CodeGenerator::findScopedProperty(const Identifier& property, int& index, size_t& stackDepth)
{
    // Cases where we cannot optimise the lookup
    if (property == m_propertyNames->arguments || !canOptimizeNonLocals()) {
        stackDepth = 0;
        index = missingSymbolMarker();
        return false;
    }

    ScopeChainIterator iter = m_scopeChain->begin();
    ScopeChainIterator end = m_scopeChain->end();
    size_t depth = 0;

    for (; iter != end; ++iter, ++depth) {
        JSObject* currentScope = *iter;
        if (!currentScope->isVariableObject()) 
            break;
        JSVariableObject* currentVariableObject = static_cast<JSVariableObject*>(currentScope);
        SymbolTableEntry entry = currentVariableObject->symbolTable().get(property.ustring().rep());

        // Found the property
        if (!entry.isEmpty()) {
            stackDepth = depth;
            index = entry.getIndex();
            return true;
        }
        if (currentVariableObject->isDynamicScope())
            break;
    }

    // Can't locate the property but we're able to avoid a few lookups
    stackDepth = depth;
    index = missingSymbolMarker();
    return true;
}

RegisterID* CodeGenerator::emitResolve(RegisterID* dst, const Identifier& property)
{
    size_t depth = 0;
    int index = 0;
    if (!findScopedProperty(property, index, depth)) {
        // We can't optimise at all :-(
        instructions().append(machine().getOpcode(op_resolve));
        instructions().append(dst->index());
        instructions().append(addConstant(property));
        return dst;
    }

    if (index == missingSymbolMarker()) {
        // In this case we are at least able to drop a few scope chains from the
        // lookup chain, although we still need to hash from then on.
        instructions().append(machine().getOpcode(op_resolve_skip));
        instructions().append(dst->index());
        instructions().append(addConstant(property));
        instructions().append(depth);
        return dst;
    }

    // Directly index the property lookup across multiple scopes.  Yay!
    return emitGetScopedVar(dst, depth, index);
}
    
RegisterID* CodeGenerator::emitGetScopedVar(RegisterID* dst, size_t depth, int index)
{
    instructions().append(machine().getOpcode(op_get_scoped_var));
    instructions().append(dst->index());
    instructions().append(index);
    instructions().append(depth);
    return dst;
}
    
RegisterID* CodeGenerator::emitPutScopedVar(size_t depth, int index, RegisterID* value)
{
    instructions().append(machine().getOpcode(op_put_scoped_var));
    instructions().append(index);
    instructions().append(depth);
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitResolveBase(RegisterID* dst, const Identifier& property)
{
    instructions().append(machine().getOpcode(op_resolve_base));
    instructions().append(dst->index());
    instructions().append(addConstant(property));
    return dst;
}
    
RegisterID* CodeGenerator::emitResolveWithBase(RegisterID* baseDst, RegisterID* propDst, const Identifier& property)
{
    instructions().append(machine().getOpcode(op_resolve_with_base));
    instructions().append(baseDst->index());
    instructions().append(propDst->index());
    instructions().append(addConstant(property));
    return baseDst;
}

RegisterID* CodeGenerator::emitResolveFunction(RegisterID* baseDst, RegisterID* funcDst, const Identifier& property)
{
    instructions().append(machine().getOpcode(op_resolve_func));
    instructions().append(baseDst->index());
    instructions().append(funcDst->index());
    instructions().append(addConstant(property));
    return baseDst;
}

RegisterID* CodeGenerator::emitGetById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    instructions().append(machine().getOpcode(op_get_by_id));
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(addConstant(property));
    return dst;
}

RegisterID* CodeGenerator::emitPutById(RegisterID* base, const Identifier& property, RegisterID* value)
{
    instructions().append(machine().getOpcode(op_put_by_id));
    instructions().append(base->index());
    instructions().append(addConstant(property));
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitPutGetter(RegisterID* base, const Identifier& property, RegisterID* value)
{
    instructions().append(machine().getOpcode(op_put_getter));
    instructions().append(base->index());
    instructions().append(addConstant(property));
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitPutSetter(RegisterID* base, const Identifier& property, RegisterID* value)
{
    instructions().append(machine().getOpcode(op_put_setter));
    instructions().append(base->index());
    instructions().append(addConstant(property));
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitDeleteById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    instructions().append(machine().getOpcode(op_del_by_id));
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(addConstant(property));
    return dst;
}

RegisterID* CodeGenerator::emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    instructions().append(machine().getOpcode(op_get_by_val));
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(property->index());
    return dst;
}

RegisterID* CodeGenerator::emitPutByVal(RegisterID* base, RegisterID* property, RegisterID* value)
{
    instructions().append(machine().getOpcode(op_put_by_val));
    instructions().append(base->index());
    instructions().append(property->index());
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitDeleteByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    instructions().append(machine().getOpcode(op_del_by_val));
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(property->index());
    return dst;
}

RegisterID* CodeGenerator::emitPutByIndex(RegisterID* base, unsigned index, RegisterID* value)
{
    instructions().append(machine().getOpcode(op_put_by_index));
    instructions().append(base->index());
    instructions().append(index);
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitNewFunction(RegisterID* r0, FuncDeclNode* n)
{
    instructions().append(machine().getOpcode(op_new_func));
    instructions().append(r0->index());
    instructions().append(addConstant(n));
    return r0;
}

RegisterID* CodeGenerator::emitNewRegExp(RegisterID* dst, RegExp* regExp)
{
    instructions().append(machine().getOpcode(op_new_regexp));
    instructions().append(dst->index());
    instructions().append(addRegExp(regExp));
    return dst;
}


RegisterID* CodeGenerator::emitNewFunctionExpression(RegisterID* r0, FuncExprNode* n)
{
    instructions().append(machine().getOpcode(op_new_func_exp));
    instructions().append(r0->index());
    instructions().append(addConstant(n));
    return r0;
}

RegisterID* CodeGenerator::emitCall(RegisterID* dst, RegisterID* func, RegisterID* base, ArgumentsNode* argumentsNode)
{
    return emitCall(op_call, dst, func, base, argumentsNode);
}

RegisterID* CodeGenerator::emitCallEval(RegisterID* dst, RegisterID* func, RegisterID* base, ArgumentsNode* argumentsNode)
{
    return emitCall(op_call_eval, dst, func, base, argumentsNode);
}

RegisterID* CodeGenerator::emitCall(OpcodeID opcodeID, RegisterID* dst, RegisterID* func, RegisterID* base, ArgumentsNode* argumentsNode)
{
    ASSERT(opcodeID == op_call || opcodeID == op_call_eval);

    RefPtr<RegisterID> refFunc = func;
    RefPtr<RegisterID> refBase = base;
    
    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, Machine::CallFrameHeaderSize> callFrame;
    for (int i = 0; i < Machine::CallFrameHeaderSize; ++i)
        callFrame.append(newTemporary());

    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    argv.append(newTemporary()); // reserve space for "this"
    for (ArgumentListNode* n = argumentsNode->m_listNode.get(); n; n = n->m_next.get()) {
        argv.append(newTemporary());
        emitNode(argv.last().get(), n);
    }

    instructions().append(machine().getOpcode(opcodeID));
    instructions().append(dst->index());
    instructions().append(func->index());
    instructions().append(base ? base->index() : missingThisObjectMarker()); // We encode the "this" value in the instruction stream, to avoid an explicit instruction for copying or loading it.
    instructions().append(argv.size() ? argv[0]->index() : m_temporaries.size()); // argv
    instructions().append(argv.size()); // argc
    return dst;
}

RegisterID* CodeGenerator::emitReturn(RegisterID* r0)
{
    instructions().append(machine().getOpcode(op_ret));
    instructions().append(r0->index());
    return r0;
}

RegisterID* CodeGenerator::emitEnd(RegisterID* dst)
{
    instructions().append(machine().getOpcode(op_end));
    instructions().append(dst->index());
    return dst;
}

RegisterID* CodeGenerator::emitConstruct(RegisterID* dst, RegisterID* func, ArgumentsNode* argumentsNode)
{
    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, Machine::CallFrameHeaderSize> callFrame;
    for (int i = 0; i < Machine::CallFrameHeaderSize; ++i)
        callFrame.append(newTemporary());

    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    argv.append(newTemporary()); // reserve space for "this"
    for (ArgumentListNode* n = argumentsNode ? argumentsNode->m_listNode.get() : 0; n; n = n->m_next.get()) {
        argv.append(newTemporary());
        emitNode(argv.last().get(), n);
    }

    instructions().append(machine().getOpcode(op_construct));
    instructions().append(dst->index());
    instructions().append(func->index());
    instructions().append(argv.size() ? argv[0]->index() : m_temporaries.size()); // argv
    instructions().append(argv.size()); // argc
    return dst;
}

RegisterID* CodeGenerator::emitPushScope(RegisterID* scope)
{
    m_codeBlock->needsFullScopeChain = true;
    instructions().append(machine().getOpcode(op_push_scope));
    instructions().append(scope->index());

    ControlFlowContext context;
    context.isFinallyBlock = false;
    m_scopeContextStack.append(context);
    m_dynamicScopeDepth++;
    return scope;
}

void CodeGenerator::emitPopScope()
{
    ASSERT(m_scopeContextStack.size());
    ASSERT(!m_scopeContextStack.last().isFinallyBlock);

    instructions().append(machine().getOpcode(op_pop_scope));

    m_scopeContextStack.removeLast();
    m_dynamicScopeDepth--;
}

void CodeGenerator::emitDebugHook(DebugHookID debugHookID, int firstLine, int lastLine)
{
    if (!m_shouldEmitDebugHooks)
        return;
    instructions().append(machine().getOpcode(op_debug));
    instructions().append(debugHookID);
    instructions().append(firstLine);
    instructions().append(lastLine);
}

void CodeGenerator::pushFinallyContext(LabelID* target, RegisterID* retAddrDst)
{
    ControlFlowContext scope;
    scope.isFinallyBlock = true;
    FinallyContext context = { target, retAddrDst };
    scope.finallyContext = context;
    m_scopeContextStack.append(scope);
    m_finallyDepth++;
}

void CodeGenerator::popFinallyContext()
{
    ASSERT(m_scopeContextStack.size());
    ASSERT(m_scopeContextStack.last().isFinallyBlock);
    ASSERT(m_finallyDepth > 0);
    m_scopeContextStack.removeLast();
    m_finallyDepth--;
}

void CodeGenerator::pushJumpContext(LabelStack* labels, LabelID* continueTarget, LabelID* breakTarget, bool isValidUnlabeledBreakTarget)
{
    JumpContext context = { labels, continueTarget, breakTarget, scopeDepth(), isValidUnlabeledBreakTarget };
    m_jumpContextStack.append(context);
    if (continueTarget)
        m_continueDepth++;
}

void CodeGenerator::popJumpContext()
{
    ASSERT(m_jumpContextStack.size());
    if (m_jumpContextStack.last().continueTarget)
        m_continueDepth--;
    m_jumpContextStack.removeLast();
}

JumpContext* CodeGenerator::jumpContextForContinue(const Identifier& label)
{
    if(!m_jumpContextStack.size())
        return 0;

    if (label.isEmpty()) {
        for (int i = m_jumpContextStack.size() - 1; i >= 0; i--) {
            JumpContext* scope = &m_jumpContextStack[i];
            if (scope->continueTarget)
                return scope;
        }
        return 0;
    }
    
    for (int i = m_jumpContextStack.size() - 1; i >= 0; i--) {
        JumpContext* scope = &m_jumpContextStack[i];
        if (scope->labels->contains(label))
            return scope;
    }
    return 0;
}

JumpContext* CodeGenerator::jumpContextForBreak(const Identifier& label)
{
    if(!m_jumpContextStack.size())
        return 0;

    if (label.isEmpty()) {
        for (int i = m_jumpContextStack.size() - 1; i >= 0; i--) {
            JumpContext* scope = &m_jumpContextStack[i];
            if (scope->isValidUnlabeledBreakTarget)
                return scope;
        }
        return 0;
    }
    
    for (int i = m_jumpContextStack.size() - 1; i >= 0; i--) {
        JumpContext* scope = &m_jumpContextStack[i];
        if (scope->labels->contains(label))
            return scope;
    }
    return 0;
}

PassRefPtr<LabelID> CodeGenerator::emitComplexJumpScopes(LabelID* target, ControlFlowContext* topScope, ControlFlowContext* bottomScope)
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
            instructions().append(machine().getOpcode(op_jmp_scopes));
            instructions().append(nNormalScopes);
            
            // If topScope == bottomScope then there isn't actually a finally block
            // left to emit, so make the jmp_scopes jump directly to the target label
            if (topScope == bottomScope) {
                instructions().append(target->offsetFrom(instructions().size()));
                return target;
            }

            // Otherwise we just use jmp_scopes to pop a group of scopes and go 
            // to the next instruction
            RefPtr<LabelID> nextInsn = newLabel();
            instructions().append(nextInsn->offsetFrom(instructions().size()));
            emitLabel(nextInsn.get());
        }

        // To get here there must be at least one finally block present
        do {
            ASSERT(topScope->isFinallyBlock);
            emitJumpSubroutine(topScope->finallyContext.retAddrDst, topScope->finallyContext.finallyAddr);
            --topScope;
            if (!topScope->isFinallyBlock)
                break;
        } while (topScope > bottomScope);
    }
    return emitJump(target);
}

PassRefPtr<LabelID> CodeGenerator::emitJumpScopes(LabelID* target, int targetScopeDepth)
{
    ASSERT(scopeDepth() - targetScopeDepth >= 0);

    size_t scopeDelta = scopeDepth() - targetScopeDepth;
    ASSERT(scopeDelta <= m_scopeContextStack.size());
    if (!scopeDelta)
        return emitJump(target);

    if (m_finallyDepth)
        return emitComplexJumpScopes(target, &m_scopeContextStack.last(), &m_scopeContextStack.last() - scopeDelta);

    instructions().append(machine().getOpcode(op_jmp_scopes));
    instructions().append(scopeDelta);
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

RegisterID* CodeGenerator::emitNextPropertyName(RegisterID* dst, RegisterID* iter, LabelID* target)
{
    instructions().append(machine().getOpcode(op_next_pname));
    instructions().append(dst->index());
    instructions().append(iter->index());
    instructions().append(target->offsetFrom(instructions().size()));
    return dst;
}

RegisterID* CodeGenerator::emitGetPropertyNames(RegisterID* dst, RegisterID* base)
{
    instructions().append(machine().getOpcode(op_get_pnames));
    instructions().append(dst->index());
    instructions().append(base->index());
    return dst;
}

RegisterID* CodeGenerator::emitCatch(RegisterID* targetRegister, LabelID* start, LabelID* end)
{
    HandlerInfo info = { start->offsetFrom(0), end->offsetFrom(0), instructions().size(), m_dynamicScopeDepth };
    exceptionHandlers().append(info);
    instructions().append(machine().getOpcode(op_catch));
    instructions().append(targetRegister->index());
    return targetRegister;
}
    
void CodeGenerator::emitThrow(RegisterID* exception)
{
    instructions().append(machine().getOpcode(op_throw));
    instructions().append(exception->index());
}

RegisterID* CodeGenerator::emitNewError(RegisterID* dst, ErrorType type, JSValue* message)
{
    instructions().append(machine().getOpcode(op_new_error));
    instructions().append(dst->index());
    instructions().append(static_cast<int>(type));
    instructions().append(addConstant(message));
    return dst;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpSubroutine(RegisterID* retAddrDst, LabelID* finally)
{
    instructions().append(machine().getOpcode(op_jsr));
    instructions().append(retAddrDst->index());
    instructions().append(finally->offsetFrom(instructions().size()));
    return finally;
}

void CodeGenerator::emitSubroutineReturn(RegisterID* retAddrSrc)
{
    instructions().append(machine().getOpcode(op_sret));
    instructions().append(retAddrSrc->index());
}

} // namespace KJS

/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FTLOutput.h"

#if ENABLE(FTL_JIT)

namespace JSC { namespace FTL {

Output::Output(LContext context)
    : IntrinsicRepository(context)
    , m_function(0)
    , m_heaps(0)
    , m_builder(llvm->CreateBuilderInContext(m_context))
    , m_block(0)
    , m_nextBlock(0)
{
}

Output::~Output()
{
    llvm->DisposeBuilder(m_builder);
}

void Output::initialize(LModule module, LValue function, AbstractHeapRepository& heaps)
{
    IntrinsicRepository::initialize(module);
    m_function = function;
    m_heaps = &heaps;
}

LBasicBlock Output::appendTo(LBasicBlock block, LBasicBlock nextBlock)
{
    appendTo(block);
    return insertNewBlocksBefore(nextBlock);
}

void Output::appendTo(LBasicBlock block)
{
    m_block = block;
    
    llvm->PositionBuilderAtEnd(m_builder, block);
}

LBasicBlock Output::newBlock(const char* name)
{
    if (!m_nextBlock)
        return appendBasicBlock(m_context, m_function, name);
    return insertBasicBlock(m_context, m_nextBlock, name);
}

LValue Output::sensibleDoubleToInt(LValue value)
{
    RELEASE_ASSERT(isX86());
    return call(
        x86SSE2CvtTSD2SIIntrinsic(),
        insertElement(
            insertElement(getUndef(vectorType(doubleType, 2)), value, int32Zero),
            doubleZero, int32One));
}

LValue Output::load(TypedPointer pointer, LType refType)
{
    LValue result = get(intToPtr(pointer.value(), refType));
    pointer.heap().decorateInstruction(result, *m_heaps);
    return result;
}

void Output::store(LValue value, TypedPointer pointer, LType refType)
{
    LValue result = set(value, intToPtr(pointer.value(), refType));
    pointer.heap().decorateInstruction(result, *m_heaps);
}

LValue Output::baseIndex(LValue base, LValue index, Scale scale, ptrdiff_t offset)
{
    LValue accumulatedOffset;
        
    switch (scale) {
    case ScaleOne:
        accumulatedOffset = index;
        break;
    case ScaleTwo:
        accumulatedOffset = shl(index, intPtrOne);
        break;
    case ScaleFour:
        accumulatedOffset = shl(index, intPtrTwo);
        break;
    case ScaleEight:
    case ScalePtr:
        accumulatedOffset = shl(index, intPtrThree);
        break;
    }
        
    if (offset)
        accumulatedOffset = add(accumulatedOffset, constIntPtr(offset));
        
    return add(base, accumulatedOffset);
}

void Output::branch(LValue condition, LBasicBlock taken, Weight takenWeight, LBasicBlock notTaken, Weight notTakenWeight)
{
    LValue branch = buildCondBr(m_builder, condition, taken, notTaken);
    
    if (!takenWeight || !notTakenWeight)
        return;
    
    double total = takenWeight.value() + notTakenWeight.value();
    
    setMetadata(
        branch, profKind,
        mdNode(
            m_context, branchWeights,
            constInt32(takenWeight.scaleToTotal(total)),
            constInt32(notTakenWeight.scaleToTotal(total))));
}

void Output::crashNonTerminal()
{
    call(intToPtr(constIntPtr(abort), pointerType(functionType(voidType))));
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)


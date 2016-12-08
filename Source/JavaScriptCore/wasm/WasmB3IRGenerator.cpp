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

#include "config.h"
#include "WasmB3IRGenerator.h"

#if ENABLE(WEBASSEMBLY)

#include "B3BasicBlockInlines.h"
#include "B3CCallValue.h"
#include "B3ConstPtrValue.h"
#include "B3FixSSA.h"
#include "B3StackmapGenerationParams.h"
#include "B3SwitchValue.h"
#include "B3Validate.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "B3WasmAddressValue.h"
#include "B3WasmBoundsCheckValue.h"
#include "VirtualRegister.h"
#include "WasmCallingConvention.h"
#include "WasmFunctionParser.h"
#include "WasmMemory.h"
#include <wtf/Optional.h>

void dumpProcedure(void* ptr)
{
    JSC::B3::Procedure* proc = static_cast<JSC::B3::Procedure*>(ptr);
    proc->dump(WTF::dataFile());
}

namespace JSC { namespace Wasm {

using namespace B3;

namespace {
const bool verbose = false;
}

class B3IRGenerator {
public:
    struct ControlData {
        ControlData(Procedure& proc, Type signature, BlockType type, BasicBlock* continuation, BasicBlock* special = nullptr)
            : blockType(type)
            , continuation(continuation)
            , special(special)
        {
            if (signature != Void)
                result.append(proc.addVariable(toB3Type(signature)));
        }

        ControlData()
        {
        }

        void dump(PrintStream& out) const
        {
            switch (type()) {
            case BlockType::If:
                out.print("If:    ");
                break;
            case BlockType::Block:
                out.print("Block: ");
                break;
            case BlockType::Loop:
                out.print("Loop:  ");
                break;
            }
            out.print("Continuation: ", *continuation, ", Special: ");
            if (special)
                out.print(*special);
            else
                out.print("None");
        }

        BlockType type() const { return blockType; }

        bool hasNonVoidSignature() const { return result.size(); }

        BasicBlock* targetBlockForBranch()
        {
            if (type() == BlockType::Loop)
                return special;
            return continuation;
        }

        void convertIfToBlock()
        {
            ASSERT(type() == BlockType::If);
            blockType = BlockType::Block;
            special = nullptr;
        }

    private:
        friend class B3IRGenerator;
        BlockType blockType;
        BasicBlock* continuation;
        BasicBlock* special;
        Vector<Variable*, 1> result;
    };

    typedef Value* ExpressionType;
    typedef ControlData ControlType;
    typedef Vector<ExpressionType, 1> ExpressionList;
    typedef Vector<Variable*, 1> ResultList;
    typedef FunctionParser<B3IRGenerator>::ControlEntry ControlEntry;

    static constexpr ExpressionType emptyExpression = nullptr;

    B3IRGenerator(Memory*, Procedure&, WasmInternalFunction*, Vector<UnlinkedWasmToWasmCall>&);

    bool WARN_UNUSED_RETURN addArguments(const Vector<Type>&);
    bool WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

    // Locals
    bool WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType& result);
    bool WARN_UNUSED_RETURN setLocal(uint32_t index, ExpressionType value);

    // Memory
    bool WARN_UNUSED_RETURN load(LoadOpType, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    bool WARN_UNUSED_RETURN store(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    // Basic operators
    template<OpType>
    bool WARN_UNUSED_RETURN addOp(ExpressionType arg, ExpressionType& result);
    template<OpType>
    bool WARN_UNUSED_RETURN addOp(ExpressionType left, ExpressionType right, ExpressionType& result);
    bool WARN_UNUSED_RETURN addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result);

    // Control flow
    ControlData WARN_UNUSED_RETURN addBlock(Type signature);
    ControlData WARN_UNUSED_RETURN addLoop(Type signature);
    bool WARN_UNUSED_RETURN addIf(ExpressionType condition, Type signature, ControlData& result);
    bool WARN_UNUSED_RETURN addElse(ControlData&, const ExpressionList&);
    bool WARN_UNUSED_RETURN addElseToUnreachable(ControlData&);

    bool WARN_UNUSED_RETURN addReturn(const ExpressionList& returnValues);
    bool WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const ExpressionList& returnValues);
    bool WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const ExpressionList& expressionStack);
    bool WARN_UNUSED_RETURN endBlock(ControlEntry&, ExpressionList& expressionStack);
    bool WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&);

    bool WARN_UNUSED_RETURN addCall(unsigned calleeIndex, const Signature*, Vector<ExpressionType>& args, ExpressionType& result);

    void dump(const Vector<ControlEntry>& controlStack, const ExpressionList& expressionStack);

    void setErrorMessage(String&&) { UNREACHABLE_FOR_PLATFORM(); }

private:
    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    ExpressionType emitLoadOp(LoadOpType, Origin, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, Origin, ExpressionType pointer, ExpressionType value, uint32_t offset);

    void unify(Variable* target, const ExpressionType source);
    void unifyValuesWithBlock(const ExpressionList& resultStack, ResultList& stack);
    Value* zeroForType(Type);

    Memory* m_memory;
    Procedure& m_proc;
    BasicBlock* m_currentBlock;
    Vector<Variable*> m_locals;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.
    GPRReg m_memoryBaseGPR;
    GPRReg m_memorySizeGPR;
    Value* m_zeroValues[numTypes];
};

B3IRGenerator::B3IRGenerator(Memory* memory, Procedure& procedure, WasmInternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls)
    : m_memory(memory)
    , m_proc(procedure)
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
{
    m_currentBlock = m_proc.addBlock();

    for (unsigned i = 0; i < numTypes; ++i) {
        switch (B3::Type b3Type = toB3Type(linearizedToType(i))) {
        case B3::Int32:
        case B3::Int64:
        case B3::Float:
        case B3::Double:
            m_zeroValues[i] = m_currentBlock->appendIntConstant(m_proc, Origin(), b3Type, 0);
            break;
        case B3::Void:
            m_zeroValues[i] = nullptr;
            break;
        }
    }

    if (m_memory) {
        m_memoryBaseGPR = m_memory->pinnedRegisters().baseMemoryPointer;
        m_proc.pinRegister(m_memoryBaseGPR);
        ASSERT(!m_memory->pinnedRegisters().sizeRegisters[0].sizeOffset);
        m_memorySizeGPR = m_memory->pinnedRegisters().sizeRegisters[0].sizeRegister;
        for (const PinnedSizeRegisterInfo& info : m_memory->pinnedRegisters().sizeRegisters)
            m_proc.pinRegister(info.sizeRegister);

        m_proc.setWasmBoundsCheckGenerator([=] (CCallHelpers& jit, GPRReg pinnedGPR, unsigned) {
            ASSERT_UNUSED(pinnedGPR, m_memorySizeGPR == pinnedGPR);
            // FIXME: This should unwind the stack and throw a JS exception. See: https://bugs.webkit.org/show_bug.cgi?id=163351
            jit.breakpoint();
        });
    }

    wasmCallingConvention().setupFrameInPrologue(compilation, m_proc, Origin(), m_currentBlock);
}

Value* B3IRGenerator::zeroForType(Type type)
{
    ASSERT(type != Void);
    Value* zeroValue = m_zeroValues[linearizeType(type)];
    ASSERT(zeroValue);
    return zeroValue;
}

bool B3IRGenerator::addLocal(Type type, uint32_t count)
{
    if (!m_locals.tryReserveCapacity(m_locals.size() + count))
        return false;

    for (uint32_t i = 0; i < count; ++i) {
        Variable* local = m_proc.addVariable(toB3Type(type));
        m_locals.uncheckedAppend(local);
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, zeroForType(type));
    }
    return true;
}

bool B3IRGenerator::addArguments(const Vector<Type>& types)
{
    ASSERT(!m_locals.size());
    if (!m_locals.tryReserveCapacity(types.size()))
        return false;

    m_locals.grow(types.size());
    wasmCallingConvention().loadArguments(types, m_proc, m_currentBlock, Origin(),
        [&] (ExpressionType argument, unsigned i) {
            Variable* argumentVariable = m_proc.addVariable(argument->type());
            m_locals[i] = argumentVariable;
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), argumentVariable, argument);
        });
    return true;
}

bool B3IRGenerator::getLocal(uint32_t index, ExpressionType& result)
{
    ASSERT(m_locals[index]);
    result = m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, Origin(), m_locals[index]);
    return true;
}

bool B3IRGenerator::setLocal(uint32_t index, ExpressionType value)
{
    ASSERT(m_locals[index]);
    m_currentBlock->appendNew<VariableValue>(m_proc, B3::Set, Origin(), m_locals[index], value);
    return true;
}

inline Value* B3IRGenerator::emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOperation)
{
    ASSERT(m_memoryBaseGPR && m_memorySizeGPR);
    ASSERT(sizeOfOperation + offset > offset);
    m_currentBlock->appendNew<WasmBoundsCheckValue>(m_proc, Origin(), pointer, m_memorySizeGPR, sizeOfOperation + offset - 1);
    pointer = m_currentBlock->appendNew<Value>(m_proc, ZExt32, Origin(), pointer);
    return m_currentBlock->appendNew<WasmAddressValue>(m_proc, Origin(), pointer, m_memoryBaseGPR);
}

inline uint32_t sizeOfLoadOp(LoadOpType op)
{
    switch (op) {
    case LoadOpType::I32Load8S:
    case LoadOpType::I32Load8U:
    case LoadOpType::I64Load8S:
    case LoadOpType::I64Load8U:
        return 1;
    case LoadOpType::I32Load16S:
    case LoadOpType::I64Load16S:
        return 2;
    case LoadOpType::I32Load:
    case LoadOpType::I64Load32S:
    case LoadOpType::I64Load32U:
        return 4;
    case LoadOpType::I64Load:
        return 8;
    case LoadOpType::I32Load16U:
    case LoadOpType::I64Load16U:
    case LoadOpType::F32Load:
    case LoadOpType::F64Load:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Value* B3IRGenerator::emitLoadOp(LoadOpType op, Origin origin, ExpressionType pointer, uint32_t offset)
{
    switch (op) {
    case LoadOpType::I32Load8S: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load8S, origin, pointer, offset);
    }

    case LoadOpType::I64Load8S: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8S, origin, pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin, value);
    }

    case LoadOpType::I32Load8U: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, origin, pointer, offset);
    }

    case LoadOpType::I64Load8U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, origin, pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin, value);
    }

    case LoadOpType::I32Load16S: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load16S, origin, pointer, offset);
    }
    case LoadOpType::I64Load16S: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load16S, origin, pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin, value);
    }

    case LoadOpType::I32Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin, pointer);
    }

    case LoadOpType::I64Load32U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin, pointer);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin, value);
    }

    case LoadOpType::I64Load32S: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin, pointer);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin, value);
    }

    case LoadOpType::I64Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin, pointer);
    }

    case LoadOpType::F32Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Float, origin, pointer);
    }

    case LoadOpType::F64Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Double, origin, pointer);
    }

    // B3 doesn't support Load16Z yet.
    case LoadOpType::I32Load16U:
    case LoadOpType::I64Load16U:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool B3IRGenerator::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t offset)
{
    ASSERT(pointer->type() == Int32);

    result = emitLoadOp(op, Origin(), emitCheckAndPreparePointer(pointer, offset, sizeOfLoadOp(op)), offset);
    return true;
}

inline uint32_t sizeOfStoreOp(StoreOpType op)
{
    switch (op) {
    case StoreOpType::I32Store8:
    case StoreOpType::I64Store8:
        return 1;
    case StoreOpType::I32Store16:
    case StoreOpType::I64Store16:
        return 2;
    case StoreOpType::I32Store:
    case StoreOpType::I64Store32:
    case StoreOpType::F32Store:
        return 4;
    case StoreOpType::I64Store:
    case StoreOpType::F64Store:
        return 8;
    }
    RELEASE_ASSERT_NOT_REACHED();
}


inline void B3IRGenerator::emitStoreOp(StoreOpType op, Origin origin, ExpressionType pointer, ExpressionType value, uint32_t offset)
{
    switch (op) {
    case StoreOpType::I64Store8:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin, value);
        FALLTHROUGH;

    case StoreOpType::I32Store8:
        m_currentBlock->appendNew<MemoryValue>(m_proc, Store8, origin, value, pointer, offset);
        return;

    case StoreOpType::I64Store16:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin, value);
        FALLTHROUGH;

    case StoreOpType::I32Store16:
        m_currentBlock->appendNew<MemoryValue>(m_proc, Store16, origin, value, pointer, offset);
        return;

    case StoreOpType::I64Store32:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin, value);
        FALLTHROUGH;

    case StoreOpType::I64Store:
    case StoreOpType::I32Store:
    case StoreOpType::F32Store:
    case StoreOpType::F64Store:
        m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin, value, pointer, offset);
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool B3IRGenerator::store(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t offset)
{
    ASSERT(pointer->type() == Int32);

    emitStoreOp(op, Origin(), emitCheckAndPreparePointer(pointer, offset, sizeOfStoreOp(op)), value, offset);
    return true;
}

bool B3IRGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result)
{
    result = m_currentBlock->appendNew<Value>(m_proc, B3::Select, Origin(), condition, nonZero, zero);
    return true;
}

B3IRGenerator::ExpressionType B3IRGenerator::addConstant(Type type, uint64_t value)
{
    switch (type) {
    case Wasm::I32:
        return m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), static_cast<int32_t>(value));
    case Wasm::I64:
        return m_currentBlock->appendNew<Const64Value>(m_proc, Origin(), value);
    case Wasm::F32:
        return m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), bitwise_cast<float>(static_cast<int32_t>(value)));
    case Wasm::F64:
        return m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), bitwise_cast<double>(value));
    case Wasm::Void:
    case Wasm::Func:
    case Wasm::Anyfunc:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

B3IRGenerator::ControlData B3IRGenerator::addBlock(Type signature)
{
    return ControlData(m_proc, signature, BlockType::Block, m_proc.addBlock());
}

B3IRGenerator::ControlData B3IRGenerator::addLoop(Type signature)
{
    BasicBlock* body = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), body);
    body->addPredecessor(m_currentBlock);
    m_currentBlock = body;
    return ControlData(m_proc, signature, BlockType::Loop, continuation, body);
}

bool B3IRGenerator::addIf(ExpressionType condition, Type signature, ControlType& result)
{
    // FIXME: This needs to do some kind of stack passing.

    BasicBlock* taken = m_proc.addBlock();
    BasicBlock* notTaken = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    m_currentBlock->appendNew<Value>(m_proc, B3::Branch, Origin(), condition);
    m_currentBlock->setSuccessors(FrequentedBlock(taken), FrequentedBlock(notTaken));
    taken->addPredecessor(m_currentBlock);
    notTaken->addPredecessor(m_currentBlock);

    m_currentBlock = taken;
    result = ControlData(m_proc, signature, BlockType::If, continuation, notTaken);
    return true;
}

bool B3IRGenerator::addElse(ControlData& data, const ExpressionList& currentStack)
{
    unifyValuesWithBlock(currentStack, data.result);
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), data.continuation);
    return addElseToUnreachable(data);
}

bool B3IRGenerator::addElseToUnreachable(ControlData& data)
{
    ASSERT(data.type() == BlockType::If);
    m_currentBlock = data.special;
    data.convertIfToBlock();
    return true;
}

bool B3IRGenerator::addReturn(const ExpressionList& returnValues)
{
    ASSERT(returnValues.size() <= 1);
    if (returnValues.size())
        m_currentBlock->appendNewControlValue(m_proc, B3::Return, Origin(), returnValues[0]);
    else
        m_currentBlock->appendNewControlValue(m_proc, B3::Return, Origin());
    return true;
}

bool B3IRGenerator::addBranch(ControlData& data, ExpressionType condition, const ExpressionList& returnValues)
{
    if (data.type() != BlockType::Loop)
        unifyValuesWithBlock(returnValues, data.result);

    BasicBlock* target = data.targetBlockForBranch();
    if (condition) {
        BasicBlock* continuation = m_proc.addBlock();
        m_currentBlock->appendNew<Value>(m_proc, B3::Branch, Origin(), condition);
        m_currentBlock->setSuccessors(FrequentedBlock(target), FrequentedBlock(continuation));
        target->addPredecessor(m_currentBlock);
        continuation->addPredecessor(m_currentBlock);
        m_currentBlock = continuation;
    } else {
        m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), FrequentedBlock(target));
        target->addPredecessor(m_currentBlock);
    }

    return true;
}

bool B3IRGenerator::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const ExpressionList& expressionStack)
{
    for (size_t i = 0; i < targets.size(); ++i)
        unifyValuesWithBlock(expressionStack, targets[i]->result);
    unifyValuesWithBlock(expressionStack, defaultTarget.result);

    SwitchValue* switchValue = m_currentBlock->appendNew<SwitchValue>(m_proc, Origin(), condition);
    switchValue->setFallThrough(FrequentedBlock(defaultTarget.targetBlockForBranch()));
    for (size_t i = 0; i < targets.size(); ++i)
        switchValue->appendCase(SwitchCase(i, FrequentedBlock(targets[i]->targetBlockForBranch())));

    return true;
}

bool B3IRGenerator::endBlock(ControlEntry& entry, ExpressionList& expressionStack)
{
    ControlData& data = entry.controlData;

    unifyValuesWithBlock(expressionStack, data.result);
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), data.continuation);
    data.continuation->addPredecessor(m_currentBlock);

    return addEndToUnreachable(entry);
}


bool B3IRGenerator::addEndToUnreachable(ControlEntry& entry)
{
    ControlData& data = entry.controlData;
    m_currentBlock = data.continuation;

    if (data.type() == BlockType::If) {
        data.special->appendNewControlValue(m_proc, Jump, Origin(), m_currentBlock);
        m_currentBlock->addPredecessor(data.special);
    }

    for (Variable* result : data.result)
        entry.enclosedExpressionStack.append(m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, Origin(), result));

    return true;
}

bool B3IRGenerator::addCall(unsigned functionIndex, const Signature* signature, Vector<ExpressionType>& args, ExpressionType& result)
{
    ASSERT(signature->arguments.size() == args.size());

    Type returnType = signature->returnType;

    size_t callIndex = m_unlinkedWasmToWasmCalls.size();
    m_unlinkedWasmToWasmCalls.grow(callIndex + 1);
    result = wasmCallingConvention().setupCall(m_proc, m_currentBlock, Origin(), args, toB3Type(returnType),
        [&] (PatchpointValue* patchpoint) {
            patchpoint->effects.writesPinned = true;
            patchpoint->effects.readsPinned = true;

            patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                AllowMacroScratchRegisterUsage allowScratch(jit);

                CCallHelpers::Call call = jit.call();

                jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                    m_unlinkedWasmToWasmCalls[callIndex] = { linkBuffer.locationOf(call), functionIndex };
                });
            });
        });
    return true;
}

void B3IRGenerator::unify(Variable* variable, ExpressionType source)
{
    m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), variable, source);
}

void B3IRGenerator::unifyValuesWithBlock(const ExpressionList& resultStack, ResultList& result)
{
    ASSERT(result.size() <= resultStack.size());

    for (size_t i = 0; i < result.size(); ++i)
        unify(result[result.size() - 1 - i], resultStack[resultStack.size() - 1 - i]);
}

static void dumpExpressionStack(const CommaPrinter& comma, const B3IRGenerator::ExpressionList& expressionStack)
{
    dataLogLn(comma, "ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLogLn(comma, *expression);
}

void B3IRGenerator::dump(const Vector<ControlEntry>& controlStack, const ExpressionList& expressionStack)
{
    dataLogLn("Processing Graph:");
    dataLog(m_proc);
    dataLogLn("With current block:", *m_currentBlock);
    dataLogLn("Control stack:");
    for (auto& data : controlStack) {
        dataLogLn("  ", data.controlData);
        if (data.enclosedExpressionStack.size()) {
            CommaPrinter comma("    ", "  with ");
            dumpExpressionStack(comma, data.enclosedExpressionStack);
        }
    }

    CommaPrinter comma("  ", "");
    dumpExpressionStack(comma, expressionStack);
    dataLogLn("\n");
}

static std::unique_ptr<Compilation> createJSToWasmWrapper(VM& vm, const Signature* signature, MacroAssemblerCodePtr mainFunction, Memory* memory)
{
    Procedure proc;
    BasicBlock* block = proc.addBlock();

    // Check argument count is sane.
    Value* framePointer = block->appendNew<B3::Value>(proc, B3::FramePointer, Origin());
    Value* offSetOfArgumentCount = block->appendNew<Const64Value>(proc, Origin(), CallFrameSlot::argumentCount * sizeof(Register));
    Value* argumentCount = block->appendNew<MemoryValue>(proc, Load, Int32, Origin(),
        block->appendNew<Value>(proc, Add, Origin(), framePointer, offSetOfArgumentCount));

    Value* expectedArgumentCount = block->appendNew<Const32Value>(proc, Origin(), signature->arguments.size());

    CheckValue* argumentCountCheck = block->appendNew<CheckValue>(proc, Check, Origin(),
        block->appendNew<Value>(proc, Above, Origin(), expectedArgumentCount, argumentCount));
    argumentCountCheck->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });

    // Move memory values to the approriate places, if needed.
    Value* baseMemory = nullptr;
    Vector<Value*> sizes;
    if (memory) {
        baseMemory = block->appendNew<ConstPtrValue>(proc, Origin(), memory->memory());
        Value* size = block->appendNew<MemoryValue>(proc, Load, Int32, Origin(),
            block->appendNew<ConstPtrValue>(proc, Origin(), bitwise_cast<char*>(memory) + Memory::offsetOfSize()));
        sizes.reserveCapacity(memory->pinnedRegisters().sizeRegisters.size());
        for (auto info : memory->pinnedRegisters().sizeRegisters) {
            sizes.append(block->appendNew<Value>(proc, Sub, Origin(), size,
                block->appendNew<Const32Value>(proc, Origin(), info.sizeOffset)));
        }
    }

    // Get our arguments.
    Vector<Value*> arguments;
    jscCallingConvention().loadArguments(signature->arguments, proc, block, Origin(), [&] (Value* argument, unsigned) {
        arguments.append(argument);
    });

    // Move the arguments into place.
    Value* result = wasmCallingConvention().setupCall(proc, block, Origin(), arguments, toB3Type(signature->returnType), [&] (PatchpointValue* patchpoint) {
        if (memory) {
            ASSERT(sizes.size() == memory->pinnedRegisters().sizeRegisters.size());
            patchpoint->append(ConstrainedValue(baseMemory, ValueRep::reg(memory->pinnedRegisters().baseMemoryPointer)));
            for (unsigned i = 0; i < sizes.size(); ++i)
                patchpoint->append(ConstrainedValue(sizes[i], ValueRep::reg(memory->pinnedRegisters().sizeRegisters[i].sizeRegister)));
        }

        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);

            CCallHelpers::Call call = jit.call();
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(call, FunctionPtr(mainFunction.executableAddress()));
            });
        });
    });

    // Return the result, if needed.
    switch (signature->returnType) {
    case Wasm::Void:
        block->appendNewControlValue(proc, B3::Return, Origin());
        break;
    case Wasm::F32:
    case Wasm::F64:
        result = block->appendNew<Value>(proc, BitwiseCast, Origin(), result);
        FALLTHROUGH;
    case Wasm::I32:
    case Wasm::I64:
        block->appendNewControlValue(proc, B3::Return, Origin(), result);
        break;
    case Wasm::Func:
    case Wasm::Anyfunc:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return std::make_unique<Compilation>(vm, proc);
}

std::unique_ptr<WasmInternalFunction> parseAndCompile(VM& vm, const uint8_t* functionStart, size_t functionLength, Memory* memory, const Signature* signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const FunctionIndexSpace& functionIndexSpace, unsigned optLevel)
{
    auto result = std::make_unique<WasmInternalFunction>();

    Procedure procedure;
    B3IRGenerator context(memory, procedure, result.get(), unlinkedWasmToWasmCalls);
    FunctionParser<B3IRGenerator> parser(context, functionStart, functionLength, signature, functionIndexSpace);
    if (!parser.parse())
        RELEASE_ASSERT_NOT_REACHED();

    procedure.resetReachability();
    validate(procedure, "After parsing:\n");

    if (verbose)
        dataLog("Pre SSA: ", procedure);
    fixSSA(procedure);
    if (verbose)
        dataLog("Post SSA: ", procedure);

    result->code = std::make_unique<Compilation>(vm, procedure, optLevel);
    result->jsToWasmEntryPoint = createJSToWasmWrapper(vm, signature, result->code->code(), memory);
    return result;
}

// Custom wasm ops. These are the ones too messy to do in wasm.json.

template<>
bool B3IRGenerator::addOp<OpType::I32Ctz>(ExpressionType arg, ExpressionType& result)
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I64Ctz>(ExpressionType arg, ExpressionType& result)
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros64(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I32Popcnt>(ExpressionType arg, ExpressionType& result)
{
    // FIXME: This should use the popcnt instruction if SSE4 is available but we don't have code to detect SSE4 yet.
    // see: https://bugs.webkit.org/show_bug.cgi?id=165363
    uint32_t (*popcount)(int32_t) = [] (int32_t value) -> uint32_t { return __builtin_popcount(value); };
    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, Origin(), bitwise_cast<void*>(popcount));
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, Origin(), Effects::none(), funcAddress, arg);
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I64Popcnt>(ExpressionType arg, ExpressionType& result)
{
    // FIXME: This should use the popcnt instruction if SSE4 is available but we don't have code to detect SSE4 yet.
    // see: https://bugs.webkit.org/show_bug.cgi?id=165363
    uint64_t (*popcount)(int64_t) = [] (int64_t value) -> uint64_t { return __builtin_popcountll(value); };
    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, Origin(), bitwise_cast<void*>(popcount));
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int64, Origin(), Effects::none(), funcAddress, arg);
    return true;
}

template<>
bool B3IRGenerator::addOp<F64ConvertUI64>(ExpressionType arg, ExpressionType& result)
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, Origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->append(ConstrainedValue(arg, ValueRep::WarmAny));
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
#if CPU(X86_64)
        jit.convertUInt64ToDouble(params[1].gpr(), params[0].fpr(), params.gpScratch(0));
#else
        jit.convertUInt64ToDouble(params[1].gpr(), params[0].fpr());
#endif
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::F32ConvertUI64>(ExpressionType arg, ExpressionType& result)
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, Origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->append(ConstrainedValue(arg, ValueRep::WarmAny));
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
#if CPU(X86_64)
        jit.convertUInt64ToFloat(params[1].gpr(), params[0].fpr(), params.gpScratch(0));
#else
        jit.convertUInt64ToFloat(params[1].gpr(), params[0].fpr());
#endif
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::F64Nearest>(ExpressionType arg, ExpressionType& result)
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardNearestIntDouble(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::F32Nearest>(ExpressionType arg, ExpressionType& result)
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardNearestIntFloat(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::F64Trunc>(ExpressionType arg, ExpressionType& result)
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardZeroDouble(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::F32Trunc>(ExpressionType arg, ExpressionType& result)
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardZeroFloat(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I32TruncSF64>(ExpressionType arg, ExpressionType& result)
{
    Value* max = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), -static_cast<double>(std::numeric_limits<int32_t>::min()));
    Value* min = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<int32_t>::min()));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I32TruncSF32>(ExpressionType arg, ExpressionType& result)
{
    Value* max = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), -static_cast<float>(std::numeric_limits<int32_t>::min()));
    Value* min = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<int32_t>::min()));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}


template<>
bool B3IRGenerator::addOp<OpType::I32TruncUF64>(ExpressionType arg, ExpressionType& result)
{
    Value* max = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0);
    Value* min = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), -1.0);
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I32TruncUF32>(ExpressionType arg, ExpressionType& result)
{
    Value* max = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<int32_t>::min()) * -2.0);
    Value* min = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), -1.0);
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I64TruncSF64>(ExpressionType arg, ExpressionType& result)
{
    Value* max = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), -static_cast<double>(std::numeric_limits<int64_t>::min()));
    Value* min = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<int64_t>::min()));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I64TruncUF64>(ExpressionType arg, ExpressionType& result)
{
    Value* max = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0);
    Value* min = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), -1.0);
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });

    Value* constant;
    if (isX86()) {
        // Since x86 doesn't have an instruction to convert floating points to unsigned integers, we at least try to do the smart thing if
        // the numbers are would be positive anyway as a signed integer. Since we cannot materialize constants into fprs we have b3 do it
        // so we can pool them if needed.
        constant = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max()));
    }
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    if (isX86()) {
        patchpoint->append(constant, ValueRep::SomeRegister);
        patchpoint->numFPScratchRegisters = 1;
    }
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        FPRReg scratch = InvalidFPRReg;
        FPRReg constant = InvalidFPRReg;
        if (isX86()) {
            scratch = params.fpScratch(0);
            constant = params[2].fpr();
        }
        jit.truncateDoubleToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I64TruncSF32>(ExpressionType arg, ExpressionType& result)
{
    Value* max = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), -static_cast<float>(std::numeric_limits<int64_t>::min()));
    Value* min = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<int64_t>::min()));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

template<>
bool B3IRGenerator::addOp<OpType::I64TruncUF32>(ExpressionType arg, ExpressionType& result)
{
    Value* max = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<int64_t>::min()) * -2.0);
    Value* min = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), -1.0);
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
        jit.breakpoint();
    });

    Value* constant;
    if (isX86()) {
        // Since x86 doesn't have an instruction to convert floating points to unsigned integers, we at least try to do the smart thing if
        // the numbers are would be positive anyway as a signed integer. Since we cannot materialize constants into fprs we have b3 do it
        // so we can pool them if needed.
        constant = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max()));
    }
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    if (isX86()) {
        patchpoint->append(constant, ValueRep::SomeRegister);
        patchpoint->numFPScratchRegisters = 1;
    }
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        FPRReg scratch = InvalidFPRReg;
        FPRReg constant = InvalidFPRReg;
        if (isX86()) {
            scratch = params.fpScratch(0);
            constant = params[2].fpr();
        }
        jit.truncateFloatToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return true;
}

} } // namespace JSC::Wasm

#include "WasmB3IRGeneratorInlines.h"

#endif // ENABLE(WEBASSEMBLY)

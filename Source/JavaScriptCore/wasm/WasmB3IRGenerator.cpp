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
#include "B3ConstPtrValue.h"
#include "B3FixSSA.h"
#include "B3StackmapGenerationParams.h"
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

namespace {

using namespace B3;

const bool verbose = false;

inline B3::Opcode toB3Op(BinaryOpType op)
{
    switch (op) {
#define CREATE_CASE(name, op, b3op) case BinaryOpType::name: return b3op;
    FOR_EACH_WASM_BINARY_OP(CREATE_CASE)
#undef CREATE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline B3::Opcode toB3Op(UnaryOpType op)
{
    switch (op) {
#define CREATE_CASE(name, op, b3op) case UnaryOpType::name: return b3op;
    FOR_EACH_WASM_UNARY_OP(CREATE_CASE)
#undef CREATE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

class B3IRGenerator {
private:
    class LazyBlock {
    public:
        LazyBlock(BasicBlock* block)
            : m_block(block)
        {
        }

        explicit operator bool() const { return !!m_block; }

        BasicBlock* get(Procedure& proc)
        {
            if (!m_block)
                m_block = proc.addBlock();
            return m_block;
        }

        void dump(PrintStream& out) const
        {
            if (m_block)
                out.print(*m_block);
            else
                out.print("Uninitialized");
        }

    private:
        BasicBlock* m_block { nullptr };
    };

public:
    struct ControlData {
        ControlData(Procedure& proc, Type signature, BasicBlock* special = nullptr, BasicBlock* continuation = nullptr)
            : continuation(continuation)
            , special(special)
        {
            if (signature != Void)
                result.append(proc.addVariable(toB3Type(signature)));
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
            out.print("Continuation: ", continuation, ", Special: ");
            if (special)
                out.print(*special);
            else
                out.print("None");
        }

        BlockType type() const
        {
            if (!special)
                return BlockType::Block;
            if (continuation)
                return BlockType::If;
            return BlockType::Loop;
        }

        BasicBlock* targetBlockForBranch(Procedure& proc)
        {
            if (type() == BlockType::Loop)
                return special;
            return continuation.get(proc);
        }

    private:
        friend class B3IRGenerator;
        // We use a LazyBlock for the continuation since B3::validate does not like orphaned blocks. Note,
        // it's possible to create an orphaned block by doing something like (block (return (...))). In
        // that example, if we eagerly allocate a BasicBlock for the continuation it will never be reachable.
        LazyBlock continuation;
        BasicBlock* special;
        Vector<Variable*, 1> result;
    };

    typedef Value* ExpressionType;
    typedef ControlData ControlType;
    typedef Vector<ExpressionType, 1> ExpressionList;
    typedef Vector<Variable*, 1> ResultList;
    static constexpr ExpressionType emptyExpression = nullptr;

    B3IRGenerator(Memory*, Procedure&, Vector<UnlinkedCall>& unlinkedCalls);

    void addArguments(const Vector<Type>&);
    void addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

    // Locals
    bool WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType& result);
    bool WARN_UNUSED_RETURN setLocal(uint32_t index, ExpressionType value);

    // Memory
    bool WARN_UNUSED_RETURN load(LoadOpType, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    bool WARN_UNUSED_RETURN store(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    // Basic operators
    bool WARN_UNUSED_RETURN binaryOp(BinaryOpType, ExpressionType left, ExpressionType right, ExpressionType& result);
    bool WARN_UNUSED_RETURN unaryOp(UnaryOpType, ExpressionType arg, ExpressionType& result);

    // Control flow
    ControlData WARN_UNUSED_RETURN addBlock(Type signature);
    ControlData WARN_UNUSED_RETURN addLoop(Type signature);
    ControlData WARN_UNUSED_RETURN addIf(ExpressionType condition, Type signature);
    bool WARN_UNUSED_RETURN addElse(ControlData&);

    bool WARN_UNUSED_RETURN addReturn(const ExpressionList& returnValues);
    bool WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const ExpressionList& returnValues);
    bool WARN_UNUSED_RETURN endBlock(ControlData&, ExpressionList& expressionStack);

    bool WARN_UNUSED_RETURN addCall(unsigned calleeIndex, const FunctionInformation&, Vector<ExpressionType>& args, ExpressionType& result);

    bool isContinuationReachable(ControlData&);

    void dump(const Vector<ControlType>& controlStack, const ExpressionList& expressionStack);

private:
    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    ExpressionType emitLoadOp(LoadOpType, Origin, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, Origin, ExpressionType pointer, ExpressionType value, uint32_t offset);

    void unify(Variable* target, const ExpressionType source);
    void unifyValuesWithBlock(const ExpressionList& resultStack, ResultList& stack);

    Memory* m_memory;
    Procedure& m_proc;
    BasicBlock* m_currentBlock;
    Vector<Variable*> m_locals;
    // m_unlikedCalls is list of each call site and the function index whose address it should be patched with.
    Vector<UnlinkedCall>& m_unlinkedCalls;
    GPRReg m_memoryBaseGPR;
    GPRReg m_memorySizeGPR;
};

B3IRGenerator::B3IRGenerator(Memory* memory, Procedure& procedure, Vector<UnlinkedCall>& unlinkedCalls)
    : m_memory(memory)
    , m_proc(procedure)
    , m_unlinkedCalls(unlinkedCalls)
{
    m_currentBlock = m_proc.addBlock();

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
}

void B3IRGenerator::addLocal(Type type, uint32_t count)
{
    m_locals.reserveCapacity(m_locals.size() + count);
    for (uint32_t i = 0; i < count; ++i)
        m_locals.append(m_proc.addVariable(toB3Type(type)));
}

void B3IRGenerator::addArguments(const Vector<Type>& types)
{
    ASSERT(!m_locals.size());
    m_locals.grow(types.size());
    wasmCallingConvention().loadArguments(types, m_proc, m_currentBlock, Origin(),
        [&] (ExpressionType argument, unsigned i) {
            Variable* argumentVariable = m_proc.addVariable(argument->type());
            m_locals[i] = argumentVariable;
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), argumentVariable, argument);
        });
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

bool B3IRGenerator::unaryOp(UnaryOpType op, ExpressionType arg, ExpressionType& result)
{
    if (!isSimple(op))
        return false;
    result = m_currentBlock->appendNew<Value>(m_proc, toB3Op(op), Origin(), arg);
    return true;
}

bool B3IRGenerator::binaryOp(BinaryOpType op, ExpressionType left, ExpressionType right, ExpressionType& result)
{
    if (!isSimple(op))
        return false;
    result = m_currentBlock->appendNew<Value>(m_proc, toB3Op(op), Origin(), left, right);
    return true;
}

B3IRGenerator::ExpressionType B3IRGenerator::addConstant(Type type, uint64_t value)
{
    switch (type) {
    case Int32:
        return m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), static_cast<int32_t>(value));
    case Int64:
        return m_currentBlock->appendNew<Const64Value>(m_proc, Origin(), value);
    case Float:
        return m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), bitwise_cast<float>(static_cast<int32_t>(value)));
    case Double:
        return m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), bitwise_cast<double>(value));
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

B3IRGenerator::ControlData B3IRGenerator::addBlock(Type signature)
{
    return ControlData(m_proc, signature);
}

B3IRGenerator::ControlData B3IRGenerator::addLoop(Type signature)
{
    BasicBlock* body = m_proc.addBlock();
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), body);
    body->addPredecessor(m_currentBlock);
    m_currentBlock = body;
    return ControlData(m_proc, signature, body);
}

B3IRGenerator::ControlData B3IRGenerator::addIf(ExpressionType condition, Type signature)
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
    return ControlData(m_proc, signature, notTaken, continuation);
}

bool B3IRGenerator::addElse(ControlData& data)
{
    ASSERT(data.continuation);
    m_currentBlock = data.special;
    // Clear the special pointer so that when we parse the end we don't think that this block is an if block.
    data.special = nullptr;
    ASSERT(data.type() == BlockType::Block);
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
    BasicBlock* target = data.targetBlockForBranch(m_proc);
    unifyValuesWithBlock(returnValues, data.result);
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

bool B3IRGenerator::endBlock(ControlData& data, ExpressionList& expressionStack)
{
    if (!data.continuation)
        return true;

    BasicBlock* continuation = data.continuation.get(m_proc);
    if (data.type() == BlockType::If) {
        ASSERT(!data.special->size() && !data.special->successors().size());
        // Since we don't have any else block we need to point the notTaken branch to the continuation.
        data.special->appendNewControlValue(m_proc, Jump, Origin());
        data.special->setSuccessors(FrequentedBlock(continuation));
        continuation->addPredecessor(data.special);
    }

    unifyValuesWithBlock(expressionStack, data.result);
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), continuation);
    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = continuation;
    return true;
}

bool B3IRGenerator::addCall(unsigned functionIndex, const FunctionInformation& info, Vector<ExpressionType>& args, ExpressionType& result)
{
    ASSERT(info.signature->arguments.size() == args.size());

    Type returnType = info.signature->returnType;

    size_t callIndex = m_unlinkedCalls.size();
    m_unlinkedCalls.grow(callIndex + 1);
    result = wasmCallingConvention().setupCall(m_proc, m_currentBlock, Origin(), args, toB3Type(returnType),
        [&] (PatchpointValue* patchpoint) {
            patchpoint->effects.writesPinned = true;
            patchpoint->effects.readsPinned = true;

            patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                AllowMacroScratchRegisterUsage allowScratch(jit);

                CCallHelpers::Call call = jit.call();

                jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                    m_unlinkedCalls[callIndex] = { linkBuffer.locationOf(call), functionIndex };
                });
            });
        });
    return true;
}

bool B3IRGenerator::isContinuationReachable(ControlData& data)
{
    // If nothing targets the continuation of the current block then we don't want to create
    // an orphaned BasicBlock since it can't be reached by fallthrough.
    if (!data.continuation)
        return false;

    m_currentBlock = data.continuation.get(m_proc);
    if (data.type() == BlockType::If) {
        data.special->appendNewControlValue(m_proc, Jump, Origin(), m_currentBlock);
        m_currentBlock->addPredecessor(data.special);
    }

    return true;
}

void B3IRGenerator::unify(Variable* variable, ExpressionType source)
{
    m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), variable, source);
}

void B3IRGenerator::unifyValuesWithBlock(const ExpressionList& resultStack, ResultList& result)
{
    ASSERT(result.size() >= resultStack.size());

    for (size_t i = 0; i < resultStack.size(); ++i)
        unify(result[i], resultStack[i]);
}

void B3IRGenerator::dump(const Vector<ControlType>& controlStack, const ExpressionList& expressionStack)
{
    dataLogLn("Processing Graph:");
    dataLog(m_proc);
    dataLogLn("With current block:", *m_currentBlock);
    dataLogLn("Control stack:");
    for (const ControlType& data : controlStack)
        dataLogLn("  ", data);
    dataLogLn("ExpressionStack:");
    for (const ExpressionType& expression : expressionStack)
        dataLogLn("  ", *expression);
    dataLogLn("\n");
}

} // anonymous namespace


static std::unique_ptr<Compilation> createJSWrapper(VM& vm, const Signature* signature, MacroAssemblerCodePtr mainFunction, Memory* memory)
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
    case Void:
        block->appendNewControlValue(proc, B3::Return, Origin());
        break;
    case F32:
    case F64:
        result = block->appendNew<Value>(proc, BitwiseCast, Origin(), result);
        FALLTHROUGH;
    case I32:
    case I64:
        block->appendNewControlValue(proc, B3::Return, Origin(), result);
        break;
    }

    return std::make_unique<Compilation>(vm, proc);
}

std::unique_ptr<FunctionCompilation> parseAndCompile(VM& vm, const uint8_t* functionStart, size_t functionLength, Memory* memory, const Signature* signature, const Vector<FunctionInformation>& functions, unsigned optLevel)
{
    auto result = std::make_unique<FunctionCompilation>();

    Procedure procedure;
    B3IRGenerator context(memory, procedure, result->unlinkedCalls);
    FunctionParser<B3IRGenerator> parser(context, functionStart, functionLength, signature, functions);
    if (!parser.parse())
        RELEASE_ASSERT_NOT_REACHED();

    procedure.resetReachability();
    validate(procedure, "After parsing:\n");

    fixSSA(procedure);
    if (verbose)
        dataLog("Post SSA: ", procedure);

    result->code = std::make_unique<Compilation>(vm, procedure, optLevel);
    result->jsEntryPoint = createJSWrapper(vm, signature, result->code->code(), memory);
    return result;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

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
#include "B3Compile.h"
#include "B3ConstPtrValue.h"
#include "B3FixSSA.h"
#include "B3Generate.h"
#include "B3StackmapGenerationParams.h"
#include "B3SwitchValue.h"
#include "B3Validate.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "B3WasmAddressValue.h"
#include "B3WasmBoundsCheckValue.h"
#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "JSWebAssemblyRuntimeError.h"
#include "VirtualRegister.h"
#include "WasmCallingConvention.h"
#include "WasmExceptionType.h"
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
                out.print("If:       ");
                break;
            case BlockType::Block:
                out.print("Block:    ");
                break;
            case BlockType::Loop:
                out.print("Loop:     ");
                break;
            case BlockType::TopLevel:
                out.print("TopLevel: ");
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

    typedef String ErrorType;
    typedef UnexpectedType<ErrorType> UnexpectedResult;
    typedef Expected<std::unique_ptr<WasmInternalFunction>, ErrorType> Result;
    typedef Expected<void, ErrorType> PartialResult;
    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString(ASCIILiteral("WebAssembly.Module failed compiling: "), makeString(args)...));
    }
#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (UNLIKELY(condition))                  \
            return fail(__VA_ARGS__);             \
    } while (0)

    B3IRGenerator(VM&, const ModuleInformation&, Procedure&, WasmInternalFunction*, Vector<UnlinkedWasmToWasmCall>&);

    PartialResult WARN_UNUSED_RETURN addArguments(const Signature*);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

    // Locals
    PartialResult WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN setLocal(uint32_t index, ExpressionType value);

    // Globals
    PartialResult WARN_UNUSED_RETURN getGlobal(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN setGlobal(uint32_t index, ExpressionType value);

    // Memory
    PartialResult WARN_UNUSED_RETURN load(LoadOpType, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN store(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN addGrowMemory(ExpressionType delta, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addCurrentMemory(ExpressionType& result);

    // Basic operators
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType arg, ExpressionType& result);
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType left, ExpressionType right, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result);

    // Control flow
    ControlData WARN_UNUSED_RETURN addTopLevel(Type signature);
    ControlData WARN_UNUSED_RETURN addBlock(Type signature);
    ControlData WARN_UNUSED_RETURN addLoop(Type signature);
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType condition, Type signature, ControlData& result);
    PartialResult WARN_UNUSED_RETURN addElse(ControlData&, const ExpressionList&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlData&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const ExpressionList& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const ExpressionList& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const ExpressionList& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, ExpressionList& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&);

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const Signature*, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(const Signature*, SignatureIndex, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addUnreachable();

    void dump(const Vector<ControlEntry>& controlStack, const ExpressionList* expressionStack);

    void emitExceptionCheck(CCallHelpers&, ExceptionType);

private:
    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    ExpressionType emitLoadOp(LoadOpType, Origin, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, Origin, ExpressionType pointer, ExpressionType value, uint32_t offset);

    void unify(Variable* target, const ExpressionType source);
    void unifyValuesWithBlock(const ExpressionList& resultStack, ResultList& stack);
    Value* zeroForType(Type);

    void emitChecksForModOrDiv(B3::Opcode, ExpressionType left, ExpressionType right);

    VM& m_vm;
    const ModuleInformation& m_info;
    Procedure& m_proc;
    BasicBlock* m_currentBlock;
    Vector<Variable*> m_locals;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.
    GPRReg m_memoryBaseGPR;
    GPRReg m_memorySizeGPR;
    Value* m_zeroValues[numTypes];
    Value* m_instanceValue;
};

B3IRGenerator::B3IRGenerator(VM& vm, const ModuleInformation& info, Procedure& procedure, WasmInternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls)
    : m_vm(vm)
    , m_info(info)
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

    // FIXME we don't really need to pin registers here if there's no memory. It makes wasm -> wasm thunks simpler for now. https://bugs.webkit.org/show_bug.cgi?id=166623
    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
    m_memoryBaseGPR = pinnedRegs.baseMemoryPointer;
    m_proc.pinRegister(m_memoryBaseGPR);
    ASSERT(!pinnedRegs.sizeRegisters[0].sizeOffset);
    m_memorySizeGPR = pinnedRegs.sizeRegisters[0].sizeRegister;
    for (const PinnedSizeRegisterInfo& regInfo : pinnedRegs.sizeRegisters)
        m_proc.pinRegister(regInfo.sizeRegister);

    if (info.hasMemory()) {
        m_proc.setWasmBoundsCheckGenerator([=] (CCallHelpers& jit, GPRReg pinnedGPR, unsigned) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            ASSERT_UNUSED(pinnedGPR, m_memorySizeGPR == pinnedGPR);
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    wasmCallingConvention().setupFrameInPrologue(&compilation->wasmCalleeMoveLocation, m_proc, Origin(), m_currentBlock);

    m_instanceValue = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), Origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, Origin(), &m_vm.topJSWebAssemblyInstance));
}

struct MemoryBaseAndSize {
    Value* base;
    Value* size;
};

static MemoryBaseAndSize getMemoryBaseAndSize(VM& vm, Value* instance, Procedure& proc, BasicBlock* block)
{
    Value* memoryObject = block->appendNew<MemoryValue>(proc, Load, pointerType(), Origin(), instance, JSWebAssemblyInstance::offsetOfMemory());

    static_assert(sizeof(decltype(vm.topJSWebAssemblyInstance->memory()->memory()->memory())) == sizeof(void*), "codegen relies on this size");
    static_assert(sizeof(decltype(vm.topJSWebAssemblyInstance->memory()->memory()->size())) == sizeof(uint64_t), "codegen relies on this size");
    MemoryBaseAndSize result;
    result.base = block->appendNew<MemoryValue>(proc, Load, pointerType(), Origin(), memoryObject, JSWebAssemblyMemory::offsetOfMemory());
    result.size = block->appendNew<MemoryValue>(proc, Load, Int64, Origin(), memoryObject, JSWebAssemblyMemory::offsetOfSize());

    return result;
}

static void restoreWebAssemblyGlobalState(VM& vm, const MemoryInformation& memory, Value* instance, Procedure& proc, BasicBlock* block)
{
    block->appendNew<MemoryValue>(proc, Store, Origin(), instance, block->appendNew<ConstPtrValue>(proc, Origin(), &vm.topJSWebAssemblyInstance));

    if (!!memory) {
        const PinnedRegisterInfo* pinnedRegs = &PinnedRegisterInfo::get();
        RegisterSet clobbers;
        clobbers.set(pinnedRegs->baseMemoryPointer);
        for (auto info : pinnedRegs->sizeRegisters)
            clobbers.set(info.sizeRegister);

        B3::PatchpointValue* patchpoint = block->appendNew<B3::PatchpointValue>(proc, B3::Void, Origin());
        patchpoint->effects = Effects::none();
        patchpoint->effects.writesPinned = true;
        patchpoint->clobber(clobbers);

        patchpoint->append(instance, ValueRep::SomeRegister);

        patchpoint->setGenerator([pinnedRegs] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            GPRReg baseMemory = pinnedRegs->baseMemoryPointer;
            jit.loadPtr(CCallHelpers::Address(params[0].gpr(), JSWebAssemblyInstance::offsetOfMemory()), baseMemory);
            const auto& sizeRegs = pinnedRegs->sizeRegisters;
            ASSERT(sizeRegs.size() >= 1);
            ASSERT(!sizeRegs[0].sizeOffset); // The following code assumes we start at 0, and calculates subsequent size registers relative to 0.
            jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyMemory::offsetOfSize()), sizeRegs[0].sizeRegister);
            jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyMemory::offsetOfMemory()), baseMemory);
            for (unsigned i = 1; i < sizeRegs.size(); ++i)
                jit.add64(CCallHelpers::TrustedImm32(-sizeRegs[i].sizeOffset), sizeRegs[0].sizeRegister, sizeRegs[i].sizeRegister);
        });
    }
}

void B3IRGenerator::emitExceptionCheck(CCallHelpers& jit, ExceptionType type)
{
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
    auto jumpToExceptionStub = jit.jump();

    VM* vm = &m_vm;
    jit.addLinkTask([vm, jumpToExceptionStub] (LinkBuffer& linkBuffer) {
        linkBuffer.link(jumpToExceptionStub, CodeLocationLabel(vm->getCTIStub(throwExceptionFromWasmThunkGenerator).code()));
    });
}

Value* B3IRGenerator::zeroForType(Type type)
{
    ASSERT(type != Void);
    Value* zeroValue = m_zeroValues[linearizeType(type)];
    ASSERT(zeroValue);
    return zeroValue;
}

auto B3IRGenerator::addLocal(Type type, uint32_t count) -> PartialResult
{
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(m_locals.size() + count), "can't allocate memory for ", m_locals.size() + count, " locals");

    for (uint32_t i = 0; i < count; ++i) {
        Variable* local = m_proc.addVariable(toB3Type(type));
        m_locals.uncheckedAppend(local);
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, zeroForType(type));
    }
    return { };
}

auto B3IRGenerator::addArguments(const Signature* signature) -> PartialResult
{
    ASSERT(!m_locals.size());
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(signature->argumentCount()), "can't allocate memory for ", signature->argumentCount(), " arguments");

    m_locals.grow(signature->argumentCount());
    wasmCallingConvention().loadArguments(signature, m_proc, m_currentBlock, Origin(),
        [&] (ExpressionType argument, unsigned i) {
            Variable* argumentVariable = m_proc.addVariable(argument->type());
            m_locals[i] = argumentVariable;
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), argumentVariable, argument);
        });
    return { };
}

auto B3IRGenerator::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    ASSERT(m_locals[index]);
    result = m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, Origin(), m_locals[index]);
    return { };
}

auto B3IRGenerator::addUnreachable() -> PartialResult
{
    B3::PatchpointValue* unreachable = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, Origin());
    unreachable->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::Unreachable);
    });
    unreachable->effects.terminal = true;
    return { };
}

auto B3IRGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    int32_t (*growMemory) (ExecState*, int32_t) = [] (ExecState* exec, int32_t delta) -> int32_t {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        JSWebAssemblyInstance* instance = vm.topJSWebAssemblyInstance;
        JSWebAssemblyMemory* wasmMemory = instance->memory();

        if (delta < 0)
            return -1;

        bool shouldThrowExceptionsOnFailure = false;
        PageCount result = wasmMemory->grow(exec, static_cast<uint32_t>(delta), shouldThrowExceptionsOnFailure);
        RELEASE_ASSERT(!scope.exception());
        if (!result)
            return -1;

        return result.pageCount();
    };

    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, Origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, Origin(), bitwise_cast<void*>(growMemory)),
        m_currentBlock->appendNew<B3::Value>(m_proc, B3::FramePointer, Origin()), delta);

    restoreWebAssemblyGlobalState(m_vm, m_info.memory, m_instanceValue, m_proc, m_currentBlock);

    return { };
}

auto B3IRGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    auto memoryValue = getMemoryBaseAndSize(m_vm, m_instanceValue, m_proc, m_currentBlock);

    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1 << shiftValue, "This must hold for the code below to be correct.");
    Value* numPages = m_currentBlock->appendNew<Value>(m_proc, ZShr, Origin(),
        memoryValue.size, m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), shiftValue));

    result = m_currentBlock->appendNew<Value>(m_proc, Trunc, Origin(), numPages);

    return { };
}

auto B3IRGenerator::setLocal(uint32_t index, ExpressionType value) -> PartialResult
{
    ASSERT(m_locals[index]);
    m_currentBlock->appendNew<VariableValue>(m_proc, B3::Set, Origin(), m_locals[index], value);
    return { };
}

auto B3IRGenerator::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    Value* globalsArray = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), Origin(), m_instanceValue, JSWebAssemblyInstance::offsetOfGlobals());
    result = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(m_info.globals[index].type), Origin(), globalsArray, index * sizeof(Register));
    return { };
}

auto B3IRGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    ASSERT(toB3Type(m_info.globals[index].type) == value->type());
    Value* globalsArray = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), Origin(), m_instanceValue, JSWebAssemblyInstance::offsetOfGlobals());
    m_currentBlock->appendNew<MemoryValue>(m_proc, Store, Origin(), value, globalsArray, index * sizeof(Register));
    return { };
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
    case LoadOpType::I32Load16U:
    case LoadOpType::I64Load16U:
        return 2;
    case LoadOpType::I32Load:
    case LoadOpType::I64Load32S:
    case LoadOpType::I64Load32U:
    case LoadOpType::F32Load:
        return 4;
    case LoadOpType::I64Load:
    case LoadOpType::F64Load:
        return 8;
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
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin, pointer, offset);
    }

    case LoadOpType::I64Load32U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin, pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin, value);
    }

    case LoadOpType::I64Load32S: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin, pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin, value);
    }

    case LoadOpType::I64Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin, pointer, offset);
    }

    case LoadOpType::F32Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Float, origin, pointer, offset);
    }

    case LoadOpType::F64Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Double, origin, pointer, offset);
    }

    // FIXME: B3 doesn't support Load16Z yet. We should lower to that value when
    // it's added. https://bugs.webkit.org/show_bug.cgi?id=165884
    case LoadOpType::I32Load16U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load16S, origin, pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(), value,
                m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), 0x0000ffff));
    }
    case LoadOpType::I64Load16U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load16S, origin, pointer, offset);
        Value* partialResult = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(), value,
                m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), 0x0000ffff));

        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, Origin(), partialResult);
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto B3IRGenerator::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfLoadOp(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, Origin());
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (op) {
        case LoadOpType::I32Load8S:
        case LoadOpType::I32Load16S:
        case LoadOpType::I32Load:
        case LoadOpType::I32Load16U:
        case LoadOpType::I32Load8U:
            result = zeroForType(I32);
            break;
        case LoadOpType::I64Load8S:
        case LoadOpType::I64Load8U:
        case LoadOpType::I64Load16S:
        case LoadOpType::I64Load32U:
        case LoadOpType::I64Load32S:
        case LoadOpType::I64Load:
        case LoadOpType::I64Load16U:
            result = zeroForType(I64);
            break;
        case LoadOpType::F32Load:
            result = zeroForType(F32);
            break;
        case LoadOpType::F64Load:
            result = zeroForType(F64);
            break;
        }

    } else
        result = emitLoadOp(op, Origin(), emitCheckAndPreparePointer(pointer, offset, sizeOfLoadOp(op)), offset);

    return { };
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

auto B3IRGenerator::store(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfStoreOp(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, Origin());
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    } else
        emitStoreOp(op, Origin(), emitCheckAndPreparePointer(pointer, offset, sizeOfStoreOp(op)), value, offset);

    return { };
}

auto B3IRGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    result = m_currentBlock->appendNew<Value>(m_proc, B3::Select, Origin(), condition, nonZero, zero);
    return { };
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

B3IRGenerator::ControlData B3IRGenerator::addTopLevel(Type signature)
{
    return ControlData(m_proc, signature, BlockType::TopLevel, m_proc.addBlock());
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

auto B3IRGenerator::addIf(ExpressionType condition, Type signature, ControlType& result) -> PartialResult
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
    return { };
}

auto B3IRGenerator::addElse(ControlData& data, const ExpressionList& currentStack) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.result);
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), data.continuation);
    return addElseToUnreachable(data);
}

auto B3IRGenerator::addElseToUnreachable(ControlData& data) -> PartialResult
{
    ASSERT(data.type() == BlockType::If);
    m_currentBlock = data.special;
    data.convertIfToBlock();
    return { };
}

auto B3IRGenerator::addReturn(const ControlData&, const ExpressionList& returnValues) -> PartialResult
{
    ASSERT(returnValues.size() <= 1);
    if (returnValues.size())
        m_currentBlock->appendNewControlValue(m_proc, B3::Return, Origin(), returnValues[0]);
    else
        m_currentBlock->appendNewControlValue(m_proc, B3::Return, Origin());
    return { };
}

auto B3IRGenerator::addBranch(ControlData& data, ExpressionType condition, const ExpressionList& returnValues) -> PartialResult
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

    return { };
}

auto B3IRGenerator::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const ExpressionList& expressionStack) -> PartialResult
{
    for (size_t i = 0; i < targets.size(); ++i)
        unifyValuesWithBlock(expressionStack, targets[i]->result);
    unifyValuesWithBlock(expressionStack, defaultTarget.result);

    SwitchValue* switchValue = m_currentBlock->appendNew<SwitchValue>(m_proc, Origin(), condition);
    switchValue->setFallThrough(FrequentedBlock(defaultTarget.targetBlockForBranch()));
    for (size_t i = 0; i < targets.size(); ++i)
        switchValue->appendCase(SwitchCase(i, FrequentedBlock(targets[i]->targetBlockForBranch())));

    return { };
}

auto B3IRGenerator::endBlock(ControlEntry& entry, ExpressionList& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;

    unifyValuesWithBlock(expressionStack, data.result);
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), data.continuation);
    data.continuation->addPredecessor(m_currentBlock);

    return addEndToUnreachable(entry);
}


auto B3IRGenerator::addEndToUnreachable(ControlEntry& entry) -> PartialResult
{
    ControlData& data = entry.controlData;
    m_currentBlock = data.continuation;

    if (data.type() == BlockType::If) {
        data.special->appendNewControlValue(m_proc, Jump, Origin(), m_currentBlock);
        m_currentBlock->addPredecessor(data.special);
    }

    for (Variable* result : data.result)
        entry.enclosedExpressionStack.append(m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, Origin(), result));

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.type() == BlockType::TopLevel)
        return addReturn(entry.controlData, entry.enclosedExpressionStack);

    return { };
}

auto B3IRGenerator::addCall(uint32_t functionIndex, const Signature* signature, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    ASSERT(signature->argumentCount() == args.size());

    Type returnType = signature->returnType();
    Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {
        // FIXME imports can be linked here, instead of generating a patchpoint, because all import stubs are generated before B3 compilation starts. https://bugs.webkit.org/show_bug.cgi?id=166462
        Value* functionImport = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), Origin(), m_instanceValue, JSWebAssemblyInstance::offsetOfImportFunction(functionIndex));
        Value* jsTypeOfImport = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Origin(), functionImport, JSCell::typeInfoTypeOffset());
        Value* isWasmCall = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), jsTypeOfImport, m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), WebAssemblyFunctionType));

        BasicBlock* isWasmBlock = m_proc.addBlock();
        BasicBlock* isJSBlock = m_proc.addBlock();
        BasicBlock* continuation = m_proc.addBlock();
        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, Origin(), isWasmCall, FrequentedBlock(isWasmBlock), FrequentedBlock(isJSBlock));

        Value* wasmCallResult = wasmCallingConvention().setupCall(m_proc, isWasmBlock, Origin(), args, toB3Type(returnType),
            [&] (PatchpointValue* patchpoint) {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;
                patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    CCallHelpers::Call call = jit.call();
                    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOf(call), functionIndex, UnlinkedWasmToWasmCall::Target::ToWasm });
                    });
                });
            });
        UpsilonValue* wasmCallResultUpsilon = returnType == Void ? nullptr : isWasmBlock->appendNew<UpsilonValue>(m_proc, Origin(), wasmCallResult);
        isWasmBlock->appendNewControlValue(m_proc, Jump, Origin(), continuation);

        Value* jsCallResult = wasmCallingConvention().setupCall(m_proc, isJSBlock, Origin(), args, toB3Type(returnType),
            [&] (PatchpointValue* patchpoint) {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;
                patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    CCallHelpers::Call call = jit.call();
                    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOf(call), functionIndex, UnlinkedWasmToWasmCall::Target::ToJs });
                    });
                });
            });
        UpsilonValue* jsCallResultUpsilon = returnType == Void ? nullptr : isJSBlock->appendNew<UpsilonValue>(m_proc, Origin(), jsCallResult);
        isJSBlock->appendNewControlValue(m_proc, Jump, Origin(), continuation);

        m_currentBlock = continuation;

        if (returnType == Void)
            result = nullptr;
        else {
            result = continuation->appendNew<Value>(m_proc, Phi, toB3Type(returnType), Origin());
            wasmCallResultUpsilon->setPhi(result);
            jsCallResultUpsilon->setPhi(result);
        }

        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState(m_vm, m_info.memory, m_instanceValue, m_proc, continuation);
    } else {
        result = wasmCallingConvention().setupCall(m_proc, m_currentBlock, Origin(), args, toB3Type(returnType),
            [&] (PatchpointValue* patchpoint) {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;
                patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    CCallHelpers::Call call = jit.call();
                    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOf(call), functionIndex, UnlinkedWasmToWasmCall::Target::ToWasm });
                    });
                });
            });
    }

    return { };
}

auto B3IRGenerator::addCallIndirect(const Signature* signature, SignatureIndex signatureIndex, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    ASSERT(signatureIndex != Signature::invalidIndex);
    ExpressionType calleeIndex = args.takeLast();
    ASSERT(signature->argumentCount() == args.size());

    ExpressionType callableFunctionBuffer;
    ExpressionType callableFunctionBufferSize;
    {
        ExpressionType table = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), Origin(),
            m_instanceValue, JSWebAssemblyInstance::offsetOfTable());
        callableFunctionBuffer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), Origin(),
            table, JSWebAssemblyTable::offsetOfFunctions());
        callableFunctionBufferSize = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, Origin(),
            table, JSWebAssemblyTable::offsetOfSize());
    }

    // Check the index we are looking for is valid.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(),
            m_currentBlock->appendNew<Value>(m_proc, AboveEqual, Origin(), calleeIndex, callableFunctionBufferSize));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsCallIndirect);
        });
    }

    // Compute the offset in the table index space we are looking for.
    ExpressionType offset = m_currentBlock->appendNew<Value>(m_proc, Mul, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, ZExt32, Origin(), calleeIndex),
        m_currentBlock->appendIntConstant(m_proc, Origin(), pointerType(), sizeof(CallableFunction)));
    ExpressionType callableFunction = m_currentBlock->appendNew<Value>(m_proc, Add, Origin(), callableFunctionBuffer, offset);

    // Check that the CallableFunction is initialized. We trap if it isn't. An "invalid" SignatureIndex indicates it's not initialized.
    static_assert(sizeof(CallableFunction::signatureIndex) == sizeof(uint32_t), "Load codegen assumes i32");
    ExpressionType calleeSignatureIndex = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, Origin(), callableFunction, OBJECT_OFFSETOF(CallableFunction, signatureIndex));
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(),
                calleeSignatureIndex,
                m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), Signature::invalidIndex)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::NullTableEntry);
        });
    }

    // Check the signature matches the value we expect.
    {
        ExpressionType expectedSignatureIndex = m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), signatureIndex);
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(),
            m_currentBlock->appendNew<Value>(m_proc, NotEqual, Origin(), calleeSignatureIndex, expectedSignatureIndex));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::BadSignature);
        });
    }

    ExpressionType calleeCode = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), Origin(), callableFunction, OBJECT_OFFSETOF(CallableFunction, code));

    Type returnType = signature->returnType();
    result = wasmCallingConvention().setupCall(m_proc, m_currentBlock, Origin(), args, toB3Type(returnType),
        [&] (PatchpointValue* patchpoint) {
            patchpoint->effects.writesPinned = true;
            patchpoint->effects.readsPinned = true;

            patchpoint->append(calleeCode, ValueRep::SomeRegister);

            patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                jit.call(params[returnType == Void ? 0 : 1].gpr());
            });
        });

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(m_vm, m_info.memory, m_instanceValue, m_proc, m_currentBlock);

    return { };
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
    dataLog(comma, "ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, *expression);
}

void B3IRGenerator::dump(const Vector<ControlEntry>& controlStack, const ExpressionList* expressionStack)
{
    dataLogLn("Processing Graph:");
    dataLog(m_proc);
    dataLogLn("With current block:", *m_currentBlock);
    dataLogLn("Control stack:");
    ASSERT(controlStack.size());
    for (size_t i = controlStack.size(); i--;) {
        dataLog("  ", controlStack[i].controlData, ": ");
        CommaPrinter comma(", ", "");
        dumpExpressionStack(comma, *expressionStack);
        expressionStack = &controlStack[i].enclosedExpressionStack;
        dataLogLn();
    }
    dataLogLn();
}

static void createJSToWasmWrapper(VM& vm, CompilationContext& compilationContext, WasmInternalFunction& function, const Signature* signature, const ModuleInformation& info)
{
    Procedure proc;
    BasicBlock* block = proc.addBlock();

    Origin origin;

    jscCallingConvention().setupFrameInPrologue(&function.jsToWasmCalleeMoveLocation, proc, origin, block);

    if (!ASSERT_DISABLED) {
        // This should be guaranteed by our JS wrapper that handles calls to us.
        // Just prevent against crazy when ASSERT is enabled.
        Value* framePointer = block->appendNew<B3::Value>(proc, B3::FramePointer, origin);
        Value* offSetOfArgumentCount = block->appendNew<Const64Value>(proc, origin, CallFrameSlot::argumentCount * sizeof(Register));
        Value* argumentCount = block->appendNew<MemoryValue>(proc, Load, Int32, origin,
            block->appendNew<Value>(proc, Add, origin, framePointer, offSetOfArgumentCount));

        Value* expectedArgumentCount = block->appendNew<Const32Value>(proc, origin, signature->argumentCount());

        CheckValue* argumentCountCheck = block->appendNew<CheckValue>(proc, Check, origin,
            block->appendNew<Value>(proc, Above, origin, expectedArgumentCount, argumentCount));

        argumentCountCheck->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams&) {
            jit.breakpoint();
        });
    }

    // FIXME The instance is currently set by the C++ code in WebAssemblyFunction::call. We shouldn't go through the extra C++ hoop. https://bugs.webkit.org/show_bug.cgi?id=166486
    Value* instance = block->appendNew<MemoryValue>(proc, Load, pointerType(), Origin(),
        block->appendNew<ConstPtrValue>(proc, Origin(), &vm.topJSWebAssemblyInstance));
    restoreWebAssemblyGlobalState(vm, info.memory, instance, proc, block);

    // Get our arguments.
    Vector<Value*> arguments;
    jscCallingConvention().loadArguments(signature, proc, block, origin, [&] (Value* argument, unsigned) {
        arguments.append(argument);
    });

    // Move the arguments into place.
    Value* result = wasmCallingConvention().setupCall(proc, block, origin, arguments, toB3Type(signature->returnType()), [&] (PatchpointValue* patchpoint) {
        CompilationContext* context = &compilationContext;

        // wasm -> wasm calls clobber pinned registers unconditionally. This JS -> wasm transition must therefore restore these pinned registers (which are usually callee-saved) to account for this.
        const PinnedRegisterInfo* pinnedRegs = &PinnedRegisterInfo::get();
        RegisterSet clobbers;
        clobbers.set(pinnedRegs->baseMemoryPointer);
        for (auto info : pinnedRegs->sizeRegisters)
            clobbers.set(info.sizeRegister);
        patchpoint->effects.writesPinned = true;
        patchpoint->clobber(clobbers);

        patchpoint->setGenerator([context] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);

            CCallHelpers::Call call = jit.call();
            context->jsEntrypointToWasmEntrypointCall = call;
        });
    });

    // Return the result, if needed.
    switch (signature->returnType()) {
    case Wasm::Void:
        block->appendNewControlValue(proc, B3::Return, origin);
        break;
    case Wasm::F32:
    case Wasm::F64:
        result = block->appendNew<Value>(proc, BitwiseCast, origin, result);
        FALLTHROUGH;
    case Wasm::I32:
    case Wasm::I64:
        block->appendNewControlValue(proc, B3::Return, origin, result);
        break;
    case Wasm::Func:
    case Wasm::Anyfunc:
        RELEASE_ASSERT_NOT_REACHED();
    }

    B3::prepareForGeneration(proc);
    B3::generate(proc, *compilationContext.jsEntrypointJIT);
    compilationContext.jsEntrypointByproducts = proc.releaseByproducts();
    function.jsToWasmEntrypoint.calleeSaveRegisters = proc.calleeSaveRegisters();
}

Expected<std::unique_ptr<WasmInternalFunction>, String> parseAndCompile(VM& vm, CompilationContext& compilationContext, const uint8_t* functionStart, size_t functionLength, const Signature* signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, const Vector<SignatureIndex>& moduleSignatureIndicesToUniquedSignatureIndices, unsigned optLevel)
{
    auto result = std::make_unique<WasmInternalFunction>();

    compilationContext.jsEntrypointJIT = std::make_unique<CCallHelpers>(&vm);
    compilationContext.wasmEntrypointJIT = std::make_unique<CCallHelpers>(&vm);

    Procedure procedure;
    B3IRGenerator context(vm, info, procedure, result.get(), unlinkedWasmToWasmCalls);
    FunctionParser<B3IRGenerator> parser(&vm, context, functionStart, functionLength, signature, info, moduleSignatureIndicesToUniquedSignatureIndices);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    procedure.resetReachability();
    validate(procedure, "After parsing:\n");

    if (verbose)
        dataLog("Pre SSA: ", procedure);
    fixSSA(procedure);
    if (verbose)
        dataLog("Post SSA: ", procedure);

    {
        B3::prepareForGeneration(procedure, optLevel);
        B3::generate(procedure, *compilationContext.wasmEntrypointJIT);
        compilationContext.wasmEntrypointByproducts = procedure.releaseByproducts();
        result->wasmEntrypoint.calleeSaveRegisters = procedure.calleeSaveRegisters();
    }

    createJSToWasmWrapper(vm, compilationContext, *result, signature, info);
    return WTFMove(result);
}

// Custom wasm ops. These are the ones too messy to do in wasm.json.

void B3IRGenerator::emitChecksForModOrDiv(B3::Opcode operation, ExpressionType left, ExpressionType right)
{
    ASSERT(operation == Div || operation == Mod || operation == UDiv || operation == UMod);
    const B3::Type type = left->type();

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), right,
                m_currentBlock->appendIntConstant(m_proc, Origin(), type, 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::DivisionByZero);
        });
    }

    if (operation == Div) {
        int64_t min = type == Int32 ? std::numeric_limits<int32_t>::min() : std::numeric_limits<int64_t>::min();

        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(),
            m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
                m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), left,
                    m_currentBlock->appendIntConstant(m_proc, Origin(), type, min)),
                m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), right,
                    m_currentBlock->appendIntConstant(m_proc, Origin(), type, -1))));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::IntegerOverflow);
        });
    }
}

template<>
auto B3IRGenerator::addOp<OpType::I32DivS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Div;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, Origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32RemS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Mod;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, chill(op), Origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32DivU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UDiv;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, Origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32RemU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UMod;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, Origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64DivS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Div;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, Origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64RemS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Mod;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, chill(op), Origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64DivU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UDiv;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, Origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64RemU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UMod;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, Origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32Ctz>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64Ctz>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros64(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32Popcnt>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    // FIXME: This should use the popcnt instruction if SSE4 is available but we don't have code to detect SSE4 yet.
    // see: https://bugs.webkit.org/show_bug.cgi?id=165363
    uint32_t (*popcount)(int32_t) = [] (int32_t value) -> uint32_t { return __builtin_popcount(value); };
    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, Origin(), bitwise_cast<void*>(popcount));
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, Origin(), Effects::none(), funcAddress, arg);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64Popcnt>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    // FIXME: This should use the popcnt instruction if SSE4 is available but we don't have code to detect SSE4 yet.
    // see: https://bugs.webkit.org/show_bug.cgi?id=165363
    uint64_t (*popcount)(int64_t) = [] (int64_t value) -> uint64_t { return __builtin_popcountll(value); };
    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, Origin(), bitwise_cast<void*>(popcount));
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int64, Origin(), Effects::none(), funcAddress, arg);
    return { };
}

template<>
auto B3IRGenerator::addOp<F64ConvertUI64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, Origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->append(ConstrainedValue(arg, ValueRep::SomeRegister));
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
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32ConvertUI64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, Origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->append(ConstrainedValue(arg, ValueRep::SomeRegister));
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
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F64Nearest>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardNearestIntDouble(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32Nearest>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardNearestIntFloat(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F64Trunc>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardZeroDouble(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32Trunc>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardZeroFloat(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncSF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), -static_cast<double>(std::numeric_limits<int32_t>::min()));
    Value* min = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<int32_t>::min()));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncSF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), -static_cast<float>(std::numeric_limits<int32_t>::min()));
    Value* min = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<int32_t>::min()));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}


template<>
auto B3IRGenerator::addOp<OpType::I32TruncUF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0);
    Value* min = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), -1.0);
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncUF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<int32_t>::min()) * -2.0);
    Value* min = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), -1.0);
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncSF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), -static_cast<double>(std::numeric_limits<int64_t>::min()));
    Value* min = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<int64_t>::min()));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncUF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0);
    Value* min = m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), -1.0);
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
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
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncSF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), -static_cast<float>(std::numeric_limits<int64_t>::min()));
    Value* min = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<int64_t>::min()));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, Origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncUF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), static_cast<float>(std::numeric_limits<int64_t>::min()) * -2.0);
    Value* min = m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), -1.0);
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, Origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, Origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, Origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, Origin(), outOfBounds, zeroForType(I32));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, Origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
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
    return { };
}

} } // namespace JSC::Wasm

#include "WasmB3IRGeneratorInlines.h"

#endif // ENABLE(WEBASSEMBLY)

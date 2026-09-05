/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WasmConstExprGenerator.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyStruct.h"
#include "WasmFunctionParser.h"
#include "WasmModuleInformation.h"
#include "WasmOperationsInlines.h"
#include "WasmParser.h"
#include "WasmTypeDefinition.h"
#include <wtf/Assertions.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

class ConstExprInterpreter {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    using UnexpectedResult = std::unexpected<String>;

    ConstExprInterpreter(size_t offsetInSource, const ModuleInformation& info, JSWebAssemblyInstance* instance)
        : m_offsetInSource(offsetInSource)
        , m_info(info)
        , m_instance(instance)
    {
    }

    std::expected<uint64_t, String> run(std::span<const uint8_t> bytes, uint32_t maxStackHeight)
    {
        Vector<ConstExprValue, 16> stack;
        stack.reserveInitialCapacity(maxStackHeight);
        auto push = [&](ConstExprValue value) ALWAYS_INLINE_LAMBDA {
            stack.append(value);
        };
        auto pop = [&] ALWAYS_INLINE_LAMBDA {
            ASSERT(!stack.isEmpty());
            return stack.takeLast();
        };
        // Immediates are decoded without the length and overflow checks WTF::LEBDecoder makes,
        // because validation already accepted them. Most are a single byte.
        size_t offset = 0;
        auto varUInt32 = [&] ALWAYS_INLINE_LAMBDA -> uint32_t {
            uint8_t byte = bytes[offset++];
            if (!(byte & 0x80)) [[likely]]
                return byte;
            uint32_t value = byte & 0x7f;
            unsigned shift = 7;
            do {
                byte = bytes[offset++];
                value |= static_cast<uint32_t>(byte & 0x7f) << shift;
                shift += 7;
            } while (byte & 0x80);
            return value;
        };
        auto varInt64 = [&] ALWAYS_INLINE_LAMBDA -> int64_t {
            uint8_t byte = bytes[offset++];
            if (!(byte & 0x80)) [[likely]]
                return static_cast<int8_t>(byte << 1) >> 1;
            int64_t value = byte & 0x7f;
            unsigned shift = 7;
            do {
                byte = bytes[offset++];
                value |= static_cast<int64_t>(byte & 0x7f) << shift;
                shift += 7;
            } while (byte & 0x80);
            if (shift < 64 && (byte & 0x40))
                value |= -(static_cast<int64_t>(1) << shift);
            return value;
        };
        auto varInt32 = [&] ALWAYS_INLINE_LAMBDA {
            return static_cast<int32_t>(varInt64());
        };
        auto uint32 = [&] ALWAYS_INLINE_LAMBDA {
            uint32_t value;
            memcpySpan(asMutableByteSpan(value), bytes.subspan(offset, sizeof(value)));
            offset += sizeof(value);
            return value;
        };
        auto uint64 = [&] ALWAYS_INLINE_LAMBDA {
            uint64_t value;
            memcpySpan(asMutableByteSpan(value), bytes.subspan(offset, sizeof(value)));
            offset += sizeof(value);
            return value;
        };

        for (;;) {
            ASSERT(offset < bytes.size());
            OpType op = static_cast<OpType>(bytes[offset++]);
            ConstExprValue result;

            switch (op) {
            case I32Const:
                push(ConstExprValue(static_cast<uint64_t>(varInt32())));
                continue;
            case I64Const:
                push(ConstExprValue(static_cast<uint64_t>(varInt64())));
                continue;
            case F32Const:
                push(ConstExprValue(static_cast<uint64_t>(uint32())));
                continue;
            case F64Const:
                push(ConstExprValue(uint64()));
                continue;
            case RefNull:
                varInt32(); // Heap type, which only affects the static type of the null.
                push(ConstExprValue(static_cast<uint64_t>(JSValue::encode(jsNull()))));
                continue;
            case RefFunc:
                result = refFunc(FunctionSpaceIndex(varUInt32()));
                break;
            case GetGlobal:
                result = loadGlobal(varUInt32());
                break;
            case I32Add:
            case I64Add: {
                ConstExprValue rhs = pop();
                result = pop() + rhs;
                break;
            }
            case I32Sub:
            case I64Sub: {
                ConstExprValue rhs = pop();
                result = pop() - rhs;
                break;
            }
            case I32Mul:
            case I64Mul: {
                ConstExprValue rhs = pop();
                result = pop() * rhs;
                break;
            }
            case ExtSIMD: {
                uint32_t extOp = varUInt32();
                ASSERT_UNUSED(extOp, static_cast<ExtSIMDOpType>(extOp) == ExtSIMDOpType::V128Const);
                v128_t vector;
                memcpySpan(asMutableByteSpan(vector), bytes.subspan(offset, sizeof(vector)));
                offset += sizeof(vector);
                push(ConstExprValue(vector));
                continue;
            }
            case ExtGC: {
                switch (static_cast<ExtGCOpType>(varUInt32())) {
                case ExtGCOpType::RefI31:
                    result = refI31(pop());
                    break;
                case ExtGCOpType::AnyConvertExtern:
                    result = anyConvertExtern(pop());
                    break;
                case ExtGCOpType::ExternConvertAny:
                    // extern.convert_any leaves the operand exactly as it is.
                    continue;
                case ExtGCOpType::StructNewDefault:
                    result = createNewStruct(TypeSignatureIndex(varUInt32()));
                    if (result.isInvalid()) [[unlikely]]
                        return failedToAllocate("struct"_s, offset);
                    break;
                case ExtGCOpType::StructNew: {
                    TypeSignatureIndex typeIndex(varUInt32());
                    size_t fieldCount = m_info.rtt(typeIndex).fieldCount();
                    ASSERT(stack.size() >= fieldCount);
                    result = createNewStruct(typeIndex, stack.subspan(stack.size() - fieldCount));
                    stack.shrink(stack.size() - fieldCount);
                    if (result.isInvalid()) [[unlikely]]
                        return failedToAllocate("struct"_s, offset);
                    break;
                }
                case ExtGCOpType::ArrayNew: {
                    TypeSignatureIndex typeIndex(varUInt32());
                    // array.new pushes the element value before the size.
                    ConstExprValue size = pop();
                    ConstExprValue value = pop();
                    result = createNewArray(typeIndex, static_cast<uint32_t>(size.getValue()), value);
                    if (result.isInvalid()) [[unlikely]]
                        return failedToAllocate("array"_s, offset);
                    break;
                }
                case ExtGCOpType::ArrayNewDefault: {
                    TypeSignatureIndex typeIndex(varUInt32());
                    result = createNewDefaultArray(typeIndex, static_cast<uint32_t>(pop().getValue()));
                    if (result.isInvalid()) [[unlikely]]
                        return failedToAllocate("array"_s, offset);
                    break;
                }
                case ExtGCOpType::ArrayNewFixed: {
                    TypeSignatureIndex typeIndex(varUInt32());
                    size_t elementCount = varUInt32();
                    ASSERT(stack.size() >= elementCount);
                    result = createNewArray(typeIndex, stack.subspan(stack.size() - elementCount));
                    stack.shrink(stack.size() - elementCount);
                    if (result.isInvalid()) [[unlikely]]
                        return failedToAllocate("array"_s, offset);
                    break;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
                break;
            }
            case End:
                ASSERT(stack.size() == 1);
                ASSERT(stack.last().type() != ConstExprValue::Vector);
                return stack.last().getValue();
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            push(result);
        }
    }

private:
    enum InvalidTag { InvalidConstExpr };

    // Represents values that a constant expression may evaluate to.
    // If a constant expression allocates an object, it should be put in a Strong handle.
    struct ConstExprValue {
        enum ConstExprValueType : uint8_t {
            Invalid,
            Numeric,
            Vector,
            Ref,
        };

        ConstExprValue(InvalidTag)
            : m_type(ConstExprValueType::Invalid)
            , m_bits(0)
        { }

        ConstExprValue()
            : m_type(ConstExprValueType::Numeric)
            , m_bits(0)
        { }

        ConstExprValue(uint64_t value)
            : m_type(ConstExprValueType::Numeric)
            , m_bits(value)
        { }

        ConstExprValue(v128_t value)
            : m_type(ConstExprValueType::Vector)
            , m_vector(value)
        { }

        ConstExprValue(JSValue value)
            : m_type(ConstExprValueType::Ref)
            , m_bits(JSValue::encode(value))
        { }

        bool NODELETE isInvalid() const
        {
            return m_type == ConstExprValueType::Invalid;
        }

        uint64_t NODELETE getValue() const
        {
            ASSERT(m_type == ConstExprValueType::Numeric || m_type == ConstExprValueType::Ref);
            return m_bits;
        }

        v128_t NODELETE getVector() const
        {
            ASSERT(m_type == ConstExprValueType::Vector);
            return m_vector;
        }

        ConstExprValueType NODELETE type() const
        {
            return m_type;
        }

        ConstExprValue NODELETE operator+(ConstExprValue value) const
        {
            ASSERT(m_type == ConstExprValueType::Numeric);
            return ConstExprValue(m_bits + value.getValue());
        }

        ConstExprValue NODELETE operator-(ConstExprValue value) const
        {
            ASSERT(m_type == ConstExprValueType::Numeric);
            return ConstExprValue(m_bits - value.getValue());
        }

        ConstExprValue NODELETE operator*(ConstExprValue value) const
        {
            ASSERT(m_type == ConstExprValueType::Numeric);
            return ConstExprValue(m_bits * value.getValue());
        }

    private:
        ConstExprValueType m_type;
        union {
            uint64_t m_bits;
            v128_t m_vector;
        };
    };

    // Allocation is the only way evaluation can fail. The offset is passed in so that the message
    // names the same byte position the module parser would have reported.
    [[nodiscard]] NEVER_INLINE UnexpectedResult failedToAllocate(ASCIILiteral kind, size_t offset) const
    {
        return UnexpectedResult(makeString("WebAssembly.Module doesn't parse at byte "_s, String::number(m_offsetInSource + offset), ": Failed to allocate new "_s, kind));
    }

    ConstExprValue loadGlobal(uint32_t index)
    {
        if (m_info.globals[index].type.kind() == TypeKind::V128)
            return ConstExprValue(m_instance->loadV128Global(index));
        return ConstExprValue(m_instance->loadI64Global(index));
    }

    ConstExprValue refFunc(FunctionSpaceIndex index)
    {
        JSValue wrapper = m_instance->ensureFunctionWrapper(index);
        ASSERT(!wrapper.isNull());
        ASSERT(wrapper.isObject());
        m_keepAlive.appendWithCrashOnOverflow(asObject(wrapper));
        return ConstExprValue(wrapper);
    }

    ConstExprValue refI31(ConstExprValue value)
    {
        JSValue i31 = JSValue((((static_cast<int32_t>(value.getValue()) & 0x7fffffff) << 1) >> 1));
        ASSERT(i31.isInt32());
        return ConstExprValue(i31);
    }

    ConstExprValue anyConvertExtern(ConstExprValue reference)
    {
        // extern.internalize only rewrites doubles that fit an i31, so anything already known to be
        // an object reference passes through unchanged, and reusing it avoids a second keep-alive
        // entry for the same object.
        if (reference.type() == ConstExprValue::Numeric)
            return ConstExprValue(externInternalize(reference.getValue()));
        return reference;
    }

    // The allocating operations return an invalid value rather than an error, so that run() reports
    // the failure at the byte offset of the operation it is currently evaluating.
    ConstExprValue trackAllocation(JSValue result)
    {
        if (result.isNull()) [[unlikely]]
            return ConstExprValue(InvalidConstExpr);
        m_keepAlive.appendWithCrashOnOverflow(asObject(result));
        return ConstExprValue(result);
    }

    ConstExprValue createNewArray(WebAssemblyGCStructure* structure, uint32_t size, ConstExprValue value)
    {
        if (value.type() == ConstExprValue::Vector)
            return trackAllocation(arrayNew(m_instance, structure, size, value.getVector()));
        return trackAllocation(arrayNew(m_instance, structure, size, value.getValue()));
    }

    ConstExprValue createNewArray(TypeSignatureIndex typeIndex, uint32_t size, ConstExprValue value)
    {
        return createNewArray(m_instance->gcObjectStructure(typeIndex.rawIndex()), size, value);
    }

    ConstExprValue createNewDefaultArray(TypeSignatureIndex typeIndex, uint32_t size)
    {
        auto* structure = m_instance->gcObjectStructure(typeIndex.rawIndex());
        auto elementType = structure->rtt().elementType().type.unpacked();
        ConstExprValue initValue { };
        if (isRefType(elementType))
            initValue = { static_cast<uint64_t>(JSValue::encode(jsNull())) };
        if (elementType == Wasm::Types::V128)
            initValue = { vectorAllZeros() };
        return createNewArray(structure, size, initValue);
    }

    ConstExprValue createNewArray(TypeSignatureIndex typeIndex, std::span<const ConstExprValue> elements)
    {
        auto* structure = m_instance->gcObjectStructure(typeIndex.rawIndex());
        VM& vm = m_instance->vm();
        JSWebAssemblyArray* array = JSWebAssemblyArray::tryCreate(vm, structure, elements.size());
        if (!array) [[unlikely]]
            return ConstExprValue(InvalidConstExpr);
        m_keepAlive.appendWithCrashOnOverflow(array);

        if (structure->rtt().elementType().type.unpacked().isV128()) {
            auto span = array->span<v128_t>();
            for (size_t i = 0; i < elements.size(); ++i)
                span[i] = elements[i].getVector();
        } else {
            array->visitSpanNonVector([&]<typename T>(std::span<T> span) ALWAYS_INLINE_LAMBDA {
                for (size_t i = 0; i < elements.size(); ++i)
                    span[i] = static_cast<T>(elements[i].getValue());
            });
            if (array->elementsAreRefTypes())
                vm.writeBarrier(array);
        }
        return ConstExprValue(JSValue(array));
    }

    ConstExprValue createNewStruct(TypeSignatureIndex typeIndex)
    {
        auto* structure = m_instance->gcObjectStructure(typeIndex.rawIndex());
        return trackAllocation(structNew(m_instance, structure, static_cast<bool>(UseDefaultValue::Yes), nullptr));
    }

    ConstExprValue createNewStruct(TypeSignatureIndex typeIndex, std::span<const ConstExprValue> fields)
    {
        auto* structure = m_instance->gcObjectStructure(typeIndex.rawIndex());
        JSWebAssemblyStruct* structObject = JSWebAssemblyStruct::tryCreate(m_instance->vm(), structure);
        if (!structObject) [[unlikely]]
            return ConstExprValue(InvalidConstExpr);
        m_keepAlive.appendWithCrashOnOverflow(structObject);

        ASSERT(fields.size() == structure->rtt().fieldCount());
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].type() == ConstExprValue::Vector)
                structObject->set(i, fields[i].getVector());
            else
                structObject->set(i, fields[i].getValue());
        }
        return ConstExprValue(JSValue(structObject));
    }

    const size_t m_offsetInSource;
    const ModuleInformation& m_info;
    JSWebAssemblyInstance* const m_instance;
    MarkedArgumentBufferWithSize<16> m_keepAlive;
};

class ConstExprGenerator {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    using ErrorType = String;
    using PartialResult = std::expected<void, ErrorType>;
    using UnexpectedResult = std::unexpected<ErrorType>;
    using CallType = CallLinkInfo::CallType;

    // Validation only type checks the expression, so no value ever flows through the parser.
    // Evaluation is ConstExprInterpreter, which walks the same bytes without the parser.
    struct ExpressionType { };

    using ResultList = Vector<ExpressionType, 8>;

    // Structured blocks should not appear in the constant expression except
    // for a dummy top-level block from parseBody() that cannot be jumped to.
    struct ControlData {
        static bool NODELETE isIf(const ControlData&) { return false; }
        static bool NODELETE isElse(const ControlData&) { return false; }
        static bool NODELETE isTry(const ControlData&) { return false; }
        static bool NODELETE isAnyCatch(const ControlData&) { return false; }
        static bool NODELETE isCatch(const ControlData&) { return false; }
        static bool NODELETE isTopLevel(const ControlData&) { return true; }
        static bool NODELETE isLoop(const ControlData&) { return false; }
        static bool NODELETE isBlock(const ControlData&) { return false; }

        ControlData()
        { }
        ControlData(BlockSignature&& signature)
            : m_signature(WTF::move(signature))
        { }

        const BlockSignature& NODELETE signature() const { return m_signature; }
        FunctionArgCount NODELETE branchTargetArity() const { return 0; }
        Type NODELETE branchTargetType(unsigned) const { return Types::Void; }
    private:
        BlockSignature m_signature;
    };

    using ControlType = ControlData;
    using ControlEntry = FunctionParser<ConstExprGenerator>::ControlEntry;
    using ControlStack = FunctionParser<ConstExprGenerator>::ControlStack;
    using Stack = FunctionParser<ConstExprGenerator>::Stack;
    using TypedExpression = FunctionParser<ConstExprGenerator>::TypedExpression;
    using CatchHandler = FunctionParser<ConstExprGenerator>::CatchHandler;
    using ArgumentList = FunctionParser<ConstExprGenerator>::ArgumentList;

    static constexpr bool shouldFuseBranchCompare = false;
    static constexpr bool NODELETE tierSupportsSIMD() { return true; }
    static constexpr bool validateFunctionBodySize = false;
    static ExpressionType NODELETE emptyExpression() { return { }; };

protected:
    template <typename ...Args>
    [[nodiscard]] NEVER_INLINE UnexpectedResult fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        size_t offset = m_offsetInSource + m_parser->offset();
        return UnexpectedResult(makeString("WebAssembly.Module doesn't parse at byte "_s, String::number(offset), ": "_s, makeString(args)...));
    }
#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (condition) [[unlikely]]                  \
            return fail(__VA_ARGS__);             \
    } while (0)

public:
    ConstExprGenerator(size_t offsetInSource, const ModuleInformation& info)
        : m_offsetInSource(offsetInSource)
        , m_info(info)
    {
    }

    const Vector<FunctionSpaceIndex>& NODELETE declaredFunctions() const { return m_declaredFunctions; }
    uint32_t NODELETE maxStackHeight() const { return m_maxStackHeight; }
    void NODELETE setParser(FunctionParser<ConstExprGenerator>* parser) { m_parser = parser; };

    bool NODELETE addArguments(const RTT&) { RELEASE_ASSERT_NOT_REACHED(); }

    ExpressionType NODELETE addConstant(Type, uint64_t) { return { }; }

#define CONST_EXPR_STUB { return fail("Invalid instruction for constant expression"); }

    PartialResult addDrop(ExpressionType) CONST_EXPR_STUB
    PartialResult NODELETE addLocal(Type, uint32_t) { RELEASE_ASSERT_NOT_REACHED(); }
    [[nodiscard]] PartialResult addTableGet(unsigned, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTableSet(unsigned, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTableInit(unsigned, unsigned, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addElemDrop(unsigned) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTableSize(unsigned, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTableGrow(unsigned, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTableFill(unsigned, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTableCopy(unsigned, unsigned, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult getLocal(uint32_t, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult setLocal(uint32_t, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult teeLocal(uint32_t, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult getGlobal(uint32_t index, ExpressionType&)
    {
        // Note that this check works for table initializers too, because no globals are registered when the table section is read and the count is 0.
        WASM_COMPILE_FAIL_IF(index >= m_info->globals.size(), "get_global's index ", index, " exceeds the number of globals ", m_info->globals.size());
        WASM_COMPILE_FAIL_IF(m_info->globals[index].mutability != Mutability::Immutable, "get_global import kind index ", index, " is mutable ");

        return { };
    }

    [[nodiscard]] PartialResult setGlobal(uint32_t, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult load(LoadOpType, ExpressionType, ExpressionType&, uint32_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult store(StoreOpType, ExpressionType, ExpressionType, uint32_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addGrowMemory(ExpressionType, ExpressionType&, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCurrentMemory(ExpressionType&, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addMemoryFill(ExpressionType, ExpressionType, ExpressionType, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addMemoryCopy(ExpressionType, ExpressionType, ExpressionType, uint8_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addMemoryInit(unsigned, ExpressionType, ExpressionType, ExpressionType, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addDataDrop(unsigned) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicLoad(ExtAtomicOpType, Type, ExpressionType, ExpressionType&, uint64_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicStore(ExtAtomicOpType, Type, ExpressionType, ExpressionType, uint64_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType&, uint64_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint64_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicWait(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint64_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicNotify(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType&, uint64_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicFence(ExtAtomicOpType, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult truncTrapping(OpType, ExpressionType, ExpressionType&, Type, Type) CONST_EXPR_STUB
    [[nodiscard]] PartialResult truncSaturated(Ext1OpType, ExpressionType, ExpressionType&, Type, Type) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Add128(ExpressionType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Sub128(ExpressionType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64MulWideS(ExpressionType, ExpressionType, ExpressionType&, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64MulWideU(ExpressionType, ExpressionType, ExpressionType&, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addRefI31(ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addI31GetS(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI31GetU(ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addArrayNew(TypeSignatureIndex, ExpressionType, ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addArrayNewDefault(TypeSignatureIndex, ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addArrayNewFixed(TypeSignatureIndex, ArgumentList&, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addArrayNewData(TypeSignatureIndex, uint32_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayNewElem(TypeSignatureIndex, uint32_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayGet(ExtGCOpType, TypeSignatureIndex, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArraySet(TypeSignatureIndex, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayLen(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayFill(TypeSignatureIndex, ExpressionType, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayCopy(TypeSignatureIndex, ExpressionType, ExpressionType, TypeSignatureIndex, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayInitElem(TypeSignatureIndex, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType) CONST_EXPR_STUB;
    [[nodiscard]] PartialResult addArrayInitData(TypeSignatureIndex, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType) CONST_EXPR_STUB;

    [[nodiscard]] PartialResult addStructNewDefault(TypeSignatureIndex, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addStructNew(TypeSignatureIndex, ArgumentList&, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addStructGet(ExtGCOpType, ExpressionType, const RTT&, uint32_t, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addStructSet(ExpressionType, const RTT&, uint32_t, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRefTest(ExpressionType, bool, int32_t, bool, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRefCast(ExpressionType, bool, int32_t, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addAnyConvertExtern(ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addExternConvertAny(ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addI32Add(ExpressionType, ExpressionType, ExpressionType&)
    {
        return { };
    }
    [[nodiscard]] PartialResult addI64Add(ExpressionType, ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addF32Add(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Add(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addI32Sub(ExpressionType, ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addI64Sub(ExpressionType, ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addF32Sub(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Sub(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addI32Mul(ExpressionType, ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addI64Mul(ExpressionType, ExpressionType, ExpressionType&)
    {
        return { };
    }

    [[nodiscard]] PartialResult addF32Mul(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Mul(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32DivS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64DivS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32DivU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64DivU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32RemS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64RemS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32RemU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64RemU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Div(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Div(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Min(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Min(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Max(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Max(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32And(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64And(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Xor(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Xor(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Or(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Or(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Shl(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Shl(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32ShrS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64ShrS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32ShrU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64ShrU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Rotl(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Rotl(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Rotr(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Rotr(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Clz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Clz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Ctz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Ctz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Eq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Eq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Ne(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Ne(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32LtS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64LtS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32LeS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64LeS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32GtS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64GtS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32GeS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64GeS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32LtU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64LtU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32LeU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64LeU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32GtU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64GtU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32GeU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64GeU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Eq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Eq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Ne(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Ne(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Lt(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Lt(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Le(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Le(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Gt(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Gt(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Ge(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Ge(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult addI32WrapI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult addI32Extend8S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Extend16S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Extend8S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Extend16S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Extend32S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64ExtendSI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64ExtendUI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Eqz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Eqz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32Popcnt(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64Popcnt(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32ReinterpretF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64ReinterpretF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32ReinterpretI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64ReinterpretI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32DemoteF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64PromoteF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32ConvertSI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32ConvertUI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32ConvertSI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32ConvertUI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64ConvertSI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64ConvertUI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64ConvertSI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64ConvertUI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Copysign(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Copysign(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Floor(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Floor(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Ceil(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Ceil(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Abs(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Abs(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Sqrt(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Sqrt(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Neg(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Neg(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Nearest(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Nearest(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF32Trunc(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Trunc(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32TruncSF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32TruncSF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32TruncUF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI32TruncUF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64TruncSF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64TruncSF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64TruncUF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI64TruncUF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRefIsNull(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRefAsNonNull(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRefEq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addRefFunc(FunctionSpaceIndex index, ExpressionType&)
    {
        m_declaredFunctions.append(index);
        return { };
    }

    ControlData NODELETE addTopLevel(BlockSignature&& signature)
    {
        return ControlData(WTF::move(signature));
    }

    [[nodiscard]] PartialResult addBlock(BlockSignature, std::span<TypedExpression>, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addLoop(BlockSignature, std::span<TypedExpression>, ControlType&, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addIf(ExpressionType, BlockSignature, std::span<TypedExpression>, ControlData&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addElse(ControlData&, std::span<const TypedExpression>) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addElseToUnreachable(ControlData&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTry(BlockSignature, std::span<TypedExpression>, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTryTable(BlockSignature, std::span<TypedExpression>, const Vector<CatchHandler>&, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCatch(unsigned, const RTT&, std::span<const TypedExpression>, ControlType&, ResultList&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCatchToUnreachable(unsigned, const RTT&, ControlType&, ResultList&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCatchAll(std::span<const TypedExpression>, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCatchAllToUnreachable(ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addDelegate(ControlType&, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addDelegateToUnreachable(ControlType&, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addThrow(unsigned, ArgumentList&, std::span<const TypedExpression>) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRethrow(unsigned, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addThrowRef(ExpressionType, std::span<const TypedExpression>) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addReturn(const ControlData&, std::span<const TypedExpression>) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addBranch(ControlData&, ExpressionType, std::span<const TypedExpression>) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addBranchNull(ControlType&, ExpressionType, std::span<const TypedExpression>, bool, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addBranchCast(ControlType&, ExpressionType, std::span<const TypedExpression>, bool, int32_t, bool) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSwitch(ExpressionType, const Vector<ControlData*>&, ControlData&, std::span<const TypedExpression>) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addFusedBranchCompare(OpType, ControlType&, ExpressionType, std::span<const TypedExpression>) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addFusedBranchCompare(OpType, ControlType&, ExpressionType, ExpressionType, std::span<const TypedExpression>) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addFusedIfCompare(OpType, ExpressionType, BlockSignature, std::span<TypedExpression>, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addFusedIfCompare(OpType, ExpressionType, ExpressionType, BlockSignature, std::span<TypedExpression>, ControlType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult endBlock(ControlEntry& entry, std::span<TypedExpression> enclosedStack)
    {
        ASSERT_UNUSED(enclosedStack, enclosedStack.size() == 1);
        ASSERT_UNUSED(entry, ControlType::isTopLevel(entry.controlData));
        return { };
    }

    [[nodiscard]] PartialResult addEndToUnreachable(ControlEntry&, std::span<TypedExpression>) CONST_EXPR_STUB

    [[nodiscard]] PartialResult endTopLevel(std::span<const TypedExpression>)
    {
        // Some opcodes like "nop" are not detectable by an error stub because the context
        // doesn't get called by the parser. This flag is set by didParseOpcode() to signal
        // such cases.
        WASM_COMPILE_FAIL_IF(m_shouldError, "Invalid instruction for constant expression");
        return { };
    }

    [[nodiscard]] PartialResult addCall(unsigned, FunctionSpaceIndex, const RTT&, ArgumentList&, ResultList&, CallType = CallType::Call) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCallIndirect(unsigned, unsigned, const RTT&, ArgumentList&, ResultList&, CallType = CallType::Call) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCallRef(unsigned, const RTT&, ArgumentList&, ResultList&, CallType = CallType::Call) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addUnreachable() CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCrash() CONST_EXPR_STUB
    bool NODELETE usesSIMD() { return false; }
    void NODELETE notifyFunctionUsesSIMD() { }
    [[nodiscard]] PartialResult addSIMDLoad(ExpressionType, uint64_t, ExpressionType&, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDStore(ExpressionType, ExpressionType, uint64_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDSplat(SIMDLane, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDShuffle(v128_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDLoadSplat(SIMDLaneOperation, ExpressionType, uint64_t, ExpressionType&, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDLoadLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint64_t, uint8_t, ExpressionType&, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDStoreLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint64_t, uint8_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDLoadExtend(SIMDLaneOperation, ExpressionType, uint64_t, ExpressionType&, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDLoadPad(SIMDLaneOperation, ExpressionType, uint64_t, ExpressionType&, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] ExpressionType NODELETE addSIMDConstant(v128_t)
    {
        RELEASE_ASSERT(Options::useWasmSIMD());
        return { };
    }
    [[nodiscard]] PartialResult addSIMDExtractLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDReplaceLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDI_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDV_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDBitwiseSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
#if ENABLE(B3_JIT)
    [[nodiscard]] PartialResult addSIMDRelOp(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, B3::Air::Arg, ExpressionType&) CONST_EXPR_STUB
#endif
    [[nodiscard]] PartialResult addSIMDV_VV(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDRelaxedFMA(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    void NODELETE dump(const ControlStack&, const Stack*) { }
    ALWAYS_INLINE void willParseOpcode() { }
    ALWAYS_INLINE void willParseExtendedOpcode() { }
    ALWAYS_INLINE void didParseOpcode() {
        if (m_parser->currentOpcode() == Nop)
            m_shouldError = true;

        m_maxStackHeight = std::max<uint32_t>(m_maxStackHeight, m_parser->expressionStack().size());
    }
    void NODELETE didFinishParsingLocals() { }
    void NODELETE didPopValueFromStack(ExpressionType, ASCIILiteral) { }


private:
    FunctionParser<ConstExprGenerator>* m_parser { nullptr };
    size_t m_offsetInSource { 0 };
    const Ref<const ModuleInformation> m_info;
    bool m_shouldError = false;
    uint32_t m_maxStackHeight { 0 };
    Vector<FunctionSpaceIndex> m_declaredFunctions;
};

std::expected<void, String> parseExtendedConstExpr(std::span<const uint8_t> source, size_t offsetInSource, size_t& offset, uint32_t& maxStackHeight, ModuleInformation& info, Type expectedType)
{
    ConstExprGenerator generator(offsetInSource, info);
    FunctionParser<ConstExprGenerator> parser(generator, source, BlockSignature { expectedType }, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parseConstantExpression());
    offset = parser.offset();
    maxStackHeight = generator.maxStackHeight();

    for (const auto& declaredFunctionIndex : generator.declaredFunctions())
        info.addDeclaredFunction(declaredFunctionIndex);

    return { };
}

std::expected<uint64_t, String> evaluateExtendedConstExpr(const ModuleInformation::ConstantExpression& constantExpression, JSWebAssemblyInstance* instance, const ModuleInformation& info)
{
    ConstExprInterpreter interpreter(constantExpression.sourceOffset, info, instance);
    return interpreter.run(constantExpression.bytes.span(), constantExpression.maxStackHeight);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

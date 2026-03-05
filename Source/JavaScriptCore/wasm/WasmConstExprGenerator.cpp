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
#include <wtf/Expected.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

class ConstExprGenerator {
public:
    using ErrorType = String;
    using PartialResult = Expected<void, ErrorType>;
    using UnexpectedResult = Unexpected<ErrorType>;
    using CallType = CallLinkInfo::CallType;

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

        bool isInvalid()
        {
            return m_type == ConstExprValueType::Invalid;
        }

        uint64_t getValue()
        {
            ASSERT(m_type == ConstExprValueType::Numeric || m_type == ConstExprValueType::Ref);
            return m_bits;
        }

        v128_t getVector()
        {
            ASSERT(m_type == ConstExprValueType::Vector);
            return m_vector;
        }

        ConstExprValueType type()
        {
            return m_type;
        }

        ConstExprValue operator+(ConstExprValue value)
        {
            ASSERT(m_type == ConstExprValueType::Numeric);
            return ConstExprValue(m_bits + value.getValue());
        }

        ConstExprValue operator-(ConstExprValue value)
        {
            ASSERT(m_type == ConstExprValueType::Numeric);
            return ConstExprValue(m_bits - value.getValue());
        }

        ConstExprValue operator*(ConstExprValue value)
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

    using ExpressionType = ConstExprValue;
    using ResultList = Vector<ExpressionType, 8>;

    // Structured blocks should not appear in the constant expression except
    // for a dummy top-level block from parseBody() that cannot be jumped to.
    struct ControlData {
        static bool isIf(const ControlData&) { return false; }
        static bool isElse(const ControlData&) { return false; }
        static bool isTry(const ControlData&) { return false; }
        static bool isAnyCatch(const ControlData&) { return false; }
        static bool isCatch(const ControlData&) { return false; }
        static bool isTopLevel(const ControlData&) { return true; }
        static bool isLoop(const ControlData&) { return false; }
        static bool isBlock(const ControlData&) { return false; }

        ControlData()
        { }
        ControlData(BlockSignature&& signature)
            : m_signature(WTF::move(signature))
        { }

        const BlockSignature& signature() const { return m_signature; }
        FunctionArgCount branchTargetArity() const { return 0; }
        Type branchTargetType(unsigned) const { return Types::Void; }
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

    enum class Mode : uint8_t {
        Validate,
        Evaluate
    };

    static constexpr bool shouldFuseBranchCompare = false;
    static constexpr bool tierSupportsSIMD() { return true; }
    static constexpr bool validateFunctionBodySize = false;
    static ExpressionType emptyExpression() { return { }; };

protected:
    template <typename ...Args>
    [[nodiscard]] NEVER_INLINE UnexpectedResult fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module doesn't parse at byte "_s, String::number(m_parser->offset() + m_offsetInSource), ": "_s, makeString(args)...));
    }
#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (condition) [[unlikely]]                  \
            return fail(__VA_ARGS__);             \
    } while (0)

public:
    ConstExprGenerator(Mode mode, size_t offsetInSource, const ModuleInformation& info)
        : m_mode(mode)
        , m_offsetInSource(offsetInSource)
        , m_info(info)
    {
        ASSERT(mode == Mode::Validate);
    }

    ConstExprGenerator(Mode mode, const ModuleInformation& info, JSWebAssemblyInstance* instance)
        : m_mode(mode)
        , m_info(info)
        , m_instance(instance)
    {
        ASSERT(mode == Mode::Evaluate);
    }

    ExpressionType result() const { return m_result; }
    const Vector<FunctionSpaceIndex>& declaredFunctions() const { return m_declaredFunctions; }
    void setParser(FunctionParser<ConstExprGenerator>* parser) { m_parser = parser; };

    bool addArguments(const TypeDefinition&) { RELEASE_ASSERT_NOT_REACHED(); }

    ExpressionType addConstant(Type type, uint64_t value)
    {
        switch (type.kind) {
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::F32:
        case TypeKind::F64:
            return ConstExprValue(value);
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Structref:
        case TypeKind::Arrayref:
        case TypeKind::Funcref:
        case TypeKind::Exnref:
        case TypeKind::Externref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Noexnref:
        case TypeKind::Noneref:
        case TypeKind::Nofuncref:
        case TypeKind::Noexternref:
            return ConstExprValue(JSValue::encode(jsNull()));
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant type.\n");
        }
    }

#define CONST_EXPR_STUB { return fail("Invalid instruction for constant expression"); }

    PartialResult addDrop(ExpressionType) CONST_EXPR_STUB
    PartialResult addLocal(Type, uint32_t) { RELEASE_ASSERT_NOT_REACHED(); }
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

    [[nodiscard]] PartialResult getGlobal(uint32_t index, ExpressionType& result)
    {
        // Note that this check works for table initializers too, because no globals are registered when the table section is read and the count is 0.
        WASM_COMPILE_FAIL_IF(index >= m_info.globals.size(), "get_global's index ", index, " exceeds the number of globals ", m_info.globals.size());
        WASM_COMPILE_FAIL_IF(m_info.globals[index].mutability != Mutability::Immutable, "get_global import kind index ", index, " is mutable ");

        if (m_mode == Mode::Evaluate) {
            if (m_info.globals[index].type.kind == TypeKind::V128)
                result = ConstExprValue(m_instance->loadV128Global(index));
            else
                result = ConstExprValue(m_instance->loadI64Global(index));
        }

        return { };
    }

    [[nodiscard]] PartialResult setGlobal(uint32_t, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult load(LoadOpType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult store(StoreOpType, ExpressionType, ExpressionType, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addGrowMemory(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCurrentMemory(ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addMemoryFill(ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addMemoryCopy(ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addMemoryInit(unsigned, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addDataDrop(unsigned) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicLoad(ExtAtomicOpType, Type, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicStore(ExtAtomicOpType, Type, ExpressionType, ExpressionType, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicWait(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicNotify(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult atomicFence(ExtAtomicOpType, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult truncTrapping(OpType, ExpressionType, ExpressionType&, Type, Type) CONST_EXPR_STUB
    [[nodiscard]] PartialResult truncSaturated(Ext1OpType, ExpressionType, ExpressionType&, Type, Type) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addRefI31(ExpressionType value, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            JSValue i31 = JSValue((((static_cast<int32_t>(value.getValue()) & 0x7fffffff) << 1) >> 1));
            ASSERT(i31.isInt32());
            result = ConstExprValue(i31);
        }
        return { };
    }

    [[nodiscard]] PartialResult addI31GetS(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addI31GetU(ExpressionType, ExpressionType&) CONST_EXPR_STUB

    ExpressionType createNewArray(WebAssemblyGCStructure* structure, uint32_t size, ExpressionType value)
    {
        JSValue result;
        if (value.type() == ConstExprValue::Vector)
            result = arrayNew(m_instance, structure, size, value.getVector());
        else
            result = arrayNew(m_instance, structure, size, value.getValue());
        if (result.isNull()) [[unlikely]]
            return ConstExprValue(InvalidConstExpr);
        m_keepAlive.appendWithCrashOnOverflow(asObject(result));
        return ConstExprValue(result);
    }

    [[nodiscard]] PartialResult addArrayNew(uint32_t typeIndex, ExpressionType size, ExpressionType value, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            auto* structure = m_instance->gcObjectStructure(typeIndex);
            result = createNewArray(structure, static_cast<uint32_t>(size.getValue()), value);
            WASM_ALLOCATOR_FAIL_IF(result.isInvalid(), "Failed to allocate new array"_s);
        }
        return { };
    }

    [[nodiscard]] PartialResult addArrayNewDefault(uint32_t typeIndex, ExpressionType size, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            auto* structure = m_instance->gcObjectStructure(typeIndex);
            const Wasm::TypeDefinition& arraySignature = structure->typeDefinition();
            auto elementType = arraySignature.as<Wasm::ArrayType>()->elementType().type.unpacked();
            ExpressionType initValue { };
            if (isRefType(elementType))
                initValue = { static_cast<uint64_t>(JSValue::encode(jsNull())) };
            if (elementType == Wasm::Types::V128)
                initValue = { vectorAllZeros() };
            result = createNewArray(structure, static_cast<uint32_t>(size.getValue()), initValue);
            WASM_ALLOCATOR_FAIL_IF(result.isInvalid(), "Failed to allocate new array"_s);
        }

        return { };
    }

    [[nodiscard]] PartialResult addArrayNewFixed(uint32_t typeIndex, ArgumentList& args, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            auto* structure = m_instance->gcObjectStructure(typeIndex);
            const Wasm::TypeDefinition& arraySignature = structure->typeDefinition();
            if (arraySignature.as<Wasm::ArrayType>()->elementType().type.unpacked().isV128()) {
                result = createNewArray(structure, args.size(), { vectorAllZeros() });
                WASM_ALLOCATOR_FAIL_IF(result.isInvalid(), "Failed to allocate new array"_s);
                JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(JSValue::decode(result.getValue()));
                for (size_t i = 0; i < args.size(); i++)
                    arrayObject->set(arrayObject->vm(), i, args[i].value().getVector());
            } else {
                result = createNewArray(structure, args.size(), { });
                WASM_ALLOCATOR_FAIL_IF(result.isInvalid(), "Failed to allocate new array"_s);
                JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(JSValue::decode(result.getValue()));
                for (size_t i = 0; i < args.size(); i++)
                    arrayObject->set(arrayObject->vm(), i, args[i].value().getValue());
            }
        }

        return { };
    }

    [[nodiscard]] PartialResult addArrayNewData(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayNewElem(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayGet(ExtGCOpType, uint32_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArraySet(uint32_t, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayLen(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayFill(uint32_t, ExpressionType, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayCopy(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addArrayInitElem(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType) CONST_EXPR_STUB;
    [[nodiscard]] PartialResult addArrayInitData(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType) CONST_EXPR_STUB;

    ExpressionType createNewStruct(uint32_t typeIndex)
    {
        auto* structure = m_instance->gcObjectStructure(typeIndex);
        JSValue result = structNew(m_instance, structure, static_cast<bool>(UseDefaultValue::Yes), nullptr);
        if (result.isNull()) [[unlikely]]
            return ConstExprValue(InvalidConstExpr);
        m_keepAlive.appendWithCrashOnOverflow(asObject(result));
        return ConstExprValue(result);
    }

    [[nodiscard]] PartialResult addStructNewDefault(uint32_t typeIndex, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            result = createNewStruct(typeIndex);
            WASM_ALLOCATOR_FAIL_IF(result.isInvalid(), "Failed to allocate new struct"_s);
        }

        return { };
    }

    [[nodiscard]] PartialResult addStructNew(uint32_t typeIndex, ArgumentList& args, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            result = createNewStruct(typeIndex);
            WASM_ALLOCATOR_FAIL_IF(result.isInvalid(), "Failed to allocate new struct"_s);
            JSWebAssemblyStruct* structObject = jsCast<JSWebAssemblyStruct*>(JSValue::decode(result.getValue()));
            for (size_t i = 0; i < args.size(); i++) {
                if (args[i].value().type() == ConstExprValue::Vector)
                    structObject->set(i, args[i].value().getVector());
                else
                    structObject->set(i, args[i].value().getValue());
            }
        }

        return { };
    }

    [[nodiscard]] PartialResult addStructGet(ExtGCOpType, ExpressionType, const StructType&, const RTT&, uint32_t, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addStructSet(ExpressionType, const StructType&, const RTT&, uint32_t, ExpressionType) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRefTest(ExpressionType, bool, int32_t, bool, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRefCast(ExpressionType, bool, int32_t, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addAnyConvertExtern(ExpressionType reference, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            if (reference.type() == ConstExprValue::Numeric)
                result = ConstExprValue(externInternalize(reference.getValue()));
            else
                // To avoid creating a new Strong handle, we pass the original reference.
                // This is valid because we know extern.internalize is a no-op on object
                // references, but if this changes in the future this will need to change.
                result = reference;
        }
        return { };
    }

    [[nodiscard]] PartialResult addExternConvertAny(ExpressionType reference, ExpressionType& result)
    {
        result = reference;
        return { };
    }

    [[nodiscard]] PartialResult addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addI32Add(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs + rhs;
        return { };
    }
    [[nodiscard]] PartialResult addI64Add(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs + rhs;
        return { };
    }

    [[nodiscard]] PartialResult addF32Add(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Add(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addI32Sub(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs - rhs;
        return { };
    }

    [[nodiscard]] PartialResult addI64Sub(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs - rhs;
        return { };
    }

    [[nodiscard]] PartialResult addF32Sub(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addF64Sub(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult addI32Mul(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs * rhs;
        return { };
    }

    [[nodiscard]] PartialResult addI64Mul(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs * rhs;
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

    [[nodiscard]] PartialResult addRefFunc(FunctionSpaceIndex index, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            JSValue wrapper = m_instance->getFunctionWrapper(index);
            ASSERT(!wrapper.isNull());
            ASSERT(wrapper.isObject());
            m_keepAlive.appendWithCrashOnOverflow(asObject(wrapper));
            result = ConstExprValue(wrapper);
        } else
            m_declaredFunctions.append(index);

        return { };
    }

    ControlData addTopLevel(BlockSignature&& signature)
    {
        return ControlData(WTF::move(signature));
    }

    [[nodiscard]] PartialResult addBlock(BlockSignature, Stack&, ControlType&, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addLoop(BlockSignature, Stack&, ControlType&, Stack&, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addIf(ExpressionType, BlockSignature, Stack&, ControlData&, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addElse(ControlData&, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addElseToUnreachable(ControlData&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTry(BlockSignature, Stack&, ControlType&, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addTryTable(BlockSignature, Stack&, const Vector<CatchHandler>&, ControlType&, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCatch(unsigned, const TypeDefinition&, Stack&, ControlType&, ResultList&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCatchToUnreachable(unsigned, const TypeDefinition&, ControlType&, ResultList&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCatchAll(Stack&, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCatchAllToUnreachable(ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addDelegate(ControlType&, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addDelegateToUnreachable(ControlType&, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addThrow(unsigned, ArgumentList&, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addRethrow(unsigned, ControlType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addThrowRef(ExpressionType, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addReturn(const ControlData&, const Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addBranch(ControlData&, ExpressionType, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addBranchNull(ControlType&, ExpressionType, Stack&, bool, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addBranchCast(ControlType&, ExpressionType, Stack&, bool, int32_t, bool) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSwitch(ExpressionType, const Vector<ControlData*>&, ControlData&, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addFusedBranchCompare(OpType, ControlType&, ExpressionType, const Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addFusedBranchCompare(OpType, ControlType&, ExpressionType, ExpressionType, const Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addFusedIfCompare(OpType, ExpressionType, BlockSignature, Stack&, ControlType&, Stack&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addFusedIfCompare(OpType, ExpressionType, ExpressionType, BlockSignature, Stack&, ControlType&, Stack&) CONST_EXPR_STUB

    [[nodiscard]] PartialResult endBlock(ControlEntry& entry, Stack& expressionStack)
    {
        ASSERT(expressionStack.size() == 1);
        ASSERT_UNUSED(entry, ControlType::isTopLevel(entry.controlData));
        m_result = expressionStack.first().value();
        return { };
    }

    [[nodiscard]] PartialResult addEndToUnreachable(ControlEntry&, Stack&, bool = true) CONST_EXPR_STUB

    [[nodiscard]] PartialResult endTopLevel(const Stack&)
    {
        // Some opcodes like "nop" are not detectable by an error stub because the context
        // doesn't get called by the parser. This flag is set by didParseOpcode() to signal
        // such cases.
        WASM_COMPILE_FAIL_IF(m_shouldError, "Invalid instruction for constant expression");
        return { };
    }

    [[nodiscard]] PartialResult addCall(unsigned, FunctionSpaceIndex, const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCallIndirect(unsigned, unsigned, const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCallRef(unsigned, const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addUnreachable() CONST_EXPR_STUB
    [[nodiscard]] PartialResult addCrash() CONST_EXPR_STUB
    bool usesSIMD() { return false; }
    void notifyFunctionUsesSIMD() { }
    [[nodiscard]] PartialResult addSIMDLoad(ExpressionType, uint32_t, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDStore(ExpressionType, ExpressionType, uint32_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDSplat(SIMDLane, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDShuffle(v128_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDLoadSplat(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDLoadLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDStoreLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDLoadExtend(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDLoadPad(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] ExpressionType addConstant(v128_t vector)
    {
        RELEASE_ASSERT(Options::useWasmSIMD());
        if (m_mode == Mode::Evaluate)
            return ConstExprValue(vector);
        return { };
    }
    [[nodiscard]] PartialResult addExtractLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addReplaceLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDI_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDV_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDBitwiseSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
#if ENABLE(B3_JIT)
    [[nodiscard]] PartialResult addSIMDRelOp(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, B3::Air::Arg, ExpressionType&) CONST_EXPR_STUB
#endif
    [[nodiscard]] PartialResult addSIMDV_VV(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    [[nodiscard]] PartialResult addSIMDRelaxedFMA(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    void dump(const ControlStack&, const Stack*) { }
    ALWAYS_INLINE void willParseOpcode() { }
    ALWAYS_INLINE void willParseExtendedOpcode() { }
    ALWAYS_INLINE void didParseOpcode() {
        if (m_parser->currentOpcode() == Nop)
            m_shouldError = true;
    }
    void didFinishParsingLocals() { }
    void didPopValueFromStack(ExpressionType, ASCIILiteral) { }

private:
    FunctionParser<ConstExprGenerator>* m_parser { nullptr };
    Mode m_mode;
    size_t m_offsetInSource;
    ExpressionType m_result;
    const ModuleInformation& m_info;
    JSWebAssemblyInstance* m_instance { nullptr };
    bool m_shouldError = false;
    Vector<FunctionSpaceIndex> m_declaredFunctions;
    MarkedArgumentBufferWithSize<16> m_keepAlive;
};

Expected<void, String> parseExtendedConstExpr(std::span<const uint8_t> source, size_t offsetInSource, size_t& offset, ModuleInformation& info, Type expectedType)
{
    ConstExprGenerator generator(ConstExprGenerator::Mode::Validate, offsetInSource, info);
    FunctionParser<ConstExprGenerator> parser(generator, source, *TypeInformation::typeDefinitionForFunction({ expectedType }, { }), info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parseConstantExpression());
    offset = parser.offset();

    for (const auto& declaredFunctionIndex : generator.declaredFunctions())
        info.addDeclaredFunction(declaredFunctionIndex);

    return { };
}

Expected<uint64_t, String> evaluateExtendedConstExpr(const Vector<uint8_t>& constantExpression, JSWebAssemblyInstance* instance, const ModuleInformation& info, Type expectedType)
{
    ConstExprGenerator generator(ConstExprGenerator::Mode::Evaluate, info, instance);
    FunctionParser<ConstExprGenerator> parser(generator, constantExpression, *TypeInformation::typeDefinitionForFunction({ expectedType }, { }), info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parseConstantExpression());

    ConstExprGenerator::ExpressionType result = generator.result();
    ASSERT(result.type() != ConstExprGenerator::ExpressionType::Vector);

    return { result.getValue() };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

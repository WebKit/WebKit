/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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

namespace JSC { namespace Wasm {

class ConstExprGenerator {
public:
    using ErrorType = String;
    using PartialResult = Expected<void, ErrorType>;
    using UnexpectedResult = Unexpected<ErrorType>;
    using CallType = CallLinkInfo::CallType;

    // Represents values that a constant expression may evaluate to.
    // If a constant expression allocates an object, it should be put in a Strong handle.
    struct ConstExprValue {
        enum ConstExprValueType : uint8_t {
            Numeric,
            Vector,
            Object,
        };

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

        ConstExprValue(Strong<JSObject> object)
            : m_type(ConstExprValueType::Object)
            , m_object(object)
        { }

        uint64_t getValue()
        {
            if (m_type == ConstExprValueType::Numeric)
                return m_bits;
            ASSERT(m_type == ConstExprValueType::Object);
            return JSValue::encode(JSValue(m_object.get()));
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
        Strong<JSObject> m_object;
    };

    using ExpressionType = ConstExprValue;
    using ResultList = Vector<ExpressionType, 8>;

    // Structured blocks should not appear in the constant expression except
    // for a dummy top-level block from parseBody() that cannot be jumped to.
    struct ControlData {
        static bool isIf(const ControlData&) { return false; }
        static bool isTry(const ControlData&) { return false; }
        static bool isAnyCatch(const ControlData&) { return false; }
        static bool isCatch(const ControlData&) { return false; }
        static bool isTopLevel(const ControlData&) { return true; }
        static bool isLoop(const ControlData&) { return false; }
        static bool isBlock(const ControlData&) { return false; }

        ControlData()
        { }
        ControlData(BlockSignature signature)
            : m_signature(signature)
        { }

        BlockSignature signature() const { return m_signature; }
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

    enum class Mode : uint8_t {
        Validate,
        Evaluate
    };

    static constexpr bool tierSupportsSIMD = true;
    static ExpressionType emptyExpression() { return 0; };

protected:
    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module doesn't parse at byte "_s, String::number(m_parser->offset() + m_offsetInSource), ": "_s, makeString(args)...));
    }
#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (UNLIKELY(condition))                  \
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

    ConstExprGenerator(Mode mode, const ModuleInformation& info, RefPtr<Instance> instance)
        : m_mode(mode)
        , m_info(info)
        , m_instance(instance)
    {
        ASSERT(mode == Mode::Evaluate);
    }

    ExpressionType result() const { return m_result; }
    const Vector<uint32_t>& declaredFunctions() const { return m_declaredFunctions; }
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
        case TypeKind::Externref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Nullref:
        case TypeKind::Nullfuncref:
        case TypeKind::Nullexternref:
            return ConstExprValue(JSValue::encode(jsNull()));
        default:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unimplemented constant type.\n");
        }
    }

#define CONST_EXPR_STUB { return fail("Invalid instruction for constant expression"); }

    PartialResult addDrop(ExpressionType) CONST_EXPR_STUB
    PartialResult addLocal(Type, uint32_t) { RELEASE_ASSERT_NOT_REACHED(); }
    PartialResult WARN_UNUSED_RETURN addTableGet(unsigned, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addTableSet(unsigned, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addTableInit(unsigned, unsigned, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addElemDrop(unsigned) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addTableSize(unsigned, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addTableGrow(unsigned, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addTableFill(unsigned, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addTableCopy(unsigned, unsigned, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN getLocal(uint32_t, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN setLocal(uint32_t, ExpressionType) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN getGlobal(uint32_t index, ExpressionType& result)
    {
        // Note that this check works for table initializers too, because no globals are registered when the table section is read and the count is 0.
        WASM_COMPILE_FAIL_IF(index >= m_info.globals.size(), "get_global's index ", index, " exceeds the number of globals ", m_info.globals.size());
        if (!Options::useWebAssemblyGC())
            WASM_COMPILE_FAIL_IF(index >= m_info.firstInternalGlobal, "get_global import kind index ", index, " exceeds the first internal global ", m_info.firstInternalGlobal);
        WASM_COMPILE_FAIL_IF(m_info.globals[index].mutability != Mutability::Immutable, "get_global import kind index ", index, " is mutable ");

        if (m_mode == Mode::Evaluate)
            result = ConstExprValue(m_instance->loadI64Global(index));

        return { };
    }

    PartialResult WARN_UNUSED_RETURN setGlobal(uint32_t, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN load(LoadOpType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN store(StoreOpType, ExpressionType, ExpressionType, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addGrowMemory(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addCurrentMemory(ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addMemoryFill(ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addMemoryCopy(ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addMemoryInit(unsigned, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addDataDrop(unsigned) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN atomicLoad(ExtAtomicOpType, Type, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN atomicStore(ExtAtomicOpType, Type, ExpressionType, ExpressionType, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN atomicWait(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN atomicNotify(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType&, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN atomicFence(ExtAtomicOpType, uint8_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN truncTrapping(OpType, ExpressionType, ExpressionType&, Type, Type) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN truncSaturated(Ext1OpType, ExpressionType, ExpressionType&, Type, Type) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN addRefI31(ExpressionType value, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            JSValue i31 = JSValue((((static_cast<int32_t>(value.getValue()) & 0x7fffffff) << 1) >> 1));
            ASSERT(i31.isInt32());
            result = ConstExprValue(JSValue::encode(i31));
        }
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addI31GetS(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI31GetU(ExpressionType, ExpressionType&) CONST_EXPR_STUB

    ExpressionType createNewArray(uint32_t typeIndex, uint32_t size, ExpressionType value)
    {
        VM& vm = m_instance->vm();
        EncodedJSValue obj;
        if (value.type() == ConstExprValue::Vector)
            obj = arrayNew(m_instance.get(), typeIndex, size, value.getVector());
        else
            obj = arrayNew(m_instance.get(), typeIndex, size, value.getValue());
        return ConstExprValue(Strong<JSObject>(vm, JSValue::decode(obj).getObject()));
    }

    PartialResult WARN_UNUSED_RETURN addArrayNew(uint32_t typeIndex, ExpressionType size, ExpressionType value, ExpressionType& result)
    {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");

        if (m_mode == Mode::Evaluate)
            result = createNewArray(typeIndex, static_cast<uint32_t>(size.getValue()), value);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addArrayNewDefault(uint32_t typeIndex, ExpressionType size, ExpressionType& result)
    {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");

        if (m_mode == Mode::Evaluate) {
            Ref<TypeDefinition> typeDef = m_info.typeSignatures[typeIndex];
            const TypeDefinition& arraySignature = typeDef->expand();
            auto elementType = arraySignature.as<ArrayType>()->elementType().type.unpacked();
            ExpressionType initValue = { 0 };
            if (isRefType(elementType))
                initValue = { static_cast<uint64_t>(JSValue::encode(jsNull())) };
            if (elementType == Wasm::Types::V128)
                initValue = { vectorAllZeros() };
            result = createNewArray(typeIndex, static_cast<uint32_t>(size.getValue()), initValue);
        }

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addArrayNewFixed(uint32_t typeIndex, Vector<ExpressionType>& args, ExpressionType& result)
    {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");

        if (m_mode == Mode::Evaluate) {
            auto* arrayType = m_info.typeSignatures[typeIndex]->expand().as<ArrayType>();
            if (arrayType->elementType().type.unpacked().isV128()) {
                result = createNewArray(typeIndex, args.size(), { vectorAllZeros() });
                JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(JSValue::decode(result.getValue()));
                for (size_t i = 0; i < args.size(); i++)
                    arrayObject->set(i, args[i].getVector());
            } else {
                result = createNewArray(typeIndex, args.size(), { });
                JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(JSValue::decode(result.getValue()));
                for (size_t i = 0; i < args.size(); i++)
                    arrayObject->set(i, args[i].getValue());
            }
        }

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addArrayNewData(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addArrayNewElem(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addArrayGet(ExtGCOpType, uint32_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addArraySet(uint32_t, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addArrayLen(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addArrayFill(uint32_t, ExpressionType, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addArrayCopy(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addArrayInitElem(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType) CONST_EXPR_STUB;
    PartialResult WARN_UNUSED_RETURN addArrayInitData(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType) CONST_EXPR_STUB;

    ExpressionType createNewStruct(uint32_t typeIndex)
    {
        VM& vm = m_instance->vm();
        EncodedJSValue obj = structNew(m_instance.get(), typeIndex, static_cast<bool>(UseDefaultValue::Yes), nullptr);
        return ConstExprValue(Strong<JSObject>(vm, JSValue::decode(obj).getObject()));
    }


    PartialResult WARN_UNUSED_RETURN addStructNewDefault(uint32_t typeIndex, ExpressionType& result)
    {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");

        if (m_mode == Mode::Evaluate)
            result = createNewStruct(typeIndex);

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addStructNew(uint32_t typeIndex, Vector<ExpressionType>& args, ExpressionType& result)
    {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");

        if (m_mode == Mode::Evaluate) {
            result = createNewStruct(typeIndex);
            JSWebAssemblyStruct* structObject = jsCast<JSWebAssemblyStruct*>(JSValue::decode(result.getValue()));
            for (size_t i = 0; i < args.size(); i++) {
                if (args[i].type() == ConstExprValue::Vector)
                    structObject->set(i, args[i].getVector());
                else
                    structObject->set(i, args[i].getValue());
            }
        }

        return { };
    }

    PartialResult WARN_UNUSED_RETURN addStructGet(ExtGCOpType, ExpressionType, const StructType&, uint32_t, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addStructSet(ExpressionType, const StructType&, uint32_t, ExpressionType) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addRefTest(ExpressionType, bool, int32_t, bool, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addRefCast(ExpressionType, bool, int32_t, ExpressionType&) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN addAnyConvertExtern(ExpressionType reference, ExpressionType& result)
    {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");
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

    PartialResult WARN_UNUSED_RETURN addExternConvertAny(ExpressionType reference, ExpressionType& result)
    {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");
        result = reference;
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN addI32Add(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs + rhs;
        return { };
    }
    PartialResult WARN_UNUSED_RETURN addI64Add(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs + rhs;
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addF32Add(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Add(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN addI32Sub(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs - rhs;
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addI64Sub(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs - rhs;
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addF32Sub(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Sub(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN addI32Mul(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs * rhs;
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addI64Mul(ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate)
            result = lhs * rhs;
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addF32Mul(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Mul(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32DivS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64DivS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32DivU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64DivU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32RemS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64RemS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32RemU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64RemU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Div(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Div(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Min(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Min(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Max(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Max(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32And(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64And(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Xor(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Xor(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Or(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Or(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Shl(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Shl(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32ShrS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64ShrS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32ShrU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64ShrU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Rotl(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Rotl(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Rotr(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Rotr(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Clz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Clz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Ctz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Ctz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Eq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Eq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Ne(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Ne(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32LtS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64LtS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32LeS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64LeS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32GtS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64GtS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32GeS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64GeS(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32LtU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64LtU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32LeU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64LeU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32GtU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64GtU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32GeU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64GeU(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Eq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Eq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Ne(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Ne(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Lt(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Lt(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Le(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Le(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Gt(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Gt(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Ge(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Ge(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult addI32WrapI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult addI32Extend8S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Extend16S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Extend8S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Extend16S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Extend32S(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64ExtendSI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64ExtendUI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Eqz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Eqz(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32Popcnt(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64Popcnt(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32ReinterpretF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64ReinterpretF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32ReinterpretI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64ReinterpretI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32DemoteF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64PromoteF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32ConvertSI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32ConvertUI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32ConvertSI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32ConvertUI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64ConvertSI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64ConvertUI32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64ConvertSI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64ConvertUI64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Copysign(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Copysign(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Floor(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Floor(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Ceil(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Ceil(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Abs(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Abs(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Sqrt(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Sqrt(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Neg(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Neg(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Nearest(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Nearest(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF32Trunc(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addF64Trunc(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32TruncSF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32TruncSF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32TruncUF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI32TruncUF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64TruncSF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64TruncSF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64TruncUF32(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addI64TruncUF64(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addRefIsNull(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addRefAsNonNull(ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addRefEq(ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN addRefFunc(uint32_t index, ExpressionType& result)
    {
        if (m_mode == Mode::Evaluate) {
            VM& vm = m_instance->vm();
            JSValue wrapper = m_instance->getFunctionWrapper(index);
            ASSERT(!wrapper.isNull());
            result = ConstExprValue(Strong<JSObject>(vm, wrapper.getObject()));
        } else
            m_declaredFunctions.append(index);

        return { };
    }

    ControlData addTopLevel(BlockSignature signature)
    {
        return ControlData(signature);
    }

    PartialResult WARN_UNUSED_RETURN addBlock(BlockSignature, Stack&, ControlType&, Stack&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addLoop(BlockSignature, Stack&, ControlType&, Stack&, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType, BlockSignature, Stack&, ControlData&, Stack&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addElse(ControlData&, Stack&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlData&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addTry(BlockSignature, Stack&, ControlType&, Stack&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addCatch(unsigned, const TypeDefinition&, Stack&, ControlType&, ResultList&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addCatchToUnreachable(unsigned, const TypeDefinition&, ControlType&, ResultList&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addCatchAll(Stack&, ControlType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addCatchAllToUnreachable(ControlType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addDelegate(ControlType&, ControlType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addDelegateToUnreachable(ControlType&, ControlType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned, Vector<ExpressionType>&, Stack&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const Stack&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType, Stack&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addBranchNull(ControlType&, ExpressionType, Stack&, bool, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addBranchCast(ControlType&, ExpressionType, Stack&, bool, int32_t, bool) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType, const Vector<ControlData*>&, ControlData&, Stack&) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry& entry, Stack& expressionStack)
    {
        ASSERT(expressionStack.size() == 1);
        ASSERT_UNUSED(entry, ControlType::isTopLevel(entry.controlData));
        m_result = expressionStack.first().value();
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, Stack&, bool = true) CONST_EXPR_STUB

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&)
    {
        // Some opcodes like "nop" are not detectable by an error stub because the context
        // doesn't get called by the parser. This flag is set by didParseOpcode() to signal
        // such cases.
        WASM_COMPILE_FAIL_IF(m_shouldError, "Invalid instruction for constant expression");
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addCall(unsigned, const TypeDefinition&, Vector<ExpressionType>&, ResultList&, CallType = CallType::Call) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned, const TypeDefinition&, Vector<ExpressionType>&, ResultList&, CallType = CallType::Call) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addCallRef(const TypeDefinition&, Vector<ExpressionType>&, ResultList&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addUnreachable() CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addCrash() CONST_EXPR_STUB
    void notifyFunctionUsesSIMD() { }
    PartialResult WARN_UNUSED_RETURN addSIMDLoad(ExpressionType, uint32_t, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDStore(ExpressionType, ExpressionType, uint32_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDSplat(SIMDLane, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDShuffle(v128_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDLoadSplat(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDLoadLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDStoreLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDLoadExtend(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDLoadPad(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) CONST_EXPR_STUB
    ExpressionType WARN_UNUSED_RETURN addConstant(v128_t vector)
    {
        RELEASE_ASSERT(Options::useWebAssemblySIMD());
        if (m_mode == Mode::Evaluate)
            return ConstExprValue(vector);
        return { };
    }
    PartialResult WARN_UNUSED_RETURN addExtractLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addReplaceLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDI_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDV_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDBitwiseSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
#if ENABLE(B3_JIT)
    PartialResult WARN_UNUSED_RETURN addSIMDRelOp(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, B3::Air::Arg, ExpressionType&) CONST_EXPR_STUB
#endif
    PartialResult WARN_UNUSED_RETURN addSIMDV_VV(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB
    PartialResult WARN_UNUSED_RETURN addSIMDRelaxedFMA(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType, ExpressionType&) CONST_EXPR_STUB

    void dump(const ControlStack&, const Stack*) { }
    ALWAYS_INLINE void willParseOpcode() { }
    ALWAYS_INLINE void didParseOpcode() {
        if (m_parser->currentOpcode() == Nop)
            m_shouldError = true;
    }
    void didFinishParsingLocals() { }
    void didPopValueFromStack(ExpressionType, String) { }

private:
    FunctionParser<ConstExprGenerator>* m_parser { nullptr };
    Mode m_mode;
    size_t m_offsetInSource;
    ExpressionType m_result;
    const ModuleInformation& m_info;
    RefPtr<Instance> m_instance;
    bool m_shouldError = false;
    Vector<uint32_t> m_declaredFunctions;
};

Expected<void, String> parseExtendedConstExpr(const uint8_t* source, size_t length, size_t offsetInSource, size_t& offset, ModuleInformation& info, Type expectedType)
{
    RELEASE_ASSERT_WITH_MESSAGE(Options::useWebAssemblyExtendedConstantExpressions(), "Wasm extended const expressions not enabled");
    ConstExprGenerator generator(ConstExprGenerator::Mode::Validate, offsetInSource, info);
    FunctionParser<ConstExprGenerator> parser(generator, source, length, *TypeInformation::typeDefinitionForFunction({ expectedType }, { }), info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parseConstantExpression());
    offset = parser.offset();

    for (const auto& declaredFunctionIndex : generator.declaredFunctions())
        info.addDeclaredFunction(declaredFunctionIndex);

    return { };
}

Expected<uint64_t, String> evaluateExtendedConstExpr(const Vector<uint8_t>& constantExpression, RefPtr<Instance> instance, const ModuleInformation& info, Type expectedType)
{
    RELEASE_ASSERT_WITH_MESSAGE(Options::useWebAssemblyExtendedConstantExpressions(), "Wasm extended const expressions not enabled");
    ConstExprGenerator generator(ConstExprGenerator::Mode::Evaluate, info, instance);
    FunctionParser<ConstExprGenerator> parser(generator, constantExpression.data(), constantExpression.size(), *TypeInformation::typeDefinitionForFunction({ expectedType }, { }), info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parseConstantExpression());

    ConstExprGenerator::ExpressionType result = generator.result();
    ASSERT(result.type() != ConstExprGenerator::ExpressionType::Vector);

    return { result.getValue() };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

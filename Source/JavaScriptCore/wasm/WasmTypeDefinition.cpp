/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "WasmTypeDefinition.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmTypeDefinitionInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/StringPrintStream.h>

namespace JSC { namespace Wasm {

String TypeDefinition::toString() const
{
    return WTF::toString(*this);
}

void TypeDefinition::dump(PrintStream& out) const
{
    if (is<FunctionSignature>())
        return as<FunctionSignature>()->dump(out);

    ASSERT(is<StructType>());
    return as<StructType>()->dump(out);
}

String FunctionSignature::toString() const
{
    return WTF::toString(*this);
}

void FunctionSignature::dump(PrintStream& out) const
{
    {
        out.print("(");
        CommaPrinter comma;
        for (FunctionArgCount arg = 0; arg < argumentCount(); ++arg)
            out.print(comma, makeString(argumentType(arg).kind));
        out.print(")");
    }

    {
        CommaPrinter comma;
        out.print(" -> [");
        for (FunctionArgCount ret = 0; ret < returnCount(); ++ret)
            out.print(comma, makeString(returnType(ret).kind));
        out.print("]");
    }
}

String StructType::toString() const
{
    return WTF::toString(*this);
}

void StructType::dump(PrintStream& out) const
{
    out.print("(");
    CommaPrinter comma;
    for (StructFieldCount fieldIndex = 0; fieldIndex < fieldCount(); ++fieldIndex) {
        out.print(comma, makeString(field(fieldIndex).type.kind));
        out.print(comma, field(fieldIndex).mutability ? "immutable" : "mutable");
    }
    out.print(")");
}

static unsigned computeSignatureHash(size_t returnCount, const Type* returnTypes, size_t argumentCount, const Type* argumentTypes)
{
    unsigned accumulator = 0xa1bcedd8u;
    for (uint32_t i = 0; i < argumentCount; ++i) {
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(argumentTypes[i].kind)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(argumentTypes[i].nullable)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<unsigned>::hash(argumentTypes[i].index));
    }
    for (uint32_t i = 0; i < returnCount; ++i) {
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(returnTypes[i].kind)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(returnTypes[i].nullable)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<unsigned>::hash(returnTypes[i].index));
    }
    return accumulator;
}

static unsigned computeStructTypeHash(size_t fieldCount, const StructField* fields)
{
    unsigned accumulator = 0x15d2546;
    for (uint32_t i = 0; i < fieldCount; ++i) {
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(fields[i].type.kind)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(fields[i].type.nullable)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(fields[i].type.index)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(fields[i].mutability)));
    }
    return accumulator;
}

unsigned TypeDefinition::hash() const
{
    if (is<FunctionSignature>()) {
        const FunctionSignature* signature = as<FunctionSignature>();
        return computeSignatureHash(signature->returnCount(), signature->storage(0), signature->argumentCount(), signature->storage(signature->returnCount()));
    }

    ASSERT(is<StructType>());
    const StructType* structType = as<StructType>();
    return computeStructTypeHash(structType->fieldCount(), structType->storage(0));
}

RefPtr<TypeDefinition> TypeDefinition::tryCreateFunctionSignature(FunctionArgCount returnCount, FunctionArgCount argumentCount)
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedFunctionSize(returnCount, argumentCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    TypeDefinition* signature = new (NotNull, memory) TypeDefinition(returnCount, argumentCount);
    return adoptRef(signature);
}

RefPtr<TypeDefinition> TypeDefinition::tryCreateStructType(StructFieldCount fieldCount)
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedStructSize(fieldCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    TypeDefinition* signature = new (NotNull, memory) TypeDefinition(fieldCount);
    return adoptRef(signature);
}

TypeInformation::TypeInformation()
{
#define MAKE_THUNK_SIGNATURE(type, enc, str, val, _) \
    do { \
        if (TypeKind::type != TypeKind::Void) { \
            RefPtr<TypeDefinition> sig = TypeDefinition::tryCreateFunctionSignature(1, 0); \
            sig->ref();                                                                    \
            sig->as<FunctionSignature>()->getReturnType(0) = Types::type;                  \
            thunkTypes[linearizeType(TypeKind::type)] = sig.get();                         \
            m_typeSet.add(TypeHash { sig.releaseNonNull() });                              \
        }                                                                                  \
    } while (false);

    FOR_EACH_WASM_TYPE(MAKE_THUNK_SIGNATURE);

    // Make Void again because we don't use the one that has void in it.
    {
        RefPtr<TypeDefinition> sig = TypeDefinition::tryCreateFunctionSignature(0, 0);
        sig->ref();
        thunkTypes[linearizeType(TypeKind::Void)] = sig.get();
        m_typeSet.add(TypeHash { sig.releaseNonNull() });
    }
}

struct FunctionParameterTypes {
    const Vector<Type, 1>& returnTypes;
    const Vector<Type>& argumentTypes;

    static unsigned hash(const FunctionParameterTypes& params)
    {
        return computeSignatureHash(params.returnTypes.size(), params.returnTypes.data(), params.argumentTypes.size(), params.argumentTypes.data());
    }

    static bool equal(const TypeHash& sig, const FunctionParameterTypes& params)
    {
        if (sig.key->is<StructType>())
            return false;

        const FunctionSignature* signature = sig.key->as<FunctionSignature>();
        if (signature->argumentCount() != params.argumentTypes.size())
            return false;
        if (signature->returnCount() != params.returnTypes.size())
            return false;

        for (unsigned i = 0; i < signature->argumentCount(); ++i) {
            if (signature->argumentType(i) != params.argumentTypes[i])
                return false;
        }

        for (unsigned i = 0; i < signature->returnCount(); ++i) {
            if (signature->returnType(i) != params.returnTypes[i])
                return false;
        }
        return true;
    }

    static void translate(TypeHash& entry, const FunctionParameterTypes& params, unsigned)
    {
        RefPtr<TypeDefinition> signature = TypeDefinition::tryCreateFunctionSignature(params.returnTypes.size(), params.argumentTypes.size());
        RELEASE_ASSERT(signature);

        for (unsigned i = 0; i < params.returnTypes.size(); ++i)
            signature->as<FunctionSignature>()->getReturnType(i) = params.returnTypes[i];

        for (unsigned i = 0; i < params.argumentTypes.size(); ++i)
            signature->as<FunctionSignature>()->getArgumentType(i) = params.argumentTypes[i];

        entry.key = WTFMove(signature);
    }
};

struct StructParameterTypes {
    const Vector<StructField>& fields;

    static unsigned hash(const StructParameterTypes& params)
    {
        return computeStructTypeHash(params.fields.size(), params.fields.data());
    }

    static bool equal(const TypeHash& sig, const StructParameterTypes& params)
    {
        if (sig.key->is<FunctionSignature>())
            return false;

        const StructType* structType = sig.key->as<StructType>();
        if (structType->fieldCount() != params.fields.size())
            return false;

        for (unsigned i = 0; i < structType->fieldCount(); ++i) {
            if (structType->field(i) != params.fields[i])
                return false;
        }

        return true;
    }

    static void translate(TypeHash& entry, const StructParameterTypes& params, unsigned)
    {
        RefPtr<TypeDefinition> signature = TypeDefinition::tryCreateStructType(params.fields.size());
        RELEASE_ASSERT(signature);

        StructType* structType = signature->as<StructType>();
        for (unsigned i = 0; i < params.fields.size(); ++i)
            structType->getField(i) = params.fields[i];

        entry.key = WTFMove(signature);
    }
};

RefPtr<TypeDefinition> TypeInformation::typeDefinitionForFunction(const Vector<Type, 1>& results, const Vector<Type>& args)
{
    if constexpr (ASSERT_ENABLED) {
        ASSERT(!results.contains(Wasm::Types::Void));
        ASSERT(!args.contains(Wasm::Types::Void));
    }
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { results, args });
    return addResult.iterator->key;
}

RefPtr<TypeDefinition> TypeInformation::typeDefinitionForStruct(const Vector<StructField>& fields)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<StructParameterTypes>(StructParameterTypes { fields });
    return addResult.iterator->key;
}

void TypeInformation::tryCleanup()
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    info.m_typeSet.removeIf([&] (auto& hash) {
        const auto& signature = hash.key;
        return signature->refCount() == 1;
    });
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

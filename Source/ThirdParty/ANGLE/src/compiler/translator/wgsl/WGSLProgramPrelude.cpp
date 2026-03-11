//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/wgsl/WGSLProgramPrelude.h"
#include "common/log_utils.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/Operator_autogen.h"
#include "compiler/translator/util.h"
#include "compiler/translator/wgsl/Utils.h"

namespace sh
{

namespace
{
constexpr ImmutableString kEndParanthesis = ImmutableString(")");

void EmitConstructorList(TInfoSinkBase &sink, const TType &type, ImmutableString scalar)
{
    ASSERT(!type.isArray());
    ASSERT(!type.getStruct());

    sink << "(";
    size_t numScalars = 1;
    if (type.isMatrix())
    {
        numScalars = type.getCols() * type.getRows();
    }
    for (size_t i = 0; i < numScalars; i++)
    {
        if (i != 0)
        {
            sink << ", ";
        }
        sink << scalar;
    }
    sink << ")";
}

ImmutableString ConcatId(const char *prefix, uint64_t funcId)
{
    return BuildConcatenatedImmutableString("ANGLE_", prefix, "_", funcId);
}

template <typename T>
uint64_t InsertIntoMapWithUniqueId(uint64_t &idCounter, TMap<T, uint64_t> &map, const T &key)
{
    auto [iterator, inserted] = map.try_emplace(key, idCounter);

    if (inserted)
    {
        idCounter++;
    }

    return iterator->second;
}
}  // namespace

WGSLWrapperFunction WGSLProgramPrelude::preIncrement(const TType &incrementedType)
{
    ASSERT(incrementedType.getBasicType() == EbtInt || incrementedType.getBasicType() == EbtUInt ||
           incrementedType.getBasicType() == EbtFloat);

    uint64_t uniqueId =
        InsertIntoMapWithUniqueId(mUniqueFuncId, mPreIncrementedTypes, incrementedType);
    switch (GetWgslAddressSpaceForPointer(incrementedType))
    {
        case WgslPointerAddressSpace::Function:
            return {BuildConcatenatedImmutableString(ConcatId("preIncFunc", uniqueId), "(&"),
                    kEndParanthesis};
        case WgslPointerAddressSpace::Private:
            // EvqGlobal and various other shader outputs/builtins are all globals.
            return {BuildConcatenatedImmutableString(ConcatId("preIncPriv", uniqueId), "(&"),
                    kEndParanthesis};
    }
}

WGSLWrapperFunction WGSLProgramPrelude::preDecrement(const TType &decrementedType)
{
    uint64_t uniqueId =
        InsertIntoMapWithUniqueId(mUniqueFuncId, mPreDecrementedTypes, decrementedType);

    switch (GetWgslAddressSpaceForPointer(decrementedType))
    {
        case WgslPointerAddressSpace::Function:
            return {BuildConcatenatedImmutableString(ConcatId("preDecFunc", uniqueId), "(&"),
                    kEndParanthesis};
        case WgslPointerAddressSpace::Private:
            // EvqGlobal and various other shader outputs/builtins are all globals.
            return {BuildConcatenatedImmutableString(ConcatId("preDecPriv", uniqueId), "(&"),
                    kEndParanthesis};
    }
}

WGSLWrapperFunction WGSLProgramPrelude::postIncrement(const TType &incrementedType)
{
    uint64_t uniqueId =
        InsertIntoMapWithUniqueId(mUniqueFuncId, mPostIncrementedTypes, incrementedType);

    switch (GetWgslAddressSpaceForPointer(incrementedType))
    {
        case WgslPointerAddressSpace::Function:
            return {BuildConcatenatedImmutableString(ConcatId("postIncFunc", uniqueId), "(&"),
                    kEndParanthesis};
        case WgslPointerAddressSpace::Private:
            // EvqGlobal and various other shader outputs/builtins are all globals.
            return {BuildConcatenatedImmutableString(ConcatId("postIncPriv", uniqueId), "(&"),
                    kEndParanthesis};
    }
}

WGSLWrapperFunction WGSLProgramPrelude::postDecrement(const TType &decrementedType)
{
    uint64_t uniqueId =
        InsertIntoMapWithUniqueId(mUniqueFuncId, mPostDecrementedTypes, decrementedType);

    switch (GetWgslAddressSpaceForPointer(decrementedType))
    {
        case WgslPointerAddressSpace::Function:
            return {BuildConcatenatedImmutableString(ConcatId("postDecFunc", uniqueId), "(&"),
                    kEndParanthesis};
        case WgslPointerAddressSpace::Private:
            // EvqGlobal and various other shader outputs/builtins are all globals.
            return {BuildConcatenatedImmutableString(ConcatId("postDecPriv", uniqueId), "(&"),
                    kEndParanthesis};
    }
}

void WGSLProgramPrelude::outputPrelude(TInfoSinkBase &sink)
{
    auto genPreIncOrDec = [&](ImmutableString addressSpace, const TType &type, ImmutableString op,
                              ImmutableString funcName) {
        TStringStream typeStr;
        WriteWgslType(typeStr, type, {});

        sink << "fn " << funcName << "(x : ptr<" << addressSpace << ", " << typeStr.str()
             << ">) -> " << typeStr.str() << " {\n";
        sink << "  (*x) " << op << " " << typeStr.str();
        EmitConstructorList(sink, type, ImmutableString("1"));
        sink << ";\n";
        sink << "  return *x;\n";
        sink << "}\n";
    };
    for (const std::pair<const TType, FuncId> &elem : mPreIncrementedTypes)
    {

        // NOTE: it's easiest just to generate increments and decrements functions for variables
        // that live in either the function-local scope or the module-local scope. TType holds a
        // qualifier, but its operator== and operator< ignore the qualifier. We could keep track of
        // the qualifiers used but that's overkill.
        genPreIncOrDec(ImmutableString("private"), elem.first, ImmutableString("+="),
                       ConcatId("preIncPriv", elem.second));
        genPreIncOrDec(ImmutableString("function"), elem.first, ImmutableString("+="),
                       ConcatId("preIncFunc", elem.second));
    }
    for (const std::pair<const TType, FuncId> &elem : mPreDecrementedTypes)
    {

        // NOTE: it's easiest just to generate increments and decrements functions for variables
        // that live in either the function-local scope or the module-local scope. TType holds a
        // qualifier, but its operator== and operator< ignore the qualifier. We could keep track of
        // the qualifiers used but that's overkill.
        genPreIncOrDec(ImmutableString("private"), elem.first, ImmutableString("-="),
                       ConcatId("preDecPriv", elem.second));
        genPreIncOrDec(ImmutableString("function"), elem.first, ImmutableString("-="),
                       ConcatId("preDecFunc", elem.second));
    }

    auto genPostIncOrDec = [&](ImmutableString addressSpace, const TType &type, ImmutableString op,
                               ImmutableString funcName) {
        TStringStream typeStr;
        WriteWgslType(typeStr, type, {});

        sink << "fn " << funcName << "(x : ptr<" << addressSpace << ", " << typeStr.str()
             << ">) -> " << typeStr.str() << " {\n";
        sink << "  var old = *x;\n";
        sink << "  (*x) " << op << " " << typeStr.str();
        EmitConstructorList(sink, type, ImmutableString("1"));
        sink << ";\n";
        sink << "  return old;\n";
        sink << "}\n";
    };
    for (const std::pair<const TType, FuncId> &elem : mPostIncrementedTypes)
    {
        // NOTE: it's easiest just to generate increments and decrements functions for variables
        // that live in either the function-local scope or the module-local scope. TType holds a
        // qualifier, but its operator== and operator< ignore the qualifier. We could keep track of
        // the qualifiers used but that's overkill.
        genPostIncOrDec(ImmutableString("private"), elem.first, ImmutableString("+="),
                        ConcatId("postIncPriv", elem.second));
        genPostIncOrDec(ImmutableString("function"), elem.first, ImmutableString("+="),
                        ConcatId("postIncFunc", elem.second));
    }
    for (const std::pair<const TType, FuncId> &elem : mPostDecrementedTypes)
    {
        // NOTE: it's easiest just to generate increments and decrements functions for variables
        // that live in either the function-local scope or the module-local scope. TType holds a
        // qualifier, but its operator== and operator< ignore the qualifier. We could keep track of
        // the qualifiers used but that's overkill.
        genPostIncOrDec(ImmutableString("private"), elem.first, ImmutableString("-="),
                        ConcatId("postDecPriv", elem.second));
        genPostIncOrDec(ImmutableString("function"), elem.first, ImmutableString("-="),
                        ConcatId("postDecFunc", elem.second));
    }
}

}  // namespace sh

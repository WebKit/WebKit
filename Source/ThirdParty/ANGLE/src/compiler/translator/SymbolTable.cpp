//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Symbol table for parsing.  Most functionaliy and main ideas
// are documented in the header file.
//

#if defined(_MSC_VER)
#pragma warning(disable: 4718)
#endif

#include "compiler/translator/SymbolTable.h"

#include <stdio.h>
#include <algorithm>

int TSymbolTable::uniqueIdCounter = 0;

//
// Functions have buried pointers to delete.
//
TFunction::~TFunction()
{
    for (TParamList::iterator i = parameters.begin(); i != parameters.end(); ++i)
        delete (*i).type;
}

//
// Symbol table levels are a map of pointers to symbols that have to be deleted.
//
TSymbolTableLevel::~TSymbolTableLevel()
{
    for (tLevel::iterator it = level.begin(); it != level.end(); ++it)
        delete (*it).second;
}

bool TSymbolTableLevel::insert(TSymbol *symbol)
{
    symbol->setUniqueId(TSymbolTable::nextUniqueId());

    // returning true means symbol was added to the table
    tInsertResult result = level.insert(tLevelPair(symbol->getMangledName(), symbol));

    return result.second;
}

bool TSymbolTableLevel::insertUnmangled(TFunction *function)
{
    function->setUniqueId(TSymbolTable::nextUniqueId());

    // returning true means symbol was added to the table
    tInsertResult result = level.insert(tLevelPair(function->getName(), function));

    return result.second;
}

TSymbol *TSymbolTableLevel::find(const TString &name) const
{
    tLevel::const_iterator it = level.find(name);
    if (it == level.end())
        return 0;
    else
        return (*it).second;
}

TSymbol *TSymbolTable::find(const TString &name, int shaderVersion,
                            bool *builtIn, bool *sameScope) const
{
    int level = currentLevel();
    TSymbol *symbol;

    do
    {
        if (level == ESSL3_BUILTINS && shaderVersion != 300)
            level--;
        if (level == ESSL1_BUILTINS && shaderVersion != 100)
            level--;

        symbol = table[level]->find(name);
    }
    while (symbol == 0 && --level >= 0);

    if (builtIn)
        *builtIn = (level <= LAST_BUILTIN_LEVEL);
    if (sameScope)
        *sameScope = (level == currentLevel());

    return symbol;
}

TSymbol *TSymbolTable::findBuiltIn(
    const TString &name, int shaderVersion) const
{
    for (int level = LAST_BUILTIN_LEVEL; level >= 0; level--)
    {
        if (level == ESSL3_BUILTINS && shaderVersion != 300)
            level--;
        if (level == ESSL1_BUILTINS && shaderVersion != 100)
            level--;

        TSymbol *symbol = table[level]->find(name);

        if (symbol)
            return symbol;
    }

    return 0;
}

TSymbolTable::~TSymbolTable()
{
    while (table.size() > 0)
        pop();
}

bool IsGenType(const TType *type)
{
    if (type)
    {
        TBasicType basicType = type->getBasicType();
        return basicType == EbtGenType || basicType == EbtGenIType || basicType == EbtGenUType || basicType == EbtGenBType;
    }

    return false;
}

bool IsVecType(const TType *type)
{
    if (type)
    {
        TBasicType basicType = type->getBasicType();
        return basicType == EbtVec || basicType == EbtIVec || basicType == EbtUVec || basicType == EbtBVec;
    }

    return false;
}

TType *SpecificType(TType *type, int size)
{
    ASSERT(size >= 1 && size <= 4);

    if (!type)
    {
        return nullptr;
    }

    ASSERT(!IsVecType(type));

    switch(type->getBasicType())
    {
      case EbtGenType:  return new TType(EbtFloat, static_cast<unsigned char>(size));
      case EbtGenIType: return new TType(EbtInt, static_cast<unsigned char>(size));
      case EbtGenUType: return new TType(EbtUInt, static_cast<unsigned char>(size));
      case EbtGenBType: return new TType(EbtBool, static_cast<unsigned char>(size));
      default: return type;
    }
}

TType *VectorType(TType *type, int size)
{
    ASSERT(size >= 2 && size <= 4);

    if (!type)
    {
        return nullptr;
    }

    ASSERT(!IsGenType(type));

    switch(type->getBasicType())
    {
      case EbtVec:  return new TType(EbtFloat, static_cast<unsigned char>(size));
      case EbtIVec: return new TType(EbtInt, static_cast<unsigned char>(size));
      case EbtUVec: return new TType(EbtUInt, static_cast<unsigned char>(size));
      case EbtBVec: return new TType(EbtBool, static_cast<unsigned char>(size));
      default: return type;
    }
}

void TSymbolTable::insertBuiltIn(ESymbolLevel level, TOperator op, const char *ext, TType *rvalue, const char *name,
                                 TType *ptype1, TType *ptype2, TType *ptype3, TType *ptype4, TType *ptype5)
{
    if (ptype1->getBasicType() == EbtGSampler2D)
    {
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? new TType(EbtFloat, 4) : rvalue, name, new TType(EbtSampler2D), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? new TType(EbtInt, 4) : rvalue, name, new TType(EbtISampler2D), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? new TType(EbtUInt, 4) : rvalue, name, new TType(EbtUSampler2D), ptype2, ptype3, ptype4, ptype5);
    }
    else if (ptype1->getBasicType() == EbtGSampler3D)
    {
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? new TType(EbtFloat, 4) : rvalue, name, new TType(EbtSampler3D), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? new TType(EbtInt, 4) : rvalue, name, new TType(EbtISampler3D), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? new TType(EbtUInt, 4) : rvalue, name, new TType(EbtUSampler3D), ptype2, ptype3, ptype4, ptype5);
    }
    else if (ptype1->getBasicType() == EbtGSamplerCube)
    {
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? new TType(EbtFloat, 4) : rvalue, name, new TType(EbtSamplerCube), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? new TType(EbtInt, 4) : rvalue, name, new TType(EbtISamplerCube), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? new TType(EbtUInt, 4) : rvalue, name, new TType(EbtUSamplerCube), ptype2, ptype3, ptype4, ptype5);
    }
    else if (ptype1->getBasicType() == EbtGSampler2DArray)
    {
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? new TType(EbtFloat, 4) : rvalue, name, new TType(EbtSampler2DArray), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? new TType(EbtInt, 4) : rvalue, name, new TType(EbtISampler2DArray), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? new TType(EbtUInt, 4) : rvalue, name, new TType(EbtUSampler2DArray), ptype2, ptype3, ptype4, ptype5);
    }
    else if (IsGenType(rvalue) || IsGenType(ptype1) || IsGenType(ptype2) || IsGenType(ptype3))
    {
        ASSERT(!ptype4 && !ptype5);
        insertBuiltIn(level, op, ext, SpecificType(rvalue, 1), name, SpecificType(ptype1, 1), SpecificType(ptype2, 1), SpecificType(ptype3, 1));
        insertBuiltIn(level, op, ext, SpecificType(rvalue, 2), name, SpecificType(ptype1, 2), SpecificType(ptype2, 2), SpecificType(ptype3, 2));
        insertBuiltIn(level, op, ext, SpecificType(rvalue, 3), name, SpecificType(ptype1, 3), SpecificType(ptype2, 3), SpecificType(ptype3, 3));
        insertBuiltIn(level, op, ext, SpecificType(rvalue, 4), name, SpecificType(ptype1, 4), SpecificType(ptype2, 4), SpecificType(ptype3, 4));
    }
    else if (IsVecType(rvalue) || IsVecType(ptype1) || IsVecType(ptype2) || IsVecType(ptype3))
    {
        ASSERT(!ptype4 && !ptype5);
        insertBuiltIn(level, op, ext, VectorType(rvalue, 2), name, VectorType(ptype1, 2), VectorType(ptype2, 2), VectorType(ptype3, 2));
        insertBuiltIn(level, op, ext, VectorType(rvalue, 3), name, VectorType(ptype1, 3), VectorType(ptype2, 3), VectorType(ptype3, 3));
        insertBuiltIn(level, op, ext, VectorType(rvalue, 4), name, VectorType(ptype1, 4), VectorType(ptype2, 4), VectorType(ptype3, 4));
    }
    else
    {
        TFunction *function = new TFunction(NewPoolTString(name), *rvalue, op, ext);

        TParameter param1 = {0, ptype1};
        function->addParameter(param1);

        if (ptype2)
        {
            TParameter param2 = {0, ptype2};
            function->addParameter(param2);
        }

        if (ptype3)
        {
            TParameter param3 = {0, ptype3};
            function->addParameter(param3);
        }

        if (ptype4)
        {
            TParameter param4 = {0, ptype4};
            function->addParameter(param4);
        }

        if (ptype5)
        {
            TParameter param5 = {0, ptype5};
            function->addParameter(param5);
        }

        insert(level, function);
    }
}

TPrecision TSymbolTable::getDefaultPrecision(TBasicType type) const
{
    if (!SupportsPrecision(type))
        return EbpUndefined;

    // unsigned integers use the same precision as signed
    TBasicType baseType = (type == EbtUInt) ? EbtInt : type;

    int level = static_cast<int>(precisionStack.size()) - 1;
    assert(level >= 0); // Just to be safe. Should not happen.
    // If we dont find anything we return this. Should we error check this?
    TPrecision prec = EbpUndefined;
    while (level >= 0)
    {
        PrecisionStackLevel::iterator it = precisionStack[level]->find(baseType);
        if (it != precisionStack[level]->end())
        {
            prec = (*it).second;
            break;
        }
        level--;
    }
    return prec;
}

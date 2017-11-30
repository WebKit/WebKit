//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Symbol table for parsing. The design principles and most of the functionality are documented in
// the header file.
//

#if defined(_MSC_VER)
#pragma warning(disable : 4718)
#endif

#include "compiler/translator/SymbolTable.h"

#include "compiler/translator/Cache.h"
#include "compiler/translator/IntermNode.h"

#include <stdio.h>
#include <algorithm>

namespace sh
{

namespace
{

static const char kFunctionMangledNameSeparator = '(';

}  // anonymous namespace

TSymbol::TSymbol(TSymbolTable *symbolTable, const TString *n)
    : uniqueId(symbolTable->nextUniqueId()), name(n), extension(TExtension::UNDEFINED)
{
}

//
// Functions have buried pointers to delete.
//
TFunction::~TFunction()
{
    clearParameters();
}

void TFunction::clearParameters()
{
    for (TParamList::iterator i = parameters.begin(); i != parameters.end(); ++i)
        delete (*i).type;
    parameters.clear();
    mangledName = nullptr;
}

void TFunction::swapParameters(const TFunction &parametersSource)
{
    clearParameters();
    for (auto parameter : parametersSource.parameters)
    {
        addParameter(parameter);
    }
}

const TString *TFunction::buildMangledName() const
{
    std::string newName = getName().c_str();
    newName += kFunctionMangledNameSeparator;

    for (const auto &p : parameters)
    {
        newName += p.type->getMangledName();
    }
    return NewPoolTString(newName.c_str());
}

const TString &TFunction::GetMangledNameFromCall(const TString &functionName,
                                                 const TIntermSequence &arguments)
{
    std::string newName = functionName.c_str();
    newName += kFunctionMangledNameSeparator;

    for (TIntermNode *argument : arguments)
    {
        newName += argument->getAsTyped()->getType().getMangledName();
    }
    return *NewPoolTString(newName.c_str());
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
    // returning true means symbol was added to the table
    tInsertResult result = level.insert(tLevelPair(symbol->getMangledName(), symbol));

    return result.second;
}

bool TSymbolTableLevel::insertUnmangled(TFunction *function)
{
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

TSymbol *TSymbolTable::find(const TString &name,
                            int shaderVersion,
                            bool *builtIn,
                            bool *sameScope) const
{
    int level = currentLevel();
    TSymbol *symbol;

    do
    {
        if (level == GLSL_BUILTINS)
            level--;
        if (level == ESSL3_1_BUILTINS && shaderVersion != 310)
            level--;
        if (level == ESSL3_BUILTINS && shaderVersion < 300)
            level--;
        if (level == ESSL1_BUILTINS && shaderVersion != 100)
            level--;

        symbol = table[level]->find(name);
    } while (symbol == 0 && --level >= 0);

    if (builtIn)
        *builtIn = (level <= LAST_BUILTIN_LEVEL);
    if (sameScope)
        *sameScope = (level == currentLevel());

    return symbol;
}

TSymbol *TSymbolTable::findGlobal(const TString &name) const
{
    ASSERT(table.size() > GLOBAL_LEVEL);
    return table[GLOBAL_LEVEL]->find(name);
}

TSymbol *TSymbolTable::findBuiltIn(const TString &name, int shaderVersion) const
{
    return findBuiltIn(name, shaderVersion, false);
}

TSymbol *TSymbolTable::findBuiltIn(const TString &name,
                                   int shaderVersion,
                                   bool includeGLSLBuiltins) const
{
    for (int level = LAST_BUILTIN_LEVEL; level >= 0; level--)
    {
        if (level == GLSL_BUILTINS && !includeGLSLBuiltins)
            level--;
        if (level == ESSL3_1_BUILTINS && shaderVersion != 310)
            level--;
        if (level == ESSL3_BUILTINS && shaderVersion < 300)
            level--;
        if (level == ESSL1_BUILTINS && shaderVersion != 100)
            level--;

        TSymbol *symbol = table[level]->find(name);

        if (symbol)
            return symbol;
    }

    return nullptr;
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
        return basicType == EbtGenType || basicType == EbtGenIType || basicType == EbtGenUType ||
               basicType == EbtGenBType;
    }

    return false;
}

bool IsVecType(const TType *type)
{
    if (type)
    {
        TBasicType basicType = type->getBasicType();
        return basicType == EbtVec || basicType == EbtIVec || basicType == EbtUVec ||
               basicType == EbtBVec;
    }

    return false;
}

const TType *SpecificType(const TType *type, int size)
{
    ASSERT(size >= 1 && size <= 4);

    if (!type)
    {
        return nullptr;
    }

    ASSERT(!IsVecType(type));

    switch (type->getBasicType())
    {
        case EbtGenType:
            return TCache::getType(EbtFloat, type->getQualifier(),
                                   static_cast<unsigned char>(size));
        case EbtGenIType:
            return TCache::getType(EbtInt, type->getQualifier(), static_cast<unsigned char>(size));
        case EbtGenUType:
            return TCache::getType(EbtUInt, type->getQualifier(), static_cast<unsigned char>(size));
        case EbtGenBType:
            return TCache::getType(EbtBool, type->getQualifier(), static_cast<unsigned char>(size));
        default:
            return type;
    }
}

const TType *VectorType(const TType *type, int size)
{
    ASSERT(size >= 2 && size <= 4);

    if (!type)
    {
        return nullptr;
    }

    ASSERT(!IsGenType(type));

    switch (type->getBasicType())
    {
        case EbtVec:
            return TCache::getType(EbtFloat, static_cast<unsigned char>(size));
        case EbtIVec:
            return TCache::getType(EbtInt, static_cast<unsigned char>(size));
        case EbtUVec:
            return TCache::getType(EbtUInt, static_cast<unsigned char>(size));
        case EbtBVec:
            return TCache::getType(EbtBool, static_cast<unsigned char>(size));
        default:
            return type;
    }
}

TVariable *TSymbolTable::declareVariable(const TString *name, const TType &type)
{
    return insertVariable(currentLevel(), name, type);
}

TVariable *TSymbolTable::declareStructType(TStructure *str)
{
    return insertStructType(currentLevel(), str);
}

TInterfaceBlockName *TSymbolTable::declareInterfaceBlockName(const TString *name)
{
    TInterfaceBlockName *blockNameSymbol = new TInterfaceBlockName(this, name);
    if (insert(currentLevel(), blockNameSymbol))
    {
        return blockNameSymbol;
    }
    return nullptr;
}

TInterfaceBlockName *TSymbolTable::insertInterfaceBlockNameExt(ESymbolLevel level,
                                                               TExtension ext,
                                                               const TString *name)
{
    TInterfaceBlockName *blockNameSymbol = new TInterfaceBlockName(this, name);
    if (insert(level, ext, blockNameSymbol))
    {
        return blockNameSymbol;
    }
    return nullptr;
}

TVariable *TSymbolTable::insertVariable(ESymbolLevel level, const char *name, const TType &type)
{
    return insertVariable(level, NewPoolTString(name), type);
}

TVariable *TSymbolTable::insertVariable(ESymbolLevel level, const TString *name, const TType &type)
{
    TVariable *var = new TVariable(this, name, type);
    if (insert(level, var))
    {
        // Do lazy initialization for struct types, so we allocate to the current scope.
        if (var->getType().getBasicType() == EbtStruct)
        {
            var->getType().realize();
        }
        return var;
    }
    return nullptr;
}

TVariable *TSymbolTable::insertVariableExt(ESymbolLevel level,
                                           TExtension ext,
                                           const char *name,
                                           const TType &type)
{
    TVariable *var = new TVariable(this, NewPoolTString(name), type);
    if (insert(level, ext, var))
    {
        if (var->getType().getBasicType() == EbtStruct)
        {
            var->getType().realize();
        }
        return var;
    }
    return nullptr;
}

TVariable *TSymbolTable::insertStructType(ESymbolLevel level, TStructure *str)
{
    TVariable *var = new TVariable(this, &str->name(), TType(str), true);
    if (insert(level, var))
    {
        var->getType().realize();
        return var;
    }
    return nullptr;
}

void TSymbolTable::insertBuiltIn(ESymbolLevel level,
                                 TOperator op,
                                 TExtension ext,
                                 const TType *rvalue,
                                 const char *name,
                                 const TType *ptype1,
                                 const TType *ptype2,
                                 const TType *ptype3,
                                 const TType *ptype4,
                                 const TType *ptype5)
{
    if (ptype1->getBasicType() == EbtGSampler2D)
    {
        insertUnmangledBuiltInName(name, level);
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtFloat, 4) : rvalue, name,
                      TCache::getType(EbtSampler2D), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtInt, 4) : rvalue, name,
                      TCache::getType(EbtISampler2D), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtUInt, 4) : rvalue, name,
                      TCache::getType(EbtUSampler2D), ptype2, ptype3, ptype4, ptype5);
    }
    else if (ptype1->getBasicType() == EbtGSampler3D)
    {
        insertUnmangledBuiltInName(name, level);
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtFloat, 4) : rvalue, name,
                      TCache::getType(EbtSampler3D), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtInt, 4) : rvalue, name,
                      TCache::getType(EbtISampler3D), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtUInt, 4) : rvalue, name,
                      TCache::getType(EbtUSampler3D), ptype2, ptype3, ptype4, ptype5);
    }
    else if (ptype1->getBasicType() == EbtGSamplerCube)
    {
        insertUnmangledBuiltInName(name, level);
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtFloat, 4) : rvalue, name,
                      TCache::getType(EbtSamplerCube), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtInt, 4) : rvalue, name,
                      TCache::getType(EbtISamplerCube), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtUInt, 4) : rvalue, name,
                      TCache::getType(EbtUSamplerCube), ptype2, ptype3, ptype4, ptype5);
    }
    else if (ptype1->getBasicType() == EbtGSampler2DArray)
    {
        insertUnmangledBuiltInName(name, level);
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtFloat, 4) : rvalue, name,
                      TCache::getType(EbtSampler2DArray), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtInt, 4) : rvalue, name,
                      TCache::getType(EbtISampler2DArray), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtUInt, 4) : rvalue, name,
                      TCache::getType(EbtUSampler2DArray), ptype2, ptype3, ptype4, ptype5);
    }
    else if (ptype1->getBasicType() == EbtGSampler2DMS)
    {
        insertUnmangledBuiltInName(name, level);
        bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtFloat, 4) : rvalue, name,
                      TCache::getType(EbtSampler2DMS), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtInt, 4) : rvalue, name,
                      TCache::getType(EbtISampler2DMS), ptype2, ptype3, ptype4, ptype5);
        insertBuiltIn(level, gvec4 ? TCache::getType(EbtUInt, 4) : rvalue, name,
                      TCache::getType(EbtUSampler2DMS), ptype2, ptype3, ptype4, ptype5);
    }
    else if (IsGImage(ptype1->getBasicType()))
    {
        insertUnmangledBuiltInName(name, level);

        const TType *floatType    = TCache::getType(EbtFloat, 4);
        const TType *intType      = TCache::getType(EbtInt, 4);
        const TType *unsignedType = TCache::getType(EbtUInt, 4);

        const TType *floatImage =
            TCache::getType(convertGImageToFloatImage(ptype1->getBasicType()));
        const TType *intImage = TCache::getType(convertGImageToIntImage(ptype1->getBasicType()));
        const TType *unsignedImage =
            TCache::getType(convertGImageToUnsignedImage(ptype1->getBasicType()));

        // GLSL ES 3.10, Revision 4, 8.12 Image Functions
        if (rvalue->getBasicType() == EbtGVec4)
        {
            // imageLoad
            insertBuiltIn(level, floatType, name, floatImage, ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, intType, name, intImage, ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, unsignedType, name, unsignedImage, ptype2, ptype3, ptype4, ptype5);
        }
        else if (rvalue->getBasicType() == EbtVoid)
        {
            // imageStore
            insertBuiltIn(level, rvalue, name, floatImage, ptype2, floatType, ptype4, ptype5);
            insertBuiltIn(level, rvalue, name, intImage, ptype2, intType, ptype4, ptype5);
            insertBuiltIn(level, rvalue, name, unsignedImage, ptype2, unsignedType, ptype4, ptype5);
        }
        else
        {
            // imageSize
            insertBuiltIn(level, rvalue, name, floatImage, ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, rvalue, name, intImage, ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, rvalue, name, unsignedImage, ptype2, ptype3, ptype4, ptype5);
        }
    }
    else if (IsGenType(rvalue) || IsGenType(ptype1) || IsGenType(ptype2) || IsGenType(ptype3) ||
             IsGenType(ptype4))
    {
        ASSERT(!ptype5);
        insertUnmangledBuiltInName(name, level);
        insertBuiltIn(level, op, ext, SpecificType(rvalue, 1), name, SpecificType(ptype1, 1),
                      SpecificType(ptype2, 1), SpecificType(ptype3, 1), SpecificType(ptype4, 1));
        insertBuiltIn(level, op, ext, SpecificType(rvalue, 2), name, SpecificType(ptype1, 2),
                      SpecificType(ptype2, 2), SpecificType(ptype3, 2), SpecificType(ptype4, 2));
        insertBuiltIn(level, op, ext, SpecificType(rvalue, 3), name, SpecificType(ptype1, 3),
                      SpecificType(ptype2, 3), SpecificType(ptype3, 3), SpecificType(ptype4, 3));
        insertBuiltIn(level, op, ext, SpecificType(rvalue, 4), name, SpecificType(ptype1, 4),
                      SpecificType(ptype2, 4), SpecificType(ptype3, 4), SpecificType(ptype4, 4));
    }
    else if (IsVecType(rvalue) || IsVecType(ptype1) || IsVecType(ptype2) || IsVecType(ptype3))
    {
        ASSERT(!ptype4 && !ptype5);
        insertUnmangledBuiltInName(name, level);
        insertBuiltIn(level, op, ext, VectorType(rvalue, 2), name, VectorType(ptype1, 2),
                      VectorType(ptype2, 2), VectorType(ptype3, 2));
        insertBuiltIn(level, op, ext, VectorType(rvalue, 3), name, VectorType(ptype1, 3),
                      VectorType(ptype2, 3), VectorType(ptype3, 3));
        insertBuiltIn(level, op, ext, VectorType(rvalue, 4), name, VectorType(ptype1, 4),
                      VectorType(ptype2, 4), VectorType(ptype3, 4));
    }
    else
    {
        TFunction *function = new TFunction(this, NewPoolTString(name), rvalue, op, ext);

        function->addParameter(TConstParameter(ptype1));

        if (ptype2)
        {
            function->addParameter(TConstParameter(ptype2));
        }

        if (ptype3)
        {
            function->addParameter(TConstParameter(ptype3));
        }

        if (ptype4)
        {
            function->addParameter(TConstParameter(ptype4));
        }

        if (ptype5)
        {
            function->addParameter(TConstParameter(ptype5));
        }

        ASSERT(hasUnmangledBuiltInAtLevel(name, level));
        insert(level, function);
    }
}

void TSymbolTable::insertBuiltInOp(ESymbolLevel level,
                                   TOperator op,
                                   const TType *rvalue,
                                   const TType *ptype1,
                                   const TType *ptype2,
                                   const TType *ptype3,
                                   const TType *ptype4,
                                   const TType *ptype5)
{
    const char *name = GetOperatorString(op);
    ASSERT(strlen(name) > 0);
    insertUnmangledBuiltInName(name, level);
    insertBuiltIn(level, op, TExtension::UNDEFINED, rvalue, name, ptype1, ptype2, ptype3, ptype4,
                  ptype5);
}

void TSymbolTable::insertBuiltInOp(ESymbolLevel level,
                                   TOperator op,
                                   TExtension ext,
                                   const TType *rvalue,
                                   const TType *ptype1,
                                   const TType *ptype2,
                                   const TType *ptype3,
                                   const TType *ptype4,
                                   const TType *ptype5)
{
    const char *name = GetOperatorString(op);
    insertUnmangledBuiltInName(name, level);
    insertBuiltIn(level, op, ext, rvalue, name, ptype1, ptype2, ptype3, ptype4, ptype5);
}

void TSymbolTable::insertBuiltInFunctionNoParameters(ESymbolLevel level,
                                                     TOperator op,
                                                     const TType *rvalue,
                                                     const char *name)
{
    insertUnmangledBuiltInName(name, level);
    insert(level, new TFunction(this, NewPoolTString(name), rvalue, op));
}

void TSymbolTable::insertBuiltInFunctionNoParametersExt(ESymbolLevel level,
                                                        TExtension ext,
                                                        TOperator op,
                                                        const TType *rvalue,
                                                        const char *name)
{
    insertUnmangledBuiltInName(name, level);
    insert(level, new TFunction(this, NewPoolTString(name), rvalue, op, ext));
}

TPrecision TSymbolTable::getDefaultPrecision(TBasicType type) const
{
    if (!SupportsPrecision(type))
        return EbpUndefined;

    // unsigned integers use the same precision as signed
    TBasicType baseType = (type == EbtUInt) ? EbtInt : type;

    int level = static_cast<int>(precisionStack.size()) - 1;
    assert(level >= 0);  // Just to be safe. Should not happen.
    // If we dont find anything we return this. Some types don't have predefined default precision.
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

void TSymbolTable::insertUnmangledBuiltInName(const char *name, ESymbolLevel level)
{
    ASSERT(level >= 0 && level < static_cast<ESymbolLevel>(table.size()));
    table[level]->insertUnmangledBuiltInName(std::string(name));
}

bool TSymbolTable::hasUnmangledBuiltInAtLevel(const char *name, ESymbolLevel level)
{
    ASSERT(level >= 0 && level < static_cast<ESymbolLevel>(table.size()));
    return table[level]->hasUnmangledBuiltIn(std::string(name));
}

bool TSymbolTable::hasUnmangledBuiltInForShaderVersion(const char *name, int shaderVersion)
{
    ASSERT(static_cast<ESymbolLevel>(table.size()) > LAST_BUILTIN_LEVEL);

    for (int level = LAST_BUILTIN_LEVEL; level >= 0; --level)
    {
        if (level == ESSL3_1_BUILTINS && shaderVersion != 310)
        {
            --level;
        }
        if (level == ESSL3_BUILTINS && shaderVersion < 300)
        {
            --level;
        }
        if (level == ESSL1_BUILTINS && shaderVersion != 100)
        {
            --level;
        }

        if (table[level]->hasUnmangledBuiltIn(name))
        {
            return true;
        }
    }
    return false;
}

}  // namespace sh

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
#include <climits>

int TSymbolTableLevel::uniqueId = 0;

TType::TType(const TPublicType &p) :
            type(p.type), precision(p.precision), qualifier(p.qualifier), primarySize(p.primarySize), secondarySize(p.secondarySize), array(p.array), layoutQualifier(p.layoutQualifier), arraySize(p.arraySize),
            interfaceBlock(0), structure(0)
{
    if (p.userDef) {
        structure = p.userDef->getStruct();
    }
}

//
// Recursively generate mangled names.
//
TString TType::buildMangledName() const
{
    TString mangledName;
    if (isMatrix())
        mangledName += 'm';
    else if (isVector())
        mangledName += 'v';

    switch (type)
    {
      case EbtFloat:                mangledName += 'f';      break;
      case EbtInt:                  mangledName += 'i';      break;
      case EbtUInt:                 mangledName += 'u';      break;
      case EbtBool:                 mangledName += 'b';      break;
      case EbtSampler2D:            mangledName += "s2";     break;
      case EbtSampler3D:            mangledName += "s3";     break;
      case EbtSamplerCube:          mangledName += "sC";     break;
      case EbtSampler2DArray:       mangledName += "s2a";    break;
      case EbtSamplerExternalOES:   mangledName += "sext";   break;
      case EbtSampler2DRect:        mangledName += "s2r";    break;
      case EbtISampler2D:           mangledName += "is2";    break;
      case EbtISampler3D:           mangledName += "is3";    break;
      case EbtISamplerCube:         mangledName += "isC";    break;
      case EbtISampler2DArray:      mangledName += "is2a";   break;
      case EbtUSampler2D:           mangledName += "us2";    break;
      case EbtUSampler3D:           mangledName += "us3";    break;
      case EbtUSamplerCube:         mangledName += "usC";    break;
      case EbtUSampler2DArray:      mangledName += "us2a";   break;
      case EbtSampler2DShadow:      mangledName += "s2s";    break;
      case EbtSamplerCubeShadow:    mangledName += "sCs";    break;
      case EbtSampler2DArrayShadow: mangledName += "s2as";   break;
      case EbtStruct:               mangledName += structure->mangledName(); break;
      case EbtInterfaceBlock:       mangledName += interfaceBlock->mangledName(); break;
      default:                      UNREACHABLE();
    }

    if (isMatrix())
    {
        mangledName += static_cast<char>('0' + getCols());
        mangledName += static_cast<char>('x');
        mangledName += static_cast<char>('0' + getRows());
    }
    else
    {
        mangledName += static_cast<char>('0' + getNominalSize());
    }

    if (isArray()) {
        char buf[20];
        snprintf(buf, sizeof(buf), "%d", arraySize);
        mangledName += '[';
        mangledName += buf;
        mangledName += ']';
    }
    return mangledName;
}

size_t TType::getObjectSize() const
{
    size_t totalSize;

    if (getBasicType() == EbtStruct)
        totalSize = structure->objectSize();
    else
        totalSize = primarySize * secondarySize;

    if (isArray()) {
        size_t arraySize = getArraySize();
        if (arraySize > INT_MAX / totalSize)
            totalSize = INT_MAX;
        else
            totalSize *= arraySize;
    }

    return totalSize;
}

bool TStructure::containsArrays() const
{
    for (size_t i = 0; i < mFields->size(); ++i) {
        const TType* fieldType = (*mFields)[i]->type();
        if (fieldType->isArray() || fieldType->isStructureContainingArrays())
            return true;
    }
    return false;
}

TString TFieldListCollection::buildMangledName() const
{
    TString mangledName(mangledNamePrefix());
    mangledName += *mName;
    for (size_t i = 0; i < mFields->size(); ++i) {
        mangledName += '-';
        mangledName += (*mFields)[i]->type()->getMangledName();
    }
    return mangledName;
}

size_t TFieldListCollection::calculateObjectSize() const
{
    size_t size = 0;
    for (size_t i = 0; i < mFields->size(); ++i) {
        size_t fieldSize = (*mFields)[i]->type()->getObjectSize();
        if (fieldSize > INT_MAX - size)
            size = INT_MAX;
        else
            size += fieldSize;
    }
    return size;
}

int TStructure::calculateDeepestNesting() const
{
    int maxNesting = 0;
    for (size_t i = 0; i < mFields->size(); ++i) {
        maxNesting = std::max(maxNesting, (*mFields)[i]->type()->getDeepestStructNesting());
    }
    return 1 + maxNesting;
}

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

//
// Change all function entries in the table with the non-mangled name
// to be related to the provided built-in operation.  This is a low
// performance operation, and only intended for symbol tables that
// live across a large number of compiles.
//
void TSymbolTableLevel::relateToOperator(const char* name, TOperator op)
{
    tLevel::iterator it;
    for (it = level.begin(); it != level.end(); ++it) {
        if ((*it).second->isFunction()) {
            TFunction* function = static_cast<TFunction*>((*it).second);
            if (function->getName() == name)
                function->relateToOperator(op);
        }
    }
}

//
// Change all function entries in the table with the non-mangled name
// to be related to the provided built-in extension. This is a low
// performance operation, and only intended for symbol tables that
// live across a large number of compiles.
//
void TSymbolTableLevel::relateToExtension(const char* name, const TString& ext)
{
    for (tLevel::iterator it = level.begin(); it != level.end(); ++it) {
        TSymbol* symbol = it->second;
        if (symbol->getName() == name) {
            symbol->relateToExtension(ext);
        }
    }
}

TSymbol::TSymbol(const TSymbol& copyOf)
{
    name = NewPoolTString(copyOf.name->c_str());
    uniqueId = copyOf.uniqueId;
}

TSymbol *TSymbolTable::find(const TString &name, int shaderVersion, bool *builtIn, bool *sameScope)
{
    int level = currentLevel();
    TSymbol *symbol;

    do
    {
        if (level == ESSL3_BUILTINS && shaderVersion != 300) level--;
        if (level == ESSL1_BUILTINS && shaderVersion != 100) level--;

        symbol = table[level]->find(name);
    }
    while (symbol == 0 && --level >= 0);

    if (builtIn)
        *builtIn = (level <= LAST_BUILTIN_LEVEL);
    if (sameScope)
        *sameScope = (level == currentLevel());

    return symbol;
}

TSymbol *TSymbolTable::findBuiltIn(const TString &name, int shaderVersion)
{
    for (int level = LAST_BUILTIN_LEVEL; level >= 0; level--)
    {
        if (level == ESSL3_BUILTINS && shaderVersion != 300) level--;
        if (level == ESSL1_BUILTINS && shaderVersion != 100) level--;

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

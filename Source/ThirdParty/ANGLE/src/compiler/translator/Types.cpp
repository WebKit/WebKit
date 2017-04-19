//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#if defined(_MSC_VER)
#pragma warning(disable : 4718)
#endif

#include "compiler/translator/Types.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/SymbolTable.h"

#include <algorithm>
#include <climits>

namespace sh
{

const char *getBasicString(TBasicType t)
{
    switch (t)
    {
        case EbtVoid:
            return "void";
        case EbtFloat:
            return "float";
        case EbtInt:
            return "int";
        case EbtUInt:
            return "uint";
        case EbtBool:
            return "bool";
        case EbtYuvCscStandardEXT:
            return "yuvCscStandardEXT";
        case EbtSampler2D:
            return "sampler2D";
        case EbtSampler3D:
            return "sampler3D";
        case EbtSamplerCube:
            return "samplerCube";
        case EbtSamplerExternalOES:
            return "samplerExternalOES";
        case EbtSamplerExternal2DY2YEXT:
            return "__samplerExternal2DY2YEXT";
        case EbtSampler2DRect:
            return "sampler2DRect";
        case EbtSampler2DArray:
            return "sampler2DArray";
        case EbtSampler2DMS:
            return "sampler2DMS";
        case EbtISampler2D:
            return "isampler2D";
        case EbtISampler3D:
            return "isampler3D";
        case EbtISamplerCube:
            return "isamplerCube";
        case EbtISampler2DArray:
            return "isampler2DArray";
        case EbtISampler2DMS:
            return "isampler2DMS";
        case EbtUSampler2D:
            return "usampler2D";
        case EbtUSampler3D:
            return "usampler3D";
        case EbtUSamplerCube:
            return "usamplerCube";
        case EbtUSampler2DArray:
            return "usampler2DArray";
        case EbtUSampler2DMS:
            return "usampler2DMS";
        case EbtSampler2DShadow:
            return "sampler2DShadow";
        case EbtSamplerCubeShadow:
            return "samplerCubeShadow";
        case EbtSampler2DArrayShadow:
            return "sampler2DArrayShadow";
        case EbtStruct:
            return "structure";
        case EbtInterfaceBlock:
            return "interface block";
        case EbtImage2D:
            return "image2D";
        case EbtIImage2D:
            return "iimage2D";
        case EbtUImage2D:
            return "uimage2D";
        case EbtImage3D:
            return "image3D";
        case EbtIImage3D:
            return "iimage3D";
        case EbtUImage3D:
            return "uimage3D";
        case EbtImage2DArray:
            return "image2DArray";
        case EbtIImage2DArray:
            return "iimage2DArray";
        case EbtUImage2DArray:
            return "uimage2DArray";
        case EbtImageCube:
            return "imageCube";
        case EbtIImageCube:
            return "iimageCube";
        case EbtUImageCube:
            return "uimageCube";
        default:
            UNREACHABLE();
            return "unknown type";
    }
}

TType::TType(const TPublicType &p)
    : type(p.getBasicType()),
      precision(p.precision),
      qualifier(p.qualifier),
      invariant(p.invariant),
      memoryQualifier(p.memoryQualifier),
      layoutQualifier(p.layoutQualifier),
      primarySize(p.getPrimarySize()),
      secondarySize(p.getSecondarySize()),
      array(p.array),
      arraySize(p.arraySize),
      interfaceBlock(0),
      structure(0)
{
    if (p.getUserDef())
        structure = p.getUserDef()->getStruct();
}

bool TStructure::equals(const TStructure &other) const
{
    return (uniqueId() == other.uniqueId());
}

const char *TType::getBuiltInTypeNameString() const
{
    if (isMatrix())
    {
        switch (getCols())
        {
            case 2:
                switch (getRows())
                {
                    case 2:
                        return "mat2";
                    case 3:
                        return "mat2x3";
                    case 4:
                        return "mat2x4";
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
            case 3:
                switch (getRows())
                {
                    case 2:
                        return "mat3x2";
                    case 3:
                        return "mat3";
                    case 4:
                        return "mat3x4";
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
            case 4:
                switch (getRows())
                {
                    case 2:
                        return "mat4x2";
                    case 3:
                        return "mat4x3";
                    case 4:
                        return "mat4";
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
            default:
                UNREACHABLE();
                return nullptr;
        }
    }
    if (isVector())
    {
        switch (getBasicType())
        {
            case EbtFloat:
                switch (getNominalSize())
                {
                    case 2:
                        return "vec2";
                    case 3:
                        return "vec3";
                    case 4:
                        return "vec4";
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
            case EbtInt:
                switch (getNominalSize())
                {
                    case 2:
                        return "ivec2";
                    case 3:
                        return "ivec3";
                    case 4:
                        return "ivec4";
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
            case EbtBool:
                switch (getNominalSize())
                {
                    case 2:
                        return "bvec2";
                    case 3:
                        return "bvec3";
                    case 4:
                        return "bvec4";
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
            case EbtUInt:
                switch (getNominalSize())
                {
                    case 2:
                        return "uvec2";
                    case 3:
                        return "uvec3";
                    case 4:
                        return "uvec4";
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
            default:
                UNREACHABLE();
                return nullptr;
        }
    }
    ASSERT(getBasicType() != EbtStruct);
    ASSERT(getBasicType() != EbtInterfaceBlock);
    return getBasicString();
}

TString TType::getCompleteString() const
{
    TStringStream stream;

    if (invariant)
        stream << "invariant ";
    if (qualifier != EvqTemporary && qualifier != EvqGlobal)
        stream << getQualifierString() << " ";
    if (precision != EbpUndefined)
        stream << getPrecisionString() << " ";
    if (array)
        stream << "array[" << getArraySize() << "] of ";
    if (isMatrix())
        stream << getCols() << "X" << getRows() << " matrix of ";
    else if (isVector())
        stream << getNominalSize() << "-component vector of ";

    stream << getBasicString();
    return stream.str();
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
        case EbtFloat:
            mangledName += 'f';
            break;
        case EbtInt:
            mangledName += 'i';
            break;
        case EbtUInt:
            mangledName += 'u';
            break;
        case EbtBool:
            mangledName += 'b';
            break;
        case EbtYuvCscStandardEXT:
            mangledName += "ycs";
            break;
        case EbtSampler2D:
            mangledName += "s2";
            break;
        case EbtSampler3D:
            mangledName += "s3";
            break;
        case EbtSamplerCube:
            mangledName += "sC";
            break;
        case EbtSampler2DArray:
            mangledName += "s2a";
            break;
        case EbtSamplerExternalOES:
            mangledName += "sext";
            break;
        case EbtSamplerExternal2DY2YEXT:
            mangledName += "sext2y2y";
            break;
        case EbtSampler2DRect:
            mangledName += "s2r";
            break;
        case EbtSampler2DMS:
            mangledName += "s2ms";
            break;
        case EbtISampler2D:
            mangledName += "is2";
            break;
        case EbtISampler3D:
            mangledName += "is3";
            break;
        case EbtISamplerCube:
            mangledName += "isC";
            break;
        case EbtISampler2DArray:
            mangledName += "is2a";
            break;
        case EbtISampler2DMS:
            mangledName += "is2ms";
            break;
        case EbtUSampler2D:
            mangledName += "us2";
            break;
        case EbtUSampler3D:
            mangledName += "us3";
            break;
        case EbtUSamplerCube:
            mangledName += "usC";
            break;
        case EbtUSampler2DArray:
            mangledName += "us2a";
            break;
        case EbtUSampler2DMS:
            mangledName += "us2ms";
            break;
        case EbtSampler2DShadow:
            mangledName += "s2s";
            break;
        case EbtSamplerCubeShadow:
            mangledName += "sCs";
            break;
        case EbtSampler2DArrayShadow:
            mangledName += "s2as";
            break;
        case EbtImage2D:
            mangledName += "im2";
            break;
        case EbtIImage2D:
            mangledName += "iim2";
            break;
        case EbtUImage2D:
            mangledName += "uim2";
            break;
        case EbtImage3D:
            mangledName += "im3";
            break;
        case EbtIImage3D:
            mangledName += "iim3";
            break;
        case EbtUImage3D:
            mangledName += "uim3";
            break;
        case EbtImage2DArray:
            mangledName += "im2a";
            break;
        case EbtIImage2DArray:
            mangledName += "iim2a";
            break;
        case EbtUImage2DArray:
            mangledName += "uim2a";
            break;
        case EbtImageCube:
            mangledName += "imc";
            break;
        case EbtIImageCube:
            mangledName += "iimc";
            break;
        case EbtUImageCube:
            mangledName += "uimc";
            break;
        case EbtStruct:
            mangledName += structure->mangledName();
            break;
        case EbtInterfaceBlock:
            mangledName += interfaceBlock->mangledName();
            break;
        default:
            // EbtVoid, EbtAddress and non types
            break;
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

    if (isArray())
    {
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

    if (isArray())
    {
        if (totalSize == 0)
            return 0;

        size_t currentArraySize = getArraySize();
        if (currentArraySize > INT_MAX / totalSize)
            totalSize = INT_MAX;
        else
            totalSize *= currentArraySize;
    }

    return totalSize;
}

int TType::getLocationCount() const
{
    int count = 1;

    if (getBasicType() == EbtStruct)
    {
        count = structure->getLocationCount();
    }

    if (isArray())
    {
        if (count == 0)
        {
            return 0;
        }

        unsigned int currentArraySize = getArraySize();
        if (currentArraySize > static_cast<unsigned int>(std::numeric_limits<int>::max() / count))
        {
            count = std::numeric_limits<int>::max();
        }
        else
        {
            count *= static_cast<int>(currentArraySize);
        }
    }

    return count;
}

TStructure::TStructure(const TString *name, TFieldList *fields)
    : TFieldListCollection(name, fields),
      mDeepestNesting(0),
      mUniqueId(TSymbolTable::nextUniqueId()),
      mAtGlobalScope(false)
{
}

bool TStructure::containsArrays() const
{
    for (size_t i = 0; i < mFields->size(); ++i)
    {
        const TType *fieldType = (*mFields)[i]->type();
        if (fieldType->isArray() || fieldType->isStructureContainingArrays())
            return true;
    }
    return false;
}

bool TStructure::containsType(TBasicType type) const
{
    for (size_t i = 0; i < mFields->size(); ++i)
    {
        const TType *fieldType = (*mFields)[i]->type();
        if (fieldType->getBasicType() == type || fieldType->isStructureContainingType(type))
            return true;
    }
    return false;
}

bool TStructure::containsSamplers() const
{
    for (size_t i = 0; i < mFields->size(); ++i)
    {
        const TType *fieldType = (*mFields)[i]->type();
        if (IsSampler(fieldType->getBasicType()) || fieldType->isStructureContainingSamplers())
            return true;
    }
    return false;
}

bool TStructure::containsImages() const
{
    for (size_t i = 0; i < mFields->size(); ++i)
    {
        const TType *fieldType = (*mFields)[i]->type();
        if (IsImage(fieldType->getBasicType()) || fieldType->isStructureContainingImages())
            return true;
    }
    return false;
}

void TStructure::createSamplerSymbols(const TString &structName,
                                      const TString &structAPIName,
                                      const unsigned int arrayOfStructsSize,
                                      TVector<TIntermSymbol *> *outputSymbols,
                                      TMap<TIntermSymbol *, TString> *outputSymbolsToAPINames) const
{
    for (auto &field : *mFields)
    {
        const TType *fieldType = field->type();
        if (IsSampler(fieldType->getBasicType()))
        {
            if (arrayOfStructsSize > 0u)
            {
                for (unsigned int arrayIndex = 0u; arrayIndex < arrayOfStructsSize; ++arrayIndex)
                {
                    TStringStream name;
                    name << structName << "_" << arrayIndex << "_" << field->name();
                    TIntermSymbol *symbol = new TIntermSymbol(0, name.str(), *fieldType);
                    outputSymbols->push_back(symbol);

                    if (outputSymbolsToAPINames)
                    {
                        TStringStream apiName;
                        apiName << structAPIName << "[" << arrayIndex << "]." << field->name();
                        (*outputSymbolsToAPINames)[symbol] = apiName.str();
                    }
                }
            }
            else
            {
                TString symbolName    = structName + "_" + field->name();
                TIntermSymbol *symbol = new TIntermSymbol(0, symbolName, *fieldType);
                outputSymbols->push_back(symbol);

                if (outputSymbolsToAPINames)
                {
                    TString apiName                    = structAPIName + "." + field->name();
                    (*outputSymbolsToAPINames)[symbol] = apiName;
                }
            }
        }
        else if (fieldType->isStructureContainingSamplers())
        {
            unsigned int nestedArrayOfStructsSize =
                fieldType->isArray() ? fieldType->getArraySize() : 0u;
            if (arrayOfStructsSize > 0)
            {
                for (unsigned int arrayIndex = 0u; arrayIndex < arrayOfStructsSize; ++arrayIndex)
                {
                    TStringStream fieldName;
                    fieldName << structName << "_" << arrayIndex << "_" << field->name();
                    TStringStream fieldAPIName;
                    if (outputSymbolsToAPINames)
                    {
                        fieldAPIName << structAPIName << "[" << arrayIndex << "]." << field->name();
                    }
                    fieldType->createSamplerSymbols(fieldName.str(), fieldAPIName.str(),
                                                    nestedArrayOfStructsSize, outputSymbols,
                                                    outputSymbolsToAPINames);
                }
            }
            else
            {
                fieldType->createSamplerSymbols(
                    structName + "_" + field->name(), structAPIName + "." + field->name(),
                    nestedArrayOfStructsSize, outputSymbols, outputSymbolsToAPINames);
            }
        }
    }
}

TString TFieldListCollection::buildMangledName(const TString &mangledNamePrefix) const
{
    TString mangledName(mangledNamePrefix);
    mangledName += *mName;
    for (size_t i = 0; i < mFields->size(); ++i)
    {
        mangledName += '-';
        mangledName += (*mFields)[i]->type()->getMangledName();
    }
    return mangledName;
}

size_t TFieldListCollection::calculateObjectSize() const
{
    size_t size = 0;
    for (const TField *field : *mFields)
    {
        size_t fieldSize = field->type()->getObjectSize();
        if (fieldSize > INT_MAX - size)
            size = INT_MAX;
        else
            size += fieldSize;
    }
    return size;
}

int TFieldListCollection::getLocationCount() const
{
    int count = 0;
    for (const TField *field : *mFields)
    {
        int fieldCount = field->type()->getLocationCount();
        if (fieldCount > std::numeric_limits<int>::max() - count)
        {
            count = std::numeric_limits<int>::max();
        }
        else
        {
            count += fieldCount;
        }
    }
    return count;
}

int TStructure::calculateDeepestNesting() const
{
    int maxNesting = 0;
    for (size_t i = 0; i < mFields->size(); ++i)
        maxNesting = std::max(maxNesting, (*mFields)[i]->type()->getDeepestStructNesting());
    return 1 + maxNesting;
}

}  // namespace sh

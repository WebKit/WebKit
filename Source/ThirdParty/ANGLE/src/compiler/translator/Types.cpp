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
        case EbtAtomicCounter:
            return "atomic_uint";
        default:
            UNREACHABLE();
            return "unknown type";
    }
}

// TType implementation.
TType::TType()
    : type(EbtVoid),
      precision(EbpUndefined),
      qualifier(EvqGlobal),
      invariant(false),
      memoryQualifier(TMemoryQualifier::Create()),
      layoutQualifier(TLayoutQualifier::Create()),
      primarySize(0),
      secondarySize(0),
      mArraySizes(nullptr),
      mInterfaceBlock(nullptr),
      mStructure(nullptr),
      mIsStructSpecifier(false),
      mMangledName(nullptr)
{
}

TType::TType(TBasicType t, unsigned char ps, unsigned char ss)
    : type(t),
      precision(EbpUndefined),
      qualifier(EvqGlobal),
      invariant(false),
      memoryQualifier(TMemoryQualifier::Create()),
      layoutQualifier(TLayoutQualifier::Create()),
      primarySize(ps),
      secondarySize(ss),
      mArraySizes(nullptr),
      mInterfaceBlock(nullptr),
      mStructure(nullptr),
      mIsStructSpecifier(false),
      mMangledName(nullptr)
{
}

TType::TType(TBasicType t, TPrecision p, TQualifier q, unsigned char ps, unsigned char ss)
    : type(t),
      precision(p),
      qualifier(q),
      invariant(false),
      memoryQualifier(TMemoryQualifier::Create()),
      layoutQualifier(TLayoutQualifier::Create()),
      primarySize(ps),
      secondarySize(ss),
      mArraySizes(nullptr),
      mInterfaceBlock(nullptr),
      mStructure(nullptr),
      mIsStructSpecifier(false),
      mMangledName(nullptr)
{
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
      mArraySizes(nullptr),
      mInterfaceBlock(nullptr),
      mStructure(nullptr),
      mIsStructSpecifier(false),
      mMangledName(nullptr)
{
    ASSERT(primarySize <= 4);
    ASSERT(secondarySize <= 4);
    if (p.isArray())
    {
        mArraySizes = new TVector<unsigned int>(*p.arraySizes);
    }
    if (p.getUserDef())
    {
        mStructure         = p.getUserDef();
        mIsStructSpecifier = p.isStructSpecifier();
    }
}

TType::TType(TStructure *userDef)
    : type(EbtStruct),
      precision(EbpUndefined),
      qualifier(EvqTemporary),
      invariant(false),
      memoryQualifier(TMemoryQualifier::Create()),
      layoutQualifier(TLayoutQualifier::Create()),
      primarySize(1),
      secondarySize(1),
      mArraySizes(nullptr),
      mInterfaceBlock(nullptr),
      mStructure(userDef),
      mIsStructSpecifier(false),
      mMangledName(nullptr)
{
}

TType::TType(TInterfaceBlock *interfaceBlockIn,
             TQualifier qualifierIn,
             TLayoutQualifier layoutQualifierIn)
    : type(EbtInterfaceBlock),
      precision(EbpUndefined),
      qualifier(qualifierIn),
      invariant(false),
      memoryQualifier(TMemoryQualifier::Create()),
      layoutQualifier(layoutQualifierIn),
      primarySize(1),
      secondarySize(1),
      mArraySizes(nullptr),
      mInterfaceBlock(interfaceBlockIn),
      mStructure(0),
      mIsStructSpecifier(false),
      mMangledName(nullptr)
{
}

TType::TType(const TType &t)
    : type(t.type),
      precision(t.precision),
      qualifier(t.qualifier),
      invariant(t.invariant),
      memoryQualifier(t.memoryQualifier),
      layoutQualifier(t.layoutQualifier),
      primarySize(t.primarySize),
      secondarySize(t.secondarySize),
      mArraySizes(t.mArraySizes ? new TVector<unsigned int>(*t.mArraySizes) : nullptr),
      mInterfaceBlock(t.mInterfaceBlock),
      mStructure(t.mStructure),
      mIsStructSpecifier(t.mIsStructSpecifier),
      mMangledName(t.mMangledName)
{
}

TType &TType::operator=(const TType &t)
{
    type               = t.type;
    precision          = t.precision;
    qualifier          = t.qualifier;
    invariant          = t.invariant;
    memoryQualifier    = t.memoryQualifier;
    layoutQualifier    = t.layoutQualifier;
    primarySize        = t.primarySize;
    secondarySize      = t.secondarySize;
    mArraySizes        = t.mArraySizes ? new TVector<unsigned int>(*t.mArraySizes) : nullptr;
    mInterfaceBlock    = t.mInterfaceBlock;
    mStructure         = t.mStructure;
    mIsStructSpecifier = t.mIsStructSpecifier;
    mMangledName       = t.mMangledName;
    return *this;
}

bool TType::canBeConstructed() const
{
    switch (type)
    {
        case EbtFloat:
        case EbtInt:
        case EbtUInt:
        case EbtBool:
        case EbtStruct:
            return true;
        default:
            return false;
    }
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
    if (mArraySizes)
    {
        for (auto arraySizeIter = mArraySizes->rbegin(); arraySizeIter != mArraySizes->rend();
             ++arraySizeIter)
        {
            stream << "array[" << (*arraySizeIter) << "] of ";
        }
    }
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
const char *TType::buildMangledName() const
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
        case EbtAtomicCounter:
            mangledName += "ac";
            break;
        case EbtStruct:
            mangledName += mStructure->mangledName();
            break;
        case EbtInterfaceBlock:
            mangledName += mInterfaceBlock->mangledName();
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

    if (mArraySizes)
    {
        for (unsigned int arraySize : *mArraySizes)
        {
            char buf[20];
            snprintf(buf, sizeof(buf), "%d", arraySize);
            mangledName += '[';
            mangledName += buf;
            mangledName += ']';
        }
    }

    mangledName += ';';

    // Copy string contents into a pool-allocated buffer, so we never need to call delete.
    size_t requiredSize = mangledName.size() + 1;
    char *buffer = reinterpret_cast<char *>(GetGlobalPoolAllocator()->allocate(requiredSize));
    memcpy(buffer, mangledName.c_str(), requiredSize);
    return buffer;
}

size_t TType::getObjectSize() const
{
    size_t totalSize;

    if (getBasicType() == EbtStruct)
        totalSize = mStructure->objectSize();
    else
        totalSize = primarySize * secondarySize;

    if (totalSize == 0)
        return 0;

    if (mArraySizes)
    {
        for (size_t arraySize : *mArraySizes)
        {
            if (arraySize > INT_MAX / totalSize)
                totalSize = INT_MAX;
            else
                totalSize *= arraySize;
        }
    }

    return totalSize;
}

int TType::getLocationCount() const
{
    int count = 1;

    if (getBasicType() == EbtStruct)
    {
        count = mStructure->getLocationCount();
    }

    if (count == 0)
    {
        return 0;
    }

    if (mArraySizes)
    {
        for (unsigned int arraySize : *mArraySizes)
        {
            if (arraySize > static_cast<unsigned int>(std::numeric_limits<int>::max() / count))
            {
                count = std::numeric_limits<int>::max();
            }
            else
            {
                count *= static_cast<int>(arraySize);
            }
        }
    }

    return count;
}

unsigned int TType::getArraySizeProduct() const
{
    if (!mArraySizes)
        return 1u;

    unsigned int product = 1u;

    for (unsigned int arraySize : *mArraySizes)
    {
        product *= arraySize;
    }
    return product;
}

bool TType::isUnsizedArray() const
{
    if (!mArraySizes)
        return false;

    for (unsigned int arraySize : *mArraySizes)
    {
        if (arraySize == 0u)
        {
            return true;
        }
    }
    return false;
}

bool TType::sameNonArrayType(const TType &right) const
{
    return (type == right.type && primarySize == right.primarySize &&
            secondarySize == right.secondarySize && mStructure == right.mStructure);
}

bool TType::isElementTypeOf(const TType &arrayType) const
{
    if (!sameNonArrayType(arrayType))
    {
        return false;
    }
    if (arrayType.getNumArraySizes() != getNumArraySizes() + 1u)
    {
        return false;
    }
    if (isArray())
    {
        for (size_t i = 0; i < mArraySizes->size(); ++i)
        {
            if ((*mArraySizes)[i] != (*arrayType.mArraySizes)[i])
            {
                return false;
            }
        }
    }
    return true;
}

void TType::sizeUnsizedArrays(const TVector<unsigned int> *newArraySizes)
{
    size_t newArraySizesSize = newArraySizes ? newArraySizes->size() : 0;
    for (size_t i = 0u; i < getNumArraySizes(); ++i)
    {
        if ((*mArraySizes)[i] == 0)
        {
            if (i < newArraySizesSize)
            {
                ASSERT(newArraySizes != nullptr);
                (*mArraySizes)[i] = (*newArraySizes)[i];
            }
            else
            {
                (*mArraySizes)[i] = 1u;
            }
        }
    }
    invalidateMangledName();
}

void TType::sizeOutermostUnsizedArray(unsigned int arraySize)
{
    ASSERT(isArray());
    ASSERT(mArraySizes->back() == 0u);
    mArraySizes->back() = arraySize;
}

void TType::setBasicType(TBasicType t)
{
    if (type != t)
    {
        type = t;
        invalidateMangledName();
    }
}

void TType::setPrimarySize(unsigned char ps)
{
    if (primarySize != ps)
    {
        ASSERT(ps <= 4);
        primarySize = ps;
        invalidateMangledName();
    }
}

void TType::setSecondarySize(unsigned char ss)
{
    if (secondarySize != ss)
    {
        ASSERT(ss <= 4);
        secondarySize = ss;
        invalidateMangledName();
    }
}

void TType::makeArray(unsigned int s)
{
    if (!mArraySizes)
        mArraySizes = new TVector<unsigned int>();

    mArraySizes->push_back(s);
    invalidateMangledName();
}

void TType::makeArrays(const TVector<unsigned int> &sizes)
{
    if (!mArraySizes)
        mArraySizes = new TVector<unsigned int>();

    mArraySizes->insert(mArraySizes->end(), sizes.begin(), sizes.end());
    invalidateMangledName();
}

void TType::setArraySize(size_t arrayDimension, unsigned int s)
{
    ASSERT(mArraySizes != nullptr);
    ASSERT(arrayDimension < mArraySizes->size());
    if (mArraySizes->at(arrayDimension) != s)
    {
        (*mArraySizes)[arrayDimension] = s;
        invalidateMangledName();
    }
}

void TType::toArrayElementType()
{
    ASSERT(mArraySizes != nullptr);
    if (mArraySizes->size() > 0)
    {
        mArraySizes->pop_back();
        invalidateMangledName();
    }
}

void TType::setInterfaceBlock(TInterfaceBlock *interfaceBlockIn)
{
    if (mInterfaceBlock != interfaceBlockIn)
    {
        mInterfaceBlock = interfaceBlockIn;
        invalidateMangledName();
    }
}

void TType::setStruct(TStructure *s)
{
    if (mStructure != s)
    {
        mStructure = s;
        invalidateMangledName();
    }
}

const char *TType::getMangledName() const
{
    if (mMangledName == nullptr)
    {
        mMangledName = buildMangledName();
    }

    return mMangledName;
}

void TType::realize()
{
    getMangledName();
}

void TType::invalidateMangledName()
{
    mMangledName = nullptr;
}

// TStructure implementation.
TStructure::TStructure(TSymbolTable *symbolTable, const TString *name, TFieldList *fields)
    : TFieldListCollection(name, fields),
      mDeepestNesting(0),
      mUniqueId(symbolTable->nextUniqueId()),
      mAtGlobalScope(false)
{
}

bool TStructure::equals(const TStructure &other) const
{
    return (uniqueId() == other.uniqueId());
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

void TType::createSamplerSymbols(const TString &namePrefix,
                                 const TString &apiNamePrefix,
                                 TVector<TIntermSymbol *> *outputSymbols,
                                 TMap<TIntermSymbol *, TString> *outputSymbolsToAPINames,
                                 TSymbolTable *symbolTable) const
{
    if (isStructureContainingSamplers())
    {
        if (isArray())
        {
            TType elementType(*this);
            elementType.toArrayElementType();
            for (unsigned int arrayIndex = 0u; arrayIndex < getOutermostArraySize(); ++arrayIndex)
            {
                TStringStream elementName;
                elementName << namePrefix << "_" << arrayIndex;
                TStringStream elementApiName;
                elementApiName << apiNamePrefix << "[" << arrayIndex << "]";
                elementType.createSamplerSymbols(elementName.str(), elementApiName.str(),
                                                 outputSymbols, outputSymbolsToAPINames,
                                                 symbolTable);
            }
        }
        else
        {
            mStructure->createSamplerSymbols(namePrefix, apiNamePrefix, outputSymbols,
                                             outputSymbolsToAPINames, symbolTable);
        }
        return;
    }

    ASSERT(IsSampler(type));
    TIntermSymbol *symbol = new TIntermSymbol(symbolTable->nextUniqueId(), namePrefix, *this);
    outputSymbols->push_back(symbol);
    if (outputSymbolsToAPINames)
    {
        (*outputSymbolsToAPINames)[symbol] = apiNamePrefix;
    }
}

void TStructure::createSamplerSymbols(const TString &namePrefix,
                                      const TString &apiNamePrefix,
                                      TVector<TIntermSymbol *> *outputSymbols,
                                      TMap<TIntermSymbol *, TString> *outputSymbolsToAPINames,
                                      TSymbolTable *symbolTable) const
{
    ASSERT(containsSamplers());
    for (auto &field : *mFields)
    {
        const TType *fieldType = field->type();
        if (IsSampler(fieldType->getBasicType()) || fieldType->isStructureContainingSamplers())
        {
            TString fieldName    = namePrefix + "_" + field->name();
            TString fieldApiName = apiNamePrefix + "." + field->name();
            fieldType->createSamplerSymbols(fieldName, fieldApiName, outputSymbols,
                                            outputSymbolsToAPINames, symbolTable);
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

// TPublicType implementation.
void TPublicType::initialize(const TTypeSpecifierNonArray &typeSpecifier, TQualifier q)
{
    typeSpecifierNonArray = typeSpecifier;
    layoutQualifier       = TLayoutQualifier::Create();
    memoryQualifier       = TMemoryQualifier::Create();
    qualifier             = q;
    invariant             = false;
    precision             = EbpUndefined;
    arraySizes            = nullptr;
}

void TPublicType::initializeBasicType(TBasicType basicType)
{
    typeSpecifierNonArray.type          = basicType;
    typeSpecifierNonArray.primarySize   = 1;
    typeSpecifierNonArray.secondarySize = 1;
    layoutQualifier                     = TLayoutQualifier::Create();
    memoryQualifier                     = TMemoryQualifier::Create();
    qualifier                           = EvqTemporary;
    invariant                           = false;
    precision                           = EbpUndefined;
    arraySizes                          = nullptr;
}

bool TPublicType::isStructureContainingArrays() const
{
    if (!typeSpecifierNonArray.userDef)
    {
        return false;
    }

    return typeSpecifierNonArray.userDef->containsArrays();
}

bool TPublicType::isStructureContainingType(TBasicType t) const
{
    if (!typeSpecifierNonArray.userDef)
    {
        return false;
    }

    return typeSpecifierNonArray.userDef->containsType(t);
}

void TPublicType::setArraySizes(TVector<unsigned int> *sizes)
{
    arraySizes = sizes;
}

bool TPublicType::isArray() const
{
    return arraySizes && !arraySizes->empty();
}

void TPublicType::clearArrayness()
{
    arraySizes = nullptr;
}

bool TPublicType::isAggregate() const
{
    return isArray() || typeSpecifierNonArray.isMatrix() || typeSpecifierNonArray.isVector();
}

}  // namespace sh

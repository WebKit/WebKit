//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Symbol table for parsing. The design principles and most of the functionality are documented in
// the header file.
//

#if defined(_MSC_VER)
#    pragma warning(disable : 4718)
#endif

#include "compiler/translator/SymbolTable.h"

#include "angle_gl.h"
#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/StaticType.h"

namespace sh
{

class TSymbolTable::TSymbolTableLevel
{
  public:
    TSymbolTableLevel() = default;

    bool insert(TSymbol *symbol);

    // Insert a function using its unmangled name as the key.
    void insertUnmangled(TFunction *function);

    TSymbol *find(const ImmutableString &name) const;

  private:
    using tLevel        = TUnorderedMap<ImmutableString,
                                 TSymbol *,
                                 ImmutableString::FowlerNollVoHash<sizeof(size_t)>>;
    using tLevelPair    = const tLevel::value_type;
    using tInsertResult = std::pair<tLevel::iterator, bool>;

    tLevel level;
};

bool TSymbolTable::TSymbolTableLevel::insert(TSymbol *symbol)
{
    // returning true means symbol was added to the table
    tInsertResult result = level.insert(tLevelPair(symbol->getMangledName(), symbol));
    return result.second;
}

void TSymbolTable::TSymbolTableLevel::insertUnmangled(TFunction *function)
{
    level.insert(tLevelPair(function->name(), function));
}

TSymbol *TSymbolTable::TSymbolTableLevel::find(const ImmutableString &name) const
{
    tLevel::const_iterator it = level.find(name);
    if (it == level.end())
        return nullptr;
    else
        return (*it).second;
}

TSymbolTable::TSymbolTable()
    : mGlobalInvariant(false),
      mUniqueIdCounter(0),
      mShaderType(GL_FRAGMENT_SHADER),
      mGlInVariableWithArraySize(nullptr)
{}

TSymbolTable::~TSymbolTable() = default;

bool TSymbolTable::isEmpty() const
{
    return mTable.empty();
}

bool TSymbolTable::atGlobalLevel() const
{
    return mTable.size() == 1u;
}

void TSymbolTable::push()
{
    mTable.emplace_back(new TSymbolTableLevel);
    mPrecisionStack.emplace_back(new PrecisionStackLevel);
}

void TSymbolTable::pop()
{
    mTable.pop_back();
    mPrecisionStack.pop_back();
}

const TFunction *TSymbolTable::markFunctionHasPrototypeDeclaration(
    const ImmutableString &mangledName,
    bool *hadPrototypeDeclarationOut) const
{
    TFunction *function         = findUserDefinedFunction(mangledName);
    *hadPrototypeDeclarationOut = function->hasPrototypeDeclaration();
    function->setHasPrototypeDeclaration();
    return function;
}

const TFunction *TSymbolTable::setFunctionParameterNamesFromDefinition(const TFunction *function,
                                                                       bool *wasDefinedOut) const
{
    TFunction *firstDeclaration = findUserDefinedFunction(function->getMangledName());
    ASSERT(firstDeclaration);
    // Note: 'firstDeclaration' could be 'function' if this is the first time we've seen function as
    // it would have just been put in the symbol table. Otherwise, we're looking up an earlier
    // occurance.
    if (function != firstDeclaration)
    {
        // The previous declaration should have the same parameters as the function definition
        // (parameter names may differ).
        firstDeclaration->shareParameters(*function);
    }

    *wasDefinedOut = firstDeclaration->isDefined();
    firstDeclaration->setDefined();
    return firstDeclaration;
}

bool TSymbolTable::setGlInArraySize(unsigned int inputArraySize)
{
    if (mGlInVariableWithArraySize)
    {
        return mGlInVariableWithArraySize->getType().getOutermostArraySize() == inputArraySize;
    }
    const TInterfaceBlock *glPerVertex = mVar_gl_PerVertex;
    TType *glInType = new TType(glPerVertex, EvqPerVertexIn, TLayoutQualifier::Create());
    glInType->makeArray(inputArraySize);
    mGlInVariableWithArraySize =
        new TVariable(this, ImmutableString("gl_in"), glInType, SymbolType::BuiltIn,
                      TExtension::EXT_geometry_shader);
    return true;
}

TVariable *TSymbolTable::getGlInVariableWithArraySize() const
{
    return mGlInVariableWithArraySize;
}

const TVariable *TSymbolTable::gl_FragData() const
{
    return mVar_gl_FragData;
}

const TVariable *TSymbolTable::gl_SecondaryFragDataEXT() const
{
    return mVar_gl_SecondaryFragDataEXT;
}

TSymbolTable::VariableMetadata *TSymbolTable::getOrCreateVariableMetadata(const TVariable &variable)
{
    int id    = variable.uniqueId().get();
    auto iter = mVariableMetadata.find(id);
    if (iter == mVariableMetadata.end())
    {
        iter = mVariableMetadata.insert(std::make_pair(id, VariableMetadata())).first;
    }
    return &iter->second;
}

void TSymbolTable::markStaticWrite(const TVariable &variable)
{
    auto metadata         = getOrCreateVariableMetadata(variable);
    metadata->staticWrite = true;
}

void TSymbolTable::markStaticRead(const TVariable &variable)
{
    auto metadata        = getOrCreateVariableMetadata(variable);
    metadata->staticRead = true;
}

bool TSymbolTable::isStaticallyUsed(const TVariable &variable) const
{
    ASSERT(!variable.getConstPointer());
    int id    = variable.uniqueId().get();
    auto iter = mVariableMetadata.find(id);
    return iter != mVariableMetadata.end() && (iter->second.staticRead || iter->second.staticWrite);
}

void TSymbolTable::addInvariantVarying(const TVariable &variable)
{
    ASSERT(atGlobalLevel());
    auto metadata       = getOrCreateVariableMetadata(variable);
    metadata->invariant = true;
}

bool TSymbolTable::isVaryingInvariant(const TVariable &variable) const
{
    ASSERT(atGlobalLevel());
    if (mGlobalInvariant)
    {
        return true;
    }
    int id    = variable.uniqueId().get();
    auto iter = mVariableMetadata.find(id);
    return iter != mVariableMetadata.end() && iter->second.invariant;
}

void TSymbolTable::setGlobalInvariant(bool invariant)
{
    ASSERT(atGlobalLevel());
    mGlobalInvariant = invariant;
}

const TSymbol *TSymbolTable::find(const ImmutableString &name, int shaderVersion) const
{
    const TSymbol *userSymbol = findUserDefined(name);
    if (userSymbol)
    {
        return userSymbol;
    }

    return findBuiltIn(name, shaderVersion);
}

const TSymbol *TSymbolTable::findUserDefined(const ImmutableString &name) const
{
    int userDefinedLevel = static_cast<int>(mTable.size()) - 1;
    while (userDefinedLevel >= 0)
    {
        const TSymbol *symbol = mTable[userDefinedLevel]->find(name);
        if (symbol)
        {
            return symbol;
        }
        userDefinedLevel--;
    }

    return nullptr;
}

TFunction *TSymbolTable::findUserDefinedFunction(const ImmutableString &name) const
{
    // User-defined functions are always declared at the global level.
    ASSERT(!mTable.empty());
    return static_cast<TFunction *>(mTable[0]->find(name));
}

const TSymbol *TSymbolTable::findGlobal(const ImmutableString &name) const
{
    ASSERT(!mTable.empty());
    return mTable[0]->find(name);
}

bool TSymbolTable::declare(TSymbol *symbol)
{
    ASSERT(!mTable.empty());
    ASSERT(symbol->symbolType() == SymbolType::UserDefined);
    ASSERT(!symbol->isFunction());
    return mTable.back()->insert(symbol);
}

bool TSymbolTable::declareInternal(TSymbol *symbol)
{
    ASSERT(!mTable.empty());
    ASSERT(symbol->symbolType() == SymbolType::AngleInternal);
    ASSERT(!symbol->isFunction());
    return mTable.back()->insert(symbol);
}

void TSymbolTable::declareUserDefinedFunction(TFunction *function, bool insertUnmangledName)
{
    ASSERT(!mTable.empty());
    if (insertUnmangledName)
    {
        // Insert the unmangled name to detect potential future redefinition as a variable.
        mTable[0]->insertUnmangled(function);
    }
    mTable[0]->insert(function);
}

void TSymbolTable::setDefaultPrecision(TBasicType type, TPrecision prec)
{
    int indexOfLastElement = static_cast<int>(mPrecisionStack.size()) - 1;
    // Uses map operator [], overwrites the current value
    (*mPrecisionStack[indexOfLastElement])[type] = prec;
}

TPrecision TSymbolTable::getDefaultPrecision(TBasicType type) const
{
    if (!SupportsPrecision(type))
        return EbpUndefined;

    // unsigned integers use the same precision as signed
    TBasicType baseType = (type == EbtUInt) ? EbtInt : type;

    int level = static_cast<int>(mPrecisionStack.size()) - 1;
    ASSERT(level >= 0);  // Just to be safe. Should not happen.
    // If we dont find anything we return this. Some types don't have predefined default precision.
    TPrecision prec = EbpUndefined;
    while (level >= 0)
    {
        PrecisionStackLevel::iterator it = mPrecisionStack[level]->find(baseType);
        if (it != mPrecisionStack[level]->end())
        {
            prec = (*it).second;
            break;
        }
        level--;
    }
    return prec;
}

void TSymbolTable::clearCompilationResults()
{
    mGlobalInvariant = false;
    mUniqueIdCounter = kLastBuiltInId + 1;
    mVariableMetadata.clear();
    mGlInVariableWithArraySize = nullptr;

    // User-defined scopes should have already been cleared when the compilation finished.
    ASSERT(mTable.empty());
}

int TSymbolTable::nextUniqueIdValue()
{
    ASSERT(mUniqueIdCounter < std::numeric_limits<int>::max());
    return ++mUniqueIdCounter;
}

void TSymbolTable::initializeBuiltIns(sh::GLenum type,
                                      ShShaderSpec spec,
                                      const ShBuiltInResources &resources)
{
    mShaderType = type;
    mResources  = resources;

    // We need just one precision stack level for predefined precisions.
    mPrecisionStack.emplace_back(new PrecisionStackLevel);

    switch (type)
    {
        case GL_FRAGMENT_SHADER:
            setDefaultPrecision(EbtInt, EbpMedium);
            break;
        case GL_VERTEX_SHADER:
        case GL_COMPUTE_SHADER:
        case GL_GEOMETRY_SHADER_EXT:
            setDefaultPrecision(EbtInt, EbpHigh);
            setDefaultPrecision(EbtFloat, EbpHigh);
            break;
        default:
            UNREACHABLE();
    }
    // Set defaults for sampler types that have default precision, even those that are
    // only available if an extension exists.
    // New sampler types in ESSL3 don't have default precision. ESSL1 types do.
    initSamplerDefaultPrecision(EbtSampler2D);
    initSamplerDefaultPrecision(EbtSamplerCube);
    // SamplerExternalOES is specified in the extension to have default precision.
    initSamplerDefaultPrecision(EbtSamplerExternalOES);
    // SamplerExternal2DY2YEXT is specified in the extension to have default precision.
    initSamplerDefaultPrecision(EbtSamplerExternal2DY2YEXT);
    // It isn't specified whether Sampler2DRect has default precision.
    initSamplerDefaultPrecision(EbtSampler2DRect);

    setDefaultPrecision(EbtAtomicCounter, EbpHigh);

    initializeBuiltInVariables(type, spec, resources);
    mUniqueIdCounter = kLastBuiltInId + 1;
}

void TSymbolTable::initSamplerDefaultPrecision(TBasicType samplerType)
{
    ASSERT(samplerType >= EbtGuardSamplerBegin && samplerType <= EbtGuardSamplerEnd);
    setDefaultPrecision(samplerType, EbpLow);
}

TSymbolTable::VariableMetadata::VariableMetadata()
    : staticRead(false), staticWrite(false), invariant(false)
{}
}  // namespace sh

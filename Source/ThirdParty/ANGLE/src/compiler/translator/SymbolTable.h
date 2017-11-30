//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_SYMBOLTABLE_H_
#define COMPILER_TRANSLATOR_SYMBOLTABLE_H_

//
// Symbol table for parsing.  Has these design characteristics:
//
// * Same symbol table can be used to compile many shaders, to preserve
//   effort of creating and loading with the large numbers of built-in
//   symbols.
//
// * Name mangling will be used to give each function a unique name
//   so that symbol table lookups are never ambiguous.  This allows
//   a simpler symbol table structure.
//
// * Pushing and popping of scope, so symbol table will really be a stack
//   of symbol tables.  Searched from the top, with new inserts going into
//   the top.
//
// * Constants:  Compile time constant symbols will keep their values
//   in the symbol table.  The parser can substitute constants at parse
//   time, including doing constant folding and constant propagation.
//
// * No temporaries:  Temporaries made from operations (+, --, .xy, etc.)
//   are tracked in the intermediate representation, not the symbol table.
//

#include <array>
#include <assert.h>
#include <set>

#include "common/angleutils.h"
#include "compiler/translator/ExtensionBehavior.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/SymbolUniqueId.h"

namespace sh
{

// Symbol base class. (Can build functions or variables out of these...)
class TSymbol : angle::NonCopyable
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TSymbol(TSymbolTable *symbolTable, const TString *n);

    virtual ~TSymbol()
    {
        // don't delete name, it's from the pool
    }

    const TString &getName() const { return *name; }
    virtual const TString &getMangledName() const { return getName(); }
    virtual bool isFunction() const { return false; }
    virtual bool isVariable() const { return false; }
    const TSymbolUniqueId &getUniqueId() const { return uniqueId; }
    void relateToExtension(TExtension ext) { extension = ext; }
    TExtension getExtension() const { return extension; }

  private:
    const TSymbolUniqueId uniqueId;
    const TString *name;
    TExtension extension;
};

// Variable, meaning a symbol that's not a function.
//
// May store the value of a constant variable of any type (float, int, bool or struct).
class TVariable : public TSymbol
{
  public:
    ~TVariable() override {}
    bool isVariable() const override { return true; }
    TType &getType() { return type; }
    const TType &getType() const { return type; }
    bool isUserType() const { return userType; }
    void setQualifier(TQualifier qualifier) { type.setQualifier(qualifier); }

    const TConstantUnion *getConstPointer() const { return unionArray; }

    void shareConstPointer(const TConstantUnion *constArray) { unionArray = constArray; }

  private:
    friend class TSymbolTable;

    TVariable(TSymbolTable *symbolTable,
              const TString *name,
              const TType &t,
              bool isUserTypeDefinition = false)
        : TSymbol(symbolTable, name), type(t), userType(isUserTypeDefinition), unionArray(0)
    {
    }

    TType type;

    // Set to true if this represents a struct type, as opposed to a variable.
    bool userType;

    // we are assuming that Pool Allocator will free the memory
    // allocated to unionArray when this object is destroyed.
    const TConstantUnion *unionArray;
};

// Immutable version of TParameter.
struct TConstParameter
{
    TConstParameter() : name(nullptr), type(nullptr) {}
    explicit TConstParameter(const TString *n) : name(n), type(nullptr) {}
    explicit TConstParameter(const TType *t) : name(nullptr), type(t) {}
    TConstParameter(const TString *n, const TType *t) : name(n), type(t) {}

    // Both constructor arguments must be const.
    TConstParameter(TString *n, TType *t)       = delete;
    TConstParameter(const TString *n, TType *t) = delete;
    TConstParameter(TString *n, const TType *t) = delete;

    const TString *const name;
    const TType *const type;
};

// The function sub-class of symbols and the parser will need to
// share this definition of a function parameter.
struct TParameter
{
    // Destructively converts to TConstParameter.
    // This method resets name and type to nullptrs to make sure
    // their content cannot be modified after the call.
    TConstParameter turnToConst()
    {
        const TString *constName = name;
        const TType *constType   = type;
        name                     = nullptr;
        type                     = nullptr;
        return TConstParameter(constName, constType);
    }

    const TString *name;
    TType *type;
};

// The function sub-class of a symbol.
class TFunction : public TSymbol
{
  public:
    TFunction(TSymbolTable *symbolTable,
              const TString *name,
              const TType *retType,
              TOperator tOp  = EOpNull,
              TExtension ext = TExtension::UNDEFINED)
        : TSymbol(symbolTable, name),
          returnType(retType),
          mangledName(nullptr),
          op(tOp),
          defined(false),
          mHasPrototypeDeclaration(false)
    {
        relateToExtension(ext);
    }
    ~TFunction() override;
    bool isFunction() const override { return true; }

    void addParameter(const TConstParameter &p)
    {
        parameters.push_back(p);
        mangledName = nullptr;
    }

    void swapParameters(const TFunction &parametersSource);

    const TString &getMangledName() const override
    {
        if (mangledName == nullptr)
        {
            mangledName = buildMangledName();
        }
        return *mangledName;
    }

    static const TString &GetMangledNameFromCall(const TString &functionName,
                                                 const TIntermSequence &arguments);

    const TType &getReturnType() const { return *returnType; }

    TOperator getBuiltInOp() const { return op; }

    void setDefined() { defined = true; }
    bool isDefined() { return defined; }
    void setHasPrototypeDeclaration() { mHasPrototypeDeclaration = true; }
    bool hasPrototypeDeclaration() const { return mHasPrototypeDeclaration; }

    size_t getParamCount() const { return parameters.size(); }
    const TConstParameter &getParam(size_t i) const { return parameters[i]; }

  private:
    void clearParameters();

    const TString *buildMangledName() const;

    typedef TVector<TConstParameter> TParamList;
    TParamList parameters;
    const TType *returnType;
    mutable const TString *mangledName;
    TOperator op;
    bool defined;
    bool mHasPrototypeDeclaration;
};

// Interface block name sub-symbol
class TInterfaceBlockName : public TSymbol
{
  public:
    virtual ~TInterfaceBlockName() {}

  private:
    friend class TSymbolTable;
    TInterfaceBlockName(TSymbolTable *symbolTable, const TString *name) : TSymbol(symbolTable, name)
    {
    }
};

class TSymbolTableLevel
{
  public:
    typedef TUnorderedMap<TString, TSymbol *> tLevel;
    typedef tLevel::const_iterator const_iterator;
    typedef const tLevel::value_type tLevelPair;
    typedef std::pair<tLevel::iterator, bool> tInsertResult;

    TSymbolTableLevel() : mGlobalInvariant(false) {}
    ~TSymbolTableLevel();

    bool insert(TSymbol *symbol);

    // Insert a function using its unmangled name as the key.
    bool insertUnmangled(TFunction *function);

    TSymbol *find(const TString &name) const;

    void addInvariantVarying(const std::string &name) { mInvariantVaryings.insert(name); }

    bool isVaryingInvariant(const std::string &name)
    {
        return (mGlobalInvariant || mInvariantVaryings.count(name) > 0);
    }

    void setGlobalInvariant(bool invariant) { mGlobalInvariant = invariant; }

    void insertUnmangledBuiltInName(const std::string &name)
    {
        mUnmangledBuiltInNames.insert(name);
    }

    bool hasUnmangledBuiltIn(const std::string &name)
    {
        return mUnmangledBuiltInNames.count(name) > 0;
    }

  protected:
    tLevel level;
    std::set<std::string> mInvariantVaryings;
    bool mGlobalInvariant;

  private:
    std::set<std::string> mUnmangledBuiltInNames;
};

// Define ESymbolLevel as int rather than an enum since level can go
// above GLOBAL_LEVEL and cause atBuiltInLevel() to fail if the
// compiler optimizes the >= of the last element to ==.
typedef int ESymbolLevel;
const int COMMON_BUILTINS    = 0;
const int ESSL1_BUILTINS     = 1;
const int ESSL3_BUILTINS     = 2;
const int ESSL3_1_BUILTINS   = 3;
// GLSL_BUILTINS are desktop GLSL builtins that don't exist in ESSL but are used to implement
// features in ANGLE's GLSL backend. They're not visible to the parser.
const int GLSL_BUILTINS      = 4;
const int LAST_BUILTIN_LEVEL = GLSL_BUILTINS;
const int GLOBAL_LEVEL       = 5;

class TSymbolTable : angle::NonCopyable
{
  public:
    TSymbolTable() : mUniqueIdCounter(0), mEmptySymbolId(this)
    {
        // The symbol table cannot be used until push() is called, but
        // the lack of an initial call to push() can be used to detect
        // that the symbol table has not been preloaded with built-ins.
    }

    ~TSymbolTable();

    // When the symbol table is initialized with the built-ins, there should
    // 'push' calls, so that built-ins are at level 0 and the shader
    // globals are at level 1.
    bool isEmpty() const { return table.empty(); }
    bool atBuiltInLevel() const { return currentLevel() <= LAST_BUILTIN_LEVEL; }
    bool atGlobalLevel() const { return currentLevel() == GLOBAL_LEVEL; }
    void push()
    {
        table.push_back(new TSymbolTableLevel);
        precisionStack.push_back(new PrecisionStackLevel);
    }

    void pop()
    {
        delete table.back();
        table.pop_back();

        delete precisionStack.back();
        precisionStack.pop_back();
    }

    // The declare* entry points are used when parsing and declare symbols at the current scope.
    // They return the created symbol in case the declaration was successful, and nullptr if the
    // declaration failed due to redefinition.
    TVariable *declareVariable(const TString *name, const TType &type);
    TVariable *declareStructType(TStructure *str);
    TInterfaceBlockName *declareInterfaceBlockName(const TString *name);

    // The insert* entry points are used when initializing the symbol table with built-ins.
    // They return the created symbol in case the declaration was successful, and nullptr if the
    // declaration failed due to redefinition.
    TVariable *insertVariable(ESymbolLevel level, const char *name, const TType &type);
    TVariable *insertVariableExt(ESymbolLevel level,
                                 TExtension ext,
                                 const char *name,
                                 const TType &type);
    TVariable *insertStructType(ESymbolLevel level, TStructure *str);
    TInterfaceBlockName *insertInterfaceBlockNameExt(ESymbolLevel level,
                                                     TExtension ext,
                                                     const TString *name);

    bool insertConstInt(ESymbolLevel level, const char *name, int value, TPrecision precision)
    {
        TVariable *constant =
            new TVariable(this, NewPoolTString(name), TType(EbtInt, precision, EvqConst, 1));
        TConstantUnion *unionArray = new TConstantUnion[1];
        unionArray[0].setIConst(value);
        constant->shareConstPointer(unionArray);
        return insert(level, constant);
    }

    bool insertConstIntExt(ESymbolLevel level,
                           TExtension ext,
                           const char *name,
                           int value,
                           TPrecision precision)
    {
        TVariable *constant =
            new TVariable(this, NewPoolTString(name), TType(EbtInt, precision, EvqConst, 1));
        TConstantUnion *unionArray = new TConstantUnion[1];
        unionArray[0].setIConst(value);
        constant->shareConstPointer(unionArray);
        return insert(level, ext, constant);
    }

    bool insertConstIvec3(ESymbolLevel level,
                          const char *name,
                          const std::array<int, 3> &values,
                          TPrecision precision)
    {
        TVariable *constantIvec3 =
            new TVariable(this, NewPoolTString(name), TType(EbtInt, precision, EvqConst, 3));

        TConstantUnion *unionArray = new TConstantUnion[3];
        for (size_t index = 0u; index < 3u; ++index)
        {
            unionArray[index].setIConst(values[index]);
        }
        constantIvec3->shareConstPointer(unionArray);

        return insert(level, constantIvec3);
    }

    void insertBuiltIn(ESymbolLevel level,
                       TOperator op,
                       TExtension ext,
                       const TType *rvalue,
                       const char *name,
                       const TType *ptype1,
                       const TType *ptype2 = 0,
                       const TType *ptype3 = 0,
                       const TType *ptype4 = 0,
                       const TType *ptype5 = 0);

    void insertBuiltIn(ESymbolLevel level,
                       const TType *rvalue,
                       const char *name,
                       const TType *ptype1,
                       const TType *ptype2 = 0,
                       const TType *ptype3 = 0,
                       const TType *ptype4 = 0,
                       const TType *ptype5 = 0)
    {
        insertUnmangledBuiltInName(name, level);
        insertBuiltIn(level, EOpNull, TExtension::UNDEFINED, rvalue, name, ptype1, ptype2, ptype3,
                      ptype4, ptype5);
    }

    void insertBuiltIn(ESymbolLevel level,
                       TExtension ext,
                       const TType *rvalue,
                       const char *name,
                       const TType *ptype1,
                       const TType *ptype2 = 0,
                       const TType *ptype3 = 0,
                       const TType *ptype4 = 0,
                       const TType *ptype5 = 0)
    {
        insertUnmangledBuiltInName(name, level);
        insertBuiltIn(level, EOpNull, ext, rvalue, name, ptype1, ptype2, ptype3, ptype4, ptype5);
    }

    void insertBuiltInOp(ESymbolLevel level,
                         TOperator op,
                         const TType *rvalue,
                         const TType *ptype1,
                         const TType *ptype2 = 0,
                         const TType *ptype3 = 0,
                         const TType *ptype4 = 0,
                         const TType *ptype5 = 0);

    void insertBuiltInOp(ESymbolLevel level,
                         TOperator op,
                         TExtension ext,
                         const TType *rvalue,
                         const TType *ptype1,
                         const TType *ptype2 = 0,
                         const TType *ptype3 = 0,
                         const TType *ptype4 = 0,
                         const TType *ptype5 = 0);

    void insertBuiltInFunctionNoParameters(ESymbolLevel level,
                                           TOperator op,
                                           const TType *rvalue,
                                           const char *name);

    void insertBuiltInFunctionNoParametersExt(ESymbolLevel level,
                                              TExtension ext,
                                              TOperator op,
                                              const TType *rvalue,
                                              const char *name);

    TSymbol *find(const TString &name,
                  int shaderVersion,
                  bool *builtIn   = nullptr,
                  bool *sameScope = nullptr) const;

    TSymbol *findGlobal(const TString &name) const;

    TSymbol *findBuiltIn(const TString &name, int shaderVersion) const;

    TSymbol *findBuiltIn(const TString &name, int shaderVersion, bool includeGLSLBuiltins) const;

    TSymbolTableLevel *getOuterLevel()
    {
        assert(currentLevel() >= 1);
        return table[currentLevel() - 1];
    }

    void setDefaultPrecision(TBasicType type, TPrecision prec)
    {
        int indexOfLastElement = static_cast<int>(precisionStack.size()) - 1;
        // Uses map operator [], overwrites the current value
        (*precisionStack[indexOfLastElement])[type] = prec;
    }

    // Searches down the precisionStack for a precision qualifier
    // for the specified TBasicType
    TPrecision getDefaultPrecision(TBasicType type) const;

    // This records invariant varyings declared through
    // "invariant varying_name;".
    void addInvariantVarying(const std::string &originalName)
    {
        ASSERT(atGlobalLevel());
        table[currentLevel()]->addInvariantVarying(originalName);
    }
    // If this returns false, the varying could still be invariant
    // if it is set as invariant during the varying variable
    // declaration - this piece of information is stored in the
    // variable's type, not here.
    bool isVaryingInvariant(const std::string &originalName) const
    {
        ASSERT(atGlobalLevel());
        return table[currentLevel()]->isVaryingInvariant(originalName);
    }

    void setGlobalInvariant(bool invariant)
    {
        ASSERT(atGlobalLevel());
        table[currentLevel()]->setGlobalInvariant(invariant);
    }

    const TSymbolUniqueId nextUniqueId() { return TSymbolUniqueId(this); }

    // The empty symbol id is shared between all empty string ("") symbols. They are used in the
    // AST for unused function parameters and struct type declarations that don't declare a
    // variable, for example.
    const TSymbolUniqueId &getEmptySymbolId() { return mEmptySymbolId; }

    // Checks whether there is a built-in accessible by a shader with the specified version.
    bool hasUnmangledBuiltInForShaderVersion(const char *name, int shaderVersion);

  private:
    friend class TSymbolUniqueId;
    int nextUniqueIdValue() { return ++mUniqueIdCounter; }

    ESymbolLevel currentLevel() const { return static_cast<ESymbolLevel>(table.size() - 1); }

    TVariable *insertVariable(ESymbolLevel level, const TString *name, const TType &type);

    bool insert(ESymbolLevel level, TSymbol *symbol) { return table[level]->insert(symbol); }

    bool insert(ESymbolLevel level, TExtension ext, TSymbol *symbol)
    {
        symbol->relateToExtension(ext);
        return table[level]->insert(symbol);
    }

    // Used to insert unmangled functions to check redeclaration of built-ins in ESSL 3.00 and
    // above.
    void insertUnmangledBuiltInName(const char *name, ESymbolLevel level);

    bool hasUnmangledBuiltInAtLevel(const char *name, ESymbolLevel level);

    std::vector<TSymbolTableLevel *> table;
    typedef TMap<TBasicType, TPrecision> PrecisionStackLevel;
    std::vector<PrecisionStackLevel *> precisionStack;

    int mUniqueIdCounter;

    const TSymbolUniqueId mEmptySymbolId;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_SYMBOLTABLE_H_

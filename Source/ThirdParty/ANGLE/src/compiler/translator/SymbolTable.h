//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef _SYMBOL_TABLE_INCLUDED_
#define _SYMBOL_TABLE_INCLUDED_

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

#include <assert.h>

#include "common/angleutils.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/intermediate.h"

//
// Symbol base class.  (Can build functions or variables out of these...)
//
class TSymbol {    
public:
    POOL_ALLOCATOR_NEW_DELETE();
    TSymbol(const TString* n) :  uniqueId(0), name(n) { }
    virtual ~TSymbol() { /* don't delete name, it's from the pool */ }

    const TString& getName() const { return *name; }
    virtual const TString& getMangledName() const { return getName(); }
    virtual bool isFunction() const { return false; }
    virtual bool isVariable() const { return false; }
    void setUniqueId(int id) { uniqueId = id; }
    int getUniqueId() const { return uniqueId; }
    void relateToExtension(const TString& ext) { extension = ext; }
    const TString& getExtension() const { return extension; }

private:
    DISALLOW_COPY_AND_ASSIGN(TSymbol);

    int uniqueId;      // For real comparing during code generation
    const TString *name;
    TString extension;
};

//
// Variable class, meaning a symbol that's not a function.
// 
// There could be a separate class heirarchy for Constant variables;
// Only one of int, bool, or float, (or none) is correct for
// any particular use, but it's easy to do this way, and doesn't
// seem worth having separate classes, and "getConst" can't simply return
// different values for different types polymorphically, so this is 
// just simple and pragmatic.
//
class TVariable : public TSymbol {
public:
    TVariable(const TString *name, const TType& t, bool uT = false ) : TSymbol(name), type(t), userType(uT), unionArray(0) { }
    virtual ~TVariable() { }
    virtual bool isVariable() const { return true; }    
    TType& getType() { return type; }    
    const TType& getType() const { return type; }
    bool isUserType() const { return userType; }
    void setQualifier(TQualifier qualifier) { type.setQualifier(qualifier); }

    ConstantUnion* getConstPointer()
    { 
        if (!unionArray)
            unionArray = new ConstantUnion[type.getObjectSize()];

        return unionArray;
    }

    ConstantUnion* getConstPointer() const { return unionArray; }

    void shareConstPointer( ConstantUnion *constArray)
    {
        if (unionArray == constArray)
            return;

        delete[] unionArray;
        unionArray = constArray;  
    }

private:
    DISALLOW_COPY_AND_ASSIGN(TVariable);

    TType type;
    bool userType;
    // we are assuming that Pool Allocator will free the memory allocated to unionArray
    // when this object is destroyed
    ConstantUnion *unionArray;
};

//
// The function sub-class of symbols and the parser will need to
// share this definition of a function parameter.
//
struct TParameter {
    TString *name;
    TType *type;
};

//
// The function sub-class of a symbol.  
//
class TFunction : public TSymbol {
public:
    TFunction(TOperator o) :
        TSymbol(0),
        returnType(TType(EbtVoid, EbpUndefined)),
        op(o),
        defined(false) { }
    TFunction(const TString *name, TType& retType, TOperator tOp = EOpNull) : 
        TSymbol(name), 
        returnType(retType),
        mangledName(TFunction::mangleName(*name)),
        op(tOp),
        defined(false) { }
    virtual ~TFunction();
    virtual bool isFunction() const { return true; }    

    static TString mangleName(const TString& name) { return name + '('; }
    static TString unmangleName(const TString& mangledName)
    {
        return TString(mangledName.c_str(), mangledName.find_first_of('('));
    }

    void addParameter(TParameter& p) 
    { 
        parameters.push_back(p);
        mangledName = mangledName + p.type->getMangledName();
    }

    const TString& getMangledName() const { return mangledName; }
    const TType& getReturnType() const { return returnType; }

    void relateToOperator(TOperator o) { op = o; }
    TOperator getBuiltInOp() const { return op; }

    void setDefined() { defined = true; }
    bool isDefined() { return defined; }

    size_t getParamCount() const { return parameters.size(); }  
    const TParameter& getParam(size_t i) const { return parameters[i]; }

private:
    DISALLOW_COPY_AND_ASSIGN(TFunction);

    typedef TVector<TParameter> TParamList;
    TParamList parameters;
    TType returnType;
    TString mangledName;
    TOperator op;
    bool defined;
};

//
// Interface block name sub-symbol
//
class TInterfaceBlockName : public TSymbol
{
public:
    TInterfaceBlockName(const TString *name)
        : TSymbol(name)
    {}

    virtual ~TInterfaceBlockName() {}
};

class TSymbolTableLevel {
public:
    typedef TMap<TString, TSymbol*> tLevel;
    typedef tLevel::const_iterator const_iterator;
    typedef const tLevel::value_type tLevelPair;
    typedef std::pair<tLevel::iterator, bool> tInsertResult;

    TSymbolTableLevel() { }
    ~TSymbolTableLevel();

    bool insert(const TString &name, TSymbol &symbol)
    {
        symbol.setUniqueId(++uniqueId);

        //
        // returning true means symbol was added to the table
        //
        tInsertResult result = level.insert(tLevelPair(name, &symbol));

        return result.second;
    }

    bool insert(TSymbol &symbol)
    {
        return insert(symbol.getMangledName(), symbol);
    }

    TSymbol* find(const TString& name) const
    {
        tLevel::const_iterator it = level.find(name);
        if (it == level.end())
            return 0;
        else
            return (*it).second;
    }

    void relateToOperator(const char* name, TOperator op);
    void relateToExtension(const char* name, const TString& ext);

protected:
    tLevel level;
    static int uniqueId;     // for unique identification in code generation
};

enum ESymbolLevel
{
    COMMON_BUILTINS = 0,
    ESSL1_BUILTINS = 1,
    ESSL3_BUILTINS = 2,
    LAST_BUILTIN_LEVEL = ESSL3_BUILTINS,
    GLOBAL_LEVEL = 3
};

class TSymbolTable {
public:
    TSymbolTable()
    {
        //
        // The symbol table cannot be used until push() is called, but
        // the lack of an initial call to push() can be used to detect
        // that the symbol table has not been preloaded with built-ins.
        //
    }

    ~TSymbolTable();

    //
    // When the symbol table is initialized with the built-ins, there should
    // 'push' calls, so that built-ins are at level 0 and the shader
    // globals are at level 1.
    //
    bool isEmpty() { return table.empty(); }
    bool atBuiltInLevel() { return currentLevel() <= LAST_BUILTIN_LEVEL; }
    bool atGlobalLevel() { return currentLevel() <= GLOBAL_LEVEL; }
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

    bool declare(TSymbol &symbol)
    {
        return insert(currentLevel(), symbol);
    }

    bool insert(ESymbolLevel level, TSymbol &symbol)
    {
        return table[level]->insert(symbol);
    }

    bool insertConstInt(ESymbolLevel level, const char *name, int value)
    {
        TVariable *constant = new TVariable(NewPoolTString(name), TType(EbtInt, EbpUndefined, EvqConst, 1));
        constant->getConstPointer()->setIConst(value);
        return insert(level, *constant);
    }

    void insertBuiltIn(ESymbolLevel level, TType *rvalue, const char *name, TType *ptype1, TType *ptype2 = 0, TType *ptype3 = 0, TType *ptype4 = 0, TType *ptype5 = 0)
    {
        if (ptype1->getBasicType() == EbtGSampler2D)
        {
            bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
            insertBuiltIn(level, gvec4 ? new TType(EbtFloat, 4) : rvalue, name, new TType(EbtSampler2D), ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, gvec4 ? new TType(EbtInt, 4) : rvalue, name, new TType(EbtISampler2D), ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, gvec4 ? new TType(EbtUInt, 4) : rvalue, name, new TType(EbtUSampler2D), ptype2, ptype3, ptype4, ptype5);
            return;
        }
        else if (ptype1->getBasicType() == EbtGSampler3D)
        {
            bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
            insertBuiltIn(level, gvec4 ? new TType(EbtFloat, 4) : rvalue, name, new TType(EbtSampler3D), ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, gvec4 ? new TType(EbtInt, 4) : rvalue, name, new TType(EbtISampler3D), ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, gvec4 ? new TType(EbtUInt, 4) : rvalue, name, new TType(EbtUSampler3D), ptype2, ptype3, ptype4, ptype5);
            return;
        }
        else if (ptype1->getBasicType() == EbtGSamplerCube)
        {
            bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
            insertBuiltIn(level, gvec4 ? new TType(EbtFloat, 4) : rvalue, name, new TType(EbtSamplerCube), ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, gvec4 ? new TType(EbtInt, 4) : rvalue, name, new TType(EbtISamplerCube), ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, gvec4 ? new TType(EbtUInt, 4) : rvalue, name, new TType(EbtUSamplerCube), ptype2, ptype3, ptype4, ptype5);
            return;
        }
        else if (ptype1->getBasicType() == EbtGSampler2DArray)
        {
            bool gvec4 = (rvalue->getBasicType() == EbtGVec4);
            insertBuiltIn(level, gvec4 ? new TType(EbtFloat, 4) : rvalue, name, new TType(EbtSampler2DArray), ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, gvec4 ? new TType(EbtInt, 4) : rvalue, name, new TType(EbtISampler2DArray), ptype2, ptype3, ptype4, ptype5);
            insertBuiltIn(level, gvec4 ? new TType(EbtUInt, 4) : rvalue, name, new TType(EbtUSampler2DArray), ptype2, ptype3, ptype4, ptype5);
            return;
        }

        TFunction *function = new TFunction(NewPoolTString(name), *rvalue);

        TParameter param1 = {NULL, ptype1};
        function->addParameter(param1);

        if (ptype2)
        {
            TParameter param2 = {NULL, ptype2};
            function->addParameter(param2);
        }

        if (ptype3)
        {
            TParameter param3 = {NULL, ptype3};
            function->addParameter(param3);
        }

        if (ptype4)
        {
            TParameter param4 = {NULL, ptype4};
            function->addParameter(param4);
        }

        if (ptype5)
        {
            TParameter param5 = {NULL, ptype5};
            function->addParameter(param5);
        }

        insert(level, *function);
    }

    TSymbol *find(const TString &name, int shaderVersion, bool *builtIn = NULL, bool *sameScope = NULL);
    TSymbol *findBuiltIn(const TString &name, int shaderVersion);
    
    TSymbolTableLevel *getOuterLevel() {
        assert(currentLevel() >= 1);
        return table[currentLevel() - 1];
    }

    void relateToOperator(ESymbolLevel level, const char* name, TOperator op) {
        table[level]->relateToOperator(name, op);
    }
    void relateToExtension(ESymbolLevel level, const char* name, const TString& ext) {
        table[level]->relateToExtension(name, ext);
    }
    void dump(TInfoSink &infoSink) const;

    bool setDefaultPrecision(const TPublicType& type, TPrecision prec) {
        if (!supportsPrecision(type.type))
            return false;
        if (type.isAggregate())
            return false; // Not allowed to set for aggregate types
        int indexOfLastElement = static_cast<int>(precisionStack.size()) - 1;
        (*precisionStack[indexOfLastElement])[type.type] = prec; // Uses map operator [], overwrites the current value
        return true;
    }

    // Searches down the precisionStack for a precision qualifier for the specified TBasicType
    TPrecision getDefaultPrecision( TBasicType type){

        if (!supportsPrecision(type))
            return EbpUndefined;

        // unsigned integers use the same precision as signed
        TBasicType baseType = (type == EbtUInt) ? EbtInt : type;

        int level = static_cast<int>(precisionStack.size()) - 1;
        assert(level >= 0); // Just to be safe. Should not happen.
        PrecisionStackLevel::iterator it;
        TPrecision prec = EbpUndefined; // If we dont find anything we return this. Should we error check this?
        while (level >= 0) {
            it = precisionStack[level]->find(baseType);
            if (it != precisionStack[level]->end()) {
                prec = (*it).second;
                break;
            }
            level--;
        }
        return prec;
    }

private:
    ESymbolLevel currentLevel() const { return static_cast<ESymbolLevel>(table.size() - 1); }

    bool supportsPrecision(TBasicType type) {
      // Only supports precision for int, float, and sampler types.
      return type == EbtFloat || type == EbtInt || type == EbtUInt || IsSampler(type);
    }

    std::vector<TSymbolTableLevel*> table;
    typedef TMap<TBasicType, TPrecision> PrecisionStackLevel;
    std::vector< PrecisionStackLevel*> precisionStack;
};

#endif // _SYMBOL_TABLE_INCLUDED_

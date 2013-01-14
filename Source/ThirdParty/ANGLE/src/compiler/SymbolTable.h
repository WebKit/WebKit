//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
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

#include "compiler/InfoSink.h"
#include "compiler/intermediate.h"

//
// Symbol base class.  (Can build functions or variables out of these...)
//
class TSymbol {    
public:
    POOL_ALLOCATOR_NEW_DELETE(GlobalPoolAllocator)
    TSymbol(const TString *n) :  name(n) { }
    virtual ~TSymbol() { /* don't delete name, it's from the pool */ }
    const TString& getName() const { return *name; }
    virtual const TString& getMangledName() const { return getName(); }
    virtual bool isFunction() const { return false; }
    virtual bool isVariable() const { return false; }
    void setUniqueId(int id) { uniqueId = id; }
    int getUniqueId() const { return uniqueId; }
    virtual void dump(TInfoSink &infoSink) const = 0;	
    TSymbol(const TSymbol&);
    virtual TSymbol* clone(TStructureMap& remapper) = 0;

protected:
    const TString *name;
    unsigned int uniqueId;      // For real comparing during code generation
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
    TVariable(const TString *name, const TType& t, bool uT = false ) : TSymbol(name), type(t), userType(uT), unionArray(0), arrayInformationType(0) { }
    virtual ~TVariable() { }
    virtual bool isVariable() const { return true; }    
    TType& getType() { return type; }    
    const TType& getType() const { return type; }
    bool isUserType() const { return userType; }
    void setQualifier(TQualifier qualifier) { type.setQualifier(qualifier); }
    void updateArrayInformationType(TType *t) { arrayInformationType = t; }
    TType* getArrayInformationType() { return arrayInformationType; }

    virtual void dump(TInfoSink &infoSink) const;

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
    TVariable(const TVariable&, TStructureMap& remapper); // copy constructor
    virtual TVariable* clone(TStructureMap& remapper);

protected:
    TType type;
    bool userType;
    // we are assuming that Pool Allocator will free the memory allocated to unionArray
    // when this object is destroyed
    ConstantUnion *unionArray;
    TType *arrayInformationType;  // this is used for updating maxArraySize in all the references to a given symbol
};

//
// The function sub-class of symbols and the parser will need to
// share this definition of a function parameter.
//
struct TParameter {
    TString *name;
    TType* type;
    void copyParam(const TParameter& param, TStructureMap& remapper)
    {
        name = NewPoolTString(param.name->c_str());
        type = param.type->clone(remapper);
    }
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

    void relateToExtension(const TString& ext) { extension = ext; }
    const TString& getExtension() const { return extension; }

    void setDefined() { defined = true; }
    bool isDefined() { return defined; }

    int getParamCount() const { return static_cast<int>(parameters.size()); }  
    const TParameter& getParam(int i) const { return parameters[i]; }

    virtual void dump(TInfoSink &infoSink) const;
    TFunction(const TFunction&, TStructureMap& remapper);
    virtual TFunction* clone(TStructureMap& remapper);

protected:
    typedef TVector<TParameter> TParamList;
    TParamList parameters;
    TType returnType;
    TString mangledName;
    TOperator op;
    TString extension;
    bool defined;
};


class TSymbolTableLevel {
public:
    typedef TMap<TString, TSymbol*> tLevel;
    typedef tLevel::const_iterator const_iterator;
    typedef const tLevel::value_type tLevelPair;
    typedef std::pair<tLevel::iterator, bool> tInsertResult;

    POOL_ALLOCATOR_NEW_DELETE(GlobalPoolAllocator)
    TSymbolTableLevel() { }
    ~TSymbolTableLevel();

    bool insert(TSymbol& symbol) 
    {
        //
        // returning true means symbol was added to the table
        //
        tInsertResult result;
        result = level.insert(tLevelPair(symbol.getMangledName(), &symbol));

        return result.second;
    }

    TSymbol* find(const TString& name) const
    {
        tLevel::const_iterator it = level.find(name);
        if (it == level.end())
            return 0;
        else
            return (*it).second;
    }

    const_iterator begin() const
    {
        return level.begin();
    }

    const_iterator end() const
    {
        return level.end();
    }

    void relateToOperator(const char* name, TOperator op);
    void relateToExtension(const char* name, const TString& ext);
    void dump(TInfoSink &infoSink) const;
    TSymbolTableLevel* clone(TStructureMap& remapper);

protected:
    tLevel level;
};

class TSymbolTable {
public:
    TSymbolTable() : uniqueId(0)
    {
        //
        // The symbol table cannot be used until push() is called, but
        // the lack of an initial call to push() can be used to detect
        // that the symbol table has not been preloaded with built-ins.
        //
    }

    ~TSymbolTable()
    {
        // level 0 is always built In symbols, so we never pop that out
        while (table.size() > 1)
            pop();
    }

    //
    // When the symbol table is initialized with the built-ins, there should
    // 'push' calls, so that built-ins are at level 0 and the shader
    // globals are at level 1.
    //
    bool isEmpty() { return table.size() == 0; }
    bool atBuiltInLevel() { return table.size() == 1; }
    bool atGlobalLevel() { return table.size() <= 2; }
    void push()
    {
        table.push_back(new TSymbolTableLevel);
        precisionStack.push_back( PrecisionStackLevel() );
    }

    void pop()
    { 
        delete table[currentLevel()]; 
        table.pop_back(); 
        precisionStack.pop_back();
    }

    bool insert(TSymbol& symbol)
    {
        symbol.setUniqueId(++uniqueId);
        return table[currentLevel()]->insert(symbol);
    }

    TSymbol* find(const TString& name, bool* builtIn = 0, bool *sameScope = 0) 
    {
        int level = currentLevel();
        TSymbol* symbol;
        do {
            symbol = table[level]->find(name);
            --level;
        } while (symbol == 0 && level >= 0);
        level++;
        if (builtIn)
            *builtIn = level == 0;
        if (sameScope)
            *sameScope = level == currentLevel();
        return symbol;
    }

    TSymbol *findBuiltIn(const TString &name)
    {
        return table[0]->find(name);
    }

    TSymbolTableLevel* getGlobalLevel() {
        assert(table.size() >= 2);
        return table[1];
    }

    TSymbolTableLevel* getOuterLevel() {
        assert(table.size() >= 2);
        return table[currentLevel() - 1];
    }

    void relateToOperator(const char* name, TOperator op) {
        table[0]->relateToOperator(name, op);
    }
    void relateToExtension(const char* name, const TString& ext) {
        table[0]->relateToExtension(name, ext);
    }
    int getMaxSymbolId() { return uniqueId; }
    void dump(TInfoSink &infoSink) const;
    void copyTable(const TSymbolTable& copyOf);

    void setDefaultPrecision( TBasicType type, TPrecision prec ){
        if( type != EbtFloat && type != EbtInt ) return; // Only set default precision for int/float
        int indexOfLastElement = static_cast<int>(precisionStack.size()) - 1;
        precisionStack[indexOfLastElement][type] = prec; // Uses map operator [], overwrites the current value
    }

    // Searches down the precisionStack for a precision qualifier for the specified TBasicType
    TPrecision getDefaultPrecision( TBasicType type){
        if( type != EbtFloat && type != EbtInt ) return EbpUndefined;
        int level = static_cast<int>(precisionStack.size()) - 1;
        assert( level >= 0); // Just to be safe. Should not happen.
        PrecisionStackLevel::iterator it;
        TPrecision prec = EbpUndefined; // If we dont find anything we return this. Should we error check this?
        while( level >= 0 ){
            it = precisionStack[level].find( type );
            if( it != precisionStack[level].end() ){
                prec = (*it).second;
                break;
            }
            level--;
        }
        return prec;
    }

protected:    
    int currentLevel() const { return static_cast<int>(table.size()) - 1; }

    std::vector<TSymbolTableLevel*> table;
    typedef std::map< TBasicType, TPrecision > PrecisionStackLevel;
    std::vector< PrecisionStackLevel > precisionStack;
    int uniqueId;     // for unique identification in code generation
};

#endif // _SYMBOL_TABLE_INCLUDED_

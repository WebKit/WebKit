//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TYPES_H_
#define COMPILER_TRANSLATOR_TYPES_H_

#include "common/angleutils.h"
#include "common/debug.h"

#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Common.h"

struct TPublicType;
class TType;
class TSymbol;
class TIntermSymbol;

class TField : angle::NonCopyable
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TField(TType *type, TString *name, const TSourceLoc &line)
        : mType(type),
          mName(name),
          mLine(line)
    {
    }

    // TODO(alokp): We should only return const type.
    // Fix it by tweaking grammar.
    TType *type()
    {
        return mType;
    }
    const TType *type() const
    {
        return mType;
    }

    const TString &name() const
    {
        return *mName;
    }
    const TSourceLoc &line() const
    {
        return mLine;
    }

  private:
    TType *mType;
    TString *mName;
    TSourceLoc mLine;
};

typedef TVector<TField *> TFieldList;
inline TFieldList *NewPoolTFieldList()
{
    void *memory = GetGlobalPoolAllocator()->allocate(sizeof(TFieldList));
    return new(memory) TFieldList;
}

class TFieldListCollection : angle::NonCopyable
{
  public:
    const TString &name() const
    {
        return *mName;
    }
    const TFieldList &fields() const
    {
        return *mFields;
    }

    size_t objectSize() const
    {
        if (mObjectSize == 0)
            mObjectSize = calculateObjectSize();
        return mObjectSize;
    };

  protected:
    TFieldListCollection(const TString *name, TFieldList *fields)
        : mName(name),
          mFields(fields),
          mObjectSize(0)
    {
    }
    TString buildMangledName(const TString &mangledNamePrefix) const;
    size_t calculateObjectSize() const;

    const TString *mName;
    TFieldList *mFields;

    mutable TString mMangledName;
    mutable size_t mObjectSize;
};

// May also represent interface blocks
class TStructure : public TFieldListCollection
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TStructure(const TString *name, TFieldList *fields);

    int deepestNesting() const
    {
        if (mDeepestNesting == 0)
            mDeepestNesting = calculateDeepestNesting();
        return mDeepestNesting;
    }
    bool containsArrays() const;
    bool containsType(TBasicType t) const;
    bool containsSamplers() const;

    void createSamplerSymbols(const TString &structName,
                              const TString &structAPIName,
                              const unsigned int arrayOfStructsSize,
                              TVector<TIntermSymbol *> *outputSymbols,
                              TMap<TIntermSymbol *, TString> *outputSymbolsToAPINames) const;

    bool equals(const TStructure &other) const;

    void setUniqueId(int uniqueId)
    {
        mUniqueId = uniqueId;
    }

    int uniqueId() const
    {
        ASSERT(mUniqueId != 0);
        return mUniqueId;
    }

    void setAtGlobalScope(bool atGlobalScope)
    {
        mAtGlobalScope = atGlobalScope;
    }

    bool atGlobalScope() const
    {
        return mAtGlobalScope;
    }

    const TString &mangledName() const
    {
        if (mMangledName.empty())
            mMangledName = buildMangledName("struct-");
        return mMangledName;
    }

  private:
    // TODO(zmo): Find a way to get rid of the const_cast in function
    // setName().  At the moment keep this function private so only
    // friend class RegenerateStructNames may call it.
    friend class RegenerateStructNames;
    void setName(const TString &name)
    {
        TString *mutableName = const_cast<TString *>(mName);
        *mutableName = name;
    }

    int calculateDeepestNesting() const;

    mutable int mDeepestNesting;
    int mUniqueId;
    bool mAtGlobalScope;
};

class TInterfaceBlock : public TFieldListCollection
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TInterfaceBlock(const TString *name, TFieldList *fields, const TString *instanceName,
                    int arraySize, const TLayoutQualifier &layoutQualifier)
        : TFieldListCollection(name, fields),
          mInstanceName(instanceName),
          mArraySize(arraySize),
          mBlockStorage(layoutQualifier.blockStorage),
          mMatrixPacking(layoutQualifier.matrixPacking)
    {
    }

    const TString &instanceName() const
    {
        return *mInstanceName;
    }
    bool hasInstanceName() const
    {
        return mInstanceName != NULL;
    }
    bool isArray() const
    {
        return mArraySize > 0;
    }
    int arraySize() const
    {
        return mArraySize;
    }
    TLayoutBlockStorage blockStorage() const
    {
        return mBlockStorage;
    }
    TLayoutMatrixPacking matrixPacking() const
    {
        return mMatrixPacking;
    }
    const TString &mangledName() const
    {
        if (mMangledName.empty())
            mMangledName = buildMangledName("iblock-");
        return mMangledName;
    }

  private:
    const TString *mInstanceName; // for interface block instance names
    int mArraySize; // 0 if not an array
    TLayoutBlockStorage mBlockStorage;
    TLayoutMatrixPacking mMatrixPacking;
};

//
// Base class for things that have a type.
//
class TType
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TType()
        : type(EbtVoid), precision(EbpUndefined), qualifier(EvqGlobal), invariant(false),
          layoutQualifier(TLayoutQualifier::create()),
          primarySize(0), secondarySize(0), array(false), arraySize(0),
          interfaceBlock(nullptr), structure(nullptr)
    {
    }
    explicit TType(TBasicType t, unsigned char ps = 1, unsigned char ss = 1)
        : type(t),
          precision(EbpUndefined),
          qualifier(EvqGlobal),
          invariant(false),
          layoutQualifier(TLayoutQualifier::create()),
          primarySize(ps),
          secondarySize(ss),
          array(false),
          arraySize(0),
          interfaceBlock(0),
          structure(0)
    {
    }
    TType(TBasicType t, TPrecision p, TQualifier q = EvqTemporary,
          unsigned char ps = 1, unsigned char ss = 1, bool a = false)
        : type(t), precision(p), qualifier(q), invariant(false),
          layoutQualifier(TLayoutQualifier::create()),
          primarySize(ps), secondarySize(ss), array(a), arraySize(0),
          interfaceBlock(0), structure(0)
    {
    }
    explicit TType(const TPublicType &p);
    explicit TType(TStructure *userDef, TPrecision p = EbpUndefined)
        : type(EbtStruct),
          precision(p),
          qualifier(EvqTemporary),
          invariant(false),
          layoutQualifier(TLayoutQualifier::create()),
          primarySize(1),
          secondarySize(1),
          array(false),
          arraySize(0),
          interfaceBlock(0),
          structure(userDef)
    {
    }
    TType(TInterfaceBlock *interfaceBlockIn, TQualifier qualifierIn,
          TLayoutQualifier layoutQualifierIn, int arraySizeIn)
        : type(EbtInterfaceBlock), precision(EbpUndefined), qualifier(qualifierIn),
          invariant(false), layoutQualifier(layoutQualifierIn),
          primarySize(1), secondarySize(1), array(arraySizeIn > 0), arraySize(arraySizeIn),
          interfaceBlock(interfaceBlockIn), structure(0)
    {
    }

    TType(const TType &) = default;
    TType &operator=(const TType &) = default;

    TBasicType getBasicType() const
    {
        return type;
    }
    void setBasicType(TBasicType t)
    {
        if (type != t)
        {
            type = t;
            invalidateMangledName();
        }
    }

    TPrecision getPrecision() const
    {
        return precision;
    }
    void setPrecision(TPrecision p)
    {
        precision = p;
    }

    TQualifier getQualifier() const
    {
        return qualifier;
    }
    void setQualifier(TQualifier q)
    {
        qualifier = q;
    }

    bool isInvariant() const
    {
        return invariant;
    }

    void setInvariant(bool i) { invariant = i; }

    TLayoutQualifier getLayoutQualifier() const
    {
        return layoutQualifier;
    }
    void setLayoutQualifier(TLayoutQualifier lq)
    {
        layoutQualifier = lq;
    }

    int getNominalSize() const
    {
        return primarySize;
    }
    int getSecondarySize() const
    {
        return secondarySize;
    }
    int getCols() const
    {
        ASSERT(isMatrix());
        return primarySize;
    }
    int getRows() const
    {
        ASSERT(isMatrix());
        return secondarySize;
    }
    void setPrimarySize(unsigned char ps)
    {
        if (primarySize != ps)
        {
            primarySize = ps;
            invalidateMangledName();
        }
    }
    void setSecondarySize(unsigned char ss)
    {
        if (secondarySize != ss)
        {
            secondarySize = ss;
            invalidateMangledName();
        }
    }

    // Full size of single instance of type
    size_t getObjectSize() const;

    bool isMatrix() const
    {
        return primarySize > 1 && secondarySize > 1;
    }
    bool isNonSquareMatrix() const
    {
        return isMatrix() && primarySize != secondarySize;
    }
    bool isArray() const
    {
        return array;
    }
    bool isUnsizedArray() const
    {
        return array && arraySize == 0u;
    }
    unsigned int getArraySize() const { return arraySize; }
    void setArraySize(unsigned int s)
    {
        if (!array || arraySize != s)
        {
            array     = true;
            arraySize = s;
            invalidateMangledName();
        }
    }
    void clearArrayness()
    {
        if (array)
        {
            array     = false;
            arraySize = 0u;
            invalidateMangledName();
        }
    }

    TInterfaceBlock *getInterfaceBlock() const
    {
        return interfaceBlock;
    }
    void setInterfaceBlock(TInterfaceBlock *interfaceBlockIn)
    {
        if (interfaceBlock != interfaceBlockIn)
        {
            interfaceBlock = interfaceBlockIn;
            invalidateMangledName();
        }
    }
    bool isInterfaceBlock() const
    {
        return type == EbtInterfaceBlock;
    }

    bool isVector() const
    {
        return primarySize > 1 && secondarySize == 1;
    }
    bool isScalar() const
    {
        return primarySize == 1 && secondarySize == 1 && !structure;
    }
    bool isScalarInt() const
    {
        return isScalar() && (type == EbtInt || type == EbtUInt);
    }

    TStructure *getStruct() const
    {
        return structure;
    }
    void setStruct(TStructure *s)
    {
        if (structure != s)
        {
            structure = s;
            invalidateMangledName();
        }
    }

    const TString &getMangledName() const
    {
        if (mangled.empty())
        {
            mangled = buildMangledName();
            mangled += ';';
        }

        return mangled;
    }

    bool sameElementType(const TType &right) const
    {
        return type == right.type &&
            primarySize == right.primarySize &&
            secondarySize == right.secondarySize &&
            structure == right.structure;
    }
    bool operator==(const TType &right) const
    {
        return type == right.type &&
            primarySize == right.primarySize &&
            secondarySize == right.secondarySize &&
            array == right.array && (!array || arraySize == right.arraySize) &&
            structure == right.structure;
        // don't check the qualifier, it's not ever what's being sought after
    }
    bool operator!=(const TType &right) const
    {
        return !operator==(right);
    }
    bool operator<(const TType &right) const
    {
        if (type != right.type)
            return type < right.type;
        if (primarySize != right.primarySize)
            return primarySize < right.primarySize;
        if (secondarySize != right.secondarySize)
            return secondarySize < right.secondarySize;
        if (array != right.array)
            return array < right.array;
        if (arraySize != right.arraySize)
            return arraySize < right.arraySize;
        if (structure != right.structure)
            return structure < right.structure;

        return false;
    }

    const char *getBasicString() const
    {
        return ::getBasicString(type);
    }

    const char *getPrecisionString() const
    {
        return ::getPrecisionString(precision);
    }
    const char *getQualifierString() const
    {
        return ::getQualifierString(qualifier);
    }

    const char *getBuiltInTypeNameString() const;

    TString getCompleteString() const;

    // If this type is a struct, returns the deepest struct nesting of
    // any field in the struct. For example:
    //   struct nesting1 {
    //     vec4 position;
    //   };
    //   struct nesting2 {
    //     nesting1 field1;
    //     vec4 field2;
    //   };
    // For type "nesting2", this method would return 2 -- the number
    // of structures through which indirection must occur to reach the
    // deepest field (nesting2.field1.position).
    int getDeepestStructNesting() const
    {
        return structure ? structure->deepestNesting() : 0;
    }

    bool isStructureContainingArrays() const
    {
        return structure ? structure->containsArrays() : false;
    }

    bool isStructureContainingType(TBasicType t) const
    {
        return structure ? structure->containsType(t) : false;
    }

    bool isStructureContainingSamplers() const
    {
        return structure ? structure->containsSamplers() : false;
    }

    void createSamplerSymbols(const TString &structName,
                              const TString &structAPIName,
                              const unsigned int arrayOfStructsSize,
                              TVector<TIntermSymbol *> *outputSymbols,
                              TMap<TIntermSymbol *, TString> *outputSymbolsToAPINames) const
    {
        ASSERT(structure != nullptr && structure->containsSamplers());
        structure->createSamplerSymbols(structName, structAPIName, arrayOfStructsSize,
                                        outputSymbols, outputSymbolsToAPINames);
    }

    // Initializes all lazily-initialized members.
    void realize()
    {
        getMangledName();
    }

  private:
    void invalidateMangledName() { mangled = ""; }
    TString buildMangledName() const;
    size_t getStructSize() const;

    TBasicType type;
    TPrecision precision;
    TQualifier qualifier;
    bool invariant;
    TLayoutQualifier layoutQualifier;
    unsigned char primarySize; // size of vector or cols matrix
    unsigned char secondarySize; // rows of a matrix
    bool array;
    unsigned int arraySize;

    // 0 unless this is an interface block, or interface block member variable
    TInterfaceBlock *interfaceBlock;

    // 0 unless this is a struct
    TStructure *structure;

    mutable TString mangled;
};

// TTypeSpecifierNonArray stores all of the necessary fields for type_specifier_nonarray from the
// grammar
struct TTypeSpecifierNonArray
{
    TBasicType type;
    unsigned char primarySize;          // size of vector or cols of matrix
    unsigned char secondarySize;        // rows of matrix
    TType *userDef;
    TSourceLoc line;

    // true if the type was defined by a struct specifier rather than a reference to a type name.
    bool isStructSpecifier;

    void initialize(TBasicType bt, const TSourceLoc &ln)
    {
        type = bt;
        primarySize = 1;
        secondarySize = 1;
        userDef           = nullptr;
        line = ln;
        isStructSpecifier = false;
    }

    void setAggregate(unsigned char size)
    {
        primarySize = size;
    }

    void setMatrix(unsigned char columns, unsigned char rows)
    {
        ASSERT(columns > 1 && rows > 1 && columns <= 4 && rows <= 4);
        primarySize   = columns;
        secondarySize = rows;
    }

    bool isMatrix() const { return primarySize > 1 && secondarySize > 1; }

    bool isVector() const { return primarySize > 1 && secondarySize == 1; }
};

//
// This is a workaround for a problem with the yacc stack,  It can't have
// types that it thinks have non-trivial constructors.  It should
// just be used while recognizing the grammar, not anything else.  Pointers
// could be used, but also trying to avoid lots of memory management overhead.
//
// Not as bad as it looks, there is no actual assumption that the fields
// match up or are name the same or anything like that.
//
struct TPublicType
{
    TTypeSpecifierNonArray typeSpecifierNonArray;
    TLayoutQualifier layoutQualifier;
    TQualifier qualifier;
    bool invariant;
    TPrecision precision;
    bool array;
    int arraySize;

    void initialize(const TTypeSpecifierNonArray &typeSpecifier, TQualifier q)
    {
        typeSpecifierNonArray = typeSpecifier;
        layoutQualifier       = TLayoutQualifier::create();
        qualifier             = q;
        invariant             = false;
        precision             = EbpUndefined;
        array                 = false;
        arraySize             = 0;
    }

    TBasicType getBasicType() const { return typeSpecifierNonArray.type; }
    void setBasicType(TBasicType basicType) { typeSpecifierNonArray.type = basicType; }

    unsigned char getPrimarySize() const { return typeSpecifierNonArray.primarySize; }
    unsigned char getSecondarySize() const { return typeSpecifierNonArray.secondarySize; }
    void initializeSizeForScalarTypes()
    {
        typeSpecifierNonArray.primarySize   = 1;
        typeSpecifierNonArray.secondarySize = 1;
    }

    const TType *getUserDef() const { return typeSpecifierNonArray.userDef; }
    const TSourceLoc &getLine() const { return typeSpecifierNonArray.line; }

    bool isStructSpecifier() const { return typeSpecifierNonArray.isStructSpecifier; }

    bool isStructureContainingArrays() const
    {
        if (!typeSpecifierNonArray.userDef)
        {
            return false;
        }

        return typeSpecifierNonArray.userDef->isStructureContainingArrays();
    }

    bool isStructureContainingType(TBasicType t) const
    {
        if (!typeSpecifierNonArray.userDef)
        {
            return false;
        }

        return typeSpecifierNonArray.userDef->isStructureContainingType(t);
    }

    bool isUnsizedArray() const { return array && arraySize == 0; }
    void setArraySize(int s)
    {
        array     = true;
        arraySize = s;
    }
    void clearArrayness()
    {
        array     = false;
        arraySize = 0;
    }

    bool isAggregate() const
    {
        return array || typeSpecifierNonArray.isMatrix() || typeSpecifierNonArray.isVector();
    }
};

#endif // COMPILER_TRANSLATOR_TYPES_H_

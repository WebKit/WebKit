//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_OUTPUTGLSLBASE_H_
#define COMPILER_TRANSLATOR_OUTPUTGLSLBASE_H_

#include <set>

#include "compiler/translator/HashNames.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermTraverse.h"

namespace sh
{

class TOutputGLSLBase : public TIntermTraverser
{
  public:
    TOutputGLSLBase(TInfoSinkBase &objSink,
                    ShArrayIndexClampingStrategy clampingStrategy,
                    ShHashFunction64 hashFunction,
                    NameMap &nameMap,
                    TSymbolTable *symbolTable,
                    sh::GLenum shaderType,
                    int shaderVersion,
                    ShShaderOutput output,
                    ShCompileOptions compileOptions);

    ShShaderOutput getShaderOutput() const { return mOutput; }

    // Return the original name if hash function pointer is NULL;
    // otherwise return the hashed name. Has special handling for internal names, which are not
    // hashed.
    TString hashName(const TName &name);

  protected:
    TInfoSinkBase &objSink() { return mObjSink; }
    void writeFloat(TInfoSinkBase &out, float f);
    void writeTriplet(Visit visit, const char *preStr, const char *inStr, const char *postStr);
    virtual void writeLayoutQualifier(TIntermTyped *variable);
    void writeInvariantQualifier(const TType &type);
    void writeVariableType(const TType &type);
    virtual bool writeVariablePrecision(TPrecision precision) = 0;
    void writeFunctionParameters(const TIntermSequence &args);
    const TConstantUnion *writeConstantUnion(const TType &type, const TConstantUnion *pConstUnion);
    void writeConstructorTriplet(Visit visit, const TType &type);
    TString getTypeName(const TType &type);

    void visitSymbol(TIntermSymbol *node) override;
    void visitConstantUnion(TIntermConstantUnion *node) override;
    bool visitSwizzle(Visit visit, TIntermSwizzle *node) override;
    bool visitBinary(Visit visit, TIntermBinary *node) override;
    bool visitUnary(Visit visit, TIntermUnary *node) override;
    bool visitTernary(Visit visit, TIntermTernary *node) override;
    bool visitIfElse(Visit visit, TIntermIfElse *node) override;
    bool visitSwitch(Visit visit, TIntermSwitch *node) override;
    bool visitCase(Visit visit, TIntermCase *node) override;
    bool visitFunctionPrototype(Visit visit, TIntermFunctionPrototype *node) override;
    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitBlock(Visit visit, TIntermBlock *node) override;
    bool visitInvariantDeclaration(Visit visit, TIntermInvariantDeclaration *node) override;
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;
    bool visitLoop(Visit visit, TIntermLoop *node) override;
    bool visitBranch(Visit visit, TIntermBranch *node) override;

    void visitCodeBlock(TIntermBlock *node);

    // Same as hashName(), but without hashing built-in variables.
    TString hashVariableName(const TName &name);
    // Same as hashName(), but without hashing internal functions or "main".
    TString hashFunctionNameIfNeeded(const TFunctionSymbolInfo &info);
    // Used to translate function names for differences between ESSL and GLSL
    virtual TString translateTextureFunction(const TString &name) { return name; }

  private:
    bool structDeclared(const TStructure *structure) const;
    void declareStruct(const TStructure *structure);

    void declareInterfaceBlockLayout(const TInterfaceBlock *interfaceBlock);
    void declareInterfaceBlock(const TInterfaceBlock *interfaceBlock);

    void writeBuiltInFunctionTriplet(Visit visit, TOperator op, bool useEmulatedFunction);

    const char *mapQualifierToString(TQualifier qialifier);

    TInfoSinkBase &mObjSink;
    bool mDeclaringVariable;

    // This set contains all the ids of the structs from every scope.
    std::set<int> mDeclaredStructs;

    ShArrayIndexClampingStrategy mClampingStrategy;

    // name hashing.
    ShHashFunction64 mHashFunction;

    NameMap &mNameMap;

    sh::GLenum mShaderType;

    const int mShaderVersion;

    ShShaderOutput mOutput;

    ShCompileOptions mCompileOptions;
};

void WriteGeometryShaderLayoutQualifiers(TInfoSinkBase &out,
                                         sh::TLayoutPrimitiveType inputPrimitive,
                                         int invocations,
                                         sh::TLayoutPrimitiveType outputPrimitive,
                                         int maxVertices);

bool NeedsToWriteLayoutQualifier(const TType &type);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_OUTPUTGLSLBASE_H_

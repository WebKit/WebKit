//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_OUTPUTHLSL_H_
#define COMPILER_OUTPUTHLSL_H_

#include <list>
#include <set>

#include "compiler/intermediate.h"
#include "compiler/ParseHelper.h"

namespace sh
{
class UnfoldSelect;

class OutputHLSL : public TIntermTraverser
{
  public:
    explicit OutputHLSL(TParseContext &context);
    ~OutputHLSL();

    void output();

    TInfoSinkBase &getBodyStream();

    TString typeString(const TType &type);
    static TString qualifierString(TQualifier qualifier);
    static TString arrayString(const TType &type);
    static TString initializer(const TType &type);
    static TString decorate(const TString &string);                      // Prepends an underscore to avoid naming clashes
    static TString decorateUniform(const TString &string, const TType &type);

  protected:
    void header();

    // Visit AST nodes and output their code to the body stream
    void visitSymbol(TIntermSymbol*);
    void visitConstantUnion(TIntermConstantUnion*);
    bool visitBinary(Visit visit, TIntermBinary*);
    bool visitUnary(Visit visit, TIntermUnary*);
    bool visitSelection(Visit visit, TIntermSelection*);
    bool visitAggregate(Visit visit, TIntermAggregate*);
    bool visitLoop(Visit visit, TIntermLoop*);
    bool visitBranch(Visit visit, TIntermBranch*);

    bool isSingleStatement(TIntermNode *node);
    bool handleExcessiveLoop(TIntermLoop *node);
    void outputTriplet(Visit visit, const TString &preString, const TString &inString, const TString &postString);
    void outputLineDirective(int line);
    TString argumentString(const TIntermSymbol *symbol);
    int vectorSize(const TType &type) const;

    void addConstructor(const TType &type, const TString &name, const TIntermSequence *parameters);
    const ConstantUnion *writeConstantUnion(const TType &type, const ConstantUnion *constUnion);

    TString scopeString(unsigned int depthLimit);
    TString scopedStruct(const TString &typeName);
    TString structLookup(const TString &typeName);

    TParseContext &mContext;
    UnfoldSelect *mUnfoldSelect;
    bool mInsideFunction;

    // Output streams
    TInfoSinkBase mHeader;
    TInfoSinkBase mBody;
    TInfoSinkBase mFooter;

    std::set<std::string> mReferencedUniforms;
    std::set<std::string> mReferencedAttributes;
    std::set<std::string> mReferencedVaryings;

    // Parameters determining what goes in the header output
    bool mUsesTexture2D;
    bool mUsesTexture2D_bias;
    bool mUsesTexture2DLod;
    bool mUsesTexture2DProj;
    bool mUsesTexture2DProj_bias;
    bool mUsesTexture2DProjLod;
    bool mUsesTextureCube;
    bool mUsesTextureCube_bias;
    bool mUsesTextureCubeLod;
    bool mUsesDepthRange;
    bool mUsesFragCoord;
    bool mUsesPointCoord;
    bool mUsesFrontFacing;
    bool mUsesPointSize;
    bool mUsesXor;
    bool mUsesMod1;
    bool mUsesMod2v;
    bool mUsesMod2f;
    bool mUsesMod3v;
    bool mUsesMod3f;
    bool mUsesMod4v;
    bool mUsesMod4f;
    bool mUsesFaceforward1;
    bool mUsesFaceforward2;
    bool mUsesFaceforward3;
    bool mUsesFaceforward4;
    bool mUsesEqualMat2;
    bool mUsesEqualMat3;
    bool mUsesEqualMat4;
    bool mUsesEqualVec2;
    bool mUsesEqualVec3;
    bool mUsesEqualVec4;
    bool mUsesEqualIVec2;
    bool mUsesEqualIVec3;
    bool mUsesEqualIVec4;
    bool mUsesEqualBVec2;
    bool mUsesEqualBVec3;
    bool mUsesEqualBVec4;
    bool mUsesAtan2_1;
    bool mUsesAtan2_2;
    bool mUsesAtan2_3;
    bool mUsesAtan2_4;

    typedef std::set<TString> Constructors;
    Constructors mConstructors;

    typedef std::set<TString> StructNames;
    StructNames mStructNames;

    typedef std::list<TString> StructDeclarations;
    StructDeclarations mStructDeclarations;

    typedef std::vector<int> ScopeBracket;
    ScopeBracket mScopeBracket;
    unsigned int mScopeDepth;

    int mUniqueIndex;   // For creating unique names
};
}

#endif   // COMPILER_OUTPUTHLSL_H_

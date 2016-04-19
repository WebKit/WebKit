//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_OUTPUTHLSL_H_
#define COMPILER_TRANSLATOR_OUTPUTHLSL_H_

#include <list>
#include <set>
#include <map>
#include <stack>

#include "angle_gl.h"
#include "compiler/translator/ASTMetadataHLSL.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/ParseContext.h"

class BuiltInFunctionEmulator;

namespace sh
{
class UnfoldShortCircuit;
class StructureHLSL;
class UniformHLSL;

typedef std::map<TString, TIntermSymbol*> ReferencedSymbols;

class OutputHLSL : public TIntermTraverser
{
  public:
    OutputHLSL(sh::GLenum shaderType, int shaderVersion,
        const TExtensionBehavior &extensionBehavior,
        const char *sourcePath, ShShaderOutput outputType,
        int numRenderTargets, const std::vector<Uniform> &uniforms,
        int compileOptions);

    ~OutputHLSL();

    void output(TIntermNode *treeRoot, TInfoSinkBase &objSink);

    const std::map<std::string, unsigned int> &getInterfaceBlockRegisterMap() const;
    const std::map<std::string, unsigned int> &getUniformRegisterMap() const;

    static TString initializer(const TType &type);

    TInfoSinkBase &getInfoSink() { ASSERT(!mInfoSinkStack.empty()); return *mInfoSinkStack.top(); }

    static bool canWriteAsHLSLLiteral(TIntermTyped *expression);

  protected:
    void header(TInfoSinkBase &out, const BuiltInFunctionEmulator *builtInFunctionEmulator);

    // Visit AST nodes and output their code to the body stream
    void visitSymbol(TIntermSymbol*);
    void visitRaw(TIntermRaw*);
    void visitConstantUnion(TIntermConstantUnion*);
    bool visitBinary(Visit visit, TIntermBinary*);
    bool visitUnary(Visit visit, TIntermUnary*);
    bool visitSelection(Visit visit, TIntermSelection*);
    bool visitSwitch(Visit visit, TIntermSwitch *);
    bool visitCase(Visit visit, TIntermCase *);
    bool visitAggregate(Visit visit, TIntermAggregate*);
    bool visitLoop(Visit visit, TIntermLoop*);
    bool visitBranch(Visit visit, TIntermBranch*);

    bool isSingleStatement(TIntermNode *node);
    bool handleExcessiveLoop(TInfoSinkBase &out, TIntermLoop *node);

    // Emit one of three strings depending on traverse phase. Called with literal strings so using const char* instead of TString.
    void outputTriplet(TInfoSinkBase &out,
                       Visit visit,
                       const char *preString,
                       const char *inString,
                       const char *postString);
    void outputLineDirective(TInfoSinkBase &out, int line);
    TString argumentString(const TIntermSymbol *symbol);
    int vectorSize(const TType &type) const;

    // Emit constructor. Called with literal names so using const char* instead of TString.
    void outputConstructor(TInfoSinkBase &out,
                           Visit visit,
                           const TType &type,
                           const char *name,
                           const TIntermSequence *parameters);
    const TConstantUnion *writeConstantUnion(TInfoSinkBase &out,
                                             const TType &type,
                                             const TConstantUnion *constUnion);

    void outputEqual(Visit visit, const TType &type, TOperator op, TInfoSinkBase &out);

    void writeEmulatedFunctionTriplet(TInfoSinkBase &out, Visit visit, const char *preStr);
    void makeFlaggedStructMaps(const std::vector<TIntermTyped *> &flaggedStructs);

    // Returns true if it found a 'same symbol' initializer (initializer that references the variable it's initting)
    bool writeSameSymbolInitializer(TInfoSinkBase &out, TIntermSymbol *symbolNode, TIntermTyped *expression);
    // Returns true if variable initializer could be written using literal {} notation.
    bool writeConstantInitialization(TInfoSinkBase &out,
                                     TIntermSymbol *symbolNode,
                                     TIntermTyped *expression);

    void writeDeferredGlobalInitializers(TInfoSinkBase &out);
    void writeSelection(TInfoSinkBase &out, TIntermSelection *node);

    // Returns the function name
    TString addStructEqualityFunction(const TStructure &structure);
    TString addArrayEqualityFunction(const TType &type);
    TString addArrayAssignmentFunction(const TType &type);
    TString addArrayConstructIntoFunction(const TType &type);

    // Ensures if the type is a struct, the struct is defined
    void ensureStructDefined(const TType &type);

    sh::GLenum mShaderType;
    int mShaderVersion;
    const TExtensionBehavior &mExtensionBehavior;
    const char *mSourcePath;
    const ShShaderOutput mOutputType;
    int mCompileOptions;

    bool mInsideFunction;

    // Output streams
    TInfoSinkBase mHeader;
    TInfoSinkBase mBody;
    TInfoSinkBase mFooter;

    // A stack is useful when we want to traverse in the header, or in helper functions, but not always
    // write to the body. Instead use an InfoSink stack to keep our current state intact.
    // TODO (jmadill): Just passing an InfoSink in function parameters would be simpler.
    std::stack<TInfoSinkBase *> mInfoSinkStack;

    ReferencedSymbols mReferencedUniforms;
    ReferencedSymbols mReferencedInterfaceBlocks;
    ReferencedSymbols mReferencedAttributes;
    ReferencedSymbols mReferencedVaryings;
    ReferencedSymbols mReferencedOutputVariables;

    StructureHLSL *mStructureHLSL;
    UniformHLSL *mUniformHLSL;

    struct TextureFunction
    {
        enum Method
        {
            IMPLICIT,   // Mipmap LOD determined implicitly (standard lookup)
            BIAS,
            LOD,
            LOD0,
            LOD0BIAS,
            SIZE,   // textureSize()
            FETCH,
            GRAD
        };

        TBasicType sampler;
        int coords;
        bool proj;
        bool offset;
        Method method;

        TString name() const;

        bool operator<(const TextureFunction &rhs) const;
    };

    typedef std::set<TextureFunction> TextureFunctionSet;

    // Parameters determining what goes in the header output
    TextureFunctionSet mUsesTexture;
    bool mUsesFragColor;
    bool mUsesFragData;
    bool mUsesDepthRange;
    bool mUsesFragCoord;
    bool mUsesPointCoord;
    bool mUsesFrontFacing;
    bool mUsesPointSize;
    bool mUsesInstanceID;
    bool mUsesVertexID;
    bool mUsesFragDepth;
    bool mUsesXor;
    bool mUsesDiscardRewriting;
    bool mUsesNestedBreak;
    bool mRequiresIEEEStrictCompiling;


    int mNumRenderTargets;

    int mUniqueIndex;   // For creating unique names

    CallDAG mCallDag;
    MetadataList mASTMetadataList;
    ASTMetadataHLSL *mCurrentFunctionMetadata;
    bool mOutputLod0Function;
    bool mInsideDiscontinuousLoop;
    int mNestedLoopDepth;

    TIntermSymbol *mExcessiveLoopIndex;

    TString structInitializerString(int indent, const TStructure &structure, const TString &rhsStructName);

    std::map<TIntermTyped*, TString> mFlaggedStructMappedNames;
    std::map<TIntermTyped*, TString> mFlaggedStructOriginalNames;

    struct HelperFunction
    {
        TString functionName;
        TString functionDefinition;

        virtual ~HelperFunction() {}
    };

    // A list of all equality comparison functions. It's important to preserve the order at
    // which we add the functions, since nested structures call each other recursively, and
    // structure equality functions may need to call array equality functions and vice versa.
    // The ownership of the pointers is maintained by the type-specific arrays.
    std::vector<HelperFunction*> mEqualityFunctions;

    struct StructEqualityFunction : public HelperFunction
    {
        const TStructure *structure;
    };
    std::vector<StructEqualityFunction*> mStructEqualityFunctions;

    struct ArrayHelperFunction : public HelperFunction
    {
        TType type;
    };
    std::vector<ArrayHelperFunction*> mArrayEqualityFunctions;

    std::vector<ArrayHelperFunction> mArrayAssignmentFunctions;

    // The construct-into functions are functions that fill an N-element array passed as an out parameter
    // with the other N parameters of the function. This is used to work around that arrays can't be
    // return values in HLSL.
    std::vector<ArrayHelperFunction> mArrayConstructIntoFunctions;

  private:
    TString samplerNamePrefixFromStruct(TIntermTyped *node);
    bool ancestorEvaluatesToSamplerInStruct(Visit visit);
};

}

#endif   // COMPILER_TRANSLATOR_OUTPUTHLSL_H_

//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_COMPILER_H_
#define COMPILER_TRANSLATOR_COMPILER_H_

//
// Machine independent part of the compiler private objects
// sent as ShHandle to the driver.
//
// This should not be included by driver code.
//

#include <GLSLANG/ShaderVars.h>

#include "common/PackedEnums.h"
#include "compiler/translator/BuiltInFunctionEmulator.h"
#include "compiler/translator/CallDAG.h"
#include "compiler/translator/Diagnostics.h"
#include "compiler/translator/ExtensionBehavior.h"
#include "compiler/translator/HashNames.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/Pragma.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/ValidateAST.h"

namespace sh
{

class TCompiler;
class TParseContext;
#ifdef ANGLE_ENABLE_HLSL
class TranslatorHLSL;
#endif  // ANGLE_ENABLE_HLSL
#ifdef ANGLE_ENABLE_METAL
class TranslatorMetalDirect;
#endif  // ANGLE_ENABLE_METAL

using SpecConstUsageBits = angle::PackedEnumBitSet<vk::SpecConstUsage, uint32_t>;

//
// Helper function to check if the shader type is GLSL.
//
bool IsGLSL130OrNewer(ShShaderOutput output);
bool IsGLSL420OrNewer(ShShaderOutput output);
bool IsGLSL410OrOlder(ShShaderOutput output);

//
// Helper function to check if the invariant qualifier can be removed.
//
bool RemoveInvariant(sh::GLenum shaderType,
                     int shaderVersion,
                     ShShaderOutput outputType,
                     const ShCompileOptions &compileOptions);

//
// The base class used to back handles returned to the driver.
//
class TShHandleBase
{
  public:
    TShHandleBase();
    virtual ~TShHandleBase();
    virtual TCompiler *getAsCompiler() { return 0; }
#ifdef ANGLE_ENABLE_HLSL
    virtual TranslatorHLSL *getAsTranslatorHLSL() { return 0; }
#endif  // ANGLE_ENABLE_HLSL
#ifdef ANGLE_ENABLE_METAL
    virtual TranslatorMetalDirect *getAsTranslatorMetalDirect() { return nullptr; }
#endif  // ANGLE_ENABLE_METAL

  protected:
    // Memory allocator. Allocates and tracks memory required by the compiler.
    // Deallocates all memory when compiler is destructed.
    angle::PoolAllocator allocator;
};

struct TFunctionMetadata
{
    bool used = false;
};

//
// The base class for the machine dependent compiler to derive from
// for managing object code from the compile.
//
class TCompiler : public TShHandleBase
{
  public:
    TCompiler(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output);
    ~TCompiler() override;
    TCompiler *getAsCompiler() override { return this; }

    bool Init(const ShBuiltInResources &resources);

    // compileTreeForTesting should be used only when tests require access to
    // the AST. Users of this function need to manually manage the global pool
    // allocator. Returns nullptr whenever there are compilation errors.
    TIntermBlock *compileTreeForTesting(const char *const shaderStrings[],
                                        size_t numStrings,
                                        const ShCompileOptions &compileOptions);

    bool compile(const char *const shaderStrings[],
                 size_t numStrings,
                 const ShCompileOptions &compileOptions);

    // Get results of the last compilation.
    int getShaderVersion() const { return mShaderVersion; }
    TInfoSink &getInfoSink() { return mInfoSink; }

    bool specifyEarlyFragmentTests() { return mEarlyFragmentTestsSpecified = true; }
    bool isEarlyFragmentTestsSpecified() const { return mEarlyFragmentTestsSpecified; }
    bool hasDiscard() const { return mHasDiscard; }
    bool enablesPerSampleShading() const { return mEnablesPerSampleShading; }
    SpecConstUsageBits getSpecConstUsageBits() const { return mSpecConstUsageBits; }

    bool isComputeShaderLocalSizeDeclared() const { return mComputeShaderLocalSizeDeclared; }
    const sh::WorkGroupSize &getComputeShaderLocalSize() const { return mComputeShaderLocalSize; }
    int getNumViews() const { return mNumViews; }

    // Clears the results from the previous compilation.
    void clearResults();

    const std::vector<sh::ShaderVariable> &getAttributes() const { return mAttributes; }
    const std::vector<sh::ShaderVariable> &getOutputVariables() const { return mOutputVariables; }
    const std::vector<sh::ShaderVariable> &getUniforms() const { return mUniforms; }
    const std::vector<sh::ShaderVariable> &getInputVaryings() const { return mInputVaryings; }
    const std::vector<sh::ShaderVariable> &getOutputVaryings() const { return mOutputVaryings; }
    const std::vector<sh::InterfaceBlock> &getInterfaceBlocks() const { return mInterfaceBlocks; }
    const std::vector<sh::InterfaceBlock> &getUniformBlocks() const { return mUniformBlocks; }
    const std::vector<sh::InterfaceBlock> &getShaderStorageBlocks() const
    {
        return mShaderStorageBlocks;
    }

    ShHashFunction64 getHashFunction() const { return mResources.HashFunction; }
    NameMap &getNameMap() { return mNameMap; }
    TSymbolTable &getSymbolTable() { return mSymbolTable; }
    ShShaderSpec getShaderSpec() const { return mShaderSpec; }
    ShShaderOutput getOutputType() const { return mOutputType; }
    ShBuiltInResources getBuiltInResources() const { return mResources; }
    const std::string &getBuiltInResourcesString() const { return mBuiltInResourcesString; }

    bool isHighPrecisionSupported() const;

    bool shouldRunLoopAndIndexingValidation(const ShCompileOptions &compileOptions) const;
    bool shouldLimitTypeSizes() const;

    // Get the resources set by InitBuiltInSymbolTable
    const ShBuiltInResources &getResources() const;

    const TPragma &getPragma() const { return mPragma; }

    int getGeometryShaderMaxVertices() const { return mGeometryShaderMaxVertices; }
    int getGeometryShaderInvocations() const { return mGeometryShaderInvocations; }
    TLayoutPrimitiveType getGeometryShaderInputPrimitiveType() const
    {
        return mGeometryShaderInputPrimitiveType;
    }
    TLayoutPrimitiveType getGeometryShaderOutputPrimitiveType() const
    {
        return mGeometryShaderOutputPrimitiveType;
    }

    unsigned int getStructSize(const ShaderVariable &var) const;

    int getTessControlShaderOutputVertices() const { return mTessControlShaderOutputVertices; }
    TLayoutTessEvaluationType getTessEvaluationShaderInputPrimitiveType() const
    {
        return mTessEvaluationShaderInputPrimitiveType;
    }
    TLayoutTessEvaluationType getTessEvaluationShaderInputVertexSpacingType() const
    {
        return mTessEvaluationShaderInputVertexSpacingType;
    }
    TLayoutTessEvaluationType getTessEvaluationShaderInputOrderingType() const
    {
        return mTessEvaluationShaderInputOrderingType;
    }
    TLayoutTessEvaluationType getTessEvaluationShaderInputPointType() const
    {
        return mTessEvaluationShaderInputPointType;
    }

    bool hasAnyPreciseType() const { return mHasAnyPreciseType; }

    AdvancedBlendEquations getAdvancedBlendEquations() const { return mAdvancedBlendEquations; }

    bool hasPixelLocalStorageUniforms() const { return mHasPixelLocalStorageUniforms; }

    unsigned int getSharedMemorySize() const;

    sh::GLenum getShaderType() const { return mShaderType; }

    // Validate the AST and produce errors if it is inconsistent.
    bool validateAST(TIntermNode *root);
    // Some transformations may need to temporarily disable validation until they are complete.  A
    // set of disable/enable helpers are used for this purpose.
    bool disableValidateFunctionCall();
    void restoreValidateFunctionCall(bool enable);
    bool disableValidateVariableReferences();
    void restoreValidateVariableReferences(bool enable);
    // When the AST is post-processed (such as to determine precise-ness of intermediate nodes),
    // it's expected to no longer transform.
    void enableValidateNoMoreTransformations();

  protected:
    // Add emulated functions to the built-in function emulator.
    virtual void initBuiltInFunctionEmulator(BuiltInFunctionEmulator *emu,
                                             const ShCompileOptions &compileOptions)
    {}
    // Translate to object code. May generate performance warnings through the diagnostics.
    [[nodiscard]] virtual bool translate(TIntermBlock *root,
                                         const ShCompileOptions &compileOptions,
                                         PerformanceDiagnostics *perfDiagnostics) = 0;
    // Get built-in extensions with default behavior.
    const TExtensionBehavior &getExtensionBehavior() const;
    const char *getSourcePath() const;
    // Relies on collectVariables having been called.
    bool isVaryingDefined(const char *varyingName);

    const BuiltInFunctionEmulator &getBuiltInFunctionEmulator() const;

    virtual bool shouldFlattenPragmaStdglInvariantAll() = 0;
    virtual bool shouldCollectVariables(const ShCompileOptions &compileOptions);

    bool wereVariablesCollected() const;
    std::vector<sh::ShaderVariable> mAttributes;
    std::vector<sh::ShaderVariable> mOutputVariables;
    std::vector<sh::ShaderVariable> mUniforms;
    std::vector<sh::ShaderVariable> mInputVaryings;
    std::vector<sh::ShaderVariable> mOutputVaryings;
    std::vector<sh::ShaderVariable> mSharedVariables;
    std::vector<sh::InterfaceBlock> mInterfaceBlocks;
    std::vector<sh::InterfaceBlock> mUniformBlocks;
    std::vector<sh::InterfaceBlock> mShaderStorageBlocks;

    // Track what should be validated given passes currently applied.
    ValidateASTOptions mValidateASTOptions;

    // Specialization constant usage bits
    SpecConstUsageBits mSpecConstUsageBits;

  private:
    // Initialize symbol-table with built-in symbols.
    bool initBuiltInSymbolTable(const ShBuiltInResources &resources);
    // Compute the string representation of the built-in resources
    void setResourceString();
    // Return false if the call depth is exceeded.
    bool checkCallDepth();
    // Insert statements to reference all members in unused uniform blocks with standard and shared
    // layout. This is to work around a Mac driver that treats unused standard/shared
    // uniform blocks as inactive.
    [[nodiscard]] bool useAllMembersInUnusedStandardAndSharedBlocks(TIntermBlock *root);
    // Insert statements to initialize output variables in the beginning of main().
    // This is to avoid undefined behaviors.
    [[nodiscard]] bool initializeOutputVariables(TIntermBlock *root);
    // Insert gl_Position = vec4(0,0,0,0) to the beginning of main().
    // It is to work around a Linux driver bug where missing this causes compile failure
    // while spec says it is allowed.
    // This function should only be applied to vertex shaders.
    [[nodiscard]] bool initializeGLPosition(TIntermBlock *root);
    // Return true if the maximum expression complexity is below the limit.
    bool limitExpressionComplexity(TIntermBlock *root);
    // Creates the function call DAG for further analysis, returning false if there is a recursion
    bool initCallDag(TIntermNode *root);
    // Return false if "main" doesn't exist
    bool tagUsedFunctions();
    void internalTagUsedFunction(size_t index);

    void collectInterfaceBlocks();

    bool mVariablesCollected;

    bool mGLPositionInitialized;

    // Removes unused function declarations and prototypes from the AST
    bool pruneUnusedFunctions(TIntermBlock *root);

    TIntermBlock *compileTreeImpl(const char *const shaderStrings[],
                                  size_t numStrings,
                                  const ShCompileOptions &compileOptions);

    // Fetches and stores shader metadata that is not stored within the AST itself, such as shader
    // version.
    void setASTMetadata(const TParseContext &parseContext);

    // Check if shader version meets the requirement.
    bool checkShaderVersion(TParseContext *parseContext);

    // Does checks that need to be run after parsing is complete and returns true if they pass.
    bool checkAndSimplifyAST(TIntermBlock *root,
                             const TParseContext &parseContext,
                             const ShCompileOptions &compileOptions);

    bool postParseChecks(const TParseContext &parseContext);

    sh::GLenum mShaderType;
    ShShaderSpec mShaderSpec;
    ShShaderOutput mOutputType;

    CallDAG mCallDag;
    std::vector<TFunctionMetadata> mFunctionMetadata;

    ShBuiltInResources mResources;
    std::string mBuiltInResourcesString;

    // Built-in symbol table for the given language, spec, and resources.
    // It is preserved from compile-to-compile.
    TSymbolTable mSymbolTable;
    // Built-in extensions with default behavior.
    TExtensionBehavior mExtensionBehavior;

    BuiltInFunctionEmulator mBuiltInFunctionEmulator;

    // Results of compilation.
    int mShaderVersion;
    TInfoSink mInfoSink;  // Output sink.
    TDiagnostics mDiagnostics;
    const char *mSourcePath;  // Path of source file or NULL

    // Fragment shader early fragment tests
    bool mEarlyFragmentTestsSpecified;

    // Fragment shader has the discard instruction
    bool mHasDiscard;

    // Whether per-sample shading is enabled by the shader.  In OpenGL, this keyword should
    // implicitly trigger per-sample shading without the API enabling it.
    bool mEnablesPerSampleShading;

    // compute shader local group size
    bool mComputeShaderLocalSizeDeclared;
    sh::WorkGroupSize mComputeShaderLocalSize;

    // GL_OVR_multiview num_views.
    int mNumViews;

    // geometry shader parameters.
    int mGeometryShaderMaxVertices;
    int mGeometryShaderInvocations;
    TLayoutPrimitiveType mGeometryShaderInputPrimitiveType;
    TLayoutPrimitiveType mGeometryShaderOutputPrimitiveType;

    // tesssellation shader parameters
    int mTessControlShaderOutputVertices;
    TLayoutTessEvaluationType mTessEvaluationShaderInputPrimitiveType;
    TLayoutTessEvaluationType mTessEvaluationShaderInputVertexSpacingType;
    TLayoutTessEvaluationType mTessEvaluationShaderInputOrderingType;
    TLayoutTessEvaluationType mTessEvaluationShaderInputPointType;

    bool mHasAnyPreciseType;

    // advanced blend equation parameters
    AdvancedBlendEquations mAdvancedBlendEquations;

    // ANGLE_shader_pixel_local_storage.
    bool mHasPixelLocalStorageUniforms;

    // name hashing.
    NameMap mNameMap;

    TPragma mPragma;

    ShCompileOptions mCompileOptions;
};

//
// This is the interface between the machine independent code
// and the machine dependent code.
//
// The machine dependent code should derive from the classes
// above. Then Construct*() and Delete*() will create and
// destroy the machine dependent objects, which contain the
// above machine independent information.
//
TCompiler *ConstructCompiler(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output);
void DeleteCompiler(TCompiler *);

struct ShaderDumpHeader
{
    uint32_t type;
    uint32_t spec;
    uint32_t output;
    uint8_t basicCompileOptions[32];
    uint8_t metalCompileOptions[32];
    uint8_t plsCompileOptions[32];
    uint8_t padding[20];
};
static_assert(sizeof(ShaderDumpHeader) == 128);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_COMPILER_H_

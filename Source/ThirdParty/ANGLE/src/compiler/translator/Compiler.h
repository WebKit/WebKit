//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
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

#include "compiler/translator/BuiltInFunctionEmulator.h"
#include "compiler/translator/CallDAG.h"
#include "compiler/translator/ExtensionBehavior.h"
#include "compiler/translator/HashNames.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/Pragma.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/VariableInfo.h"
#include "third_party/compiler/ArrayBoundsClamper.h"

class TCompiler;
#ifdef ANGLE_ENABLE_HLSL
class TranslatorHLSL;
#endif // ANGLE_ENABLE_HLSL

//
// Helper function to identify specs that are based on the WebGL spec.
//
bool IsWebGLBasedSpec(ShShaderSpec spec);

//
// Helper function to check if the shader type is GLSL.
//
bool IsGLSL130OrNewer(ShShaderOutput output);

//
// The base class used to back handles returned to the driver.
//
class TShHandleBase {
public:
    TShHandleBase();
    virtual ~TShHandleBase();
    virtual TCompiler* getAsCompiler() { return 0; }
#ifdef ANGLE_ENABLE_HLSL
    virtual TranslatorHLSL* getAsTranslatorHLSL() { return 0; }
#endif // ANGLE_ENABLE_HLSL

protected:
    // Memory allocator. Allocates and tracks memory required by the compiler.
    // Deallocates all memory when compiler is destructed.
    TPoolAllocator allocator;
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

    bool Init(const ShBuiltInResources& resources);

    // compileTreeForTesting should be used only when tests require access to
    // the AST. Users of this function need to manually manage the global pool
    // allocator. Returns nullptr whenever there are compilation errors.
    TIntermBlock *compileTreeForTesting(const char *const shaderStrings[],
                                        size_t numStrings,
                                        ShCompileOptions compileOptions);

    bool compile(const char *const shaderStrings[],
                 size_t numStrings,
                 ShCompileOptions compileOptions);

    // Get results of the last compilation.
    int getShaderVersion() const { return shaderVersion; }
    TInfoSink& getInfoSink() { return infoSink; }

    bool isComputeShaderLocalSizeDeclared() const { return mComputeShaderLocalSizeDeclared; }
    const sh::WorkGroupSize &getComputeShaderLocalSize() { return mComputeShaderLocalSize; }

    // Clears the results from the previous compilation.
    void clearResults();

    const std::vector<sh::Attribute> &getAttributes() const { return attributes; }
    const std::vector<sh::OutputVariable> &getOutputVariables() const { return outputVariables; }
    const std::vector<sh::Uniform> &getUniforms() const { return uniforms; }
    const std::vector<sh::Varying> &getVaryings() const { return varyings; }
    const std::vector<sh::InterfaceBlock> &getInterfaceBlocks() const { return interfaceBlocks; }

    ShHashFunction64 getHashFunction() const { return hashFunction; }
    NameMap& getNameMap() { return nameMap; }
    TSymbolTable& getSymbolTable() { return symbolTable; }
    ShShaderSpec getShaderSpec() const { return shaderSpec; }
    ShShaderOutput getOutputType() const { return outputType; }
    const std::string &getBuiltInResourcesString() const { return builtInResourcesString; }

    bool shouldRunLoopAndIndexingValidation(ShCompileOptions compileOptions) const;

    // Get the resources set by InitBuiltInSymbolTable
    const ShBuiltInResources& getResources() const;

  protected:
    sh::GLenum getShaderType() const { return shaderType; }
    // Initialize symbol-table with built-in symbols.
    bool InitBuiltInSymbolTable(const ShBuiltInResources& resources);
    // Compute the string representation of the built-in resources
    void setResourceString();
    // Return false if the call depth is exceeded.
    bool checkCallDepth();
    // Returns true if a program has no conflicting or missing fragment outputs
    bool validateOutputs(TIntermNode* root);
    // Returns true if the given shader does not exceed the minimum
    // functionality mandated in GLSL 1.0 spec Appendix A.
    bool validateLimitations(TIntermNode* root);
    // Collect info for all attribs, uniforms, varyings.
    void collectVariables(TIntermNode* root);
    // Add emulated functions to the built-in function emulator.
    virtual void initBuiltInFunctionEmulator(BuiltInFunctionEmulator *emu,
                                             ShCompileOptions compileOptions){};
    // Translate to object code.
    virtual void translate(TIntermNode *root, ShCompileOptions compileOptions) = 0;
    // Returns true if, after applying the packing rules in the GLSL 1.017 spec
    // Appendix A, section 7, the shader does not use too many uniforms.
    bool enforcePackingRestrictions();
    // Insert statements to reference all members in unused uniform blocks with standard and shared
    // layout. This is to work around a Mac driver that treats unused standard/shared
    // uniform blocks as inactive.
    void useAllMembersInUnusedStandardAndSharedBlocks(TIntermNode *root);
    // Insert statements to initialize output variables in the beginning of main().
    // This is to avoid undefined behaviors.
    void initializeOutputVariables(TIntermNode *root);
    // Insert gl_Position = vec4(0,0,0,0) to the beginning of main().
    // It is to work around a Linux driver bug where missing this causes compile failure
    // while spec says it is allowed.
    // This function should only be applied to vertex shaders.
    void initializeGLPosition(TIntermNode* root);
    // Return true if the maximum expression complexity is below the limit.
    bool limitExpressionComplexity(TIntermNode* root);
    // Get built-in extensions with default behavior.
    const TExtensionBehavior& getExtensionBehavior() const;
    const char *getSourcePath() const;
    const TPragma& getPragma() const { return mPragma; }
    void writePragma(ShCompileOptions compileOptions);
    unsigned int *getTemporaryIndex() { return &mTemporaryIndex; }
    // Relies on collectVariables having been called.
    bool isVaryingDefined(const char *varyingName);

    const ArrayBoundsClamper& getArrayBoundsClamper() const;
    ShArrayIndexClampingStrategy getArrayIndexClampingStrategy() const;
    const BuiltInFunctionEmulator& getBuiltInFunctionEmulator() const;

    virtual bool shouldCollectVariables(ShCompileOptions compileOptions)
    {
        return (compileOptions & SH_VARIABLES) != 0;
    }

    virtual bool shouldFlattenPragmaStdglInvariantAll() = 0;

    std::vector<sh::Attribute> attributes;
    std::vector<sh::OutputVariable> outputVariables;
    std::vector<sh::Uniform> uniforms;
    std::vector<sh::ShaderVariable> expandedUniforms;
    std::vector<sh::Varying> varyings;
    std::vector<sh::InterfaceBlock> interfaceBlocks;
    bool variablesCollected;

  private:
    // Creates the function call DAG for further analysis, returning false if there is a recursion
    bool initCallDag(TIntermNode *root);
    // Return false if "main" doesn't exist
    bool tagUsedFunctions();
    void internalTagUsedFunction(size_t index);

    void initSamplerDefaultPrecision(TBasicType samplerType);

    // Removes unused function declarations and prototypes from the AST
    class UnusedPredicate;
    bool pruneUnusedFunctions(TIntermBlock *root);

    TIntermBlock *compileTreeImpl(const char *const shaderStrings[],
                                  size_t numStrings,
                                  const ShCompileOptions compileOptions);

    sh::GLenum shaderType;
    ShShaderSpec shaderSpec;
    ShShaderOutput outputType;

    struct FunctionMetadata
    {
        FunctionMetadata()
            : used(false)
        {
        }
        bool used;
    };

    CallDAG mCallDag;
    std::vector<FunctionMetadata> functionMetadata;

    int maxUniformVectors;
    int maxExpressionComplexity;
    int maxCallStackDepth;
    int maxFunctionParameters;

    ShBuiltInResources compileResources;
    std::string builtInResourcesString;

    // Built-in symbol table for the given language, spec, and resources.
    // It is preserved from compile-to-compile.
    TSymbolTable symbolTable;
    // Built-in extensions with default behavior.
    TExtensionBehavior extensionBehavior;
    bool fragmentPrecisionHigh;

    ArrayBoundsClamper arrayBoundsClamper;
    ShArrayIndexClampingStrategy clampingStrategy;
    BuiltInFunctionEmulator builtInFunctionEmulator;

    // Results of compilation.
    int shaderVersion;
    TInfoSink infoSink;  // Output sink.
    const char *mSourcePath; // Path of source file or NULL

    // compute shader local group size
    bool mComputeShaderLocalSizeDeclared;
    sh::WorkGroupSize mComputeShaderLocalSize;

    // name hashing.
    ShHashFunction64 hashFunction;
    NameMap nameMap;

    TPragma mPragma;

    unsigned int mTemporaryIndex;
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
TCompiler* ConstructCompiler(
    sh::GLenum type, ShShaderSpec spec, ShShaderOutput output);
void DeleteCompiler(TCompiler*);

#endif // COMPILER_TRANSLATOR_COMPILER_H_

//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/BuiltInFunctionEmulator.h"
#include "compiler/translator/DetectCallDepth.h"
#include "compiler/translator/ForLoopUnroll.h"
#include "compiler/translator/Initialize.h"
#include "compiler/translator/InitializeParseContext.h"
#include "compiler/translator/InitializeVariables.h"
#include "compiler/translator/ParseContext.h"
#include "compiler/translator/RenameFunction.h"
#include "compiler/translator/ShHandle.h"
#include "compiler/translator/UnfoldShortCircuitAST.h"
#include "compiler/translator/ValidateLimitations.h"
#include "compiler/translator/ValidateOutputs.h"
#include "compiler/translator/VariablePacker.h"
#include "compiler/translator/depgraph/DependencyGraph.h"
#include "compiler/translator/depgraph/DependencyGraphOutput.h"
#include "compiler/translator/timing/RestrictFragmentShaderTiming.h"
#include "compiler/translator/timing/RestrictVertexShaderTiming.h"
#include "third_party/compiler/ArrayBoundsClamper.h"

bool IsWebGLBasedSpec(ShShaderSpec spec)
{
     return spec == SH_WEBGL_SPEC || spec == SH_CSS_SHADERS_SPEC;
}

size_t GetGlobalMaxTokenSize(ShShaderSpec spec)
{
    // WebGL defines a max token legnth of 256, while ES2 leaves max token
    // size undefined. ES3 defines a max size of 1024 characters.
    if (IsWebGLBasedSpec(spec))
    {
        return 256;
    }
    else
    {
        return 1024;
    }
}

namespace {
class TScopedPoolAllocator
{
  public:
    TScopedPoolAllocator(TPoolAllocator* allocator) : mAllocator(allocator)
    {
        mAllocator->push();
        SetGlobalPoolAllocator(mAllocator);
    }
    ~TScopedPoolAllocator()
    {
        SetGlobalPoolAllocator(NULL);
        mAllocator->pop();
    }

  private:
    TPoolAllocator* mAllocator;
};

class TScopedSymbolTableLevel
{
  public:
    TScopedSymbolTableLevel(TSymbolTable* table) : mTable(table)
    {
        ASSERT(mTable->atBuiltInLevel());
        mTable->push();
    }
    ~TScopedSymbolTableLevel()
    {
        while (!mTable->atBuiltInLevel())
            mTable->pop();
    }

  private:
    TSymbolTable* mTable;
};
}  // namespace

TShHandleBase::TShHandleBase()
{
    allocator.push();
    SetGlobalPoolAllocator(&allocator);
}

TShHandleBase::~TShHandleBase()
{
    SetGlobalPoolAllocator(NULL);
    allocator.popAll();
}

TCompiler::TCompiler(ShShaderType type, ShShaderSpec spec)
    : shaderType(type),
      shaderSpec(spec),
      maxUniformVectors(0),
      maxVaryingVectors(0),
      maxExpressionComplexity(0),
      maxCallStackDepth(0),
      fragmentPrecisionHigh(false),
      clampingStrategy(SH_CLAMP_WITH_CLAMP_INTRINSIC),
      builtInFunctionEmulator(type)
{
}

TCompiler::~TCompiler()
{
}

bool TCompiler::Init(const ShBuiltInResources& resources)
{
    shaderVersion = 100;
    maxUniformVectors = (shaderType == SH_VERTEX_SHADER) ?
        resources.MaxVertexUniformVectors :
        resources.MaxFragmentUniformVectors;
    maxVaryingVectors = resources.MaxVaryingVectors;
    maxExpressionComplexity = resources.MaxExpressionComplexity;
    maxCallStackDepth = resources.MaxCallStackDepth;

    SetGlobalPoolAllocator(&allocator);

    // Generate built-in symbol table.
    if (!InitBuiltInSymbolTable(resources))
        return false;
    InitExtensionBehavior(resources, extensionBehavior);
    fragmentPrecisionHigh = resources.FragmentPrecisionHigh == 1;

    arrayBoundsClamper.SetClampingStrategy(resources.ArrayIndexClampingStrategy);
    clampingStrategy = resources.ArrayIndexClampingStrategy;

    hashFunction = resources.HashFunction;

    return true;
}

bool TCompiler::compile(const char* const shaderStrings[],
                        size_t numStrings,
                        int compileOptions)
{
    TScopedPoolAllocator scopedAlloc(&allocator);
    clearResults();

    if (numStrings == 0)
        return true;

    // If compiling for WebGL, validate loop and indexing as well.
    if (IsWebGLBasedSpec(shaderSpec))
        compileOptions |= SH_VALIDATE_LOOP_INDEXING;

    // First string is path of source file if flag is set. The actual source follows.
    const char* sourcePath = NULL;
    size_t firstSource = 0;
    if (compileOptions & SH_SOURCE_PATH)
    {
        sourcePath = shaderStrings[0];
        ++firstSource;
    }

    TIntermediate intermediate(infoSink);
    TParseContext parseContext(symbolTable, extensionBehavior, intermediate,
                               shaderType, shaderSpec, compileOptions, true,
                               sourcePath, infoSink);
    parseContext.fragmentPrecisionHigh = fragmentPrecisionHigh;
    SetGlobalParseContext(&parseContext);

    // We preserve symbols at the built-in level from compile-to-compile.
    // Start pushing the user-defined symbols at global level.
    TScopedSymbolTableLevel scopedSymbolLevel(&symbolTable);

    // Parse shader.
    bool success =
        (PaParseStrings(numStrings - firstSource, &shaderStrings[firstSource], NULL, &parseContext) == 0) &&
        (parseContext.treeRoot != NULL);

    shaderVersion = parseContext.getShaderVersion();

    if (success)
    {
        TIntermNode* root = parseContext.treeRoot;
        success = intermediate.postProcess(root);

        // Disallow expressions deemed too complex.
        if (success && (compileOptions & SH_LIMIT_EXPRESSION_COMPLEXITY))
            success = limitExpressionComplexity(root);

        if (success)
            success = detectCallDepth(root, infoSink, (compileOptions & SH_LIMIT_CALL_STACK_DEPTH) != 0);

        if (success && shaderVersion == 300 && shaderType == SH_FRAGMENT_SHADER)
            success = validateOutputs(root);

        if (success && (compileOptions & SH_VALIDATE_LOOP_INDEXING))
            success = validateLimitations(root);

        if (success && (compileOptions & SH_TIMING_RESTRICTIONS))
            success = enforceTimingRestrictions(root, (compileOptions & SH_DEPENDENCY_GRAPH) != 0);

        if (success && shaderSpec == SH_CSS_SHADERS_SPEC)
            rewriteCSSShader(root);

        // Unroll for-loop markup needs to happen after validateLimitations pass.
        if (success && (compileOptions & SH_UNROLL_FOR_LOOP_WITH_INTEGER_INDEX))
        {
            ForLoopUnrollMarker marker(ForLoopUnrollMarker::kIntegerIndex);
            root->traverse(&marker);
        }
        if (success && (compileOptions & SH_UNROLL_FOR_LOOP_WITH_SAMPLER_ARRAY_INDEX))
        {
            ForLoopUnrollMarker marker(ForLoopUnrollMarker::kSamplerArrayIndex);
            root->traverse(&marker);
            if (marker.samplerArrayIndexIsFloatLoopIndex())
            {
                infoSink.info.prefix(EPrefixError);
                infoSink.info << "sampler array index is float loop index";
                success = false;
            }
        }

        // Built-in function emulation needs to happen after validateLimitations pass.
        if (success && (compileOptions & SH_EMULATE_BUILT_IN_FUNCTIONS))
            builtInFunctionEmulator.MarkBuiltInFunctionsForEmulation(root);

        // Clamping uniform array bounds needs to happen after validateLimitations pass.
        if (success && (compileOptions & SH_CLAMP_INDIRECT_ARRAY_BOUNDS))
            arrayBoundsClamper.MarkIndirectArrayBoundsForClamping(root);

        if (success && shaderType == SH_VERTEX_SHADER && (compileOptions & SH_INIT_GL_POSITION))
            initializeGLPosition(root);

        if (success && (compileOptions & SH_UNFOLD_SHORT_CIRCUIT))
        {
            UnfoldShortCircuitAST unfoldShortCircuit;
            root->traverse(&unfoldShortCircuit);
            unfoldShortCircuit.updateTree();
        }

        if (success && (compileOptions & SH_VARIABLES))
        {
            collectVariables(root);
            if (compileOptions & SH_ENFORCE_PACKING_RESTRICTIONS) {
                success = enforcePackingRestrictions();
                if (!success) {
                    infoSink.info.prefix(EPrefixError);
                    infoSink.info << "too many uniforms";
                    return false;
                }
            }

            if (success && shaderType == SH_VERTEX_SHADER &&
                (compileOptions & SH_INIT_VARYINGS_WITHOUT_STATIC_USE))
                initializeVaryingsWithoutStaticUse(root);
        }

        if (success && (compileOptions & SH_INTERMEDIATE_TREE))
            intermediate.outputTree(root);

        if (success && (compileOptions & SH_OBJECT_CODE))
            translate(root);
    }

    // Cleanup memory.
    intermediate.remove(parseContext.treeRoot);
    SetGlobalParseContext(NULL);
    return success;
}

bool TCompiler::InitBuiltInSymbolTable(const ShBuiltInResources &resources)
{
    compileResources = resources;

    assert(symbolTable.isEmpty());
    symbolTable.push();   // COMMON_BUILTINS
    symbolTable.push();   // ESSL1_BUILTINS
    symbolTable.push();   // ESSL3_BUILTINS

    TPublicType integer;
    integer.type = EbtInt;
    integer.primarySize = 1;
    integer.secondarySize = 1;
    integer.array = false;

    TPublicType floatingPoint;
    floatingPoint.type = EbtFloat;
    floatingPoint.primarySize = 1;
    floatingPoint.secondarySize = 1;
    floatingPoint.array = false;

    TPublicType sampler;
    sampler.primarySize = 1;
    sampler.secondarySize = 1;
    sampler.array = false;

    switch(shaderType)
    {
      case SH_FRAGMENT_SHADER:
        symbolTable.setDefaultPrecision(integer, EbpMedium);
        break;
      case SH_VERTEX_SHADER:
        symbolTable.setDefaultPrecision(integer, EbpHigh);
        symbolTable.setDefaultPrecision(floatingPoint, EbpHigh);
        break;
      default:
        assert(false && "Language not supported");
    }
    // We set defaults for all the sampler types, even those that are
    // only available if an extension exists.
    for (int samplerType = EbtGuardSamplerBegin + 1;
         samplerType < EbtGuardSamplerEnd; ++samplerType)
    {
        sampler.type = static_cast<TBasicType>(samplerType);
        symbolTable.setDefaultPrecision(sampler, EbpLow);
    }

    InsertBuiltInFunctions(shaderType, shaderSpec, resources, symbolTable);

    IdentifyBuiltIns(shaderType, shaderSpec, resources, symbolTable);

    return true;
}

void TCompiler::clearResults()
{
    arrayBoundsClamper.Cleanup();
    infoSink.info.erase();
    infoSink.obj.erase();
    infoSink.debug.erase();

    attribs.clear();
    uniforms.clear();
    varyings.clear();

    builtInFunctionEmulator.Cleanup();

    nameMap.clear();
}

bool TCompiler::detectCallDepth(TIntermNode* root, TInfoSink& infoSink, bool limitCallStackDepth)
{
    DetectCallDepth detect(infoSink, limitCallStackDepth, maxCallStackDepth);
    root->traverse(&detect);
    switch (detect.detectCallDepth())
    {
      case DetectCallDepth::kErrorNone:
        return true;
      case DetectCallDepth::kErrorMissingMain:
        infoSink.info.prefix(EPrefixError);
        infoSink.info << "Missing main()";
        return false;
      case DetectCallDepth::kErrorRecursion:
        infoSink.info.prefix(EPrefixError);
        infoSink.info << "Function recursion detected";
        return false;
      case DetectCallDepth::kErrorMaxDepthExceeded:
        infoSink.info.prefix(EPrefixError);
        infoSink.info << "Function call stack too deep";
        return false;
      default:
        UNREACHABLE();
        return false;
    }
}

bool TCompiler::validateOutputs(TIntermNode* root)
{
    ValidateOutputs validateOutputs(infoSink.info, compileResources.MaxDrawBuffers);
    root->traverse(&validateOutputs);
    return (validateOutputs.numErrors() == 0);
}

void TCompiler::rewriteCSSShader(TIntermNode* root)
{
    RenameFunction renamer("main(", "css_main(");
    root->traverse(&renamer);
}

bool TCompiler::validateLimitations(TIntermNode* root)
{
    ValidateLimitations validate(shaderType, infoSink.info);
    root->traverse(&validate);
    return validate.numErrors() == 0;
}

bool TCompiler::enforceTimingRestrictions(TIntermNode* root, bool outputGraph)
{
    if (shaderSpec != SH_WEBGL_SPEC)
    {
        infoSink.info << "Timing restrictions must be enforced under the WebGL spec.";
        return false;
    }

    if (shaderType == SH_FRAGMENT_SHADER)
    {
        TDependencyGraph graph(root);

        // Output any errors first.
        bool success = enforceFragmentShaderTimingRestrictions(graph);

        // Then, output the dependency graph.
        if (outputGraph)
        {
            TDependencyGraphOutput output(infoSink.info);
            output.outputAllSpanningTrees(graph);
        }

        return success;
    }
    else
    {
        return enforceVertexShaderTimingRestrictions(root);
    }
}

bool TCompiler::limitExpressionComplexity(TIntermNode* root)
{
    TMaxDepthTraverser traverser(maxExpressionComplexity+1);
    root->traverse(&traverser);

    if (traverser.getMaxDepth() > maxExpressionComplexity)
    {
        infoSink.info << "Expression too complex.";
        return false;
    }

    TDependencyGraph graph(root);

    for (TFunctionCallVector::const_iterator iter = graph.beginUserDefinedFunctionCalls();
         iter != graph.endUserDefinedFunctionCalls();
         ++iter)
    {
        TGraphFunctionCall* samplerSymbol = *iter;
        TDependencyGraphTraverser graphTraverser;
        samplerSymbol->traverse(&graphTraverser);
    }

    return true;
}

bool TCompiler::enforceFragmentShaderTimingRestrictions(const TDependencyGraph& graph)
{
    RestrictFragmentShaderTiming restrictor(infoSink.info);
    restrictor.enforceRestrictions(graph);
    return restrictor.numErrors() == 0;
}

bool TCompiler::enforceVertexShaderTimingRestrictions(TIntermNode* root)
{
    RestrictVertexShaderTiming restrictor(infoSink.info);
    restrictor.enforceRestrictions(root);
    return restrictor.numErrors() == 0;
}

void TCompiler::collectVariables(TIntermNode* root)
{
    CollectVariables collect(attribs, uniforms, varyings, hashFunction);
    root->traverse(&collect);
}

bool TCompiler::enforcePackingRestrictions()
{
    VariablePacker packer;
    return packer.CheckVariablesWithinPackingLimits(maxUniformVectors, uniforms);
}

void TCompiler::initializeGLPosition(TIntermNode* root)
{
    InitializeVariables::InitVariableInfoList variables;
    InitializeVariables::InitVariableInfo var(
        "gl_Position", TType(EbtFloat, EbpUndefined, EvqPosition, 4));
    variables.push_back(var);
    InitializeVariables initializer(variables);
    root->traverse(&initializer);
}

void TCompiler::initializeVaryingsWithoutStaticUse(TIntermNode* root)
{
    InitializeVariables::InitVariableInfoList variables;
    for (size_t ii = 0; ii < varyings.size(); ++ii)
    {
        const TVariableInfo& varying = varyings[ii];
        if (varying.staticUse)
            continue;
        unsigned char primarySize = 1, secondarySize = 1;
        switch (varying.type)
        {
          case SH_FLOAT:
            break;
          case SH_FLOAT_VEC2:
            primarySize = 2;
            break;
          case SH_FLOAT_VEC3:
            primarySize = 3;
            break;
          case SH_FLOAT_VEC4:
            primarySize = 4;
            break;
          case SH_FLOAT_MAT2:
            primarySize = 2;
            secondarySize = 2;
            break;
          case SH_FLOAT_MAT3:
            primarySize = 3;
            secondarySize = 3;
            break;
          case SH_FLOAT_MAT4:
            primarySize = 4;
            secondarySize = 4;
            break;
          default:
            ASSERT(false);
        }
        TType type(EbtFloat, EbpUndefined, EvqVaryingOut, primarySize, secondarySize, varying.isArray);
        TString name = varying.name.c_str();
        if (varying.isArray)
        {
            type.setArraySize(varying.size);
            name = name.substr(0, name.find_first_of('['));
        }

        InitializeVariables::InitVariableInfo var(name, type);
        variables.push_back(var);
    }
    InitializeVariables initializer(variables);
    root->traverse(&initializer);
}

const TExtensionBehavior& TCompiler::getExtensionBehavior() const
{
    return extensionBehavior;
}

const ShBuiltInResources& TCompiler::getResources() const
{
    return compileResources;
}

const ArrayBoundsClamper& TCompiler::getArrayBoundsClamper() const
{
    return arrayBoundsClamper;
}

ShArrayIndexClampingStrategy TCompiler::getArrayIndexClampingStrategy() const
{
    return clampingStrategy;
}

const BuiltInFunctionEmulator& TCompiler::getBuiltInFunctionEmulator() const
{
    return builtInFunctionEmulator;
}

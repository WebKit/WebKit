//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/Compiler.h"

#include <sstream>

#include "angle_gl.h"
#include "common/utilities.h"
#include "compiler/translator/AddAndTrueToLoopCondition.h"
#include "compiler/translator/Cache.h"
#include "compiler/translator/CallDAG.h"
#include "compiler/translator/DeferGlobalInitializers.h"
#include "compiler/translator/EmulateGLFragColorBroadcast.h"
#include "compiler/translator/EmulatePrecision.h"
#include "compiler/translator/ForLoopUnroll.h"
#include "compiler/translator/Initialize.h"
#include "compiler/translator/InitializeParseContext.h"
#include "compiler/translator/InitializeVariables.h"
#include "compiler/translator/ParseContext.h"
#include "compiler/translator/PruneEmptyDeclarations.h"
#include "compiler/translator/RegenerateStructNames.h"
#include "compiler/translator/RemovePow.h"
#include "compiler/translator/RewriteDoWhile.h"
#include "compiler/translator/ScalarizeVecAndMatConstructorArgs.h"
#include "compiler/translator/UnfoldShortCircuitAST.h"
#include "compiler/translator/UseInterfaceBlockFields.h"
#include "compiler/translator/ValidateLimitations.h"
#include "compiler/translator/ValidateMaxParameters.h"
#include "compiler/translator/ValidateOutputs.h"
#include "compiler/translator/VariablePacker.h"
#include "third_party/compiler/ArrayBoundsClamper.h"

namespace
{

#if defined(ANGLE_ENABLE_FUZZER_CORPUS_OUTPUT)
void DumpFuzzerCase(char const *const *shaderStrings,
                    size_t numStrings,
                    uint32_t type,
                    uint32_t spec,
                    uint32_t output,
                    uint64_t options)
{
    static int fileIndex = 0;

    std::ostringstream o;
    o << "corpus/" << fileIndex++ << ".sample";
    std::string s = o.str();

    // Must match the input format of the fuzzer
    FILE *f = fopen(s.c_str(), "w");
    fwrite(&type, sizeof(type), 1, f);
    fwrite(&spec, sizeof(spec), 1, f);
    fwrite(&output, sizeof(output), 1, f);
    fwrite(&options, sizeof(options), 1, f);

    char zero[128 - 20] = {0};
    fwrite(&zero, 128 - 20, 1, f);

    for (size_t i = 0; i < numStrings; i++)
    {
        fwrite(shaderStrings[i], sizeof(char), strlen(shaderStrings[i]), f);
    }
    fwrite(&zero, 1, 1, f);

    fclose(f);
}
#endif  // defined(ANGLE_ENABLE_FUZZER_CORPUS_OUTPUT)
}  // anonymous namespace

bool IsWebGLBasedSpec(ShShaderSpec spec)
{
    return (spec == SH_WEBGL_SPEC || spec == SH_WEBGL2_SPEC || spec == SH_WEBGL3_SPEC);
}

bool IsGLSL130OrNewer(ShShaderOutput output)
{
    return (output == SH_GLSL_130_OUTPUT ||
            output == SH_GLSL_140_OUTPUT ||
            output == SH_GLSL_150_CORE_OUTPUT ||
            output == SH_GLSL_330_CORE_OUTPUT ||
            output == SH_GLSL_400_CORE_OUTPUT ||
            output == SH_GLSL_410_CORE_OUTPUT ||
            output == SH_GLSL_420_CORE_OUTPUT ||
            output == SH_GLSL_430_CORE_OUTPUT ||
            output == SH_GLSL_440_CORE_OUTPUT ||
            output == SH_GLSL_450_CORE_OUTPUT);
}

size_t GetGlobalMaxTokenSize(ShShaderSpec spec)
{
    // WebGL defines a max token legnth of 256, while ES2 leaves max token
    // size undefined. ES3 defines a max size of 1024 characters.
    switch (spec)
    {
      case SH_WEBGL_SPEC:
        return 256;
      default:
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

int MapSpecToShaderVersion(ShShaderSpec spec)
{
    switch (spec)
    {
      case SH_GLES2_SPEC:
      case SH_WEBGL_SPEC:
        return 100;
      case SH_GLES3_SPEC:
      case SH_WEBGL2_SPEC:
        return 300;
      case SH_GLES3_1_SPEC:
      case SH_WEBGL3_SPEC:
          return 310;
      default:
        UNREACHABLE();
        return 0;
    }
}

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

TCompiler::TCompiler(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
    : variablesCollected(false),
      shaderType(type),
      shaderSpec(spec),
      outputType(output),
      maxUniformVectors(0),
      maxExpressionComplexity(0),
      maxCallStackDepth(0),
      maxFunctionParameters(0),
      fragmentPrecisionHigh(false),
      clampingStrategy(SH_CLAMP_WITH_CLAMP_INTRINSIC),
      builtInFunctionEmulator(),
      mSourcePath(NULL),
      mComputeShaderLocalSizeDeclared(false),
      mTemporaryIndex(0)
{
    mComputeShaderLocalSize.fill(1);
}

TCompiler::~TCompiler()
{
}

bool TCompiler::shouldRunLoopAndIndexingValidation(ShCompileOptions compileOptions) const
{
    // If compiling an ESSL 1.00 shader for WebGL, or if its been requested through the API,
    // validate loop and indexing as well (to verify that the shader only uses minimal functionality
    // of ESSL 1.00 as in Appendix A of the spec).
    return (IsWebGLBasedSpec(shaderSpec) && shaderVersion == 100) ||
           (compileOptions & SH_VALIDATE_LOOP_INDEXING);
}

bool TCompiler::Init(const ShBuiltInResources& resources)
{
    shaderVersion = 100;
    maxUniformVectors = (shaderType == GL_VERTEX_SHADER) ?
        resources.MaxVertexUniformVectors :
        resources.MaxFragmentUniformVectors;
    maxExpressionComplexity = resources.MaxExpressionComplexity;
    maxCallStackDepth       = resources.MaxCallStackDepth;
    maxFunctionParameters   = resources.MaxFunctionParameters;

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

TIntermBlock *TCompiler::compileTreeForTesting(const char *const shaderStrings[],
                                               size_t numStrings,
                                               ShCompileOptions compileOptions)
{
    return compileTreeImpl(shaderStrings, numStrings, compileOptions);
}

TIntermBlock *TCompiler::compileTreeImpl(const char *const shaderStrings[],
                                         size_t numStrings,
                                         const ShCompileOptions compileOptions)
{
    clearResults();

    ASSERT(numStrings > 0);
    ASSERT(GetGlobalPoolAllocator());

    // Reset the extension behavior for each compilation unit.
    ResetExtensionBehavior(extensionBehavior);

    // First string is path of source file if flag is set. The actual source follows.
    size_t firstSource = 0;
    if (compileOptions & SH_SOURCE_PATH)
    {
        mSourcePath = shaderStrings[0];
        ++firstSource;
    }

    TParseContext parseContext(symbolTable, extensionBehavior, shaderType, shaderSpec,
                               compileOptions, true, infoSink, getResources());

    parseContext.setFragmentPrecisionHighOnESSL1(fragmentPrecisionHigh);
    SetGlobalParseContext(&parseContext);

    // We preserve symbols at the built-in level from compile-to-compile.
    // Start pushing the user-defined symbols at global level.
    TScopedSymbolTableLevel scopedSymbolLevel(&symbolTable);

    // Parse shader.
    bool success =
        (PaParseStrings(numStrings - firstSource, &shaderStrings[firstSource], nullptr, &parseContext) == 0) &&
        (parseContext.getTreeRoot() != nullptr);

    shaderVersion = parseContext.getShaderVersion();
    if (success && MapSpecToShaderVersion(shaderSpec) < shaderVersion)
    {
        infoSink.info.prefix(EPrefixError);
        infoSink.info << "unsupported shader version";
        success = false;
    }

    TIntermBlock *root = nullptr;

    if (success)
    {
        mPragma = parseContext.pragma();
        symbolTable.setGlobalInvariant(mPragma.stdgl.invariantAll);

        mComputeShaderLocalSizeDeclared = parseContext.isComputeShaderLocalSizeDeclared();
        mComputeShaderLocalSize         = parseContext.getComputeShaderLocalSize();

        root = parseContext.getTreeRoot();

        // Highp might have been auto-enabled based on shader version
        fragmentPrecisionHigh = parseContext.getFragmentPrecisionHigh();

        // Disallow expressions deemed too complex.
        if (success && (compileOptions & SH_LIMIT_EXPRESSION_COMPLEXITY))
            success = limitExpressionComplexity(root);

        // Create the function DAG and check there is no recursion
        if (success)
            success = initCallDag(root);

        if (success && (compileOptions & SH_LIMIT_CALL_STACK_DEPTH))
            success = checkCallDepth();

        // Checks which functions are used and if "main" exists
        if (success)
        {
            functionMetadata.clear();
            functionMetadata.resize(mCallDag.size());
            success = tagUsedFunctions();
        }

        if (success && !(compileOptions & SH_DONT_PRUNE_UNUSED_FUNCTIONS))
            success = pruneUnusedFunctions(root);

        // Prune empty declarations to work around driver bugs and to keep declaration output simple.
        if (success)
            PruneEmptyDeclarations(root);

        if (success && shaderVersion == 300 && shaderType == GL_FRAGMENT_SHADER)
            success = validateOutputs(root);

        if (success && shouldRunLoopAndIndexingValidation(compileOptions))
            success = validateLimitations(root);

        // Fail compilation if precision emulation not supported.
        if (success && getResources().WEBGL_debug_shader_precision &&
            getPragma().debugShaderPrecision)
        {
            if (!EmulatePrecision::SupportedInLanguage(outputType))
            {
                infoSink.info.prefix(EPrefixError);
                infoSink.info << "Precision emulation not supported for this output type.";
                success = false;
            }
        }

        // Unroll for-loop markup needs to happen after validateLimitations pass.
        if (success && (compileOptions & SH_UNROLL_FOR_LOOP_WITH_INTEGER_INDEX))
        {
            ForLoopUnrollMarker marker(ForLoopUnrollMarker::kIntegerIndex,
                                       shouldRunLoopAndIndexingValidation(compileOptions));
            root->traverse(&marker);
        }
        if (success && (compileOptions & SH_UNROLL_FOR_LOOP_WITH_SAMPLER_ARRAY_INDEX))
        {
            ForLoopUnrollMarker marker(ForLoopUnrollMarker::kSamplerArrayIndex,
                                       shouldRunLoopAndIndexingValidation(compileOptions));
            root->traverse(&marker);
            if (marker.samplerArrayIndexIsFloatLoopIndex())
            {
                infoSink.info.prefix(EPrefixError);
                infoSink.info << "sampler array index is float loop index";
                success = false;
            }
        }

        // Built-in function emulation needs to happen after validateLimitations pass.
        if (success)
        {
            // TODO(jmadill): Remove global pool allocator.
            GetGlobalPoolAllocator()->lock();
            initBuiltInFunctionEmulator(&builtInFunctionEmulator, compileOptions);
            GetGlobalPoolAllocator()->unlock();
            builtInFunctionEmulator.MarkBuiltInFunctionsForEmulation(root);
        }

        // Clamping uniform array bounds needs to happen after validateLimitations pass.
        if (success && (compileOptions & SH_CLAMP_INDIRECT_ARRAY_BOUNDS))
            arrayBoundsClamper.MarkIndirectArrayBoundsForClamping(root);

        // gl_Position is always written in compatibility output mode
        if (success && shaderType == GL_VERTEX_SHADER &&
            ((compileOptions & SH_INIT_GL_POSITION) ||
             (outputType == SH_GLSL_COMPATIBILITY_OUTPUT)))
            initializeGLPosition(root);

        // This pass might emit short circuits so keep it before the short circuit unfolding
        if (success && (compileOptions & SH_REWRITE_DO_WHILE_LOOPS))
            RewriteDoWhile(root, getTemporaryIndex());

        if (success && (compileOptions & SH_ADD_AND_TRUE_TO_LOOP_CONDITION))
            sh::AddAndTrueToLoopCondition(root);

        if (success && (compileOptions & SH_UNFOLD_SHORT_CIRCUIT))
        {
            UnfoldShortCircuitAST unfoldShortCircuit;
            root->traverse(&unfoldShortCircuit);
            unfoldShortCircuit.updateTree();
        }

        if (success && (compileOptions & SH_REMOVE_POW_WITH_CONSTANT_EXPONENT))
        {
            RemovePow(root);
        }

        if (success && shouldCollectVariables(compileOptions))
        {
            collectVariables(root);
            if (compileOptions & SH_USE_UNUSED_STANDARD_SHARED_BLOCKS)
            {
                useAllMembersInUnusedStandardAndSharedBlocks(root);
            }
            if (compileOptions & SH_ENFORCE_PACKING_RESTRICTIONS)
            {
                success = enforcePackingRestrictions();
                if (!success)
                {
                    infoSink.info.prefix(EPrefixError);
                    infoSink.info << "too many uniforms";
                }
            }
            if (success && (compileOptions & SH_INIT_OUTPUT_VARIABLES))
            {
                initializeOutputVariables(root);
            }
        }

        if (success && (compileOptions & SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS))
        {
            ScalarizeVecAndMatConstructorArgs scalarizer(
                shaderType, fragmentPrecisionHigh);
            root->traverse(&scalarizer);
        }

        if (success && (compileOptions & SH_REGENERATE_STRUCT_NAMES))
        {
            RegenerateStructNames gen(symbolTable, shaderVersion);
            root->traverse(&gen);
        }

        if (success && shaderType == GL_FRAGMENT_SHADER && shaderVersion == 100 &&
            compileResources.EXT_draw_buffers && compileResources.MaxDrawBuffers > 1 &&
            IsExtensionEnabled(extensionBehavior, "GL_EXT_draw_buffers"))
        {
            EmulateGLFragColorBroadcast(root, compileResources.MaxDrawBuffers, &outputVariables);
        }

        if (success)
        {
            DeferGlobalInitializers(root);
        }
    }

    SetGlobalParseContext(NULL);
    if (success)
        return root;

    return NULL;
}

bool TCompiler::compile(const char *const shaderStrings[],
                        size_t numStrings,
                        ShCompileOptions compileOptionsIn)
{
#if defined(ANGLE_ENABLE_FUZZER_CORPUS_OUTPUT)
    DumpFuzzerCase(shaderStrings, numStrings, shaderType, shaderSpec, outputType, compileOptionsIn);
#endif  // defined(ANGLE_ENABLE_FUZZER_CORPUS_OUTPUT)

    if (numStrings == 0)
        return true;

    ShCompileOptions compileOptions = compileOptionsIn;

    // Apply key workarounds.
    if (shouldFlattenPragmaStdglInvariantAll())
    {
        // This should be harmless to do in all cases, but for the moment, do it only conditionally.
        compileOptions |= SH_FLATTEN_PRAGMA_STDGL_INVARIANT_ALL;
    }

    ShCompileOptions unrollFlags =
        SH_UNROLL_FOR_LOOP_WITH_INTEGER_INDEX | SH_UNROLL_FOR_LOOP_WITH_SAMPLER_ARRAY_INDEX;
    if ((compileOptions & SH_ADD_AND_TRUE_TO_LOOP_CONDITION) != 0 &&
        (compileOptions & unrollFlags) != 0)
    {
        infoSink.info.prefix(EPrefixError);
        infoSink.info
            << "Unsupported compile flag combination: unroll & ADD_TRUE_TO_LOOP_CONDITION";
        return false;
    }

    TScopedPoolAllocator scopedAlloc(&allocator);
    TIntermBlock *root = compileTreeImpl(shaderStrings, numStrings, compileOptions);

    if (root)
    {
        if (compileOptions & SH_INTERMEDIATE_TREE)
            TIntermediate::outputTree(root, infoSink.info);

        if (compileOptions & SH_OBJECT_CODE)
            translate(root, compileOptions);

        // The IntermNode tree doesn't need to be deleted here, since the
        // memory will be freed in a big chunk by the PoolAllocator.
        return true;
    }
    return false;
}

bool TCompiler::InitBuiltInSymbolTable(const ShBuiltInResources &resources)
{
    compileResources = resources;
    setResourceString();

    assert(symbolTable.isEmpty());
    symbolTable.push();   // COMMON_BUILTINS
    symbolTable.push();   // ESSL1_BUILTINS
    symbolTable.push();   // ESSL3_BUILTINS
    symbolTable.push();   // ESSL3_1_BUILTINS

    TPublicType integer;
    integer.setBasicType(EbtInt);
    integer.initializeSizeForScalarTypes();
    integer.array = false;

    TPublicType floatingPoint;
    floatingPoint.setBasicType(EbtFloat);
    floatingPoint.initializeSizeForScalarTypes();
    floatingPoint.array = false;

    switch(shaderType)
    {
      case GL_FRAGMENT_SHADER:
        symbolTable.setDefaultPrecision(integer, EbpMedium);
        break;
      case GL_VERTEX_SHADER:
        symbolTable.setDefaultPrecision(integer, EbpHigh);
        symbolTable.setDefaultPrecision(floatingPoint, EbpHigh);
        break;
      case GL_COMPUTE_SHADER:
          symbolTable.setDefaultPrecision(integer, EbpHigh);
          symbolTable.setDefaultPrecision(floatingPoint, EbpHigh);
          break;
      default:
        assert(false && "Language not supported");
    }
    // Set defaults for sampler types that have default precision, even those that are
    // only available if an extension exists.
    // New sampler types in ESSL3 don't have default precision. ESSL1 types do.
    initSamplerDefaultPrecision(EbtSampler2D);
    initSamplerDefaultPrecision(EbtSamplerCube);
    // SamplerExternalOES is specified in the extension to have default precision.
    initSamplerDefaultPrecision(EbtSamplerExternalOES);
    // It isn't specified whether Sampler2DRect has default precision.
    initSamplerDefaultPrecision(EbtSampler2DRect);

    InsertBuiltInFunctions(shaderType, shaderSpec, resources, symbolTable);

    IdentifyBuiltIns(shaderType, shaderSpec, resources, symbolTable);

    return true;
}

void TCompiler::initSamplerDefaultPrecision(TBasicType samplerType)
{
    ASSERT(samplerType > EbtGuardSamplerBegin && samplerType < EbtGuardSamplerEnd);
    TPublicType sampler;
    sampler.initializeSizeForScalarTypes();
    sampler.setBasicType(samplerType);
    sampler.array         = false;
    symbolTable.setDefaultPrecision(sampler, EbpLow);
}

void TCompiler::setResourceString()
{
    std::ostringstream strstream;

    // clang-format off
    strstream << ":MaxVertexAttribs:" << compileResources.MaxVertexAttribs
              << ":MaxVertexUniformVectors:" << compileResources.MaxVertexUniformVectors
              << ":MaxVaryingVectors:" << compileResources.MaxVaryingVectors
              << ":MaxVertexTextureImageUnits:" << compileResources.MaxVertexTextureImageUnits
              << ":MaxCombinedTextureImageUnits:" << compileResources.MaxCombinedTextureImageUnits
              << ":MaxTextureImageUnits:" << compileResources.MaxTextureImageUnits
              << ":MaxFragmentUniformVectors:" << compileResources.MaxFragmentUniformVectors
              << ":MaxDrawBuffers:" << compileResources.MaxDrawBuffers
              << ":OES_standard_derivatives:" << compileResources.OES_standard_derivatives
              << ":OES_EGL_image_external:" << compileResources.OES_EGL_image_external
              << ":OES_EGL_image_external_essl3:" << compileResources.OES_EGL_image_external_essl3
              << ":NV_EGL_stream_consumer_external:" << compileResources.NV_EGL_stream_consumer_external
              << ":ARB_texture_rectangle:" << compileResources.ARB_texture_rectangle
              << ":EXT_draw_buffers:" << compileResources.EXT_draw_buffers
              << ":FragmentPrecisionHigh:" << compileResources.FragmentPrecisionHigh
              << ":MaxExpressionComplexity:" << compileResources.MaxExpressionComplexity
              << ":MaxCallStackDepth:" << compileResources.MaxCallStackDepth
              << ":MaxFunctionParameters:" << compileResources.MaxFunctionParameters
              << ":EXT_blend_func_extended:" << compileResources.EXT_blend_func_extended
              << ":EXT_frag_depth:" << compileResources.EXT_frag_depth
              << ":EXT_shader_texture_lod:" << compileResources.EXT_shader_texture_lod
              << ":EXT_shader_framebuffer_fetch:" << compileResources.EXT_shader_framebuffer_fetch
              << ":NV_shader_framebuffer_fetch:" << compileResources.NV_shader_framebuffer_fetch
              << ":ARM_shader_framebuffer_fetch:" << compileResources.ARM_shader_framebuffer_fetch
              << ":MaxVertexOutputVectors:" << compileResources.MaxVertexOutputVectors
              << ":MaxFragmentInputVectors:" << compileResources.MaxFragmentInputVectors
              << ":MinProgramTexelOffset:" << compileResources.MinProgramTexelOffset
              << ":MaxProgramTexelOffset:" << compileResources.MaxProgramTexelOffset
              << ":MaxDualSourceDrawBuffers:" << compileResources.MaxDualSourceDrawBuffers
              << ":NV_draw_buffers:" << compileResources.NV_draw_buffers
              << ":WEBGL_debug_shader_precision:" << compileResources.WEBGL_debug_shader_precision
              << ":MaxImageUnits:" << compileResources.MaxImageUnits
              << ":MaxVertexImageUniforms:" << compileResources.MaxVertexImageUniforms
              << ":MaxFragmentImageUniforms:" << compileResources.MaxFragmentImageUniforms
              << ":MaxComputeImageUniforms:" << compileResources.MaxComputeImageUniforms
              << ":MaxCombinedImageUniforms:" << compileResources.MaxCombinedImageUniforms
              << ":MaxCombinedShaderOutputResources:" << compileResources.MaxCombinedShaderOutputResources
              << ":MaxComputeWorkGroupCountX:" << compileResources.MaxComputeWorkGroupCount[0]
              << ":MaxComputeWorkGroupCountY:" << compileResources.MaxComputeWorkGroupCount[1]
              << ":MaxComputeWorkGroupCountZ:" << compileResources.MaxComputeWorkGroupCount[2]
              << ":MaxComputeWorkGroupSizeX:" << compileResources.MaxComputeWorkGroupSize[0]
              << ":MaxComputeWorkGroupSizeY:" << compileResources.MaxComputeWorkGroupSize[1]
              << ":MaxComputeWorkGroupSizeZ:" << compileResources.MaxComputeWorkGroupSize[2]
              << ":MaxComputeUniformComponents:" << compileResources.MaxComputeUniformComponents
              << ":MaxComputeTextureImageUnits:" << compileResources.MaxComputeTextureImageUnits
              << ":MaxComputeAtomicCounters:" << compileResources.MaxComputeAtomicCounters
              << ":MaxComputeAtomicCounterBuffers:" << compileResources.MaxComputeAtomicCounterBuffers
              << ":MaxVertexAtomicCounters:" << compileResources.MaxVertexAtomicCounters
              << ":MaxFragmentAtomicCounters:" << compileResources.MaxFragmentAtomicCounters
              << ":MaxCombinedAtomicCounters:" << compileResources.MaxCombinedAtomicCounters
              << ":MaxAtomicCounterBindings:" << compileResources.MaxAtomicCounterBindings
              << ":MaxVertexAtomicCounterBuffers:" << compileResources.MaxVertexAtomicCounterBuffers
              << ":MaxFragmentAtomicCounterBuffers:" << compileResources.MaxFragmentAtomicCounterBuffers
              << ":MaxCombinedAtomicCounterBuffers:" << compileResources.MaxCombinedAtomicCounterBuffers
              << ":MaxAtomicCounterBufferSize:" << compileResources.MaxAtomicCounterBufferSize;
    // clang-format on

    builtInResourcesString = strstream.str();
}

void TCompiler::clearResults()
{
    arrayBoundsClamper.Cleanup();
    infoSink.info.erase();
    infoSink.obj.erase();
    infoSink.debug.erase();

    attributes.clear();
    outputVariables.clear();
    uniforms.clear();
    expandedUniforms.clear();
    varyings.clear();
    interfaceBlocks.clear();
    variablesCollected = false;

    builtInFunctionEmulator.Cleanup();

    nameMap.clear();

    mSourcePath = NULL;
    mTemporaryIndex = 0;
}

bool TCompiler::initCallDag(TIntermNode *root)
{
    mCallDag.clear();

    switch (mCallDag.init(root, &infoSink.info))
    {
      case CallDAG::INITDAG_SUCCESS:
        return true;
      case CallDAG::INITDAG_RECURSION:
        infoSink.info.prefix(EPrefixError);
        infoSink.info << "Function recursion detected";
        return false;
      case CallDAG::INITDAG_UNDEFINED:
        infoSink.info.prefix(EPrefixError);
        infoSink.info << "Unimplemented function detected";
        return false;
    }

    UNREACHABLE();
    return true;
}

bool TCompiler::checkCallDepth()
{
    std::vector<int> depths(mCallDag.size());

    for (size_t i = 0; i < mCallDag.size(); i++)
    {
        int depth = 0;
        auto &record = mCallDag.getRecordFromIndex(i);

        for (auto &calleeIndex : record.callees)
        {
            depth = std::max(depth, depths[calleeIndex] + 1);
        }

        depths[i] = depth;

        if (depth >= maxCallStackDepth)
        {
            // Trace back the function chain to have a meaningful info log.
            infoSink.info.prefix(EPrefixError);
            infoSink.info << "Call stack too deep (larger than " << maxCallStackDepth
                          << ") with the following call chain: " << record.name;

            int currentFunction = static_cast<int>(i);
            int currentDepth = depth;

            while (currentFunction != -1)
            {
                infoSink.info << " -> " << mCallDag.getRecordFromIndex(currentFunction).name;

                int nextFunction = -1;
                for (auto& calleeIndex : mCallDag.getRecordFromIndex(currentFunction).callees)
                {
                    if (depths[calleeIndex] == currentDepth - 1)
                    {
                        currentDepth--;
                        nextFunction = calleeIndex;
                    }
                }

                currentFunction = nextFunction;
            }

            return false;
        }
    }

    return true;
}

bool TCompiler::tagUsedFunctions()
{
    // Search from main, starting from the end of the DAG as it usually is the root.
    for (size_t i = mCallDag.size(); i-- > 0;)
    {
        if (mCallDag.getRecordFromIndex(i).name == "main(")
        {
            internalTagUsedFunction(i);
            return true;
        }
    }

    infoSink.info.prefix(EPrefixError);
    infoSink.info << "Missing main()\n";
    return false;
}

void TCompiler::internalTagUsedFunction(size_t index)
{
    if (functionMetadata[index].used)
    {
        return;
    }

    functionMetadata[index].used = true;

    for (int calleeIndex : mCallDag.getRecordFromIndex(index).callees)
    {
        internalTagUsedFunction(calleeIndex);
    }
}

// A predicate for the stl that returns if a top-level node is unused
class TCompiler::UnusedPredicate
{
  public:
    UnusedPredicate(const CallDAG *callDag, const std::vector<FunctionMetadata> *metadatas)
        : mCallDag(callDag),
          mMetadatas(metadatas)
    {
    }

    bool operator ()(TIntermNode *node)
    {
        const TIntermAggregate *asAggregate = node->getAsAggregate();
        const TIntermFunctionDefinition *asFunction = node->getAsFunctionDefinition();

        const TFunctionSymbolInfo *functionInfo = nullptr;

        if (asFunction)
        {
            functionInfo = asFunction->getFunctionSymbolInfo();
        }
        else if (asAggregate)
        {
            if (asAggregate->getOp() == EOpPrototype)
            {
                functionInfo = asAggregate->getFunctionSymbolInfo();
            }
        }
        if (functionInfo == nullptr)
        {
            return false;
        }

        size_t callDagIndex = mCallDag->findIndex(functionInfo);
        if (callDagIndex == CallDAG::InvalidIndex)
        {
            // This happens only for unimplemented prototypes which are thus unused
            ASSERT(asAggregate && asAggregate->getOp() == EOpPrototype);
            return true;
        }

        ASSERT(callDagIndex < mMetadatas->size());
        return !(*mMetadatas)[callDagIndex].used;
    }

  private:
    const CallDAG *mCallDag;
    const std::vector<FunctionMetadata> *mMetadatas;
};

bool TCompiler::pruneUnusedFunctions(TIntermBlock *root)
{
    UnusedPredicate isUnused(&mCallDag, &functionMetadata);
    TIntermSequence *sequence = root->getSequence();

    if (!sequence->empty())
    {
        sequence->erase(std::remove_if(sequence->begin(), sequence->end(), isUnused), sequence->end());
    }

    return true;
}

bool TCompiler::validateOutputs(TIntermNode* root)
{
    ValidateOutputs validateOutputs(getExtensionBehavior(), compileResources.MaxDrawBuffers);
    root->traverse(&validateOutputs);
    return (validateOutputs.validateAndCountErrors(infoSink.info) == 0);
}

bool TCompiler::validateLimitations(TIntermNode* root)
{
    ValidateLimitations validate(shaderType, &infoSink.info);
    root->traverse(&validate);
    return validate.numErrors() == 0;
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

    if (!ValidateMaxParameters::validate(root, maxFunctionParameters))
    {
        infoSink.info << "Function has too many parameters.";
        return false;
    }

    return true;
}

void TCompiler::collectVariables(TIntermNode* root)
{
    if (!variablesCollected)
    {
        sh::CollectVariables collect(&attributes, &outputVariables, &uniforms, &varyings,
                                     &interfaceBlocks, hashFunction, symbolTable, extensionBehavior);
        root->traverse(&collect);

        // This is for enforcePackingRestriction().
        sh::ExpandUniforms(uniforms, &expandedUniforms);
        variablesCollected = true;
    }
}

bool TCompiler::enforcePackingRestrictions()
{
    VariablePacker packer;
    return packer.CheckVariablesWithinPackingLimits(maxUniformVectors, expandedUniforms);
}

void TCompiler::initializeGLPosition(TIntermNode* root)
{
    InitVariableList list;
    sh::ShaderVariable var(GL_FLOAT_VEC4, 0);
    var.name = "gl_Position";
    list.push_back(var);
    InitializeVariables(root, list);
}

void TCompiler::useAllMembersInUnusedStandardAndSharedBlocks(TIntermNode *root)
{
    sh::InterfaceBlockList list;

    for (auto block : interfaceBlocks)
    {
        if (!block.staticUse &&
            (block.layout == sh::BLOCKLAYOUT_STANDARD || block.layout == sh::BLOCKLAYOUT_SHARED))
        {
            list.push_back(block);
        }
    }

    sh::UseInterfaceBlockFields(root, list);
}

void TCompiler::initializeOutputVariables(TIntermNode *root)
{
    InitVariableList list;
    if (shaderType == GL_VERTEX_SHADER)
    {
        for (auto var : varyings)
        {
            list.push_back(var);
        }
    }
    else
    {
        ASSERT(shaderType == GL_FRAGMENT_SHADER);
        for (auto var : outputVariables)
        {
            list.push_back(var);
        }
    }
    InitializeVariables(root, list);
}

const TExtensionBehavior& TCompiler::getExtensionBehavior() const
{
    return extensionBehavior;
}

const char *TCompiler::getSourcePath() const
{
    return mSourcePath;
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

void TCompiler::writePragma(ShCompileOptions compileOptions)
{
    if (!(compileOptions & SH_FLATTEN_PRAGMA_STDGL_INVARIANT_ALL))
    {
        TInfoSinkBase &sink = infoSink.obj;
        if (mPragma.stdgl.invariantAll)
            sink << "#pragma STDGL invariant(all)\n";
    }
}

bool TCompiler::isVaryingDefined(const char *varyingName)
{
    ASSERT(variablesCollected);
    for (size_t ii = 0; ii < varyings.size(); ++ii)
    {
        if (varyings[ii].name == varyingName)
        {
            return true;
        }
    }

    return false;
}

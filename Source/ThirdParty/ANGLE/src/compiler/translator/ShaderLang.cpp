//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Implement the top-level of interface to the compiler,
// as defined in ShaderLang.h
//

#include "GLSLANG/ShaderLang.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/InitializeDll.h"
#include "compiler/translator/length_limits.h"
#ifdef ANGLE_ENABLE_HLSL
#include "compiler/translator/TranslatorHLSL.h"
#endif  // ANGLE_ENABLE_HLSL
#include "compiler/translator/VariablePacker.h"
#include "angle_gl.h"

namespace sh
{

namespace
{

bool isInitialized = false;

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler.
//

template <typename VarT>
const std::vector<VarT> *GetVariableList(const TCompiler *compiler);

template <>
const std::vector<Uniform> *GetVariableList(const TCompiler *compiler)
{
    return &compiler->getUniforms();
}

template <>
const std::vector<Varying> *GetVariableList(const TCompiler *compiler)
{
    switch (compiler->getShaderType())
    {
        case GL_VERTEX_SHADER:
            return &compiler->getOutputVaryings();
        case GL_FRAGMENT_SHADER:
            return &compiler->getInputVaryings();
        case GL_COMPUTE_SHADER:
            ASSERT(compiler->getOutputVaryings().empty() && compiler->getInputVaryings().empty());
            return &compiler->getOutputVaryings();
        // Since geometry shaders have both input and output varyings, we shouldn't call GetVaryings
        // on a geometry shader.
        default:
            return nullptr;
    }
}

template <>
const std::vector<Attribute> *GetVariableList(const TCompiler *compiler)
{
    return &compiler->getAttributes();
}

template <>
const std::vector<OutputVariable> *GetVariableList(const TCompiler *compiler)
{
    return &compiler->getOutputVariables();
}

template <>
const std::vector<InterfaceBlock> *GetVariableList(const TCompiler *compiler)
{
    return &compiler->getInterfaceBlocks();
}

TCompiler *GetCompilerFromHandle(ShHandle handle)
{
    if (!handle)
    {
        return nullptr;
    }

    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    return base->getAsCompiler();
}

template <typename VarT>
const std::vector<VarT> *GetShaderVariables(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    if (!compiler)
    {
        return nullptr;
    }

    return GetVariableList<VarT>(compiler);
}

#ifdef ANGLE_ENABLE_HLSL
TranslatorHLSL *GetTranslatorHLSLFromHandle(ShHandle handle)
{
    if (!handle)
        return nullptr;
    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    return base->getAsTranslatorHLSL();
}
#endif  // ANGLE_ENABLE_HLSL

GLenum GetGeometryShaderPrimitiveTypeEnum(sh::TLayoutPrimitiveType primitiveType)
{
    switch (primitiveType)
    {
        case EptPoints:
            return GL_POINTS;
        case EptLines:
            return GL_LINES;
        case EptLinesAdjacency:
            return GL_LINES_ADJACENCY_EXT;
        case EptTriangles:
            return GL_TRIANGLES;
        case EptTrianglesAdjacency:
            return GL_TRIANGLES_ADJACENCY_EXT;

        case EptLineStrip:
            return GL_LINE_STRIP;
        case EptTriangleStrip:
            return GL_TRIANGLE_STRIP;

        case EptUndefined:
            return GL_INVALID_VALUE;

        default:
            UNREACHABLE();
            return GL_INVALID_VALUE;
    }
}

}  // anonymous namespace

//
// Driver must call this first, once, before doing any other compiler operations.
// Subsequent calls to this function are no-op.
//
bool Initialize()
{
    if (!isInitialized)
    {
        isInitialized = InitProcess();
    }
    return isInitialized;
}

//
// Cleanup symbol tables
//
bool Finalize()
{
    if (isInitialized)
    {
        DetachProcess();
        isInitialized = false;
    }
    return true;
}

//
// Initialize built-in resources with minimum expected values.
//
void InitBuiltInResources(ShBuiltInResources *resources)
{
    // Make comparable.
    memset(resources, 0, sizeof(*resources));

    // Constants.
    resources->MaxVertexAttribs             = 8;
    resources->MaxVertexUniformVectors      = 128;
    resources->MaxVaryingVectors            = 8;
    resources->MaxVertexTextureImageUnits   = 0;
    resources->MaxCombinedTextureImageUnits = 8;
    resources->MaxTextureImageUnits         = 8;
    resources->MaxFragmentUniformVectors    = 16;
    resources->MaxDrawBuffers               = 1;

    // Extensions.
    resources->OES_standard_derivatives        = 0;
    resources->OES_EGL_image_external          = 0;
    resources->OES_EGL_image_external_essl3    = 0;
    resources->NV_EGL_stream_consumer_external = 0;
    resources->ARB_texture_rectangle           = 0;
    resources->EXT_blend_func_extended         = 0;
    resources->EXT_draw_buffers                = 0;
    resources->EXT_frag_depth                  = 0;
    resources->EXT_shader_texture_lod          = 0;
    resources->WEBGL_debug_shader_precision    = 0;
    resources->EXT_shader_framebuffer_fetch    = 0;
    resources->NV_shader_framebuffer_fetch     = 0;
    resources->ARM_shader_framebuffer_fetch    = 0;
    resources->OVR_multiview                   = 0;
    resources->EXT_YUV_target                  = 0;
    resources->OES_geometry_shader             = 0;

    resources->NV_draw_buffers = 0;

    // Disable highp precision in fragment shader by default.
    resources->FragmentPrecisionHigh = 0;

    // GLSL ES 3.0 constants.
    resources->MaxVertexOutputVectors  = 16;
    resources->MaxFragmentInputVectors = 15;
    resources->MinProgramTexelOffset   = -8;
    resources->MaxProgramTexelOffset   = 7;

    // Extensions constants.
    resources->MaxDualSourceDrawBuffers = 0;

    resources->MaxViewsOVR = 4;

    // Disable name hashing by default.
    resources->HashFunction = nullptr;

    resources->ArrayIndexClampingStrategy = SH_CLAMP_WITH_CLAMP_INTRINSIC;

    resources->MaxExpressionComplexity = 256;
    resources->MaxCallStackDepth       = 256;
    resources->MaxFunctionParameters   = 1024;

    // ES 3.1 Revision 4, 7.2 Built-in Constants

    // ES 3.1, Revision 4, 8.13 Texture minification
    // "The value of MIN_PROGRAM_TEXTURE_GATHER_OFFSET must be less than or equal to the value of
    // MIN_PROGRAM_TEXEL_OFFSET. The value of MAX_PROGRAM_TEXTURE_GATHER_OFFSET must be greater than
    // or equal to the value of MAX_PROGRAM_TEXEL_OFFSET"
    resources->MinProgramTextureGatherOffset = -8;
    resources->MaxProgramTextureGatherOffset = 7;

    resources->MaxImageUnits            = 4;
    resources->MaxVertexImageUniforms   = 0;
    resources->MaxFragmentImageUniforms = 0;
    resources->MaxComputeImageUniforms  = 4;
    resources->MaxCombinedImageUniforms = 4;

    resources->MaxUniformLocations = 1024;

    resources->MaxCombinedShaderOutputResources = 4;

    resources->MaxComputeWorkGroupCount[0] = 65535;
    resources->MaxComputeWorkGroupCount[1] = 65535;
    resources->MaxComputeWorkGroupCount[2] = 65535;
    resources->MaxComputeWorkGroupSize[0]  = 128;
    resources->MaxComputeWorkGroupSize[1]  = 128;
    resources->MaxComputeWorkGroupSize[2]  = 64;
    resources->MaxComputeUniformComponents = 512;
    resources->MaxComputeTextureImageUnits = 16;

    resources->MaxComputeAtomicCounters       = 8;
    resources->MaxComputeAtomicCounterBuffers = 1;

    resources->MaxVertexAtomicCounters   = 0;
    resources->MaxFragmentAtomicCounters = 0;
    resources->MaxCombinedAtomicCounters = 8;
    resources->MaxAtomicCounterBindings  = 1;

    resources->MaxVertexAtomicCounterBuffers   = 0;
    resources->MaxFragmentAtomicCounterBuffers = 0;
    resources->MaxCombinedAtomicCounterBuffers = 1;
    resources->MaxAtomicCounterBufferSize      = 32;

    resources->MaxUniformBufferBindings = 32;
    resources->MaxShaderStorageBufferBindings = 4;

    resources->MaxGeometryUniformComponents     = 1024;
    resources->MaxGeometryUniformBlocks         = 12;
    resources->MaxGeometryInputComponents       = 64;
    resources->MaxGeometryOutputComponents      = 64;
    resources->MaxGeometryOutputVertices        = 256;
    resources->MaxGeometryTotalOutputComponents = 1024;
    resources->MaxGeometryTextureImageUnits     = 16;
    resources->MaxGeometryAtomicCounterBuffers  = 0;
    resources->MaxGeometryAtomicCounters        = 0;
    resources->MaxGeometryShaderStorageBlocks   = 0;
    resources->MaxGeometryShaderInvocations     = 32;
    resources->MaxGeometryImageUniforms         = 0;
}

//
// Driver calls these to create and destroy compiler objects.
//
ShHandle ConstructCompiler(sh::GLenum type,
                           ShShaderSpec spec,
                           ShShaderOutput output,
                           const ShBuiltInResources *resources)
{
    TShHandleBase *base = static_cast<TShHandleBase *>(ConstructCompiler(type, spec, output));
    if (base == nullptr)
    {
        return 0;
    }

    TCompiler *compiler = base->getAsCompiler();
    if (compiler == nullptr)
    {
        return 0;
    }

    // Generate built-in symbol table.
    if (!compiler->Init(*resources))
    {
        Destruct(base);
        return 0;
    }

    return reinterpret_cast<void *>(base);
}

void Destruct(ShHandle handle)
{
    if (handle == 0)
        return;

    TShHandleBase *base = static_cast<TShHandleBase *>(handle);

    if (base->getAsCompiler())
        DeleteCompiler(base->getAsCompiler());
}

const std::string &GetBuiltInResourcesString(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    ASSERT(compiler);
    return compiler->getBuiltInResourcesString();
}

//
// Do an actual compile on the given strings.  The result is left
// in the given compile object.
//
// Return:  The return value of ShCompile is really boolean, indicating
// success or failure.
//
bool Compile(const ShHandle handle,
             const char *const shaderStrings[],
             size_t numStrings,
             ShCompileOptions compileOptions)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    ASSERT(compiler);

    return compiler->compile(shaderStrings, numStrings, compileOptions);
}

void ClearResults(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    ASSERT(compiler);
    compiler->clearResults();
}

int GetShaderVersion(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    ASSERT(compiler);
    return compiler->getShaderVersion();
}

ShShaderOutput GetShaderOutputType(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    ASSERT(compiler);
    return compiler->getOutputType();
}

//
// Return any compiler log of messages for the application.
//
const std::string &GetInfoLog(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    ASSERT(compiler);

    TInfoSink &infoSink = compiler->getInfoSink();
    return infoSink.info.str();
}

//
// Return any object code.
//
const std::string &GetObjectCode(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    ASSERT(compiler);

    TInfoSink &infoSink = compiler->getInfoSink();
    return infoSink.obj.str();
}

const std::map<std::string, std::string> *GetNameHashingMap(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    ASSERT(compiler);
    return &(compiler->getNameMap());
}

const std::vector<Uniform> *GetUniforms(const ShHandle handle)
{
    return GetShaderVariables<Uniform>(handle);
}

const std::vector<Varying> *GetInputVaryings(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    if (compiler == nullptr)
    {
        return nullptr;
    }
    return &compiler->getInputVaryings();
}

const std::vector<Varying> *GetOutputVaryings(const ShHandle handle)
{
    TCompiler *compiler = GetCompilerFromHandle(handle);
    if (compiler == nullptr)
    {
        return nullptr;
    }
    return &compiler->getOutputVaryings();
}

const std::vector<Varying> *GetVaryings(const ShHandle handle)
{
    return GetShaderVariables<Varying>(handle);
}

const std::vector<Attribute> *GetAttributes(const ShHandle handle)
{
    return GetShaderVariables<Attribute>(handle);
}

const std::vector<OutputVariable> *GetOutputVariables(const ShHandle handle)
{
    return GetShaderVariables<OutputVariable>(handle);
}

const std::vector<InterfaceBlock> *GetInterfaceBlocks(const ShHandle handle)
{
    return GetShaderVariables<InterfaceBlock>(handle);
}

const std::vector<InterfaceBlock> *GetUniformBlocks(const ShHandle handle)
{
    ASSERT(handle);
    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    TCompiler *compiler = base->getAsCompiler();
    ASSERT(compiler);

    return &compiler->getUniformBlocks();
}

const std::vector<InterfaceBlock> *GetShaderStorageBlocks(const ShHandle handle)
{
    ASSERT(handle);
    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    TCompiler *compiler = base->getAsCompiler();
    ASSERT(compiler);

    return &compiler->getShaderStorageBlocks();
}

WorkGroupSize GetComputeShaderLocalGroupSize(const ShHandle handle)
{
    ASSERT(handle);

    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    TCompiler *compiler = base->getAsCompiler();
    ASSERT(compiler);

    return compiler->getComputeShaderLocalSize();
}

int GetVertexShaderNumViews(const ShHandle handle)
{
    ASSERT(handle);
    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    TCompiler *compiler = base->getAsCompiler();
    ASSERT(compiler);

    return compiler->getNumViews();
}

bool CheckVariablesWithinPackingLimits(int maxVectors, const std::vector<ShaderVariable> &variables)
{
    return CheckVariablesInPackingLimits(maxVectors, variables);
}

bool GetUniformBlockRegister(const ShHandle handle,
                             const std::string &uniformBlockName,
                             unsigned int *indexOut)
{
#ifdef ANGLE_ENABLE_HLSL
    ASSERT(indexOut);

    TranslatorHLSL *translator = GetTranslatorHLSLFromHandle(handle);
    ASSERT(translator);

    if (!translator->hasUniformBlock(uniformBlockName))
    {
        return false;
    }

    *indexOut = translator->getUniformBlockRegister(uniformBlockName);
    return true;
#else
    return false;
#endif  // ANGLE_ENABLE_HLSL
}

const std::map<std::string, unsigned int> *GetUniformRegisterMap(const ShHandle handle)
{
#ifdef ANGLE_ENABLE_HLSL
    TranslatorHLSL *translator = GetTranslatorHLSLFromHandle(handle);
    ASSERT(translator);

    return translator->getUniformRegisterMap();
#else
    return nullptr;
#endif  // ANGLE_ENABLE_HLSL
}

GLenum GetGeometryShaderInputPrimitiveType(const ShHandle handle)
{
    ASSERT(handle);

    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    TCompiler *compiler = base->getAsCompiler();
    ASSERT(compiler);

    return GetGeometryShaderPrimitiveTypeEnum(compiler->getGeometryShaderInputPrimitiveType());
}

GLenum GetGeometryShaderOutputPrimitiveType(const ShHandle handle)
{
    ASSERT(handle);

    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    TCompiler *compiler = base->getAsCompiler();
    ASSERT(compiler);

    return GetGeometryShaderPrimitiveTypeEnum(compiler->getGeometryShaderOutputPrimitiveType());
}

int GetGeometryShaderInvocations(const ShHandle handle)
{
    ASSERT(handle);

    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    TCompiler *compiler = base->getAsCompiler();
    ASSERT(compiler);

    return compiler->getGeometryShaderInvocations();
}

int GetGeometryShaderMaxVertices(const ShHandle handle)
{
    ASSERT(handle);

    TShHandleBase *base = static_cast<TShHandleBase *>(handle);
    TCompiler *compiler = base->getAsCompiler();
    ASSERT(compiler);

    return compiler->getGeometryShaderMaxVertices();
}

}  // namespace sh

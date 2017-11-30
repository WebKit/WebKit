//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DynamicHLSL.cpp: Implementation for link and run-time HLSL generation
//

#include "libANGLE/renderer/d3d/DynamicHLSL.h"

#include "common/string_utils.h"
#include "common/utilities.h"
#include "compiler/translator/blocklayoutHLSL.h"
#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/Shader.h"
#include "libANGLE/VaryingPacking.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"

using namespace gl;

namespace rx
{

namespace
{

// This class needs to match OutputHLSL::decorate
class DecorateVariable final : angle::NonCopyable
{
  public:
    explicit DecorateVariable(const std::string &str) : mName(str) {}
    const std::string &getName() const { return mName; }

  private:
    const std::string &mName;
};

std::ostream &operator<<(std::ostream &o, const DecorateVariable &dv)
{
    if (dv.getName().compare(0, 3, "gl_") != 0)
    {
        o << "_";
    }
    o << dv.getName();
    return o;
}

const char *HLSLComponentTypeString(GLenum componentType)
{
    switch (componentType)
    {
        case GL_UNSIGNED_INT:
            return "uint";
        case GL_INT:
            return "int";
        case GL_UNSIGNED_NORMALIZED:
        case GL_SIGNED_NORMALIZED:
        case GL_FLOAT:
            return "float";
        default:
            UNREACHABLE();
            return "not-component-type";
    }
}

void HLSLComponentTypeString(std::ostringstream &ostream, GLenum componentType, int componentCount)
{
    ostream << HLSLComponentTypeString(componentType);
    if (componentCount > 1)
    {
        ostream << componentCount;
    }
}

const char *HLSLMatrixTypeString(GLenum type)
{
    switch (type)
    {
        case GL_FLOAT_MAT2:
            return "float2x2";
        case GL_FLOAT_MAT3:
            return "float3x3";
        case GL_FLOAT_MAT4:
            return "float4x4";
        case GL_FLOAT_MAT2x3:
            return "float2x3";
        case GL_FLOAT_MAT3x2:
            return "float3x2";
        case GL_FLOAT_MAT2x4:
            return "float2x4";
        case GL_FLOAT_MAT4x2:
            return "float4x2";
        case GL_FLOAT_MAT3x4:
            return "float3x4";
        case GL_FLOAT_MAT4x3:
            return "float4x3";
        default:
            UNREACHABLE();
            return "not-matrix-type";
    }
}

void HLSLTypeString(std::ostringstream &ostream, GLenum type)
{
    if (gl::IsMatrixType(type))
    {
        ostream << HLSLMatrixTypeString(type);
        return;
    }

    HLSLComponentTypeString(ostream, gl::VariableComponentType(type),
                            gl::VariableComponentCount(type));
}

const PixelShaderOutputVariable *FindOutputAtLocation(
    const std::vector<PixelShaderOutputVariable> &outputVariables,
    unsigned int location)
{
    for (size_t variableIndex = 0; variableIndex < outputVariables.size(); ++variableIndex)
    {
        if (outputVariables[variableIndex].outputIndex == location)
        {
            return &outputVariables[variableIndex];
        }
    }

    return nullptr;
}

void WriteArrayString(std::ostringstream &strstr, unsigned int i)
{
    static_assert(GL_INVALID_INDEX == UINT_MAX,
                  "GL_INVALID_INDEX must be equal to the max unsigned int.");
    if (i == UINT_MAX)
    {
        return;
    }

    strstr << "[";
    strstr << i;
    strstr << "]";
}

constexpr const char *VERTEX_ATTRIBUTE_STUB_STRING = "@@ VERTEX ATTRIBUTES @@";
constexpr const char *PIXEL_OUTPUT_STUB_STRING     = "@@ PIXEL OUTPUT @@";
}  // anonymous namespace

// BuiltinInfo implementation

BuiltinInfo::BuiltinInfo()  = default;
BuiltinInfo::~BuiltinInfo() = default;

// DynamicHLSL implementation

DynamicHLSL::DynamicHLSL(RendererD3D *const renderer) : mRenderer(renderer)
{
}

std::string DynamicHLSL::generateVertexShaderForInputLayout(
    const std::string &sourceShader,
    const InputLayout &inputLayout,
    const std::vector<sh::Attribute> &shaderAttributes) const
{
    std::ostringstream structStream;
    std::ostringstream initStream;

    structStream << "struct VS_INPUT\n"
                 << "{\n";

    int semanticIndex       = 0;
    unsigned int inputIndex = 0;

    // If gl_PointSize is used in the shader then pointsprites rendering is expected.
    // If the renderer does not support Geometry shaders then Instanced PointSprite emulation
    // must be used.
    bool usesPointSize = sourceShader.find("GL_USES_POINT_SIZE") != std::string::npos;
    bool useInstancedPointSpriteEmulation =
        usesPointSize && mRenderer->getWorkarounds().useInstancedPointSpriteEmulation;

    // Instanced PointSprite emulation requires additional entries in the
    // VS_INPUT structure to support the vertices that make up the quad vertices.
    // These values must be in sync with the cooresponding values added during inputlayout creation
    // in InputLayoutCache::applyVertexBuffers().
    //
    // The additional entries must appear first in the VS_INPUT layout because
    // Windows Phone 8 era devices require per vertex data to physically come
    // before per instance data in the shader.
    if (useInstancedPointSpriteEmulation)
    {
        structStream << "    float3 spriteVertexPos : SPRITEPOSITION0;\n"
                     << "    float2 spriteTexCoord : SPRITETEXCOORD0;\n";
    }

    for (size_t attributeIndex = 0; attributeIndex < shaderAttributes.size(); ++attributeIndex)
    {
        const sh::Attribute &shaderAttribute = shaderAttributes[attributeIndex];
        if (!shaderAttribute.name.empty())
        {
            ASSERT(inputIndex < MAX_VERTEX_ATTRIBS);
            VertexFormatType vertexFormatType =
                inputIndex < inputLayout.size() ? inputLayout[inputIndex] : VERTEX_FORMAT_INVALID;

            // HLSL code for input structure
            if (IsMatrixType(shaderAttribute.type))
            {
                // Matrix types are always transposed
                structStream << "    "
                             << HLSLMatrixTypeString(TransposeMatrixType(shaderAttribute.type));
            }
            else
            {
                GLenum componentType = mRenderer->getVertexComponentType(vertexFormatType);

                if (shaderAttribute.name == "gl_InstanceID" ||
                    shaderAttribute.name == "gl_VertexID")
                {
                    // The input types of the instance ID and vertex ID in HLSL (uint) differs from
                    // the ones in ESSL (int).
                    structStream << " uint";
                }
                else
                {
                    structStream << "    ";
                    HLSLComponentTypeString(structStream, componentType,
                                            VariableComponentCount(shaderAttribute.type));
                }
            }

            structStream << " " << DecorateVariable(shaderAttribute.name) << " : ";

            if (shaderAttribute.name == "gl_InstanceID")
            {
                structStream << "SV_InstanceID";
            }
            else if (shaderAttribute.name == "gl_VertexID")
            {
                structStream << "SV_VertexID";
            }
            else
            {
                structStream << "TEXCOORD" << semanticIndex;
                semanticIndex += VariableRegisterCount(shaderAttribute.type);
            }

            structStream << ";\n";

            // HLSL code for initialization
            initStream << "    " << DecorateVariable(shaderAttribute.name) << " = ";

            // Mismatched vertex attribute to vertex input may result in an undefined
            // data reinterpretation (eg for pure integer->float, float->pure integer)
            // TODO: issue warning with gl debug info extension, when supported
            if (IsMatrixType(shaderAttribute.type) ||
                (mRenderer->getVertexConversionType(vertexFormatType) & VERTEX_CONVERT_GPU) != 0)
            {
                GenerateAttributeConversionHLSL(vertexFormatType, shaderAttribute, initStream);
            }
            else
            {
                initStream << "input." << DecorateVariable(shaderAttribute.name);
            }

            initStream << ";\n";

            inputIndex += VariableRowCount(TransposeMatrixType(shaderAttribute.type));
        }
    }

    structStream << "};\n"
                    "\n"
                    "void initAttributes(VS_INPUT input)\n"
                    "{\n"
                 << initStream.str() << "}\n";

    std::string vertexHLSL(sourceShader);

    bool success =
        angle::ReplaceSubstring(&vertexHLSL, VERTEX_ATTRIBUTE_STUB_STRING, structStream.str());
    ASSERT(success);

    return vertexHLSL;
}

std::string DynamicHLSL::generatePixelShaderForOutputSignature(
    const std::string &sourceShader,
    const std::vector<PixelShaderOutputVariable> &outputVariables,
    bool usesFragDepth,
    const std::vector<GLenum> &outputLayout) const
{
    const int shaderModel      = mRenderer->getMajorShaderModel();
    std::string targetSemantic = (shaderModel >= 4) ? "SV_TARGET" : "COLOR";
    std::string depthSemantic  = (shaderModel >= 4) ? "SV_Depth" : "DEPTH";

    std::ostringstream declarationStream;
    std::ostringstream copyStream;

    declarationStream << "struct PS_OUTPUT\n"
                         "{\n";

    size_t numOutputs = outputLayout.size();

    // Workaround for HLSL 3.x: We can't do a depth/stencil only render, the runtime will complain.
    if (numOutputs == 0 && (shaderModel == 3 || !mRenderer->getShaderModelSuffix().empty()))
    {
        numOutputs = 1u;
    }
    const PixelShaderOutputVariable defaultOutput(GL_FLOAT_VEC4, "dummy", "float4(0, 0, 0, 1)", 0);

    for (size_t layoutIndex = 0; layoutIndex < numOutputs; ++layoutIndex)
    {
        GLenum binding = outputLayout.empty() ? GL_COLOR_ATTACHMENT0 : outputLayout[layoutIndex];

        if (binding != GL_NONE)
        {
            unsigned int location = (binding - GL_COLOR_ATTACHMENT0);

            const PixelShaderOutputVariable *outputVariable =
                outputLayout.empty() ? &defaultOutput
                                     : FindOutputAtLocation(outputVariables, location);

            // OpenGL ES 3.0 spec $4.2.1
            // If [...] not all user-defined output variables are written, the values of fragment
            // colors
            // corresponding to unwritten variables are similarly undefined.
            if (outputVariable)
            {
                declarationStream << "    ";
                HLSLTypeString(declarationStream, outputVariable->type);
                declarationStream << " " << outputVariable->name << " : " << targetSemantic
                                  << static_cast<int>(layoutIndex) << ";\n";

                copyStream << "    output." << outputVariable->name << " = "
                           << outputVariable->source << ";\n";
            }
        }
    }

    if (usesFragDepth)
    {
        declarationStream << "    float gl_Depth : " << depthSemantic << ";\n";
        copyStream << "    output.gl_Depth = gl_Depth; \n";
    }

    declarationStream << "};\n"
                         "\n"
                         "PS_OUTPUT generateOutput()\n"
                         "{\n"
                         "    PS_OUTPUT output;\n"
                      << copyStream.str() << "    return output;\n"
                                             "}\n";

    std::string pixelHLSL(sourceShader);

    bool success =
        angle::ReplaceSubstring(&pixelHLSL, PIXEL_OUTPUT_STUB_STRING, declarationStream.str());
    ASSERT(success);

    return pixelHLSL;
}

void DynamicHLSL::generateVaryingLinkHLSL(const VaryingPacking &varyingPacking,
                                          const BuiltinInfo &builtins,
                                          bool programUsesPointSize,
                                          std::ostringstream &hlslStream) const
{
    ASSERT(builtins.dxPosition.enabled);
    hlslStream << "{\n"
               << "    float4 dx_Position : " << builtins.dxPosition.str() << ";\n";

    if (builtins.glPosition.enabled)
    {
        hlslStream << "    float4 gl_Position : " << builtins.glPosition.str() << ";\n";
    }

    if (builtins.glFragCoord.enabled)
    {
        hlslStream << "    float4 gl_FragCoord : " << builtins.glFragCoord.str() << ";\n";
    }

    if (builtins.glPointCoord.enabled)
    {
        hlslStream << "    float2 gl_PointCoord : " << builtins.glPointCoord.str() << ";\n";
    }

    if (builtins.glPointSize.enabled)
    {
        hlslStream << "    float gl_PointSize : " << builtins.glPointSize.str() << ";\n";
    }

    if (builtins.glViewIDOVR.enabled)
    {
        hlslStream << "    nointerpolation uint gl_ViewID_OVR : " << builtins.glViewIDOVR.str()
                   << ";\n";
    }

    if (builtins.glViewportIndex.enabled)
    {
        hlslStream << "    nointerpolation uint gl_ViewportIndex : "
                   << builtins.glViewportIndex.str() << ";\n";
    }

    if (builtins.glLayer.enabled)
    {
        hlslStream << "    nointerpolation uint gl_Layer : " << builtins.glLayer.str() << ";\n";
    }

    std::string varyingSemantic =
        GetVaryingSemantic(mRenderer->getMajorShaderModel(), programUsesPointSize);

    for (const PackedVaryingRegister &registerInfo : varyingPacking.getRegisterList())
    {
        const auto &varying = *registerInfo.packedVarying->varying;
        ASSERT(!varying.isStruct());

        // TODO: Add checks to ensure D3D interpolation modifiers don't result in too many
        // registers being used.
        // For example, if there are N registers, and we have N vec3 varyings and 1 float
        // varying, then D3D will pack them into N registers.
        // If the float varying has the 'nointerpolation' modifier on it then we would need
        // N + 1 registers, and D3D compilation will fail.

        switch (registerInfo.packedVarying->interpolation)
        {
            case sh::INTERPOLATION_SMOOTH:
                hlslStream << "    ";
                break;
            case sh::INTERPOLATION_FLAT:
                hlslStream << "    nointerpolation ";
                break;
            case sh::INTERPOLATION_CENTROID:
                hlslStream << "    centroid ";
                break;
            default:
                UNREACHABLE();
        }

        GLenum transposedType = gl::TransposeMatrixType(varying.type);
        GLenum componentType  = gl::VariableComponentType(transposedType);
        int columnCount       = gl::VariableColumnCount(transposedType);
        HLSLComponentTypeString(hlslStream, componentType, columnCount);
        unsigned int semanticIndex = registerInfo.semanticIndex;
        hlslStream << " v" << semanticIndex << " : " << varyingSemantic << semanticIndex << ";\n";
    }

    hlslStream << "};\n";
}

void DynamicHLSL::generateShaderLinkHLSL(const gl::Context *context,
                                         const gl::ProgramState &programData,
                                         const ProgramD3DMetadata &programMetadata,
                                         const VaryingPacking &varyingPacking,
                                         const BuiltinVaryingsD3D &builtinsD3D,
                                         std::string *pixelHLSL,
                                         std::string *vertexHLSL) const
{
    ASSERT(pixelHLSL->empty() && vertexHLSL->empty());

    const auto &data                   = context->getContextState();
    gl::Shader *vertexShaderGL         = programData.getAttachedVertexShader();
    gl::Shader *fragmentShaderGL       = programData.getAttachedFragmentShader();
    const ShaderD3D *fragmentShader    = GetImplAs<ShaderD3D>(fragmentShaderGL);
    const int shaderModel              = mRenderer->getMajorShaderModel();

    // usesViewScale() isn't supported in the D3D9 renderer
    ASSERT(shaderModel >= 4 || !programMetadata.usesViewScale());

    bool useInstancedPointSpriteEmulation =
        programMetadata.usesPointSize() &&
        mRenderer->getWorkarounds().useInstancedPointSpriteEmulation;

    // Validation done in the compiler
    ASSERT(!fragmentShader->usesFragColor() || !fragmentShader->usesFragData());

    std::ostringstream vertexStream;
    vertexStream << vertexShaderGL->getTranslatedSource(context);

    // Instanced PointSprite emulation requires additional entries originally generated in the
    // GeometryShader HLSL. These include pointsize clamp values.
    if (useInstancedPointSpriteEmulation)
    {
        vertexStream << "static float minPointSize = "
                     << static_cast<int>(data.getCaps().minAliasedPointSize) << ".0f;\n"
                     << "static float maxPointSize = "
                     << static_cast<int>(data.getCaps().maxAliasedPointSize) << ".0f;\n";
    }

    // Add stub string to be replaced when shader is dynamically defined by its layout
    vertexStream << "\n" << std::string(VERTEX_ATTRIBUTE_STUB_STRING) << "\n";

    const auto &vertexBuiltins = builtinsD3D[gl::SHADER_VERTEX];

    // Write the HLSL input/output declarations
    vertexStream << "struct VS_OUTPUT\n";
    generateVaryingLinkHLSL(varyingPacking, vertexBuiltins, builtinsD3D.usesPointSize(),
                            vertexStream);
    vertexStream << "\n"
                 << "VS_OUTPUT main(VS_INPUT input)\n"
                 << "{\n"
                 << "    initAttributes(input);\n";

    vertexStream << "\n"
                 << "    gl_main();\n"
                 << "\n"
                 << "    VS_OUTPUT output;\n";

    if (vertexBuiltins.glPosition.enabled)
    {
        vertexStream << "    output.gl_Position = gl_Position;\n";
    }

    if (vertexBuiltins.glViewIDOVR.enabled)
    {
        vertexStream << "    output.gl_ViewID_OVR = _ViewID_OVR;\n";
    }
    if (programMetadata.hasANGLEMultiviewEnabled() && programMetadata.canSelectViewInVertexShader())
    {
        ASSERT(vertexBuiltins.glViewportIndex.enabled && vertexBuiltins.glLayer.enabled);
        vertexStream << "    if (multiviewSelectViewportIndex)\n"
                     << "    {\n"
                     << "         output.gl_ViewportIndex = _ViewID_OVR;\n"
                     << "    } else {\n"
                     << "         output.gl_ViewportIndex = 0;\n"
                     << "         output.gl_Layer = _ViewID_OVR;\n"
                     << "    }\n";
    }

    // On D3D9 or D3D11 Feature Level 9, we need to emulate large viewports using dx_ViewAdjust.
    if (shaderModel >= 4 && mRenderer->getShaderModelSuffix() == "")
    {
        vertexStream << "    output.dx_Position.x = gl_Position.x;\n";

        if (programMetadata.usesViewScale())
        {
            // This code assumes that dx_ViewScale.y = -1.0f when rendering to texture, and +1.0f
            // when rendering to the default framebuffer. No other values are valid.
            vertexStream << "    output.dx_Position.y = dx_ViewScale.y * gl_Position.y;\n";
        }
        else
        {
            vertexStream << "    output.dx_Position.y = - gl_Position.y;\n";
        }

        vertexStream << "    output.dx_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n"
                     << "    output.dx_Position.w = gl_Position.w;\n";
    }
    else
    {
        vertexStream << "    output.dx_Position.x = gl_Position.x * dx_ViewAdjust.z + "
                        "dx_ViewAdjust.x * gl_Position.w;\n";

        // If usesViewScale() is true and we're using the D3D11 renderer via Feature Level 9_*,
        // then we need to multiply the gl_Position.y by the viewScale.
        // usesViewScale() isn't supported when using the D3D9 renderer.
        if (programMetadata.usesViewScale() &&
            (shaderModel >= 4 && mRenderer->getShaderModelSuffix() != ""))
        {
            vertexStream << "    output.dx_Position.y = dx_ViewScale.y * (gl_Position.y * "
                            "dx_ViewAdjust.w + dx_ViewAdjust.y * gl_Position.w);\n";
        }
        else
        {
            vertexStream << "    output.dx_Position.y = -(gl_Position.y * dx_ViewAdjust.w + "
                            "dx_ViewAdjust.y * gl_Position.w);\n";
        }

        vertexStream << "    output.dx_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n"
                     << "    output.dx_Position.w = gl_Position.w;\n";
    }

    // We don't need to output gl_PointSize if we use are emulating point sprites via instancing.
    if (vertexBuiltins.glPointSize.enabled)
    {
        vertexStream << "    output.gl_PointSize = gl_PointSize;\n";
    }

    if (vertexBuiltins.glFragCoord.enabled)
    {
        vertexStream << "    output.gl_FragCoord = gl_Position;\n";
    }

    for (const PackedVaryingRegister &registerInfo : varyingPacking.getRegisterList())
    {
        const auto &packedVarying = *registerInfo.packedVarying;
        const auto &varying = *packedVarying.varying;
        ASSERT(!varying.isStruct());

        vertexStream << "    output.v" << registerInfo.semanticIndex << " = ";

        if (packedVarying.isStructField())
        {
            vertexStream << DecorateVariable(packedVarying.parentStructName) << ".";
        }

        vertexStream << DecorateVariable(varying.name);

        if (varying.isArray())
        {
            WriteArrayString(vertexStream, registerInfo.varyingArrayIndex);
        }

        if (VariableRowCount(varying.type) > 1)
        {
            WriteArrayString(vertexStream, registerInfo.varyingRowIndex);
        }

        vertexStream << ";\n";
    }

    // Instanced PointSprite emulation requires additional entries to calculate
    // the final output vertex positions of the quad that represents each sprite.
    if (useInstancedPointSpriteEmulation)
    {
        vertexStream << "\n"
                     << "    gl_PointSize = clamp(gl_PointSize, minPointSize, maxPointSize);\n";

        vertexStream << "    output.dx_Position.x += (input.spriteVertexPos.x * gl_PointSize / "
                        "(dx_ViewCoords.x*2)) * output.dx_Position.w;";

        if (programMetadata.usesViewScale())
        {
            // Multiply by ViewScale to invert the rendering when appropriate
            vertexStream << "    output.dx_Position.y += (-dx_ViewScale.y * "
                            "input.spriteVertexPos.y * gl_PointSize / (dx_ViewCoords.y*2)) * "
                            "output.dx_Position.w;";
        }
        else
        {
            vertexStream << "    output.dx_Position.y += (input.spriteVertexPos.y * gl_PointSize / "
                            "(dx_ViewCoords.y*2)) * output.dx_Position.w;";
        }

        vertexStream
            << "    output.dx_Position.z += input.spriteVertexPos.z * output.dx_Position.w;\n";

        if (programMetadata.usesPointCoord())
        {
            vertexStream << "\n"
                         << "    output.gl_PointCoord = input.spriteTexCoord;\n";
        }
    }

    // Renderers that enable instanced pointsprite emulation require the vertex shader output member
    // gl_PointCoord to be set to a default value if used without gl_PointSize. 0.5,0.5 is the same
    // default value used in the generated pixel shader.
    if (programMetadata.usesInsertedPointCoordValue())
    {
        ASSERT(!useInstancedPointSpriteEmulation);
        vertexStream << "\n"
                     << "    output.gl_PointCoord = float2(0.5, 0.5);\n";
    }

    vertexStream << "\n"
                 << "    return output;\n"
                 << "}\n";

    const auto &pixelBuiltins = builtinsD3D[gl::SHADER_FRAGMENT];

    std::ostringstream pixelStream;
    pixelStream << fragmentShaderGL->getTranslatedSource(context);
    pixelStream << "struct PS_INPUT\n";
    generateVaryingLinkHLSL(varyingPacking, pixelBuiltins, builtinsD3D.usesPointSize(),
                            pixelStream);
    pixelStream << "\n";

    pixelStream << std::string(PIXEL_OUTPUT_STUB_STRING) << "\n";

    if (fragmentShader->usesFrontFacing())
    {
        if (shaderModel >= 4)
        {
            pixelStream << "PS_OUTPUT main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace)\n"
                        << "{\n";
        }
        else
        {
            pixelStream << "PS_OUTPUT main(PS_INPUT input, float vFace : VFACE)\n"
                        << "{\n";
        }
    }
    else
    {
        pixelStream << "PS_OUTPUT main(PS_INPUT input)\n"
                    << "{\n";
    }

    if (fragmentShader->usesViewID())
    {
        ASSERT(pixelBuiltins.glViewIDOVR.enabled);
        pixelStream << "    _ViewID_OVR = input.gl_ViewID_OVR;\n";
    }

    if (pixelBuiltins.glFragCoord.enabled)
    {
        pixelStream << "    float rhw = 1.0 / input.gl_FragCoord.w;\n";

        // Certain Shader Models (4_0+ and 3_0) allow reading from dx_Position in the pixel shader.
        // Other Shader Models (4_0_level_9_3 and 2_x) don't support this, so we emulate it using
        // dx_ViewCoords.
        if (shaderModel >= 4 && mRenderer->getShaderModelSuffix() == "")
        {
            pixelStream << "    gl_FragCoord.x = input.dx_Position.x;\n"
                        << "    gl_FragCoord.y = input.dx_Position.y;\n";
        }
        else if (shaderModel == 3)
        {
            pixelStream << "    gl_FragCoord.x = input.dx_Position.x + 0.5;\n"
                        << "    gl_FragCoord.y = input.dx_Position.y + 0.5;\n";
        }
        else
        {
            // dx_ViewCoords contains the viewport width/2, height/2, center.x and center.y. See
            // Renderer::setViewport()
            pixelStream << "    gl_FragCoord.x = (input.gl_FragCoord.x * rhw) * dx_ViewCoords.x + "
                           "dx_ViewCoords.z;\n"
                        << "    gl_FragCoord.y = (input.gl_FragCoord.y * rhw) * dx_ViewCoords.y + "
                           "dx_ViewCoords.w;\n";
        }

        if (programMetadata.usesViewScale())
        {
            // For Feature Level 9_3 and below, we need to correct gl_FragCoord.y to account
            // for dx_ViewScale. On Feature Level 10_0+, gl_FragCoord.y is calculated above using
            // dx_ViewCoords and is always correct irrespective of dx_ViewScale's value.
            // NOTE: usesViewScale() can only be true on D3D11 (i.e. Shader Model 4.0+).
            if (shaderModel >= 4 && mRenderer->getShaderModelSuffix() == "")
            {
                // Some assumptions:
                //  - dx_ViewScale.y = -1.0f when rendering to texture
                //  - dx_ViewScale.y = +1.0f when rendering to the default framebuffer
                //  - gl_FragCoord.y has been set correctly above.
                //
                // When rendering to the backbuffer, the code inverts gl_FragCoord's y coordinate.
                // This involves subtracting the y coordinate from the height of the area being
                // rendered to.
                //
                // First we calculate the height of the area being rendered to:
                //    render_area_height = (2.0f / (1.0f - input.gl_FragCoord.y * rhw)) *
                //    gl_FragCoord.y
                //
                // Note that when we're rendering to default FB, we want our output to be
                // equivalent to:
                //    "gl_FragCoord.y = render_area_height - gl_FragCoord.y"
                //
                // When we're rendering to a texture, we want our output to be equivalent to:
                //    "gl_FragCoord.y = gl_FragCoord.y;"
                //
                // If we set scale_factor = ((1.0f + dx_ViewScale.y) / 2.0f), then notice that
                //  - When rendering to default FB: scale_factor = 1.0f
                //  - When rendering to texture:    scale_factor = 0.0f
                //
                // Therefore, we can get our desired output by setting:
                //    "gl_FragCoord.y = scale_factor * render_area_height - dx_ViewScale.y *
                //    gl_FragCoord.y"
                //
                // Simplifying, this becomes:
                pixelStream
                    << "    gl_FragCoord.y = (1.0f + dx_ViewScale.y) * gl_FragCoord.y /"
                       "(1.0f - input.gl_FragCoord.y * rhw)  - dx_ViewScale.y * gl_FragCoord.y;\n";
            }
        }

        pixelStream << "    gl_FragCoord.z = (input.gl_FragCoord.z * rhw) * dx_DepthFront.x + "
                       "dx_DepthFront.y;\n"
                    << "    gl_FragCoord.w = rhw;\n";
    }

    if (pixelBuiltins.glPointCoord.enabled && shaderModel >= 3)
    {
        pixelStream << "    gl_PointCoord.x = input.gl_PointCoord.x;\n"
                    << "    gl_PointCoord.y = 1.0 - input.gl_PointCoord.y;\n";
    }

    if (fragmentShader->usesFrontFacing())
    {
        if (shaderModel <= 3)
        {
            pixelStream << "    gl_FrontFacing = (vFace * dx_DepthFront.z >= 0.0);\n";
        }
        else
        {
            pixelStream << "    gl_FrontFacing = isFrontFace;\n";
        }
    }

    for (const PackedVaryingRegister &registerInfo : varyingPacking.getRegisterList())
    {
        const auto &packedVarying = *registerInfo.packedVarying;
        const auto &varying = *packedVarying.varying;
        ASSERT(!varying.isBuiltIn() && !varying.isStruct());

        // Don't reference VS-only transform feedback varyings in the PS. Note that we're relying on
        // that the staticUse flag is set according to usage in the fragment shader.
        if (packedVarying.vertexOnly || !varying.staticUse)
            continue;

        pixelStream << "    ";

        if (packedVarying.isStructField())
        {
            pixelStream << DecorateVariable(packedVarying.parentStructName) << ".";
        }

        pixelStream << DecorateVariable(varying.name);

        if (varying.isArray())
        {
            WriteArrayString(pixelStream, registerInfo.varyingArrayIndex);
        }

        GLenum transposedType = TransposeMatrixType(varying.type);
        if (VariableRowCount(transposedType) > 1)
        {
            WriteArrayString(pixelStream, registerInfo.varyingRowIndex);
        }

        pixelStream << " = input.v" << registerInfo.semanticIndex;

        switch (VariableColumnCount(transposedType))
        {
            case 1:
                pixelStream << ".x";
                break;
            case 2:
                pixelStream << ".xy";
                break;
            case 3:
                pixelStream << ".xyz";
                break;
            case 4:
                break;
            default:
                UNREACHABLE();
        }
        pixelStream << ";\n";
    }

    pixelStream << "\n"
                << "    gl_main();\n"
                << "\n"
                << "    return generateOutput();\n"
                << "}\n";

    *vertexHLSL = vertexStream.str();
    *pixelHLSL  = pixelStream.str();
}

std::string DynamicHLSL::generateComputeShaderLinkHLSL(const gl::Context *context,
                                                       const gl::ProgramState &programData) const
{
    gl::Shader *computeShaderGL = programData.getAttachedComputeShader();
    std::stringstream computeStream;
    std::string translatedSource = computeShaderGL->getTranslatedSource(context);
    computeStream << translatedSource;

    bool usesWorkGroupID = translatedSource.find("GL_USES_WORK_GROUP_ID") != std::string::npos;
    bool usesLocalInvocationID =
        translatedSource.find("GL_USES_LOCAL_INVOCATION_ID") != std::string::npos;
    bool usesGlobalInvocationID =
        translatedSource.find("GL_USES_GLOBAL_INVOCATION_ID") != std::string::npos;
    bool usesLocalInvocationIndex =
        translatedSource.find("GL_USES_LOCAL_INVOCATION_INDEX") != std::string::npos;

    computeStream << "\nstruct CS_INPUT\n{\n";
    if (usesWorkGroupID)
    {
        computeStream << "    uint3 dx_WorkGroupID : "
                      << "SV_GroupID;\n";
    }

    if (usesLocalInvocationID)
    {
        computeStream << "    uint3 dx_LocalInvocationID : "
                      << "SV_GroupThreadID;\n";
    }

    if (usesGlobalInvocationID)
    {
        computeStream << "    uint3 dx_GlobalInvocationID : "
                      << "SV_DispatchThreadID;\n";
    }

    if (usesLocalInvocationIndex)
    {
        computeStream << "    uint dx_LocalInvocationIndex : "
                      << "SV_GroupIndex;\n";
    }

    computeStream << "};\n\n";

    const sh::WorkGroupSize &localSize = computeShaderGL->getWorkGroupSize(context);
    computeStream << "[numthreads(" << localSize[0] << ", " << localSize[1] << ", " << localSize[2]
                  << ")]\n";

    computeStream << "void main(CS_INPUT input)\n"
                  << "{\n";

    if (usesWorkGroupID)
    {
        computeStream << "    gl_WorkGroupID = input.dx_WorkGroupID;\n";
    }
    if (usesLocalInvocationID)
    {
        computeStream << "    gl_LocalInvocationID = input.dx_LocalInvocationID;\n";
    }
    if (usesGlobalInvocationID)
    {
        computeStream << "    gl_GlobalInvocationID = input.dx_GlobalInvocationID;\n";
    }
    if (usesLocalInvocationIndex)
    {
        computeStream << "    gl_LocalInvocationIndex = input.dx_LocalInvocationIndex;\n";
    }

    computeStream << "\n"
                  << "    gl_main();\n"
                  << "}\n";

    return computeStream.str();
}

std::string DynamicHLSL::generateGeometryShaderPreamble(const VaryingPacking &varyingPacking,
                                                        const BuiltinVaryingsD3D &builtinsD3D,
                                                        const bool hasANGLEMultiviewEnabled,
                                                        const bool selectViewInVS) const
{
    ASSERT(mRenderer->getMajorShaderModel() >= 4);

    std::ostringstream preambleStream;

    const auto &vertexBuiltins = builtinsD3D[gl::SHADER_VERTEX];

    preambleStream << "struct GS_INPUT\n";
    generateVaryingLinkHLSL(varyingPacking, vertexBuiltins, builtinsD3D.usesPointSize(),
                            preambleStream);
    preambleStream << "\n"
                   << "struct GS_OUTPUT\n";
    generateVaryingLinkHLSL(varyingPacking, builtinsD3D[gl::SHADER_GEOMETRY],
                            builtinsD3D.usesPointSize(), preambleStream);
    preambleStream
        << "\n"
        << "void copyVertex(inout GS_OUTPUT output, GS_INPUT input, GS_INPUT flatinput)\n"
        << "{\n"
        << "    output.gl_Position = input.gl_Position;\n";

    if (vertexBuiltins.glPointSize.enabled)
    {
        preambleStream << "    output.gl_PointSize = input.gl_PointSize;\n";
    }

    if (hasANGLEMultiviewEnabled)
    {
        preambleStream << "    output.gl_ViewID_OVR = input.gl_ViewID_OVR;\n";
        if (selectViewInVS)
        {
            ASSERT(builtinsD3D[gl::SHADER_GEOMETRY].glViewportIndex.enabled &&
                   builtinsD3D[gl::SHADER_GEOMETRY].glLayer.enabled);

            // If the view is already selected in the VS, then we just pass the gl_ViewportIndex and
            // gl_Layer to the output.
            preambleStream << "    output.gl_ViewportIndex = input.gl_ViewportIndex;\n"
                           << "    output.gl_Layer = input.gl_Layer;\n";
        }
    }

    for (const PackedVaryingRegister &varyingRegister : varyingPacking.getRegisterList())
    {
        preambleStream << "    output.v" << varyingRegister.semanticIndex << " = ";
        if (varyingRegister.packedVarying->interpolation == sh::INTERPOLATION_FLAT)
        {
            preambleStream << "flat";
        }
        preambleStream << "input.v" << varyingRegister.semanticIndex << "; \n";
    }

    if (vertexBuiltins.glFragCoord.enabled)
    {
        preambleStream << "    output.gl_FragCoord = input.gl_FragCoord;\n";
    }

    // Only write the dx_Position if we aren't using point sprites
    preambleStream << "#ifndef ANGLE_POINT_SPRITE_SHADER\n"
                   << "    output.dx_Position = input.dx_Position;\n"
                   << "#endif  // ANGLE_POINT_SPRITE_SHADER\n"
                   << "}\n";

    if (hasANGLEMultiviewEnabled && !selectViewInVS)
    {
        ASSERT(builtinsD3D[gl::SHADER_GEOMETRY].glViewportIndex.enabled &&
               builtinsD3D[gl::SHADER_GEOMETRY].glLayer.enabled);

        // According to the HLSL reference, using SV_RenderTargetArrayIndex is only valid if the
        // render target is an array resource. Because of this we do not write to gl_Layer if we are
        // taking the side-by-side code path. We still select the viewport index in the layered code
        // path as that is always valid. See:
        // https://msdn.microsoft.com/en-us/library/windows/desktop/bb509647(v=vs.85).aspx
        preambleStream << "\n"
                       << "void selectView(inout GS_OUTPUT output, GS_INPUT input)\n"
                       << "{\n"
                       << "    if (multiviewSelectViewportIndex)\n"
                       << "    {\n"
                       << "        output.gl_ViewportIndex = input.gl_ViewID_OVR;\n"
                       << "    } else {\n"
                       << "        output.gl_ViewportIndex = 0;\n"
                       << "        output.gl_Layer = input.gl_ViewID_OVR;\n"
                       << "    }\n"
                       << "}\n";
    }

    return preambleStream.str();
}

std::string DynamicHLSL::generateGeometryShaderHLSL(const gl::Context *context,
                                                    gl::PrimitiveType primitiveType,
                                                    const gl::ProgramState &programData,
                                                    const bool useViewScale,
                                                    const bool hasANGLEMultiviewEnabled,
                                                    const bool selectViewInVS,
                                                    const bool pointSpriteEmulation,
                                                    const std::string &preambleString) const
{
    ASSERT(mRenderer->getMajorShaderModel() >= 4);

    std::stringstream shaderStream;

    const bool pointSprites   = (primitiveType == PRIMITIVE_POINTS) && pointSpriteEmulation;
    const bool usesPointCoord = preambleString.find("gl_PointCoord") != std::string::npos;

    const char *inputPT  = nullptr;
    const char *outputPT = nullptr;
    int inputSize        = 0;
    int maxVertexOutput  = 0;

    switch (primitiveType)
    {
        case PRIMITIVE_POINTS:
            inputPT         = "point";
            inputSize       = 1;

            if (pointSprites)
            {
                outputPT        = "Triangle";
                maxVertexOutput = 4;
            }
            else
            {
                outputPT        = "Point";
                maxVertexOutput = 1;
            }

            break;

        case PRIMITIVE_LINES:
        case PRIMITIVE_LINE_STRIP:
        case PRIMITIVE_LINE_LOOP:
            inputPT         = "line";
            outputPT        = "Line";
            inputSize       = 2;
            maxVertexOutput = 2;
            break;

        case PRIMITIVE_TRIANGLES:
        case PRIMITIVE_TRIANGLE_STRIP:
        case PRIMITIVE_TRIANGLE_FAN:
            inputPT         = "triangle";
            outputPT        = "Triangle";
            inputSize       = 3;
            maxVertexOutput = 3;
            break;

        default:
            UNREACHABLE();
            break;
    }

    if (pointSprites || hasANGLEMultiviewEnabled)
    {
        shaderStream << "cbuffer DriverConstants : register(b0)\n"
                        "{\n";

        if (pointSprites)
        {
            shaderStream << "    float4 dx_ViewCoords : packoffset(c1);\n";
            if (useViewScale)
            {
                shaderStream << "    float2 dx_ViewScale : packoffset(c3);\n";
            }
        }

        if (hasANGLEMultiviewEnabled)
        {
            // We have to add a value which we can use to keep track of which multi-view code path
            // is to be selected in the GS.
            shaderStream << "    float multiviewSelectViewportIndex : packoffset(c3.z);\n";
        }

        shaderStream << "};\n\n";
    }

    if (pointSprites)
    {
        shaderStream << "#define ANGLE_POINT_SPRITE_SHADER\n"
                        "\n"
                        "static float2 pointSpriteCorners[] = \n"
                        "{\n"
                        "    float2( 0.5f, -0.5f),\n"
                        "    float2( 0.5f,  0.5f),\n"
                        "    float2(-0.5f, -0.5f),\n"
                        "    float2(-0.5f,  0.5f)\n"
                        "};\n"
                        "\n"
                        "static float2 pointSpriteTexcoords[] = \n"
                        "{\n"
                        "    float2(1.0f, 1.0f),\n"
                        "    float2(1.0f, 0.0f),\n"
                        "    float2(0.0f, 1.0f),\n"
                        "    float2(0.0f, 0.0f)\n"
                        "};\n"
                        "\n"
                        "static float minPointSize = "
                     << static_cast<int>(context->getCaps().minAliasedPointSize)
                     << ".0f;\n"
                        "static float maxPointSize = "
                     << static_cast<int>(context->getCaps().maxAliasedPointSize) << ".0f;\n"
                     << "\n";
    }

    shaderStream << preambleString << "\n"
                 << "[maxvertexcount(" << maxVertexOutput << ")]\n"
                 << "void main(" << inputPT << " GS_INPUT input[" << inputSize << "], ";

    if (primitiveType == PRIMITIVE_TRIANGLE_STRIP)
    {
        shaderStream << "uint primitiveID : SV_PrimitiveID, ";
    }

    shaderStream << " inout " << outputPT << "Stream<GS_OUTPUT> outStream)\n"
                 << "{\n"
                 << "    GS_OUTPUT output = (GS_OUTPUT)0;\n";

    if (primitiveType == PRIMITIVE_TRIANGLE_STRIP)
    {
        shaderStream << "    uint lastVertexIndex = (primitiveID % 2 == 0 ? 2 : 1);\n";
    }
    else
    {
        shaderStream << "    uint lastVertexIndex = " << (inputSize - 1) << ";\n";
    }

    for (int vertexIndex = 0; vertexIndex < inputSize; ++vertexIndex)
    {
        shaderStream << "    copyVertex(output, input[" << vertexIndex
                     << "], input[lastVertexIndex]);\n";
        if (hasANGLEMultiviewEnabled && !selectViewInVS)
        {
            shaderStream << "   selectView(output, input[" << vertexIndex << "]);\n";
        }
        if (!pointSprites)
        {
            ASSERT(inputSize == maxVertexOutput);
            shaderStream << "    outStream.Append(output);\n";
        }
    }

    if (pointSprites)
    {
        shaderStream << "\n"
                        "    float4 dx_Position = input[0].dx_Position;\n"
                        "    float gl_PointSize = clamp(input[0].gl_PointSize, minPointSize, "
                        "maxPointSize);\n"
                        "    float2 viewportScale = float2(1.0f / dx_ViewCoords.x, 1.0f / "
                        "dx_ViewCoords.y) * dx_Position.w;\n";

        for (int corner = 0; corner < 4; corner++)
        {
            if (useViewScale)
            {
                shaderStream << "    \n"
                                "    output.dx_Position = dx_Position + float4(1.0f, "
                                "-dx_ViewScale.y, 1.0f, 1.0f)"
                                "        * float4(pointSpriteCorners["
                             << corner << "] * viewportScale * gl_PointSize, 0.0f, 0.0f);\n";
            }
            else
            {
                shaderStream << "\n"
                                "    output.dx_Position = dx_Position + float4(pointSpriteCorners["
                             << corner << "] * viewportScale * gl_PointSize, 0.0f, 0.0f);\n";
            }

            if (usesPointCoord)
            {
                shaderStream << "    output.gl_PointCoord = pointSpriteTexcoords[" << corner
                             << "];\n";
            }

            shaderStream << "    outStream.Append(output);\n";
        }
    }

    shaderStream << "    \n"
                    "    outStream.RestartStrip();\n"
                    "}\n";

    return shaderStream.str();
}

// static
void DynamicHLSL::GenerateAttributeConversionHLSL(gl::VertexFormatType vertexFormatType,
                                                  const sh::ShaderVariable &shaderAttrib,
                                                  std::ostringstream &outStream)
{
    // Matrix
    if (IsMatrixType(shaderAttrib.type))
    {
        outStream << "transpose(input." << DecorateVariable(shaderAttrib.name) << ")";
        return;
    }

    GLenum shaderComponentType = VariableComponentType(shaderAttrib.type);
    int shaderComponentCount   = VariableComponentCount(shaderAttrib.type);
    const gl::VertexFormat &vertexFormat = gl::GetVertexFormatFromType(vertexFormatType);

    // Perform integer to float conversion (if necessary)
    if (shaderComponentType == GL_FLOAT && vertexFormat.type != GL_FLOAT)
    {
        // TODO: normalization for 32-bit integer formats
        ASSERT(!vertexFormat.normalized && !vertexFormat.pureInteger);
        outStream << "float" << shaderComponentCount << "(input."
                  << DecorateVariable(shaderAttrib.name) << ")";
        return;
    }

    // No conversion necessary
    outStream << "input." << DecorateVariable(shaderAttrib.name);
}

void DynamicHLSL::getPixelShaderOutputKey(const gl::ContextState &data,
                                          const gl::ProgramState &programData,
                                          const ProgramD3DMetadata &metadata,
                                          std::vector<PixelShaderOutputVariable> *outPixelShaderKey)
{
    // Two cases when writing to gl_FragColor and using ESSL 1.0:
    // - with a 3.0 context, the output color is copied to channel 0
    // - with a 2.0 context, the output color is broadcast to all channels
    bool broadcast = metadata.usesBroadcast(data);
    const unsigned int numRenderTargets =
        (broadcast || metadata.usesMultipleFragmentOuts() ? data.getCaps().maxDrawBuffers : 1);

    if (metadata.getMajorShaderVersion() < 300)
    {
        for (unsigned int renderTargetIndex = 0; renderTargetIndex < numRenderTargets;
             renderTargetIndex++)
        {
            PixelShaderOutputVariable outputKeyVariable;
            outputKeyVariable.type = GL_FLOAT_VEC4;
            outputKeyVariable.name = "gl_Color" + Str(renderTargetIndex);
            outputKeyVariable.source =
                broadcast ? "gl_Color[0]" : "gl_Color[" + Str(renderTargetIndex) + "]";
            outputKeyVariable.outputIndex = renderTargetIndex;

            outPixelShaderKey->push_back(outputKeyVariable);
        }
    }
    else
    {
        const auto &shaderOutputVars =
            metadata.getFragmentShader()->getData().getActiveOutputVariables();

        for (size_t outputLocationIndex = 0u;
             outputLocationIndex < programData.getOutputLocations().size(); ++outputLocationIndex)
        {
            const VariableLocation &outputLocation =
                programData.getOutputLocations().at(outputLocationIndex);
            if (!outputLocation.used())
            {
                continue;
            }
            const sh::ShaderVariable &outputVariable = shaderOutputVars[outputLocation.index];
            const std::string &variableName          = "out_" + outputVariable.name;

            // Fragment outputs can't be arrays of arrays. ESSL 3.10 section 4.3.6.
            const std::string &elementString =
                (outputVariable.isArray() ? Str(outputLocation.arrayIndex) : "");

            ASSERT(outputVariable.staticUse);

            PixelShaderOutputVariable outputKeyVariable;
            outputKeyVariable.type        = outputVariable.type;
            outputKeyVariable.name        = variableName + elementString;
            outputKeyVariable.source =
                variableName +
                (outputVariable.isArray() ? ArrayString(outputLocation.arrayIndex) : "");
            outputKeyVariable.outputIndex = outputLocationIndex;

            outPixelShaderKey->push_back(outputKeyVariable);
        }
    }
}

// BuiltinVarying Implementation.
BuiltinVarying::BuiltinVarying() : enabled(false), index(0), systemValue(false)
{
}

std::string BuiltinVarying::str() const
{
    return (systemValue ? semantic : (semantic + Str(index)));
}

void BuiltinVarying::enableSystem(const std::string &systemValueSemantic)
{
    enabled     = true;
    semantic    = systemValueSemantic;
    systemValue = true;
}

void BuiltinVarying::enable(const std::string &semanticVal, unsigned int indexVal)
{
    enabled  = true;
    semantic = semanticVal;
    index    = indexVal;
}

// BuiltinVaryingsD3D Implementation.
BuiltinVaryingsD3D::BuiltinVaryingsD3D(const ProgramD3DMetadata &metadata,
                                       const VaryingPacking &packing)
{
    updateBuiltins(gl::SHADER_VERTEX, metadata, packing);
    updateBuiltins(gl::SHADER_FRAGMENT, metadata, packing);
    if (metadata.getRendererMajorShaderModel() >= 4)
    {
        updateBuiltins(gl::SHADER_GEOMETRY, metadata, packing);
    }
}

BuiltinVaryingsD3D::~BuiltinVaryingsD3D() = default;

void BuiltinVaryingsD3D::updateBuiltins(gl::ShaderType shaderType,
                                        const ProgramD3DMetadata &metadata,
                                        const VaryingPacking &packing)
{
    const std::string &userSemantic = GetVaryingSemantic(metadata.getRendererMajorShaderModel(),
                                                         metadata.usesSystemValuePointSize());

    unsigned int reservedSemanticIndex = packing.getMaxSemanticIndex();

    BuiltinInfo *builtins = &mBuiltinInfo[shaderType];

    if (metadata.getRendererMajorShaderModel() >= 4)
    {
        builtins->dxPosition.enableSystem("SV_Position");
    }
    else if (shaderType == gl::SHADER_FRAGMENT)
    {
        builtins->dxPosition.enableSystem("VPOS");
    }
    else
    {
        builtins->dxPosition.enableSystem("POSITION");
    }

    if (metadata.usesTransformFeedbackGLPosition())
    {
        builtins->glPosition.enable(userSemantic, reservedSemanticIndex++);
    }

    if (metadata.usesFragCoord())
    {
        builtins->glFragCoord.enable(userSemantic, reservedSemanticIndex++);
    }

    if (shaderType == gl::SHADER_VERTEX ? metadata.addsPointCoordToVertexShader()
                                        : metadata.usesPointCoord())
    {
        // SM3 reserves the TEXCOORD semantic for point sprite texcoords (gl_PointCoord)
        // In D3D11 we manually compute gl_PointCoord in the GS.
        if (metadata.getRendererMajorShaderModel() >= 4)
        {
            builtins->glPointCoord.enable(userSemantic, reservedSemanticIndex++);
        }
        else
        {
            builtins->glPointCoord.enable("TEXCOORD", 0);
        }
    }

    if (shaderType == gl::SHADER_VERTEX && metadata.hasANGLEMultiviewEnabled())
    {
        builtins->glViewIDOVR.enable(userSemantic, reservedSemanticIndex++);
        if (metadata.canSelectViewInVertexShader())
        {
            builtins->glViewportIndex.enableSystem("SV_ViewportArrayIndex");
            builtins->glLayer.enableSystem("SV_RenderTargetArrayIndex");
        }
    }

    if (shaderType == gl::SHADER_FRAGMENT && metadata.hasANGLEMultiviewEnabled())
    {
        builtins->glViewIDOVR.enable(userSemantic, reservedSemanticIndex++);
    }

    if (shaderType == gl::SHADER_GEOMETRY && metadata.hasANGLEMultiviewEnabled())
    {
        // Although it is possible to retrieve gl_ViewID_OVR from the value of
        // SV_ViewportArrayIndex or SV_RenderTargetArrayIndex based on the multi-view state in the
        // driver constant buffer, it is easier and cleaner to pass it as a varying.
        builtins->glViewIDOVR.enable(userSemantic, reservedSemanticIndex++);

        // gl_Layer and gl_ViewportIndex are necessary so that we can write to either based on the
        // multiview state in the driver constant buffer.
        builtins->glViewportIndex.enableSystem("SV_ViewportArrayIndex");
        builtins->glLayer.enableSystem("SV_RenderTargetArrayIndex");
    }

    // Special case: do not include PSIZE semantic in HLSL 3 pixel shaders
    if (metadata.usesSystemValuePointSize() &&
        (shaderType != gl::SHADER_FRAGMENT || metadata.getRendererMajorShaderModel() >= 4))
    {
        builtins->glPointSize.enableSystem("PSIZE");
    }
}

}  // namespace rx

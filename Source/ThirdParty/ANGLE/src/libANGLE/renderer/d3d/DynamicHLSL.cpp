//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DynamicHLSL.cpp: Implementation for link and run-time HLSL generation
//

#include "libANGLE/renderer/d3d/DynamicHLSL.h"

#include "common/utilities.h"
#include "compiler/translator/blocklayoutHLSL.h"
#include "libANGLE/Program.h"
#include "libANGLE/Shader.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/VaryingPacking.h"

using namespace gl;

namespace rx
{

namespace
{

std::string HLSLComponentTypeString(GLenum componentType)
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

std::string HLSLComponentTypeString(GLenum componentType, int componentCount)
{
    return HLSLComponentTypeString(componentType) + (componentCount > 1 ? Str(componentCount) : "");
}

std::string HLSLMatrixTypeString(GLenum type)
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

std::string HLSLTypeString(GLenum type)
{
    if (gl::IsMatrixType(type))
    {
        return HLSLMatrixTypeString(type);
    }

    return HLSLComponentTypeString(gl::VariableComponentType(type),
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

void WriteArrayString(std::stringstream &strstr, unsigned int i)
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

const std::string VERTEX_ATTRIBUTE_STUB_STRING = "@@ VERTEX ATTRIBUTES @@";
const std::string PIXEL_OUTPUT_STUB_STRING     = "@@ PIXEL OUTPUT @@";
}  // anonymous namespace

std::string GetVaryingSemantic(int majorShaderModel, bool programUsesPointSize)
{
    // SM3 reserves the TEXCOORD semantic for point sprite texcoords (gl_PointCoord)
    // In D3D11 we manually compute gl_PointCoord in the GS.
    return ((programUsesPointSize && majorShaderModel < 4) ? "COLOR" : "TEXCOORD");
}

// DynamicHLSL implementation

DynamicHLSL::DynamicHLSL(RendererD3D *const renderer) : mRenderer(renderer)
{
}

void DynamicHLSL::generateVaryingHLSL(const VaryingPacking &varyingPacking,
                                      std::stringstream &hlslStream) const
{
    std::string varyingSemantic =
        GetVaryingSemantic(mRenderer->getMajorShaderModel(), varyingPacking.usesPointSize());

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
        int columnCount = gl::VariableColumnCount(transposedType);
        hlslStream << HLSLComponentTypeString(componentType, columnCount);
        unsigned int semanticIndex = registerInfo.semanticIndex;
        hlslStream << " v" << semanticIndex << " : " << varyingSemantic << semanticIndex << ";\n";
    }
}

std::string DynamicHLSL::generateVertexShaderForInputLayout(
    const std::string &sourceShader,
    const InputLayout &inputLayout,
    const std::vector<sh::Attribute> &shaderAttributes) const
{
    std::stringstream structStream;
    std::stringstream initStream;

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
                    structStream << "    " << HLSLComponentTypeString(
                                                  componentType,
                                                  VariableComponentCount(shaderAttribute.type));
                }
            }

            structStream << " " << decorateVariable(shaderAttribute.name) << " : ";

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
            initStream << "    " << decorateVariable(shaderAttribute.name) << " = ";

            // Mismatched vertex attribute to vertex input may result in an undefined
            // data reinterpretation (eg for pure integer->float, float->pure integer)
            // TODO: issue warning with gl debug info extension, when supported
            if (IsMatrixType(shaderAttribute.type) ||
                (mRenderer->getVertexConversionType(vertexFormatType) & VERTEX_CONVERT_GPU) != 0)
            {
                initStream << generateAttributeConversionHLSL(vertexFormatType, shaderAttribute);
            }
            else
            {
                initStream << "input." << decorateVariable(shaderAttribute.name);
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

    size_t copyInsertionPos = vertexHLSL.find(VERTEX_ATTRIBUTE_STUB_STRING);
    vertexHLSL.replace(copyInsertionPos, VERTEX_ATTRIBUTE_STUB_STRING.length(), structStream.str());

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

    std::stringstream declarationStream;
    std::stringstream copyStream;

    declarationStream << "struct PS_OUTPUT\n"
                         "{\n";

    for (size_t layoutIndex = 0; layoutIndex < outputLayout.size(); ++layoutIndex)
    {
        GLenum binding = outputLayout[layoutIndex];

        if (binding != GL_NONE)
        {
            unsigned int location = (binding - GL_COLOR_ATTACHMENT0);

            const PixelShaderOutputVariable *outputVariable =
                FindOutputAtLocation(outputVariables, location);

            // OpenGL ES 3.0 spec $4.2.1
            // If [...] not all user-defined output variables are written, the values of fragment
            // colors
            // corresponding to unwritten variables are similarly undefined.
            if (outputVariable)
            {
                declarationStream << "    " + HLSLTypeString(outputVariable->type) << " "
                                  << outputVariable->name << " : " << targetSemantic
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

    size_t outputInsertionPos = pixelHLSL.find(PIXEL_OUTPUT_STUB_STRING);
    pixelHLSL.replace(outputInsertionPos, PIXEL_OUTPUT_STUB_STRING.length(),
                      declarationStream.str());

    return pixelHLSL;
}

void DynamicHLSL::generateVaryingLinkHLSL(ShaderType shaderType,
                                          const VaryingPacking &varyingPacking,
                                          std::stringstream &linkStream) const
{
    const auto &builtins = varyingPacking.builtins(shaderType);
    ASSERT(builtins.dxPosition.enabled);
    linkStream << "{\n"
               << "    float4 dx_Position : " << builtins.dxPosition.str() << ";\n";

    if (builtins.glPosition.enabled)
    {
        linkStream << "    float4 gl_Position : " << builtins.glPosition.str() << ";\n";
    }

    if (builtins.glFragCoord.enabled)
    {
        linkStream << "    float4 gl_FragCoord : " << builtins.glFragCoord.str() << ";\n";
    }

    if (builtins.glPointCoord.enabled)
    {
        linkStream << "    float2 gl_PointCoord : " << builtins.glPointCoord.str() << ";\n";
    }

    if (builtins.glPointSize.enabled)
    {
        linkStream << "    float gl_PointSize : " << builtins.glPointSize.str() << ";\n";
    }

    // Do this after gl_PointSize, to potentially combine gl_PointCoord and gl_PointSize into the
    // same register.
    generateVaryingHLSL(varyingPacking, linkStream);

    linkStream << "};\n";
}

bool DynamicHLSL::generateShaderLinkHLSL(const gl::ContextState &data,
                                         const gl::ProgramState &programData,
                                         const ProgramD3DMetadata &programMetadata,
                                         const VaryingPacking &varyingPacking,
                                         std::string *pixelHLSL,
                                         std::string *vertexHLSL) const
{
    ASSERT(pixelHLSL->empty() && vertexHLSL->empty());

    const gl::Shader *vertexShaderGL   = programData.getAttachedVertexShader();
    const gl::Shader *fragmentShaderGL = programData.getAttachedFragmentShader();
    const ShaderD3D *fragmentShader    = GetImplAs<ShaderD3D>(fragmentShaderGL);
    const int shaderModel              = mRenderer->getMajorShaderModel();

    // usesViewScale() isn't supported in the D3D9 renderer
    ASSERT(shaderModel >= 4 || !programMetadata.usesViewScale());

    bool useInstancedPointSpriteEmulation =
        programMetadata.usesPointSize() &&
        mRenderer->getWorkarounds().useInstancedPointSpriteEmulation;

    // Validation done in the compiler
    ASSERT(!fragmentShader->usesFragColor() || !fragmentShader->usesFragData());

    std::stringstream vertexStream;
    vertexStream << vertexShaderGL->getTranslatedSource();

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
    vertexStream << "\n" << VERTEX_ATTRIBUTE_STUB_STRING + "\n";

    // Write the HLSL input/output declarations
    vertexStream << "struct VS_OUTPUT\n";
    generateVaryingLinkHLSL(SHADER_VERTEX, varyingPacking, vertexStream);
    vertexStream << "\n"
                 << "VS_OUTPUT main(VS_INPUT input)\n"
                 << "{\n"
                 << "    initAttributes(input);\n";

    vertexStream << "\n"
                 << "    gl_main();\n"
                 << "\n"
                 << "    VS_OUTPUT output;\n";

    const auto &vertexBuiltins = varyingPacking.builtins(SHADER_VERTEX);

    if (vertexBuiltins.glPosition.enabled)
    {
        vertexStream << "    output.gl_Position = gl_Position;\n";
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
            vertexStream << decorateVariable(packedVarying.parentStructName) << ".";
        }

        vertexStream << decorateVariable(varying.name);

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

    std::stringstream pixelStream;
    pixelStream << fragmentShaderGL->getTranslatedSource();
    pixelStream << "struct PS_INPUT\n";
    generateVaryingLinkHLSL(SHADER_PIXEL, varyingPacking, pixelStream);
    pixelStream << "\n";

    pixelStream << PIXEL_OUTPUT_STUB_STRING + "\n";

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

    const auto &pixelBuiltins = varyingPacking.builtins(SHADER_PIXEL);

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

        // Don't reference VS-only transform feedback varyings in the PS.
        // TODO: Consider updating the fragment shader's varyings with a parameter signaling that a
        // varying is only used in the vertex shader in MergeVaryings
        if (packedVarying.vertexOnly || (!varying.staticUse && !packedVarying.isStructField()))
            continue;

        pixelStream << "    ";

        if (packedVarying.isStructField())
        {
            pixelStream << decorateVariable(packedVarying.parentStructName) << ".";
        }

        pixelStream << decorateVariable(varying.name);

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

    return true;
}

std::string DynamicHLSL::generateGeometryShaderPreamble(const VaryingPacking &varyingPacking) const
{
    ASSERT(mRenderer->getMajorShaderModel() >= 4);

    std::stringstream preambleStream;

    const auto &builtins = varyingPacking.builtins(SHADER_VERTEX);

    preambleStream << "struct GS_INPUT\n";
    generateVaryingLinkHLSL(SHADER_VERTEX, varyingPacking, preambleStream);
    preambleStream << "\n"
                   << "struct GS_OUTPUT\n";
    generateVaryingLinkHLSL(SHADER_GEOMETRY, varyingPacking, preambleStream);
    preambleStream
        << "\n"
        << "void copyVertex(inout GS_OUTPUT output, GS_INPUT input, GS_INPUT flatinput)\n"
        << "{\n"
        << "    output.gl_Position = input.gl_Position;\n";

    if (builtins.glPointSize.enabled)
    {
        preambleStream << "    output.gl_PointSize = input.gl_PointSize;\n";
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

    if (builtins.glFragCoord.enabled)
    {
        preambleStream << "    output.gl_FragCoord = input.gl_FragCoord;\n";
    }

    // Only write the dx_Position if we aren't using point sprites
    preambleStream << "#ifndef ANGLE_POINT_SPRITE_SHADER\n"
                   << "    output.dx_Position = input.dx_Position;\n"
                   << "#endif  // ANGLE_POINT_SPRITE_SHADER\n"
                   << "}\n";

    return preambleStream.str();
}

std::string DynamicHLSL::generateGeometryShaderHLSL(gl::PrimitiveType primitiveType,
                                                    const gl::ContextState &data,
                                                    const gl::ProgramState &programData,
                                                    const bool useViewScale,
                                                    const std::string &preambleString) const
{
    ASSERT(mRenderer->getMajorShaderModel() >= 4);

    std::stringstream shaderStream;

    const bool pointSprites   = (primitiveType == PRIMITIVE_POINTS);
    const bool usesPointCoord = preambleString.find("gl_PointCoord") != std::string::npos;

    const char *inputPT  = nullptr;
    const char *outputPT = nullptr;
    int inputSize        = 0;
    int maxVertexOutput  = 0;

    switch (primitiveType)
    {
        case PRIMITIVE_POINTS:
            inputPT         = "point";
            outputPT        = "Triangle";
            inputSize       = 1;
            maxVertexOutput = 4;
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

    if (pointSprites)
    {
        shaderStream << "#define ANGLE_POINT_SPRITE_SHADER\n"
                        "\n"
                        "uniform float4 dx_ViewCoords : register(c1);\n";

        if (useViewScale)
        {
            shaderStream << "uniform float2 dx_ViewScale : register(c3);\n";
        }

        shaderStream << "\n"
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
                     << static_cast<int>(data.getCaps().minAliasedPointSize)
                     << ".0f;\n"
                        "static float maxPointSize = "
                     << static_cast<int>(data.getCaps().maxAliasedPointSize) << ".0f;\n"
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

// This method needs to match OutputHLSL::decorate
std::string DynamicHLSL::decorateVariable(const std::string &name)
{
    if (name.compare(0, 3, "gl_") != 0)
    {
        return "_" + name;
    }

    return name;
}

std::string DynamicHLSL::generateAttributeConversionHLSL(
    gl::VertexFormatType vertexFormatType,
    const sh::ShaderVariable &shaderAttrib) const
{
    const gl::VertexFormat &vertexFormat = gl::GetVertexFormatFromType(vertexFormatType);
    std::string attribString             = "input." + decorateVariable(shaderAttrib.name);

    // Matrix
    if (IsMatrixType(shaderAttrib.type))
    {
        return "transpose(" + attribString + ")";
    }

    GLenum shaderComponentType = VariableComponentType(shaderAttrib.type);
    int shaderComponentCount   = VariableComponentCount(shaderAttrib.type);

    // Perform integer to float conversion (if necessary)
    bool requiresTypeConversion =
        (shaderComponentType == GL_FLOAT && vertexFormat.type != GL_FLOAT);

    if (requiresTypeConversion)
    {
        // TODO: normalization for 32-bit integer formats
        ASSERT(!vertexFormat.normalized && !vertexFormat.pureInteger);
        return "float" + Str(shaderComponentCount) + "(" + attribString + ")";
    }

    // No conversion necessary
    return attribString;
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

        for (auto outputPair : programData.getOutputVariables())
        {
            const VariableLocation &outputLocation   = outputPair.second;
            const sh::ShaderVariable &outputVariable = shaderOutputVars[outputLocation.index];
            const std::string &variableName = "out_" + outputLocation.name;
            const std::string &elementString =
                (outputLocation.element == GL_INVALID_INDEX ? "" : Str(outputLocation.element));

            ASSERT(outputVariable.staticUse);

            PixelShaderOutputVariable outputKeyVariable;
            outputKeyVariable.type        = outputVariable.type;
            outputKeyVariable.name        = variableName + elementString;
            outputKeyVariable.source      = variableName + ArrayString(outputLocation.element);
            outputKeyVariable.outputIndex = outputPair.first;

            outPixelShaderKey->push_back(outputKeyVariable);
        }
    }
}

}  // namespace rx

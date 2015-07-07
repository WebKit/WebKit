//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DynamicHLSL.cpp: Implementation for link and run-time HLSL generation
//

#include "precompiled.h"

#include "libGLESv2/DynamicHLSL.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/renderer/Renderer.h"
#include "common/utilities.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/formatutils.h"
#include "common/blocklayout.h"

static std::string Str(int i)
{
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d", i);
    return buffer;
}

namespace gl_d3d
{

std::string HLSLComponentTypeString(GLenum componentType)
{
    switch (componentType)
    {
      case GL_UNSIGNED_INT:         return "uint";
      case GL_INT:                  return "int";
      case GL_UNSIGNED_NORMALIZED:
      case GL_SIGNED_NORMALIZED:
      case GL_FLOAT:                return "float";
      default: UNREACHABLE();       return "not-component-type";
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
      case GL_FLOAT_MAT2:     return "float2x2";
      case GL_FLOAT_MAT3:     return "float3x3";
      case GL_FLOAT_MAT4:     return "float4x4";
      case GL_FLOAT_MAT2x3:   return "float2x3";
      case GL_FLOAT_MAT3x2:   return "float3x2";
      case GL_FLOAT_MAT2x4:   return "float2x4";
      case GL_FLOAT_MAT4x2:   return "float4x2";
      case GL_FLOAT_MAT3x4:   return "float3x4";
      case GL_FLOAT_MAT4x3:   return "float4x3";
      default: UNREACHABLE(); return "not-matrix-type";
    }
}

std::string HLSLTypeString(GLenum type)
{
    if (gl::IsMatrixType(type))
    {
        return HLSLMatrixTypeString(type);
    }

    return HLSLComponentTypeString(gl::UniformComponentType(type), gl::UniformComponentCount(type));
}

}

namespace gl
{

std::string ArrayString(unsigned int i)
{
    return (i == GL_INVALID_INDEX ? "" : "[" + Str(i) + "]");
}

const std::string DynamicHLSL::VERTEX_ATTRIBUTE_STUB_STRING = "@@ VERTEX ATTRIBUTES @@";

DynamicHLSL::DynamicHLSL(rx::Renderer *const renderer)
    : mRenderer(renderer)
{
}

static bool packVarying(Varying *varying, const int maxVaryingVectors, const ShaderVariable *packing[][4])
{
    GLenum transposedType = TransposeMatrixType(varying->type);

    // matrices within varying structs are not transposed
    int registers = (varying->isStruct() ? HLSLVariableRegisterCount(*varying) : VariableRowCount(transposedType)) * varying->elementCount();
    int elements = (varying->isStruct() ? 4 : VariableColumnCount(transposedType));
    bool success = false;

    if (elements == 2 || elements == 3 || elements == 4)
    {
        for (int r = 0; r <= maxVaryingVectors - registers && !success; r++)
        {
            bool available = true;

            for (int y = 0; y < registers && available; y++)
            {
                for (int x = 0; x < elements && available; x++)
                {
                    if (packing[r + y][x])
                    {
                        available = false;
                    }
                }
            }

            if (available)
            {
                varying->registerIndex = r;
                varying->elementIndex = 0;

                for (int y = 0; y < registers; y++)
                {
                    for (int x = 0; x < elements; x++)
                    {
                        packing[r + y][x] = &*varying;
                    }
                }

                success = true;
            }
        }

        if (!success && elements == 2)
        {
            for (int r = maxVaryingVectors - registers; r >= 0 && !success; r--)
            {
                bool available = true;

                for (int y = 0; y < registers && available; y++)
                {
                    for (int x = 2; x < 4 && available; x++)
                    {
                        if (packing[r + y][x])
                        {
                            available = false;
                        }
                    }
                }

                if (available)
                {
                    varying->registerIndex = r;
                    varying->elementIndex = 2;

                    for (int y = 0; y < registers; y++)
                    {
                        for (int x = 2; x < 4; x++)
                        {
                            packing[r + y][x] = &*varying;
                        }
                    }

                    success = true;
                }
            }
        }
    }
    else if (elements == 1)
    {
        int space[4] = { 0 };

        for (int y = 0; y < maxVaryingVectors; y++)
        {
            for (int x = 0; x < 4; x++)
            {
                space[x] += packing[y][x] ? 0 : 1;
            }
        }

        int column = 0;

        for (int x = 0; x < 4; x++)
        {
            if (space[x] >= registers && space[x] < space[column])
            {
                column = x;
            }
        }

        if (space[column] >= registers)
        {
            for (int r = 0; r < maxVaryingVectors; r++)
            {
                if (!packing[r][column])
                {
                    varying->registerIndex = r;

                    for (int y = r; y < r + registers; y++)
                    {
                        packing[y][column] = &*varying;
                    }

                    break;
                }
            }

            varying->elementIndex = column;

            success = true;
        }
    }
    else UNREACHABLE();

    return success;
}

// Packs varyings into generic varying registers, using the algorithm from [OpenGL ES Shading Language 1.00 rev. 17] appendix A section 7 page 111
// Returns the number of used varying registers, or -1 if unsuccesful
int DynamicHLSL::packVaryings(InfoLog &infoLog, const ShaderVariable *packing[][4], FragmentShader *fragmentShader,
                              VertexShader *vertexShader, const std::vector<std::string>& transformFeedbackVaryings)
{
    const int maxVaryingVectors = mRenderer->getMaxVaryingVectors();

    vertexShader->resetVaryingsRegisterAssignment();
    fragmentShader->resetVaryingsRegisterAssignment();

    std::set<std::string> packedVaryings;

    for (unsigned int varyingIndex = 0; varyingIndex < fragmentShader->mVaryings.size(); varyingIndex++)
    {
        Varying *varying = &fragmentShader->mVaryings[varyingIndex];
        if (packVarying(varying, maxVaryingVectors, packing))
        {
            packedVaryings.insert(varying->name);
        }
        else
        {
            infoLog.append("Could not pack varying %s", varying->name.c_str());
            return -1;
        }
    }

    for (unsigned int feedbackVaryingIndex = 0; feedbackVaryingIndex < transformFeedbackVaryings.size(); feedbackVaryingIndex++)
    {
        const std::string &transformFeedbackVarying = transformFeedbackVaryings[feedbackVaryingIndex];
        if (packedVaryings.find(transformFeedbackVarying) == packedVaryings.end())
        {
            bool found = false;
            for (unsigned int varyingIndex = 0; varyingIndex < vertexShader->mVaryings.size(); varyingIndex++)
            {
                Varying *varying = &vertexShader->mVaryings[varyingIndex];
                if (transformFeedbackVarying == varying->name)
                {
                    if (!packVarying(varying, maxVaryingVectors, packing))
                    {
                        infoLog.append("Could not pack varying %s", varying->name.c_str());
                        return -1;
                    }

                    found = true;
                    break;
                }
            }

            if (!found && transformFeedbackVarying != "gl_Position" && transformFeedbackVarying != "gl_PointSize")
            {
                infoLog.append("Transform feedback varying %s does not exist in the vertex shader.", transformFeedbackVarying.c_str());
                return -1;
            }
        }
    }

    // Return the number of used registers
    int registers = 0;

    for (int r = 0; r < maxVaryingVectors; r++)
    {
        if (packing[r][0] || packing[r][1] || packing[r][2] || packing[r][3])
        {
            registers++;
        }
    }

    return registers;
}

std::string DynamicHLSL::generateVaryingHLSL(VertexShader *shader, const std::string &varyingSemantic,
                                             std::vector<LinkedVarying> *linkedVaryings) const
{
    std::string varyingHLSL;

    for (unsigned int varyingIndex = 0; varyingIndex < shader->mVaryings.size(); varyingIndex++)
    {
        Varying *varying = &shader->mVaryings[varyingIndex];
        if (varying->registerAssigned())
        {
            GLenum transposedType = TransposeMatrixType(varying->type);
            int variableRows = (varying->isStruct() ? 1 : VariableRowCount(transposedType));

            for (unsigned int elementIndex = 0; elementIndex < varying->elementCount(); elementIndex++)
            {
                for (int row = 0; row < variableRows; row++)
                {
                    switch (varying->interpolation)
                    {
                      case INTERPOLATION_SMOOTH:   varyingHLSL += "    ";                 break;
                      case INTERPOLATION_FLAT:     varyingHLSL += "    nointerpolation "; break;
                      case INTERPOLATION_CENTROID: varyingHLSL += "    centroid ";        break;
                      default:  UNREACHABLE();
                    }

                    unsigned int semanticIndex = elementIndex * variableRows + varying->registerIndex + row;
                    std::string n = Str(semanticIndex);

                    std::string typeString;

                    if (varying->isStruct())
                    {
                        // matrices within structs are not transposed, so
                        // do not use the special struct prefix "rm"
                        typeString = decorateVariable(varying->structName);
                    }
                    else
                    {
                        GLenum componentType = UniformComponentType(transposedType);
                        int columnCount = VariableColumnCount(transposedType);
                        typeString = gl_d3d::HLSLComponentTypeString(componentType, columnCount);
                    }
                    varyingHLSL += typeString + " v" + n + " : " + varyingSemantic + n + ";\n";
                }
            }

            if (linkedVaryings)
            {
                linkedVaryings->push_back(LinkedVarying(varying->name, varying->type, varying->elementCount(),
                                                        varyingSemantic, varying->registerIndex,
                                                        variableRows * varying->elementCount()));
            }
        }
    }

    return varyingHLSL;
}

std::string DynamicHLSL::generateInputLayoutHLSL(const VertexFormat inputLayout[], const Attribute shaderAttributes[]) const
{
    std::string structHLSL, initHLSL;

    int semanticIndex = 0;
    unsigned int inputIndex = 0;

    for (unsigned int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        ASSERT(inputIndex < MAX_VERTEX_ATTRIBS);

        const VertexFormat &vertexFormat = inputLayout[inputIndex];
        const Attribute &shaderAttribute = shaderAttributes[attributeIndex];

        if (!shaderAttribute.name.empty())
        {
            // HLSL code for input structure
            if (IsMatrixType(shaderAttribute.type))
            {
                // Matrix types are always transposed
                structHLSL += "    " + gl_d3d::HLSLMatrixTypeString(TransposeMatrixType(shaderAttribute.type));
            }
            else
            {
                GLenum componentType = mRenderer->getVertexComponentType(vertexFormat);
                structHLSL += "    " + gl_d3d::HLSLComponentTypeString(componentType, UniformComponentCount(shaderAttribute.type));
            }

            structHLSL += " " + decorateVariable(shaderAttribute.name) + " : TEXCOORD" + Str(semanticIndex) + ";\n";
            semanticIndex += AttributeRegisterCount(shaderAttribute.type);

            // HLSL code for initialization
            initHLSL += "    " + decorateVariable(shaderAttribute.name) + " = ";

            // Mismatched vertex attribute to vertex input may result in an undefined
            // data reinterpretation (eg for pure integer->float, float->pure integer)
            // TODO: issue warning with gl debug info extension, when supported
            if (IsMatrixType(shaderAttribute.type) ||
                (mRenderer->getVertexConversionType(vertexFormat) & rx::VERTEX_CONVERT_GPU) != 0)
            {
                initHLSL += generateAttributeConversionHLSL(vertexFormat, shaderAttribute);
            }
            else
            {
                initHLSL += "input." + decorateVariable(shaderAttribute.name);
            }

            initHLSL += ";\n";

            inputIndex += VariableRowCount(TransposeMatrixType(shaderAttribute.type));
        }
    }

    return "struct VS_INPUT\n"
           "{\n" +
           structHLSL +
           "};\n"
           "\n"
           "void initAttributes(VS_INPUT input)\n"
           "{\n" +
           initHLSL +
           "}\n";
}

bool DynamicHLSL::generateShaderLinkHLSL(InfoLog &infoLog, int registers, const ShaderVariable *packing[][4],
                                         std::string& pixelHLSL, std::string& vertexHLSL,
                                         FragmentShader *fragmentShader, VertexShader *vertexShader,
                                         const std::vector<std::string>& transformFeedbackVaryings,
                                         std::vector<LinkedVarying> *linkedVaryings,
                                         std::map<int, VariableLocation> *programOutputVars) const
{
    if (pixelHLSL.empty() || vertexHLSL.empty())
    {
        return false;
    }

    bool usesMRT = fragmentShader->mUsesMultipleRenderTargets;
    bool usesFragColor = fragmentShader->mUsesFragColor;
    bool usesFragData = fragmentShader->mUsesFragData;
    if (usesFragColor && usesFragData)
    {
        infoLog.append("Cannot use both gl_FragColor and gl_FragData in the same fragment shader.");
        return false;
    }

    // Write the HLSL input/output declarations
    const int shaderModel = mRenderer->getMajorShaderModel();
    const int maxVaryingVectors = mRenderer->getMaxVaryingVectors();

    const int registersNeeded = registers + (fragmentShader->mUsesFragCoord ? 1 : 0) + (fragmentShader->mUsesPointCoord ? 1 : 0);

    // Two cases when writing to gl_FragColor and using ESSL 1.0:
    // - with a 3.0 context, the output color is copied to channel 0
    // - with a 2.0 context, the output color is broadcast to all channels
    const bool broadcast = (fragmentShader->mUsesFragColor && mRenderer->getCurrentClientVersion() < 3);
    const unsigned int numRenderTargets = (broadcast || usesMRT ? mRenderer->getMaxRenderTargets() : 1);

    int shaderVersion = vertexShader->getShaderVersion();

    if (registersNeeded > maxVaryingVectors)
    {
        infoLog.append("No varying registers left to support gl_FragCoord/gl_PointCoord");

        return false;
    }

    std::string varyingSemantic = (vertexShader->mUsesPointSize && shaderModel == 3) ? "COLOR" : "TEXCOORD";
    std::string targetSemantic = (shaderModel >= 4) ? "SV_Target" : "COLOR";
    std::string dxPositionSemantic = (shaderModel >= 4) ? "SV_Position" : "POSITION";
    std::string depthSemantic = (shaderModel >= 4) ? "SV_Depth" : "DEPTH";

    std::string varyingHLSL = generateVaryingHLSL(vertexShader, varyingSemantic, linkedVaryings);

    // special varyings that use reserved registers
    int reservedRegisterIndex = registers;

    unsigned int glPositionSemanticIndex = reservedRegisterIndex++;
    std::string glPositionSemantic = varyingSemantic;

    std::string fragCoordSemantic;
    unsigned int fragCoordSemanticIndex = 0;
    if (fragmentShader->mUsesFragCoord)
    {
        fragCoordSemanticIndex = reservedRegisterIndex++;
        fragCoordSemantic = varyingSemantic;
    }

    std::string pointCoordSemantic;
    unsigned int pointCoordSemanticIndex = 0;
    if (fragmentShader->mUsesPointCoord)
    {
        // Shader model 3 uses a special TEXCOORD semantic for point sprite texcoords.
        // In DX11 we compute this in the GS.
        if (shaderModel == 3)
        {
            pointCoordSemanticIndex = 0;
            pointCoordSemantic = "TEXCOORD0";
        }
        else if (shaderModel >= 4)
        {
            pointCoordSemanticIndex = reservedRegisterIndex++;
            pointCoordSemantic = varyingSemantic;
        }
    }

    // Add stub string to be replaced when shader is dynamically defined by its layout
    vertexHLSL += "\n" + VERTEX_ATTRIBUTE_STUB_STRING + "\n";

    vertexHLSL += "struct VS_OUTPUT\n"
                  "{\n";

    if (shaderModel < 4)
    {
        vertexHLSL += "    float4 _dx_Position : " + dxPositionSemantic + ";\n";
        vertexHLSL += "    float4 gl_Position : " + glPositionSemantic + Str(glPositionSemanticIndex) + ";\n";
        linkedVaryings->push_back(LinkedVarying("gl_Position", GL_FLOAT_VEC4, 1, glPositionSemantic, glPositionSemanticIndex, 1));

    }

    vertexHLSL += varyingHLSL;

    if (fragmentShader->mUsesFragCoord)
    {
        vertexHLSL += "    float4 gl_FragCoord : " + fragCoordSemantic + Str(fragCoordSemanticIndex) + ";\n";
        linkedVaryings->push_back(LinkedVarying("gl_FragCoord", GL_FLOAT_VEC4, 1, fragCoordSemantic, fragCoordSemanticIndex, 1));
    }

    if (vertexShader->mUsesPointSize && shaderModel >= 3)
    {
        vertexHLSL += "    float gl_PointSize : PSIZE;\n";
        linkedVaryings->push_back(LinkedVarying("gl_PointSize", GL_FLOAT, 1, "PSIZE", 0, 1));
    }

    if (shaderModel >= 4)
    {
        vertexHLSL += "    float4 _dx_Position : " + dxPositionSemantic + ";\n";
        vertexHLSL += "    float4 gl_Position : " + glPositionSemantic + Str(glPositionSemanticIndex) + ";\n";
        linkedVaryings->push_back(LinkedVarying("gl_Position", GL_FLOAT_VEC4, 1, glPositionSemantic, glPositionSemanticIndex, 1));
    }

    vertexHLSL += "};\n"
                  "\n"
                  "VS_OUTPUT main(VS_INPUT input)\n"
                  "{\n"
                  "    initAttributes(input);\n";

    if (shaderModel >= 4)
    {
        vertexHLSL += "\n"
                      "    gl_main();\n"
                      "\n"
                      "    VS_OUTPUT output;\n"
                      "    output.gl_Position = gl_Position;\n"
                      "    output._dx_Position.x = gl_Position.x;\n"
                      "    output._dx_Position.y = -gl_Position.y;\n"
                      "    output._dx_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n"
                      "    output._dx_Position.w = gl_Position.w;\n";
    }
    else
    {
        vertexHLSL += "\n"
                      "    gl_main();\n"
                      "\n"
                      "    VS_OUTPUT output;\n"
                      "    output.gl_Position = gl_Position;\n"
                      "    output._dx_Position.x = gl_Position.x * dx_ViewAdjust.z + dx_ViewAdjust.x * gl_Position.w;\n"
                      "    output._dx_Position.y = -(gl_Position.y * dx_ViewAdjust.w + dx_ViewAdjust.y * gl_Position.w);\n"
                      "    output._dx_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n"
                      "    output._dx_Position.w = gl_Position.w;\n";
    }

    if (vertexShader->mUsesPointSize && shaderModel >= 3)
    {
        vertexHLSL += "    output.gl_PointSize = gl_PointSize;\n";
    }

    if (fragmentShader->mUsesFragCoord)
    {
        vertexHLSL += "    output.gl_FragCoord = gl_Position;\n";
    }

    for (unsigned int vertVaryingIndex = 0; vertVaryingIndex < vertexShader->mVaryings.size(); vertVaryingIndex++)
    {
        Varying *varying = &vertexShader->mVaryings[vertVaryingIndex];
        if (varying->registerAssigned())
        {
            for (unsigned int elementIndex = 0; elementIndex < varying->elementCount(); elementIndex++)
            {
                int variableRows = (varying->isStruct() ? 1 : VariableRowCount(TransposeMatrixType(varying->type)));

                for (int row = 0; row < variableRows; row++)
                {
                    int r = varying->registerIndex + elementIndex * variableRows + row;
                    vertexHLSL += "    output.v" + Str(r);

                    bool sharedRegister = false;   // Register used by multiple varyings

                    for (int x = 0; x < 4; x++)
                    {
                        if (packing[r][x] && packing[r][x] != packing[r][0])
                        {
                            sharedRegister = true;
                            break;
                        }
                    }

                    if(sharedRegister)
                    {
                        vertexHLSL += ".";

                        for (int x = 0; x < 4; x++)
                        {
                            if (packing[r][x] == &*varying)
                            {
                                switch(x)
                                {
                                  case 0: vertexHLSL += "x"; break;
                                  case 1: vertexHLSL += "y"; break;
                                  case 2: vertexHLSL += "z"; break;
                                  case 3: vertexHLSL += "w"; break;
                                }
                            }
                        }
                    }

                    vertexHLSL += " = _" + varying->name;

                    if (varying->isArray())
                    {
                        vertexHLSL += ArrayString(elementIndex);
                    }

                    if (variableRows > 1)
                    {
                        vertexHLSL += ArrayString(row);
                    }

                    vertexHLSL += ";\n";
                }
            }
        }
    }

    vertexHLSL += "\n"
                  "    return output;\n"
                  "}\n";

    pixelHLSL += "struct PS_INPUT\n"
                 "{\n";

    pixelHLSL += varyingHLSL;

    if (fragmentShader->mUsesFragCoord)
    {
        pixelHLSL += "    float4 gl_FragCoord : " + fragCoordSemantic + Str(fragCoordSemanticIndex) + ";\n";
    }

    if (fragmentShader->mUsesPointCoord && shaderModel >= 3)
    {
        pixelHLSL += "    float2 gl_PointCoord : " + pointCoordSemantic + Str(pointCoordSemanticIndex) + ";\n";
    }

    // Must consume the PSIZE element if the geometry shader is not active
    // We won't know if we use a GS until we draw
    if (vertexShader->mUsesPointSize && shaderModel >= 4)
    {
        pixelHLSL += "    float gl_PointSize : PSIZE;\n";
    }

    if (fragmentShader->mUsesFragCoord)
    {
        if (shaderModel >= 4)
        {
            pixelHLSL += "    float4 dx_VPos : SV_Position;\n";
        }
        else if (shaderModel >= 3)
        {
            pixelHLSL += "    float2 dx_VPos : VPOS;\n";
        }
    }

    pixelHLSL += "};\n"
                 "\n"
                 "struct PS_OUTPUT\n"
                 "{\n";

    if (shaderVersion < 300)
    {
        for (unsigned int renderTargetIndex = 0; renderTargetIndex < numRenderTargets; renderTargetIndex++)
        {
            pixelHLSL += "    float4 gl_Color" + Str(renderTargetIndex) + " : " + targetSemantic + Str(renderTargetIndex) + ";\n";
        }

        if (fragmentShader->mUsesFragDepth)
        {
            pixelHLSL += "    float gl_Depth : " + depthSemantic + ";\n";
        }
    }
    else
    {
        defineOutputVariables(fragmentShader, programOutputVars);

        const std::vector<Attribute> &shaderOutputVars = fragmentShader->getOutputVariables();
        for (auto locationIt = programOutputVars->begin(); locationIt != programOutputVars->end(); locationIt++)
        {
            const VariableLocation &outputLocation = locationIt->second;
            const ShaderVariable &outputVariable = shaderOutputVars[outputLocation.index];
            const std::string &elementString = (outputLocation.element == GL_INVALID_INDEX ? "" : Str(outputLocation.element));

            pixelHLSL += "    " + gl_d3d::HLSLTypeString(outputVariable.type) +
                         " out_" + outputLocation.name + elementString +
                         " : " + targetSemantic + Str(locationIt->first) + ";\n";
        }
    }

    pixelHLSL += "};\n"
                 "\n";

    if (fragmentShader->mUsesFrontFacing)
    {
        if (shaderModel >= 4)
        {
            pixelHLSL += "PS_OUTPUT main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace)\n"
                         "{\n";
        }
        else
        {
            pixelHLSL += "PS_OUTPUT main(PS_INPUT input, float vFace : VFACE)\n"
                         "{\n";
        }
    }
    else
    {
        pixelHLSL += "PS_OUTPUT main(PS_INPUT input)\n"
                     "{\n";
    }

    if (fragmentShader->mUsesFragCoord)
    {
        pixelHLSL += "    float rhw = 1.0 / input.gl_FragCoord.w;\n";

        if (shaderModel >= 4)
        {
            pixelHLSL += "    gl_FragCoord.x = input.dx_VPos.x;\n"
                         "    gl_FragCoord.y = input.dx_VPos.y;\n";
        }
        else if (shaderModel >= 3)
        {
            pixelHLSL += "    gl_FragCoord.x = input.dx_VPos.x + 0.5;\n"
                         "    gl_FragCoord.y = input.dx_VPos.y + 0.5;\n";
        }
        else
        {
            // dx_ViewCoords contains the viewport width/2, height/2, center.x and center.y. See Renderer::setViewport()
            pixelHLSL += "    gl_FragCoord.x = (input.gl_FragCoord.x * rhw) * dx_ViewCoords.x + dx_ViewCoords.z;\n"
                         "    gl_FragCoord.y = (input.gl_FragCoord.y * rhw) * dx_ViewCoords.y + dx_ViewCoords.w;\n";
        }

        pixelHLSL += "    gl_FragCoord.z = (input.gl_FragCoord.z * rhw) * dx_DepthFront.x + dx_DepthFront.y;\n"
                     "    gl_FragCoord.w = rhw;\n";
    }

    if (fragmentShader->mUsesPointCoord && shaderModel >= 3)
    {
        pixelHLSL += "    gl_PointCoord.x = input.gl_PointCoord.x;\n";
        pixelHLSL += "    gl_PointCoord.y = 1.0 - input.gl_PointCoord.y;\n";
    }

    if (fragmentShader->mUsesFrontFacing)
    {
        if (shaderModel <= 3)
        {
            pixelHLSL += "    gl_FrontFacing = (vFace * dx_DepthFront.z >= 0.0);\n";
        }
        else
        {
            pixelHLSL += "    gl_FrontFacing = isFrontFace;\n";
        }
    }

    for (unsigned int varyingIndex = 0; varyingIndex < fragmentShader->mVaryings.size(); varyingIndex++)
    {
        Varying *varying = &fragmentShader->mVaryings[varyingIndex];
        if (varying->registerAssigned())
        {
            for (unsigned int elementIndex = 0; elementIndex < varying->elementCount(); elementIndex++)
            {
                GLenum transposedType = TransposeMatrixType(varying->type);
                int variableRows = (varying->isStruct() ? 1 : VariableRowCount(transposedType));
                for (int row = 0; row < variableRows; row++)
                {
                    std::string n = Str(varying->registerIndex + elementIndex * variableRows + row);
                    pixelHLSL += "    _" + varying->name;

                    if (varying->isArray())
                    {
                        pixelHLSL += ArrayString(elementIndex);
                    }

                    if (variableRows > 1)
                    {
                        pixelHLSL += ArrayString(row);
                    }

                    if (varying->isStruct())
                    {
                        pixelHLSL += " = input.v" + n + ";\n";   break;
                    }
                    else
                    {
                        switch (VariableColumnCount(transposedType))
                        {
                          case 1: pixelHLSL += " = input.v" + n + ".x;\n";   break;
                          case 2: pixelHLSL += " = input.v" + n + ".xy;\n";  break;
                          case 3: pixelHLSL += " = input.v" + n + ".xyz;\n"; break;
                          case 4: pixelHLSL += " = input.v" + n + ";\n";     break;
                          default: UNREACHABLE();
                        }
                    }
                }
            }
        }
        else UNREACHABLE();
    }

    pixelHLSL += "\n"
                 "    gl_main();\n"
                 "\n"
                 "    PS_OUTPUT output;\n";

    if (shaderVersion < 300)
    {
        for (unsigned int renderTargetIndex = 0; renderTargetIndex < numRenderTargets; renderTargetIndex++)
        {
            unsigned int sourceColorIndex = broadcast ? 0 : renderTargetIndex;

            pixelHLSL += "    output.gl_Color" + Str(renderTargetIndex) + " = gl_Color[" + Str(sourceColorIndex) + "];\n";
        }

        if (fragmentShader->mUsesFragDepth)
        {
            pixelHLSL += "    output.gl_Depth = gl_Depth;\n";
        }
    }
    else
    {
        for (auto locationIt = programOutputVars->begin(); locationIt != programOutputVars->end(); locationIt++)
        {
            const VariableLocation &outputLocation = locationIt->second;
            const std::string &variableName = "out_" + outputLocation.name;
            const std::string &outVariableName = variableName + (outputLocation.element == GL_INVALID_INDEX ? "" : Str(outputLocation.element));
            const std::string &staticVariableName = variableName + ArrayString(outputLocation.element);

            pixelHLSL += "    output." + outVariableName + " = " + staticVariableName + ";\n";
        }
    }

    pixelHLSL += "\n"
                 "    return output;\n"
                 "}\n";

    return true;
}

void DynamicHLSL::defineOutputVariables(FragmentShader *fragmentShader, std::map<int, VariableLocation> *programOutputVars) const
{
    const std::vector<Attribute> &shaderOutputVars = fragmentShader->getOutputVariables();

    for (unsigned int outputVariableIndex = 0; outputVariableIndex < shaderOutputVars.size(); outputVariableIndex++)
    {
        const Attribute &outputVariable = shaderOutputVars[outputVariableIndex];
        const int baseLocation = outputVariable.location == -1 ? 0 : outputVariable.location;

        if (outputVariable.arraySize > 0)
        {
            for (unsigned int elementIndex = 0; elementIndex < outputVariable.arraySize; elementIndex++)
            {
                const int location = baseLocation + elementIndex;
                ASSERT(programOutputVars->count(location) == 0);
                (*programOutputVars)[location] = VariableLocation(outputVariable.name, elementIndex, outputVariableIndex);
            }
        }
        else
        {
            ASSERT(programOutputVars->count(baseLocation) == 0);
            (*programOutputVars)[baseLocation] = VariableLocation(outputVariable.name, GL_INVALID_INDEX, outputVariableIndex);
        }
    }
}

std::string DynamicHLSL::generateGeometryShaderHLSL(int registers, const ShaderVariable *packing[][4], FragmentShader *fragmentShader, VertexShader *vertexShader) const
{
    // for now we only handle point sprite emulation
    ASSERT(vertexShader->mUsesPointSize && mRenderer->getMajorShaderModel() >= 4);
    return generatePointSpriteHLSL(registers, packing, fragmentShader, vertexShader);
}

std::string DynamicHLSL::generatePointSpriteHLSL(int registers, const ShaderVariable *packing[][4], FragmentShader *fragmentShader, VertexShader *vertexShader) const
{
    ASSERT(registers >= 0);
    ASSERT(vertexShader->mUsesPointSize);
    ASSERT(mRenderer->getMajorShaderModel() >= 4);

    std::string geomHLSL;

    std::string varyingSemantic = "TEXCOORD";

    std::string fragCoordSemantic;
    std::string pointCoordSemantic;

    int reservedRegisterIndex = registers;

    if (fragmentShader->mUsesFragCoord)
    {
        fragCoordSemantic = varyingSemantic + Str(reservedRegisterIndex++);
    }

    if (fragmentShader->mUsesPointCoord)
    {
        pointCoordSemantic = varyingSemantic + Str(reservedRegisterIndex++);
    }

    geomHLSL += "uniform float4 dx_ViewCoords : register(c1);\n"
                "\n"
                "struct GS_INPUT\n"
                "{\n";

    std::string varyingHLSL = generateVaryingHLSL(vertexShader, varyingSemantic, NULL);

    geomHLSL += varyingHLSL;

    if (fragmentShader->mUsesFragCoord)
    {
        geomHLSL += "    float4 gl_FragCoord : " + fragCoordSemantic + ";\n";
    }

    geomHLSL += "    float gl_PointSize : PSIZE;\n"
                "    float4 gl_Position : SV_Position;\n"
                "};\n"
                "\n"
                "struct GS_OUTPUT\n"
                "{\n";

    geomHLSL += varyingHLSL;

    if (fragmentShader->mUsesFragCoord)
    {
        geomHLSL += "    float4 gl_FragCoord : " + fragCoordSemantic + ";\n";
    }

    if (fragmentShader->mUsesPointCoord)
    {
        geomHLSL += "    float2 gl_PointCoord : " + pointCoordSemantic + ";\n";
    }

    geomHLSL +=   "    float gl_PointSize : PSIZE;\n"
                  "    float4 gl_Position : SV_Position;\n"
                  "};\n"
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
                  "static float minPointSize = " + Str(ALIASED_POINT_SIZE_RANGE_MIN) + ".0f;\n"
                  "static float maxPointSize = " + Str(mRenderer->getMaxPointSize()) + ".0f;\n"
                  "\n"
                  "[maxvertexcount(4)]\n"
                  "void main(point GS_INPUT input[1], inout TriangleStream<GS_OUTPUT> outStream)\n"
                  "{\n"
                  "    GS_OUTPUT output = (GS_OUTPUT)0;\n"
                  "    output.gl_PointSize = input[0].gl_PointSize;\n";

    for (int r = 0; r < registers; r++)
    {
        geomHLSL += "    output.v" + Str(r) + " = input[0].v" + Str(r) + ";\n";
    }

    if (fragmentShader->mUsesFragCoord)
    {
        geomHLSL += "    output.gl_FragCoord = input[0].gl_FragCoord;\n";
    }

    geomHLSL += "    \n"
                "    float gl_PointSize = clamp(input[0].gl_PointSize, minPointSize, maxPointSize);\n"
                "    float4 gl_Position = input[0].gl_Position;\n"
                "    float2 viewportScale = float2(1.0f / dx_ViewCoords.x, 1.0f / dx_ViewCoords.y) * gl_Position.w;\n";

    for (int corner = 0; corner < 4; corner++)
    {
        geomHLSL += "    \n"
                    "    output.gl_Position = gl_Position + float4(pointSpriteCorners[" + Str(corner) + "] * viewportScale * gl_PointSize, 0.0f, 0.0f);\n";

        if (fragmentShader->mUsesPointCoord)
        {
            geomHLSL += "    output.gl_PointCoord = pointSpriteTexcoords[" + Str(corner) + "];\n";
        }

        geomHLSL += "    outStream.Append(output);\n";
    }

    geomHLSL += "    \n"
                "    outStream.RestartStrip();\n"
                "}\n";

    return geomHLSL;
}

// This method needs to match OutputHLSL::decorate
std::string DynamicHLSL::decorateVariable(const std::string &name)
{
    if (name.compare(0, 3, "gl_") != 0 && name.compare(0, 3, "dx_") != 0)
    {
        return "_" + name;
    }

    return name;
}

std::string DynamicHLSL::generateAttributeConversionHLSL(const VertexFormat &vertexFormat, const ShaderVariable &shaderAttrib) const
{
    std::string attribString = "input." + decorateVariable(shaderAttrib.name);

    // Matrix
    if (IsMatrixType(shaderAttrib.type))
    {
        return "transpose(" + attribString + ")";
    }

    GLenum shaderComponentType = UniformComponentType(shaderAttrib.type);
    int shaderComponentCount = UniformComponentCount(shaderAttrib.type);

    std::string padString = "";

    // Perform integer to float conversion (if necessary)
    bool requiresTypeConversion = (shaderComponentType == GL_FLOAT && vertexFormat.mType != GL_FLOAT);

    // TODO: normalization for 32-bit integer formats
    ASSERT(!requiresTypeConversion || !vertexFormat.mNormalized);

    if (requiresTypeConversion || !padString.empty())
    {
        return "float" + Str(shaderComponentCount) + "(" + attribString + padString + ")";
    }

    // No conversion necessary
    return attribString;
}

}

//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Wrapper for Khronos glslang compiler.
//

#include "libANGLE/renderer/glslang_wrapper_utils.h"

// glslang has issues with some specific warnings.
ANGLE_DISABLE_EXTRA_SEMI_WARNING
ANGLE_DISABLE_SHADOWING_WARNING

// glslang's version of ShaderLang.h, not to be confused with ANGLE's.
#include <glslang/Public/ShaderLang.h>

// Other glslang includes.
#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/ResourceLimits.h>

ANGLE_REENABLE_SHADOWING_WARNING
ANGLE_REENABLE_EXTRA_SEMI_WARNING

// SPIR-V headers include for AST transformation.
#include <spirv/unified1/spirv.hpp>

// SPIR-V tools include for AST validation.
#include <spirv-tools/libspirv.hpp>

#include <array>
#include <numeric>

#include "common/FixedVector.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/ProgramLinkedResources.h"

#define ANGLE_GLSLANG_CHECK(CALLBACK, TEST, ERR) \
    do                                           \
    {                                            \
        if (ANGLE_UNLIKELY(!(TEST)))             \
        {                                        \
            return CALLBACK(ERR);                \
        }                                        \
                                                 \
    } while (0)

namespace rx
{
namespace
{
constexpr char kXfbDeclMarker[]    = "@@ XFB-DECL @@";
constexpr char kXfbOutMarker[]     = "@@ XFB-OUT @@;";
constexpr char kXfbBuiltInPrefix[] = "xfbANGLE";

constexpr gl::ShaderMap<const char *> kDefaultUniformNames = {
    {gl::ShaderType::Vertex, sh::vk::kDefaultUniformsNameVS},
    {gl::ShaderType::Geometry, sh::vk::kDefaultUniformsNameGS},
    {gl::ShaderType::Fragment, sh::vk::kDefaultUniformsNameFS},
    {gl::ShaderType::Compute, sh::vk::kDefaultUniformsNameCS},
};

template <size_t N>
constexpr size_t ConstStrLen(const char (&)[N])
{
    static_assert(N > 0, "C++ shouldn't allow N to be zero");

    // The length of a string defined as a char array is the size of the array minus 1 (the
    // terminating '\0').
    return N - 1;
}

void GetBuiltInResourcesFromCaps(const gl::Caps &caps, TBuiltInResource *outBuiltInResources)
{
    outBuiltInResources->maxDrawBuffers                   = caps.maxDrawBuffers;
    outBuiltInResources->maxAtomicCounterBindings         = caps.maxAtomicCounterBufferBindings;
    outBuiltInResources->maxAtomicCounterBufferSize       = caps.maxAtomicCounterBufferSize;
    outBuiltInResources->maxClipPlanes                    = caps.maxClipPlanes;
    outBuiltInResources->maxCombinedAtomicCounterBuffers  = caps.maxCombinedAtomicCounterBuffers;
    outBuiltInResources->maxCombinedAtomicCounters        = caps.maxCombinedAtomicCounters;
    outBuiltInResources->maxCombinedImageUniforms         = caps.maxCombinedImageUniforms;
    outBuiltInResources->maxCombinedTextureImageUnits     = caps.maxCombinedTextureImageUnits;
    outBuiltInResources->maxCombinedShaderOutputResources = caps.maxCombinedShaderOutputResources;
    outBuiltInResources->maxComputeWorkGroupCountX        = caps.maxComputeWorkGroupCount[0];
    outBuiltInResources->maxComputeWorkGroupCountY        = caps.maxComputeWorkGroupCount[1];
    outBuiltInResources->maxComputeWorkGroupCountZ        = caps.maxComputeWorkGroupCount[2];
    outBuiltInResources->maxComputeWorkGroupSizeX         = caps.maxComputeWorkGroupSize[0];
    outBuiltInResources->maxComputeWorkGroupSizeY         = caps.maxComputeWorkGroupSize[1];
    outBuiltInResources->maxComputeWorkGroupSizeZ         = caps.maxComputeWorkGroupSize[2];
    outBuiltInResources->minProgramTexelOffset            = caps.minProgramTexelOffset;
    outBuiltInResources->maxFragmentUniformVectors        = caps.maxFragmentUniformVectors;
    outBuiltInResources->maxFragmentInputComponents       = caps.maxFragmentInputComponents;
    outBuiltInResources->maxGeometryInputComponents       = caps.maxGeometryInputComponents;
    outBuiltInResources->maxGeometryOutputComponents      = caps.maxGeometryOutputComponents;
    outBuiltInResources->maxGeometryOutputVertices        = caps.maxGeometryOutputVertices;
    outBuiltInResources->maxGeometryTotalOutputComponents = caps.maxGeometryTotalOutputComponents;
    outBuiltInResources->maxLights                        = caps.maxLights;
    outBuiltInResources->maxProgramTexelOffset            = caps.maxProgramTexelOffset;
    outBuiltInResources->maxVaryingComponents             = caps.maxVaryingComponents;
    outBuiltInResources->maxVaryingVectors                = caps.maxVaryingVectors;
    outBuiltInResources->maxVertexAttribs                 = caps.maxVertexAttributes;
    outBuiltInResources->maxVertexOutputComponents        = caps.maxVertexOutputComponents;
    outBuiltInResources->maxVertexUniformVectors          = caps.maxVertexUniformVectors;
}

// Test if there are non-zero indices in the uniform name, returning false in that case.  This
// happens for multi-dimensional arrays, where a uniform is created for every possible index of the
// array (except for the innermost dimension).  When assigning decorations (set/binding/etc), only
// the indices corresponding to the first element of the array should be specified.  This function
// is used to skip the other indices.
//
// If useOldRewriteStructSamplers, there are multiple samplers extracted out of struct arrays
// though, so the above only applies to the sampler array defined in the struct.
bool UniformNameIsIndexZero(const std::string &name, bool excludeCheckForOwningStructArrays)
{
    size_t lastBracketClose = 0;

    if (excludeCheckForOwningStructArrays)
    {
        size_t lastDot = name.find_last_of('.');
        if (lastDot != std::string::npos)
        {
            lastBracketClose = lastDot;
        }
    }

    while (true)
    {
        size_t openBracket = name.find('[', lastBracketClose);
        if (openBracket == std::string::npos)
        {
            break;
        }
        size_t closeBracket = name.find(']', openBracket);

        // If the index between the brackets is not zero, ignore this uniform.
        if (name.substr(openBracket + 1, closeBracket - openBracket - 1) != "0")
        {
            return false;
        }
        lastBracketClose = closeBracket;
    }

    return true;
}

// Strip indices from the name.  If there are non-zero indices, return false to indicate that this
// image uniform doesn't require set/binding.  That is done on index 0.
bool GetImageNameWithoutIndices(std::string *name)
{
    if (name->back() != ']')
    {
        return true;
    }

    if (!UniformNameIsIndexZero(*name, false))
    {
        return false;
    }

    // Strip all indices
    *name = name->substr(0, name->find('['));
    return true;
}

bool MappedSamplerNameNeedsUserDefinedPrefix(const std::string &originalName)
{
    return originalName.find('.') == std::string::npos;
}

std::string GetMappedSamplerNameOld(const std::string &originalName)
{
    std::string samplerName = gl::ParseResourceName(originalName, nullptr);

    // Samplers in structs are extracted.
    std::replace(samplerName.begin(), samplerName.end(), '.', '_');

    // Samplers in arrays of structs are also extracted.
    std::replace(samplerName.begin(), samplerName.end(), '[', '_');
    samplerName.erase(std::remove(samplerName.begin(), samplerName.end(), ']'), samplerName.end());

    if (MappedSamplerNameNeedsUserDefinedPrefix(originalName))
    {
        samplerName = sh::kUserDefinedNamePrefix + samplerName;
    }

    return samplerName;
}

template <typename OutputIter, typename ImplicitIter>
uint32_t CountExplicitOutputs(OutputIter outputsBegin,
                              OutputIter outputsEnd,
                              ImplicitIter implicitsBegin,
                              ImplicitIter implicitsEnd)
{
    auto reduce = [implicitsBegin, implicitsEnd](uint32_t count, const sh::ShaderVariable &var) {
        bool isExplicit = std::find(implicitsBegin, implicitsEnd, var.name) == implicitsEnd;
        return count + isExplicit;
    };

    return std::accumulate(outputsBegin, outputsEnd, 0, reduce);
}

ShaderInterfaceVariableInfo *AddShaderInterfaceVariable(ShaderInterfaceVariableInfoMap *infoMap,
                                                        const std::string &varName)
{
    ASSERT(infoMap->find(varName) == infoMap->end());
    return &(*infoMap)[varName];
}

ShaderInterfaceVariableInfo *GetShaderInterfaceVariable(ShaderInterfaceVariableInfoMap *infoMap,
                                                        const std::string &varName)
{
    ASSERT(infoMap->find(varName) != infoMap->end());
    return &(*infoMap)[varName];
}

ShaderInterfaceVariableInfo *AddResourceInfo(ShaderInterfaceVariableInfoMap *infoMap,
                                             const std::string &varName,
                                             uint32_t descriptorSet,
                                             uint32_t binding)
{
    gl::ShaderBitSet allStages;
    allStages.set();

    ShaderInterfaceVariableInfo *info = AddShaderInterfaceVariable(infoMap, varName);
    info->descriptorSet               = descriptorSet;
    info->binding                     = binding;
    info->activeStages                = allStages;
    return info;
}

// Add location information for an in/out variable.
ShaderInterfaceVariableInfo *AddLocationInfo(ShaderInterfaceVariableInfoMap *infoMap,
                                             const std::string &varName,
                                             uint32_t location,
                                             uint32_t component,
                                             gl::ShaderBitSet activeStages)
{
    // The info map for this name may or may not exist already.  This function merges the
    // location/component information.
    ShaderInterfaceVariableInfo *info = &(*infoMap)[varName];

    for (const gl::ShaderType shaderType : activeStages)
    {
        ASSERT(info->descriptorSet == ShaderInterfaceVariableInfo::kInvalid);
        ASSERT(info->binding == ShaderInterfaceVariableInfo::kInvalid);
        ASSERT(info->location[shaderType] == ShaderInterfaceVariableInfo::kInvalid);
        ASSERT(info->component[shaderType] == ShaderInterfaceVariableInfo::kInvalid);

        info->location[shaderType]  = location;
        info->component[shaderType] = component;
    }
    info->activeStages |= activeStages;

    return info;
}

// Modify an existing out variable and add transform feedback information.
ShaderInterfaceVariableInfo *SetXfbInfo(ShaderInterfaceVariableInfoMap *infoMap,
                                        const std::string &varName,
                                        uint32_t xfbBuffer,
                                        uint32_t xfbOffset,
                                        uint32_t xfbStride)
{
    ShaderInterfaceVariableInfo *info = GetShaderInterfaceVariable(infoMap, varName);

    ASSERT(info->xfbBuffer == ShaderInterfaceVariableInfo::kInvalid);
    ASSERT(info->xfbOffset == ShaderInterfaceVariableInfo::kInvalid);
    ASSERT(info->xfbStride == ShaderInterfaceVariableInfo::kInvalid);

    info->xfbBuffer = xfbBuffer;
    info->xfbOffset = xfbOffset;
    info->xfbStride = xfbStride;
    return info;
}

std::string SubstituteTransformFeedbackMarkers(const std::string &originalSource,
                                               const std::string &xfbDecl,
                                               const std::string &xfbOut)
{
    const size_t xfbDeclMarkerStart = originalSource.find(kXfbDeclMarker);
    const size_t xfbDeclMarkerEnd   = xfbDeclMarkerStart + ConstStrLen(kXfbDeclMarker);

    const size_t xfbOutMarkerStart = originalSource.find(kXfbOutMarker, xfbDeclMarkerStart);
    const size_t xfbOutMarkerEnd   = xfbOutMarkerStart + ConstStrLen(kXfbOutMarker);

    // The shader is the following form:
    //
    // ..part1..
    // @@ XFB-DECL @@
    // ..part2..
    // @@ XFB-OUT @@;
    // ..part3..
    //
    // Construct the string by concatenating these five pieces, replacing the markers with the given
    // values.
    std::string result;

    result.append(&originalSource[0], &originalSource[xfbDeclMarkerStart]);
    result.append(xfbDecl);
    result.append(&originalSource[xfbDeclMarkerEnd], &originalSource[xfbOutMarkerStart]);
    result.append(xfbOut);
    result.append(&originalSource[xfbOutMarkerEnd], &originalSource[originalSource.size()]);

    return result;
}

std::string GenerateTransformFeedbackVaryingOutput(const gl::TransformFeedbackVarying &varying,
                                                   const gl::UniformTypeInfo &info,
                                                   size_t strideBytes,
                                                   size_t offset,
                                                   const std::string &bufferIndex)
{
    std::ostringstream result;

    ASSERT(strideBytes % 4 == 0);
    size_t stride = strideBytes / 4;

    const size_t arrayIndexStart = varying.arrayIndex == GL_INVALID_INDEX ? 0 : varying.arrayIndex;
    const size_t arrayIndexEnd   = arrayIndexStart + varying.size();

    for (size_t arrayIndex = arrayIndexStart; arrayIndex < arrayIndexEnd; ++arrayIndex)
    {
        for (int col = 0; col < info.columnCount; ++col)
        {
            for (int row = 0; row < info.rowCount; ++row)
            {
                result << "xfbOut" << bufferIndex << "[" << sh::vk::kDriverUniformsVarName
                       << ".xfbBufferOffsets[" << bufferIndex
                       << "] + (gl_VertexIndex + gl_InstanceIndex * "
                       << sh::vk::kDriverUniformsVarName << ".xfbVerticesPerDraw) * " << stride
                       << " + " << offset << "] = " << info.glslAsFloat << "("
                       << varying.mappedName;

                if (varying.isArray())
                {
                    result << "[" << arrayIndex << "]";
                }

                if (info.columnCount > 1)
                {
                    result << "[" << col << "]";
                }

                if (info.rowCount > 1)
                {
                    result << "[" << row << "]";
                }

                result << ");\n";
                ++offset;
            }
        }
    }

    return result.str();
}

void GenerateTransformFeedbackEmulationOutputs(const GlslangSourceOptions &options,
                                               const gl::ProgramState &programState,
                                               std::string *vertexShader,
                                               ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    const std::vector<gl::TransformFeedbackVarying> &varyings =
        programState.getLinkedTransformFeedbackVaryings();
    const std::vector<GLsizei> &bufferStrides = programState.getTransformFeedbackStrides();
    const bool isInterleaved =
        programState.getTransformFeedbackBufferMode() == GL_INTERLEAVED_ATTRIBS;
    const size_t bufferCount = isInterleaved ? 1 : varyings.size();

    const std::string xfbSet = Str(options.uniformsAndXfbDescriptorSetIndex);
    std::vector<std::string> xfbIndices(bufferCount);

    std::string xfbDecl;

    for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        const std::string xfbBinding = Str(options.xfbBindingIndexStart + bufferIndex);
        xfbIndices[bufferIndex]      = Str(bufferIndex);

        std::string bufferName = "xfbBuffer" + xfbIndices[bufferIndex];

        xfbDecl += "layout(set = " + xfbSet + ", binding = " + xfbBinding + ") buffer " +
                   bufferName + " { float xfbOut" + xfbIndices[bufferIndex] + "[]; };\n";

        // Add this entry to the info map, so we can easily assert that every resource has an entry
        // in this map.
        AddResourceInfo(variableInfoMapOut, bufferName, options.uniformsAndXfbDescriptorSetIndex,
                        options.xfbBindingIndexStart + bufferIndex);
    }

    std::string xfbOut =
        "if (" + std::string(sh::vk::kDriverUniformsVarName) + ".xfbActiveUnpaused != 0)\n{\n";
    size_t outputOffset = 0;
    for (size_t varyingIndex = 0; varyingIndex < varyings.size(); ++varyingIndex)
    {
        const size_t bufferIndex                    = isInterleaved ? 0 : varyingIndex;
        const gl::TransformFeedbackVarying &varying = varyings[varyingIndex];

        // For every varying, output to the respective buffer packed.  If interleaved, the output is
        // always to the same buffer, but at different offsets.
        const gl::UniformTypeInfo &info = gl::GetUniformTypeInfo(varying.type);
        xfbOut += GenerateTransformFeedbackVaryingOutput(varying, info, bufferStrides[bufferIndex],
                                                         outputOffset, xfbIndices[bufferIndex]);

        if (isInterleaved)
        {
            outputOffset += info.columnCount * info.rowCount * varying.size();
        }
    }
    xfbOut += "}\n";

    *vertexShader = SubstituteTransformFeedbackMarkers(*vertexShader, xfbDecl, xfbOut);
}

bool IsFirstRegisterOfVarying(const gl::PackedVaryingRegister &varyingReg)
{
    const gl::PackedVarying &varying = *varyingReg.packedVarying;

    // In Vulkan GLSL, struct fields are not allowed to have location assignments.  The varying of a
    // struct type is thus given a location equal to the one assigned to its first field.
    if (varying.isStructField() && varying.fieldIndex > 0)
    {
        return false;
    }

    // Similarly, assign array varying locations to the assigned location of the first element.
    if (varyingReg.varyingArrayIndex != 0 || (varying.isArrayElement() && varying.arrayIndex != 0))
    {
        return false;
    }

    // Similarly, assign matrix varying locations to the assigned location of the first row.
    if (varyingReg.varyingRowIndex != 0)
    {
        return false;
    }

    return true;
}

// Calculates XFB layout qualifier arguments for each tranform feedback varying.  Stores calculated
// values for the SPIR-V transformation.
void GenerateTransformFeedbackExtensionOutputs(const gl::ProgramState &programState,
                                               const gl::ProgramLinkedResources &resources,
                                               std::string *vertexShader,
                                               ShaderInterfaceVariableInfoMap *variableInfoMapOut,
                                               uint32_t *locationsUsedForXfbExtensionOut)
{
    const std::vector<gl::TransformFeedbackVarying> &tfVaryings =
        programState.getLinkedTransformFeedbackVaryings();

    std::string xfbDecl;
    std::string xfbOut;

    for (uint32_t varyingIndex = 0; varyingIndex < tfVaryings.size(); ++varyingIndex)
    {
        const gl::TransformFeedbackVarying &tfVarying = tfVaryings[varyingIndex];
        const std::string &tfVaryingName              = tfVarying.mappedName;

        if (tfVarying.isBuiltIn())
        {
            // For simplicity, create a copy of every builtin that's captured so xfb qualifiers
            // could be added to that instead.  This allows the SPIR-V transformation to ignore
            // OpMemberName and OpMemberDecorate instructions.  Note that capturing gl_Position
            // already requires such a copy, since the translator modifies this value at the end of
            // main.  Capturing the rest of the built-ins are niche enough that the inefficiency
            // involved in doing this is not a concern.

            uint32_t xfbVaryingLocation = resources.varyingPacking.getMaxSemanticIndex() +
                                          ++(*locationsUsedForXfbExtensionOut);

            std::string xfbVaryingName = kXfbBuiltInPrefix + tfVaryingName;

            // Add declaration and initialization code for the new varying.
            std::string varyingType = gl::GetGLSLTypeString(tfVarying.type);
            xfbDecl += "layout(location = " + Str(xfbVaryingLocation) + ") out " + varyingType +
                       " " + xfbVaryingName + ";\n";
            xfbOut += xfbVaryingName + " = " + tfVaryingName + ";\n";
        }
    }

    *vertexShader = SubstituteTransformFeedbackMarkers(*vertexShader, xfbDecl, xfbOut);
}

void AssignAttributeLocations(const gl::ProgramState &programState,
                              ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    gl::ShaderBitSet vertexOnly;
    vertexOnly.set(gl::ShaderType::Vertex);

    // Assign attribute locations for the vertex shader.
    for (const sh::ShaderVariable &attribute : programState.getProgramInputs())
    {
        ASSERT(attribute.active);

        AddLocationInfo(variableInfoMapOut, attribute.mappedName, attribute.location,
                        ShaderInterfaceVariableInfo::kInvalid, vertexOnly);
    }
}

void AssignOutputLocations(const gl::ProgramState &programState,
                           ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    // Assign output locations for the fragment shader.
    // TODO(syoussefi): Add support for EXT_blend_func_extended.  http://anglebug.com/3385
    const auto &outputLocations                      = programState.getOutputLocations();
    const auto &outputVariables                      = programState.getOutputVariables();
    const std::array<std::string, 3> implicitOutputs = {"gl_FragDepth", "gl_SampleMask",
                                                        "gl_FragStencilRefARB"};

    gl::ShaderBitSet fragmentOnly;
    fragmentOnly.set(gl::ShaderType::Fragment);

    for (const gl::VariableLocation &outputLocation : outputLocations)
    {
        if (outputLocation.arrayIndex == 0 && outputLocation.used() && !outputLocation.ignored)
        {
            const sh::ShaderVariable &outputVar = outputVariables[outputLocation.index];

            uint32_t location = 0;
            if (outputVar.location != -1)
            {
                location = outputVar.location;
            }
            else if (std::find(implicitOutputs.begin(), implicitOutputs.end(), outputVar.name) ==
                     implicitOutputs.end())
            {
                // If there is only one output, it is allowed not to have a location qualifier, in
                // which case it defaults to 0.  GLSL ES 3.00 spec, section 4.3.8.2.
                ASSERT(CountExplicitOutputs(outputVariables.begin(), outputVariables.end(),
                                            implicitOutputs.begin(), implicitOutputs.end()) == 1);
            }

            AddLocationInfo(variableInfoMapOut, outputVar.mappedName, location,
                            ShaderInterfaceVariableInfo::kInvalid, fragmentOnly);
        }
    }

    // When no fragment output is specified by the shader, the translator outputs webgl_FragColor or
    // webgl_FragData.  Add an entry for these.  Even though the translator is already assigning
    // location 0 to these entries, adding an entry for them here allows us to ASSERT that every
    // shader interface variable is processed during the SPIR-V transformation.  This is done when
    // iterating the ids provided by OpEntryPoint.
    AddLocationInfo(variableInfoMapOut, "webgl_FragColor", 0, 0, fragmentOnly);
    AddLocationInfo(variableInfoMapOut, "webgl_FragData", 0, 0, fragmentOnly);
}

void AssignVaryingLocations(const GlslangSourceOptions &options,
                            const gl::ProgramState &programState,
                            const gl::ProgramLinkedResources &resources,
                            uint32_t locationsUsedForXfbExtension,
                            ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    uint32_t locationsUsedForEmulation = locationsUsedForXfbExtension;

    // Substitute layout and qualifier strings for the position varying added for line raster
    // emulation.
    if (options.emulateBresenhamLines)
    {
        uint32_t lineRasterEmulationPositionLocation = locationsUsedForEmulation++;

        gl::ShaderBitSet allActiveStages;
        allActiveStages.set();

        AddLocationInfo(variableInfoMapOut, sh::vk::kLineRasterEmulationPosition,
                        lineRasterEmulationPositionLocation, ShaderInterfaceVariableInfo::kInvalid,
                        allActiveStages);
    }

    // Assign varying locations.
    for (const gl::PackedVaryingRegister &varyingReg : resources.varyingPacking.getRegisterList())
    {
        if (!IsFirstRegisterOfVarying(varyingReg))
        {
            continue;
        }

        const gl::PackedVarying &varying = *varyingReg.packedVarying;

        // In the following:
        //
        //     struct S { vec4 field; };
        //     out S varStruct;
        //
        // "_uvarStruct" is found through |parentStructMappedName|, with |varying->mappedName|
        // being "_ufield".  In such a case, use |parentStructMappedName|.
        const std::string &name =
            varying.isStructField() ? varying.parentStructMappedName : varying.varying->mappedName;

        uint32_t location  = varyingReg.registerRow + locationsUsedForEmulation;
        uint32_t component = ShaderInterfaceVariableInfo::kInvalid;
        if (varyingReg.registerColumn > 0)
        {
            ASSERT(!varying.varying->isStruct());
            ASSERT(!gl::IsMatrixType(varying.varying->type));
            component = varyingReg.registerColumn;
        }

        AddLocationInfo(variableInfoMapOut, name, location, component, varying.shaderStages);
    }

    // Add an entry for inactive varyings.
    for (const std::string &varyingName : resources.varyingPacking.getInactiveVaryingMappedNames())
    {
        bool isBuiltin = angle::BeginsWith(varyingName, "gl_");
        if (isBuiltin)
        {
            continue;
        }

        // TODO(syoussefi): inactive varying names should be unique.  However, due to mishandling of
        // partially captured arrays, a varying name can end up in both the active and inactive
        // lists.  The test below should be removed once that issue is resolved.
        // http://anglebug.com/4140
        if (variableInfoMapOut->find(varyingName) != variableInfoMapOut->end())
        {
            continue;
        }

        AddLocationInfo(variableInfoMapOut, varyingName, ShaderInterfaceVariableInfo::kInvalid,
                        ShaderInterfaceVariableInfo::kInvalid, gl::ShaderBitSet());
    }
}

// Calculates XFB layout qualifier arguments for each tranform feedback varying.  Stores calculated
// values for the SPIR-V transformation.
void AssignTransformFeedbackExtensionQualifiers(const gl::ProgramState &programState,
                                                const gl::ProgramLinkedResources &resources,
                                                uint32_t locationsUsedForXfbExtension,
                                                ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    const std::vector<gl::TransformFeedbackVarying> &tfVaryings =
        programState.getLinkedTransformFeedbackVaryings();
    const std::vector<GLsizei> &varyingStrides = programState.getTransformFeedbackStrides();
    const bool isInterleaved =
        programState.getTransformFeedbackBufferMode() == GL_INTERLEAVED_ATTRIBS;

    std::string xfbDecl;
    std::string xfbOut;
    uint32_t currentOffset          = 0;
    uint32_t currentStride          = 0;
    uint32_t bufferIndex            = 0;
    uint32_t currentBuiltinLocation = 0;

    for (uint32_t varyingIndex = 0; varyingIndex < tfVaryings.size(); ++varyingIndex)
    {
        if (isInterleaved)
        {
            bufferIndex = 0;
            if (varyingIndex > 0)
            {
                const gl::TransformFeedbackVarying &prev = tfVaryings[varyingIndex - 1];
                currentOffset += prev.size() * gl::VariableExternalSize(prev.type);
            }
            currentStride = varyingStrides[0];
        }
        else
        {
            bufferIndex   = varyingIndex;
            currentOffset = 0;
            currentStride = varyingStrides[varyingIndex];
        }

        const gl::TransformFeedbackVarying &tfVarying = tfVaryings[varyingIndex];
        const std::string &tfVaryingName              = tfVarying.mappedName;

        if (tfVarying.isBuiltIn())
        {
            uint32_t xfbVaryingLocation = currentBuiltinLocation++;
            std::string xfbVaryingName  = kXfbBuiltInPrefix + tfVaryingName;

            ASSERT(xfbVaryingLocation < locationsUsedForXfbExtension);

            gl::ShaderBitSet vertexOnly;
            vertexOnly.set(gl::ShaderType::Vertex);

            AddLocationInfo(variableInfoMapOut, xfbVaryingName, xfbVaryingLocation,
                            ShaderInterfaceVariableInfo::kInvalid, vertexOnly);
            SetXfbInfo(variableInfoMapOut, xfbVaryingName, bufferIndex, currentOffset,
                       currentStride);
        }
        else if (!tfVarying.isArray() || tfVarying.arrayIndex == 0)
        {
            // Note: capturing individual array elements using the Vulkan transform feedback
            // extension is not supported, and it unlikely to be ever supported (on the contrary, it
            // may be removed from the GLES spec).  http://anglebug.com/4140

            // Find the varying with this name.  If a struct is captured, we would be iterating over
            // its fields, and the name of the varying is found through parentStructMappedName.  Not
            // only that, but also we should only do this for the first field of the struct.
            const gl::PackedVarying *originalVarying = nullptr;
            for (const gl::PackedVaryingRegister &varyingReg :
                 resources.varyingPacking.getRegisterList())
            {
                if (!IsFirstRegisterOfVarying(varyingReg))
                {
                    continue;
                }

                const gl::PackedVarying *varying = varyingReg.packedVarying;

                if (varying->varying->name == tfVarying.name)
                {
                    originalVarying = varying;
                    break;
                }
            }

            if (originalVarying)
            {
                const std::string &mappedName = originalVarying->isStructField()
                                                    ? originalVarying->parentStructMappedName
                                                    : originalVarying->varying->mappedName;

                // Set xfb info for this varying.  AssignVaryingLocations should have already added
                // location information for these varyings.
                SetXfbInfo(variableInfoMapOut, mappedName, bufferIndex, currentOffset,
                           currentStride);
            }
        }
    }
}

void AssignUniformBindings(const GlslangSourceOptions &options,
                           gl::ShaderMap<std::string> *shaderSources,
                           ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    // Assign binding to the default uniforms block of each shader stage.
    uint32_t bindingIndex = 0;
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const std::string &shaderSource = (*shaderSources)[shaderType];
        if (!shaderSource.empty())
        {
            AddResourceInfo(variableInfoMapOut, kDefaultUniformNames[shaderType],
                            options.uniformsAndXfbDescriptorSetIndex, bindingIndex);
            ++bindingIndex;
        }
    }

    // Assign binding to the driver uniforms block
    AddResourceInfo(variableInfoMapOut, sh::vk::kDriverUniformsVarName,
                    options.driverUniformsDescriptorSetIndex, 0);
}

uint32_t AssignInterfaceBlockBindings(const GlslangSourceOptions &options,
                                      const std::vector<gl::InterfaceBlock> &blocks,
                                      uint32_t bindingStart,
                                      ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    uint32_t bindingIndex = bindingStart;
    for (const gl::InterfaceBlock &block : blocks)
    {
        if (!block.isArray || block.arrayElement == 0)
        {
            AddResourceInfo(variableInfoMapOut, block.mappedName,
                            options.shaderResourceDescriptorSetIndex, bindingIndex);
            ++bindingIndex;
        }
    }

    return bindingIndex;
}

uint32_t AssignAtomicCounterBufferBindings(const GlslangSourceOptions &options,
                                           const std::vector<gl::AtomicCounterBuffer> &buffers,
                                           uint32_t bindingStart,
                                           ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    if (buffers.size() == 0)
    {
        return bindingStart;
    }

    AddResourceInfo(variableInfoMapOut, sh::vk::kAtomicCountersVarName,
                    options.shaderResourceDescriptorSetIndex, bindingStart);

    return bindingStart + 1;
}

uint32_t AssignImageBindings(const GlslangSourceOptions &options,
                             const std::vector<gl::LinkedUniform> &uniforms,
                             const gl::RangeUI &imageUniformRange,
                             uint32_t bindingStart,
                             ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    uint32_t bindingIndex = bindingStart;
    for (unsigned int uniformIndex : imageUniformRange)
    {
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        std::string name = imageUniform.mappedName;
        if (GetImageNameWithoutIndices(&name))
        {
            AddResourceInfo(variableInfoMapOut, name, options.shaderResourceDescriptorSetIndex,
                            bindingIndex);
        }
        ++bindingIndex;
    }

    return bindingIndex;
}

void AssignNonTextureBindings(const GlslangSourceOptions &options,
                              const gl::ProgramState &programState,
                              ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    uint32_t bindingStart = 0;

    const std::vector<gl::InterfaceBlock> &uniformBlocks = programState.getUniformBlocks();
    bindingStart =
        AssignInterfaceBlockBindings(options, uniformBlocks, bindingStart, variableInfoMapOut);

    const std::vector<gl::InterfaceBlock> &storageBlocks = programState.getShaderStorageBlocks();
    bindingStart =
        AssignInterfaceBlockBindings(options, storageBlocks, bindingStart, variableInfoMapOut);

    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers =
        programState.getAtomicCounterBuffers();
    bindingStart = AssignAtomicCounterBufferBindings(options, atomicCounterBuffers, bindingStart,
                                                     variableInfoMapOut);

    const std::vector<gl::LinkedUniform> &uniforms = programState.getUniforms();
    const gl::RangeUI &imageUniformRange           = programState.getImageUniformRange();
    bindingStart =
        AssignImageBindings(options, uniforms, imageUniformRange, bindingStart, variableInfoMapOut);
}

void AssignTextureBindings(const GlslangSourceOptions &options,
                           const gl::ProgramState &programState,
                           ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    // Assign textures to a descriptor set and binding.
    uint32_t bindingIndex                          = 0;
    const std::vector<gl::LinkedUniform> &uniforms = programState.getUniforms();

    for (unsigned int uniformIndex : programState.getSamplerUniformRange())
    {
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

        if (!options.useOldRewriteStructSamplers &&
            gl::SamplerNameContainsNonZeroArrayElement(samplerUniform.name))
        {
            continue;
        }

        if (UniformNameIsIndexZero(samplerUniform.name, options.useOldRewriteStructSamplers))
        {
            // Samplers in structs are extracted and renamed.
            const std::string samplerName = options.useOldRewriteStructSamplers
                                                ? GetMappedSamplerNameOld(samplerUniform.name)
                                                : GlslangGetMappedSamplerName(samplerUniform.name);

            AddResourceInfo(variableInfoMapOut, samplerName, options.textureDescriptorSetIndex,
                            bindingIndex);
        }

        ++bindingIndex;
    }
}

constexpr gl::ShaderMap<EShLanguage> kShLanguageMap = {
    {gl::ShaderType::Vertex, EShLangVertex},
    {gl::ShaderType::Geometry, EShLangGeometry},
    {gl::ShaderType::Fragment, EShLangFragment},
    {gl::ShaderType::Compute, EShLangCompute},
};

angle::Result GetShaderSpirvCode(GlslangErrorCallback callback,
                                 const gl::Caps &glCaps,
                                 const gl::ShaderMap<std::string> &shaderSources,
                                 gl::ShaderMap<std::vector<uint32_t>> *spirvBlobsOut)
{
    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

    TBuiltInResource builtInResources(glslang::DefaultTBuiltInResource);
    GetBuiltInResourcesFromCaps(glCaps, &builtInResources);

    glslang::TShader vertexShader(EShLangVertex);
    glslang::TShader fragmentShader(EShLangFragment);
    glslang::TShader geometryShader(EShLangGeometry);
    glslang::TShader computeShader(EShLangCompute);

    gl::ShaderMap<glslang::TShader *> shaders = {
        {gl::ShaderType::Vertex, &vertexShader},
        {gl::ShaderType::Fragment, &fragmentShader},
        {gl::ShaderType::Geometry, &geometryShader},
        {gl::ShaderType::Compute, &computeShader},
    };
    glslang::TProgram program;

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (shaderSources[shaderType].empty())
        {
            continue;
        }

        const char *shaderString = shaderSources[shaderType].c_str();
        int shaderLength         = static_cast<int>(shaderSources[shaderType].size());

        glslang::TShader *shader = shaders[shaderType];
        shader->setStringsWithLengths(&shaderString, &shaderLength, 1);
        shader->setEntryPoint("main");

        bool result = shader->parse(&builtInResources, 450, ECoreProfile, false, false, messages);
        if (!result)
        {
            ERR() << "Internal error parsing Vulkan shader corresponding to " << shaderType << ":\n"
                  << shader->getInfoLog() << "\n"
                  << shader->getInfoDebugLog() << "\n";
            ANGLE_GLSLANG_CHECK(callback, false, GlslangError::InvalidShader);
        }

        program.addShader(shader);
    }

    bool linkResult = program.link(messages);
    if (!linkResult)
    {
        ERR() << "Internal error linking Vulkan shaders:\n" << program.getInfoLog() << "\n";
        ANGLE_GLSLANG_CHECK(callback, false, GlslangError::InvalidShader);
    }

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (shaderSources[shaderType].empty())
        {
            continue;
        }

        glslang::TIntermediate *intermediate = program.getIntermediate(kShLanguageMap[shaderType]);
        glslang::GlslangToSpv(*intermediate, (*spirvBlobsOut)[shaderType]);
    }

    return angle::Result::Continue;
}

void ValidateSpirvMessage(spv_message_level_t level,
                          const char *source,
                          const spv_position_t &position,
                          const char *message)
{
    WARN() << "Level" << level << ": " << message;
}

bool ValidateSpirv(const std::vector<uint32_t> &spirvBlob)
{
    spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);

    spirvTools.SetMessageConsumer(ValidateSpirvMessage);
    bool result = spirvTools.Validate(spirvBlob);

    if (!result)
    {
        std::string readableSpirv;
        result = spirvTools.Disassemble(spirvBlob, &readableSpirv,
                                        SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
        WARN() << "Invalid SPIR-V:\n" << readableSpirv;
    }

    return result;
}

// A SPIR-V transformer.  It walks the instructions and modifies them as necessary, for example to
// assign bindings or locations.
class SpirvTransformer final : angle::NonCopyable
{
  public:
    SpirvTransformer(const std::vector<uint32_t> &spirvBlobIn,
                     const ShaderInterfaceVariableInfoMap &variableInfoMap,
                     gl::ShaderType shaderType,
                     SpirvBlob *spirvBlobOut)
        : mSpirvBlobIn(spirvBlobIn),
          mShaderType(shaderType),
          mHasTransformFeedbackOutput(false),
          mVariableInfoMap(variableInfoMap),
          mSpirvBlobOut(spirvBlobOut)
    {
        gl::ShaderBitSet allStages;
        allStages.set();

        mBuiltinVariableInfo.activeStages = allStages;
    }

    bool transform();

  private:
    // SPIR-V 1.0 Table 1: First Words of Physical Layout
    enum HeaderIndex
    {
        kHeaderIndexMagic        = 0,
        kHeaderIndexVersion      = 1,
        kHeaderIndexGenerator    = 2,
        kHeaderIndexIndexBound   = 3,
        kHeaderIndexSchema       = 4,
        kHeaderIndexInstructions = 5,
    };

    // A prepass to resolve interesting ids:
    void resolveVariableIds();

    // Transform instructions:
    void transformInstruction();

    // Instructions that are purely informational:
    void visitName(const uint32_t *instruction);
    void visitTypeHelper(const uint32_t *instruction, size_t idIndex, size_t typeIdIndex);
    void visitTypeArray(const uint32_t *instruction);
    void visitTypePointer(const uint32_t *instruction);
    void visitVariable(const uint32_t *instruction);

    // Instructions that potentially need transformation.  They return true if the instruction is
    // transformed.  If false is returned, the instruction should be copied as-is.
    bool transformAccessChain(const uint32_t *instruction, size_t wordCount);
    bool transformCapability(const uint32_t *instruction, size_t wordCount);
    bool transformEntryPoint(const uint32_t *instruction, size_t wordCount);
    bool transformDecorate(const uint32_t *instruction, size_t wordCount);
    bool transformTypePointer(const uint32_t *instruction, size_t wordCount);
    bool transformVariable(const uint32_t *instruction, size_t wordCount);

    // Any other instructions:
    size_t copyInstruction(const uint32_t *instruction, size_t wordCount);
    uint32_t getNewId();

    // SPIR-V to transform:
    const std::vector<uint32_t> &mSpirvBlobIn;
    const gl::ShaderType mShaderType;
    bool mHasTransformFeedbackOutput;

    // Input shader variable info map:
    const ShaderInterfaceVariableInfoMap &mVariableInfoMap;
    ShaderInterfaceVariableInfo mBuiltinVariableInfo;

    // Transformed SPIR-V:
    SpirvBlob *mSpirvBlobOut;

    // Traversal state:
    size_t mCurrentWord       = 0;
    bool mIsInFunctionSection = false;

    // Transformation state:

    // Shader variable info per id, if id is a shader variable.
    std::vector<const ShaderInterfaceVariableInfo *> mVariableInfoById;

    // Each OpTypePointer instruction that defines a type with the Output storage class is
    // duplicated with a similar instruction but which defines a type with the Private storage
    // class.  If inactive varyings are encountered, its type is changed to the Private one.  The
    // following vector maps the Output type id to the corresponding Private one.
    std::vector<uint32_t> mTypePointerTransformedId;
};

bool SpirvTransformer::transform()
{
    // Glslang succeeded in outputting SPIR-V, so we assume it's valid.
    ASSERT(mSpirvBlobIn.size() >= kHeaderIndexInstructions);
    // Since SPIR-V comes from a local call to glslang, it necessarily has the same endianness as
    // the running architecture, so no byte-swapping is necessary.
    ASSERT(mSpirvBlobIn[kHeaderIndexMagic] == spv::MagicNumber);

    // Make sure the transformer is not reused to avoid having to reinitialize it here.
    ASSERT(mCurrentWord == 0);
    ASSERT(mIsInFunctionSection == false);

    // Make sure the SpirvBlob is not reused.
    ASSERT(mSpirvBlobOut->empty());

    // First, find all necessary ids and associate them with the information required to transform
    // their decorations.
    resolveVariableIds();

    // Copy the header to SpirvBlob
    mSpirvBlobOut->assign(mSpirvBlobIn.begin(), mSpirvBlobIn.begin() + kHeaderIndexInstructions);

    mCurrentWord = kHeaderIndexInstructions;
    while (mCurrentWord < mSpirvBlobIn.size())
    {
        transformInstruction();
    }

    return true;
}

// SPIR-V 1.0 Table 2: Instruction Physical Layout
uint32_t GetSpirvInstructionLength(const uint32_t *instruction)
{
    return instruction[0] >> 16;
}

uint32_t GetSpirvInstructionOp(const uint32_t *instruction)
{
    constexpr uint32_t kOpMask = 0xFFFFu;
    return instruction[0] & kOpMask;
}

void SetSpirvInstructionLength(uint32_t *instruction, size_t length)
{
    ASSERT(length < 0xFFFFu);

    constexpr uint32_t kLengthMask = 0xFFFF0000u;
    instruction[0] &= ~kLengthMask;
    instruction[0] |= length << 16;
}

void SetSpirvInstructionOp(uint32_t *instruction, uint32_t op)
{
    constexpr uint32_t kOpMask = 0xFFFFu;
    instruction[0] &= ~kOpMask;
    instruction[0] |= op;
}

void SpirvTransformer::resolveVariableIds()
{
    size_t indexBound = mSpirvBlobIn[kHeaderIndexIndexBound];

    // Allocate storage for id-to-info map.  If %i is the id of a name in mVariableInfoMap, index i
    // in this vector will hold a pointer to the ShaderInterfaceVariableInfo object associated with
    // that name in mVariableInfoMap.
    mVariableInfoById.resize(indexBound + 1, nullptr);

    // Allocate storage for Output type pointer map.  At index i, this vector holds the identical
    // type as %i except for its storage class turned to Private.
    mTypePointerTransformedId.resize(indexBound + 1, 0);

    size_t currentWord = kHeaderIndexInstructions;

    while (currentWord < mSpirvBlobIn.size())
    {
        const uint32_t *instruction = &mSpirvBlobIn[currentWord];

        const uint32_t wordCount = GetSpirvInstructionLength(instruction);
        const uint32_t opCode    = GetSpirvInstructionOp(instruction);

        switch (opCode)
        {
            case spv::OpName:
                visitName(instruction);
                break;
            case spv::OpTypeArray:
                visitTypeArray(instruction);
                break;
            case spv::OpTypePointer:
                visitTypePointer(instruction);
                break;
            case spv::OpVariable:
                visitVariable(instruction);
                break;
            case spv::OpFunction:
                // SPIR-V is structured in sections (SPIR-V 1.0 Section 2.4 Logical Layout of a
                // Module). Names appear before decorations, which are followed by type+variables
                // and finally functions.  We are only interested in name and variable declarations
                // (as well as type declarations for the sake of nameless interface blocks).  Early
                // out when the function declaration section is met.
                return;
            default:
                break;
        }

        currentWord += wordCount;
    }
}

void SpirvTransformer::transformInstruction()
{
    const uint32_t *instruction = &mSpirvBlobIn[mCurrentWord];

    const uint32_t wordCount = GetSpirvInstructionLength(instruction);
    const uint32_t opCode    = GetSpirvInstructionOp(instruction);

    // Since glslang succeeded in producing SPIR-V, we assume it to be valid.
    ASSERT(mCurrentWord + wordCount <= mSpirvBlobIn.size());

    if (opCode == spv::OpFunction)
    {
        // SPIR-V is structured in sections.  Function declarations come last.  Only Op*Access*
        // opcodes inside functions need to be inspected.
        mIsInFunctionSection = true;
    }

    // Only look at interesting instructions.
    bool transformed = false;

    if (mIsInFunctionSection)
    {
        // Look at in-function opcodes.
        switch (opCode)
        {
            case spv::OpAccessChain:
            case spv::OpInBoundsAccessChain:
            case spv::OpPtrAccessChain:
            case spv::OpInBoundsPtrAccessChain:
                transformed = transformAccessChain(instruction, wordCount);
                break;
            default:
                break;
        }
    }
    else
    {
        // Look at global declaration opcodes.
        switch (opCode)
        {
            case spv::OpCapability:
                transformed = transformCapability(instruction, wordCount);
                break;
            case spv::OpEntryPoint:
                transformed = transformEntryPoint(instruction, wordCount);
                break;
            case spv::OpDecorate:
                transformed = transformDecorate(instruction, wordCount);
                break;
            case spv::OpTypePointer:
                transformed = transformTypePointer(instruction, wordCount);
                break;
            case spv::OpVariable:
                transformed = transformVariable(instruction, wordCount);
                break;
            default:
                break;
        }
    }

    // If the instruction was not transformed, copy it to output as is.
    if (!transformed)
    {
        copyInstruction(instruction, wordCount);
    }

    // Advance to next instruction.
    mCurrentWord += wordCount;
}

void SpirvTransformer::visitName(const uint32_t *instruction)
{
    // We currently don't have any big-endian devices in the list of supported platforms.  Literal
    // strings in SPIR-V are stored little-endian (SPIR-V 1.0 Section 2.2.1, Literal String), so if
    // a big-endian device is to be supported, the string matching here should be specialized.
    ASSERT(IsLittleEndian());

    // SPIR-V 1.0 Section 3.32 Instructions, OpName
    constexpr size_t kIdIndex   = 1;
    constexpr size_t kNameIndex = 2;

    const uint32_t id = instruction[kIdIndex];
    const char *name  = reinterpret_cast<const char *>(&instruction[kNameIndex]);

    // The names and ids are unique
    ASSERT(id < mVariableInfoById.size());
    ASSERT(mVariableInfoById[id] == nullptr);

    bool isBuiltin = angle::BeginsWith(name, "gl_");
    if (isBuiltin)
    {
        // Make all builtins point to this no-op info.  Adding this entry allows us to ASSERT that
        // every shader interface variable is processed during the SPIR-V transformation.  This is
        // done when iterating the ids provided by OpEntryPoint.
        mVariableInfoById[id] = &mBuiltinVariableInfo;
        return;
    }

    auto infoIter = mVariableInfoMap.find(name);
    if (infoIter == mVariableInfoMap.end())
    {
        return;
    }

    const ShaderInterfaceVariableInfo *info = &infoIter->second;

    // Associate the id of this name with its info.
    mVariableInfoById[id] = info;

    // Note if the variable is captured by transform feedback.  In that case, the TransformFeedback
    // capability needs to be added.
    if (mShaderType != gl::ShaderType::Fragment &&
        info->xfbBuffer != ShaderInterfaceVariableInfo::kInvalid && info->activeStages[mShaderType])
    {
        mHasTransformFeedbackOutput = true;
    }
}

void SpirvTransformer::visitTypeHelper(const uint32_t *instruction,
                                       const size_t idIndex,
                                       const size_t typeIdIndex)
{
    const uint32_t id     = instruction[idIndex];
    const uint32_t typeId = instruction[typeIdIndex];

    // Every type id is declared only once.
    ASSERT(typeId < mVariableInfoById.size());

    if (mVariableInfoById[typeId] != nullptr)
    {
        // Carry the info forward from the base type.  This is only necessary for interface blocks,
        // as the variable info is associated with the block name instead of the variable name (to
        // support nameless interface blocks).  In that case, the variable itself doesn't yet have
        // an associated info.
        ASSERT(id < mVariableInfoById.size());
        ASSERT(mVariableInfoById[id] == nullptr);

        mVariableInfoById[id] = mVariableInfoById[typeId];
    }
}

void SpirvTransformer::visitTypeArray(const uint32_t *instruction)
{
    // SPIR-V 1.0 Section 3.32 Instructions, OpTypeArray
    constexpr size_t kIdIndex            = 1;
    constexpr size_t kElementTypeIdIndex = 2;

    visitTypeHelper(instruction, kIdIndex, kElementTypeIdIndex);
}

void SpirvTransformer::visitTypePointer(const uint32_t *instruction)
{
    // SPIR-V 1.0 Section 3.32 Instructions, OpTypePointer
    constexpr size_t kIdIndex     = 1;
    constexpr size_t kTypeIdIndex = 3;

    visitTypeHelper(instruction, kIdIndex, kTypeIdIndex);
}

void SpirvTransformer::visitVariable(const uint32_t *instruction)
{
    // SPIR-V 1.0 Section 3.32 Instructions, OpVariable
    constexpr size_t kTypeIdIndex       = 1;
    constexpr size_t kIdIndex           = 2;
    constexpr size_t kStorageClassIndex = 3;

    visitTypeHelper(instruction, kIdIndex, kTypeIdIndex);

    // All resources that take set/binding should be transformed.
    const uint32_t id           = instruction[kIdIndex];
    const uint32_t storageClass = instruction[kStorageClassIndex];

    ASSERT((storageClass != spv::StorageClassUniform && storageClass != spv::StorageClassImage &&
            storageClass != spv::StorageClassStorageBuffer) ||
           mVariableInfoById[id] != nullptr);
}

bool SpirvTransformer::transformDecorate(const uint32_t *instruction, size_t wordCount)
{
    // SPIR-V 1.0 Section 3.32 Instructions, OpDecorate
    constexpr size_t kIdIndex              = 1;
    constexpr size_t kDecorationIndex      = 2;
    constexpr size_t kDecorationValueIndex = 3;

    uint32_t id         = instruction[kIdIndex];
    uint32_t decoration = instruction[kDecorationIndex];

    const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];

    // If variable is not a shader interface variable that needs modification, there's nothing to
    // do.
    if (info == nullptr)
    {
        return false;
    }

    // If it's an inactive varying, remove the decoration altogether.
    if (!info->activeStages[mShaderType])
    {
        return true;
    }

    uint32_t newDecorationValue = ShaderInterfaceVariableInfo::kInvalid;

    switch (decoration)
    {
        case spv::DecorationLocation:
            newDecorationValue = info->location[mShaderType];
            break;
        case spv::DecorationBinding:
            newDecorationValue = info->binding;
            break;
        case spv::DecorationDescriptorSet:
            newDecorationValue = info->descriptorSet;
            break;
        default:
            break;
    }

    // If the decoration is not something we care about modifying, there's nothing to do.
    if (newDecorationValue == ShaderInterfaceVariableInfo::kInvalid)
    {
        return false;
    }

    // Copy the decoration declaration and modify it.
    const size_t instructionOffset = copyInstruction(instruction, wordCount);
    (*mSpirvBlobOut)[instructionOffset + kDecorationValueIndex] = newDecorationValue;

    // If there are decorations to be added, add them right after the Location decoration is
    // encountered.
    if (decoration != spv::DecorationLocation)
    {
        return true;
    }

    // Add component decoration, if any.
    if (info->component[mShaderType] != ShaderInterfaceVariableInfo::kInvalid)
    {
        // Copy the location decoration declaration and modify it to contain the Component
        // decoration.
        const size_t instOffset                         = copyInstruction(instruction, wordCount);
        (*mSpirvBlobOut)[instOffset + kDecorationIndex] = spv::DecorationComponent;
        (*mSpirvBlobOut)[instOffset + kDecorationValueIndex] = info->component[mShaderType];
    }

    // Add Xfb decorations, if any.
    if (mShaderType != gl::ShaderType::Fragment &&
        info->xfbBuffer != ShaderInterfaceVariableInfo::kInvalid)
    {
        ASSERT(info->xfbStride != ShaderInterfaceVariableInfo::kInvalid);
        ASSERT(info->xfbOffset != ShaderInterfaceVariableInfo::kInvalid);

        constexpr size_t kXfbDecorationCount                   = 3;
        constexpr uint32_t xfbDecorations[kXfbDecorationCount] = {
            spv::DecorationXfbBuffer,
            spv::DecorationXfbStride,
            spv::DecorationOffset,
        };
        const uint32_t xfbDecorationValues[kXfbDecorationCount] = {
            info->xfbBuffer,
            info->xfbStride,
            info->xfbOffset,
        };

        // Copy the location decoration declaration three times, and modify them to contain the
        // XfbBuffer, XfbStride and Offset decorations.
        for (size_t i = 0; i < kXfbDecorationCount; ++i)
        {
            const size_t xfbInstructionOffset = copyInstruction(instruction, wordCount);
            (*mSpirvBlobOut)[xfbInstructionOffset + kDecorationIndex]      = xfbDecorations[i];
            (*mSpirvBlobOut)[xfbInstructionOffset + kDecorationValueIndex] = xfbDecorationValues[i];
        }
    }

    return true;
}

bool SpirvTransformer::transformCapability(const uint32_t *instruction, size_t wordCount)
{
    if (!mHasTransformFeedbackOutput)
    {
        return false;
    }

    // SPIR-V 1.0 Section 3.32 Instructions, OpCapability
    constexpr size_t kCapabilityIndex = 1;

    uint32_t capability = instruction[kCapabilityIndex];

    // Transform feedback capability shouldn't have already been specified.
    ASSERT(capability != spv::CapabilityTransformFeedback);

    // Vulkan shaders have either Shader, Geometry or Tessellation capability.  We find this
    // capability, and add the TransformFeedback capability after it.
    if (capability != spv::CapabilityShader && capability != spv::CapabilityGeometry &&
        capability != spv::CapabilityTessellation)
    {
        return false;
    }

    // Copy the original capability declaration.
    copyInstruction(instruction, wordCount);

    // Create the TransformFeedback capability declaration.

    // SPIR-V 1.0 Section 3.32 Instructions, OpCapability
    constexpr size_t kCapabilityInstructionLength = 2;

    std::array<uint32_t, kCapabilityInstructionLength> newCapabilityDeclaration = {
        instruction[0],  // length+opcode is identical
    };
    // Fill the fields.
    newCapabilityDeclaration[kCapabilityIndex] = spv::CapabilityTransformFeedback;

    copyInstruction(newCapabilityDeclaration.data(), kCapabilityInstructionLength);

    return true;
}

bool SpirvTransformer::transformEntryPoint(const uint32_t *instruction, size_t wordCount)
{
    // Remove inactive varyings from the shader interface declaration.

    // SPIR-V 1.0 Section 3.32 Instructions, OpEntryPoint
    constexpr size_t kNameIndex = 3;

    // Calculate the length of entry point name in words.  Note that endianness of the string
    // doesn't matter, since we are looking for the '\0' character and rounding up to the word size.
    // This calculates (strlen(name)+1+3) / 4, which is equal to strlen(name)/4+1.
    const size_t nameLength =
        strlen(reinterpret_cast<const char *>(&instruction[kNameIndex])) / 4 + 1;
    const uint32_t instructionLength = GetSpirvInstructionLength(instruction);
    const size_t interfaceStart      = kNameIndex + nameLength;
    const size_t interfaceCount      = instructionLength - interfaceStart;

    // Create a copy of the entry point for modification.
    std::vector<uint32_t> filteredEntryPoint(instruction, instruction + wordCount);

    // Filter out inactive varyings from entry point interface declaration.
    size_t writeIndex = interfaceStart;
    for (size_t index = 0; index < interfaceCount; ++index)
    {
        uint32_t id                             = instruction[interfaceStart + index];
        const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];

        ASSERT(info);

        if (!info->activeStages[mShaderType])
        {
            continue;
        }

        filteredEntryPoint[writeIndex] = id;
        ++writeIndex;
    }

    // Update the length of the instruction.
    const size_t newLength = writeIndex;
    SetSpirvInstructionLength(filteredEntryPoint.data(), newLength);

    // Copy to output.
    copyInstruction(filteredEntryPoint.data(), newLength);

    // Add an OpExecutionMode Xfb instruction if necessary.
    if (!mHasTransformFeedbackOutput)
    {
        return true;
    }

    // SPIR-V 1.0 Section 3.32 Instructions, OpEntryPoint
    constexpr size_t kEntryPointIdIndex = 2;

    // SPIR-V 1.0 Section 3.32 Instructions, OpExecutionMode
    constexpr size_t kExecutionModeInstructionLength  = 3;
    constexpr size_t kExecutionModeIdIndex            = 1;
    constexpr size_t kExecutionModeExecutionModeIndex = 2;

    std::array<uint32_t, kExecutionModeInstructionLength> newExecutionModeDeclaration = {};

    // Fill the fields.
    SetSpirvInstructionOp(newExecutionModeDeclaration.data(), spv::OpExecutionMode);
    SetSpirvInstructionLength(newExecutionModeDeclaration.data(), kExecutionModeInstructionLength);
    newExecutionModeDeclaration[kExecutionModeIdIndex]            = instruction[kEntryPointIdIndex];
    newExecutionModeDeclaration[kExecutionModeExecutionModeIndex] = spv::ExecutionModeXfb;

    copyInstruction(newExecutionModeDeclaration.data(), kExecutionModeInstructionLength);

    return true;
}

bool SpirvTransformer::transformTypePointer(const uint32_t *instruction, size_t wordCount)
{
    // SPIR-V 1.0 Section 3.32 Instructions, OpTypePointer
    constexpr size_t kIdIndex           = 1;
    constexpr size_t kStorageClassIndex = 2;

    const uint32_t id           = instruction[kIdIndex];
    const uint32_t storageClass = instruction[kStorageClassIndex];

    // If the storage class is output, this may be used to create a variable corresponding to an
    // inactive varying, or if that varying is a struct, an Op*AccessChain retrieving a field of
    // that inactive varying.
    //
    // Unfortunately, SPIR-V specifies the storage class both on the type and the variable
    // declaration.  Otherwise it would have been sufficient to modify the OpVariable instruction.
    // For simplicty, copy every "OpTypePointer Output" instruction except with the Private storage
    // class, in case it may be necessary later.

    if (storageClass != spv::StorageClassOutput)
    {
        return false;
    }

    // Cannot create a Private type declaration from the builtin gl_PerVertex.  Note that
    // mVariableInfoById is only ever set for variables, except for nameless interface blocks and
    // the builtin gl_PerVertex.  Since the storage class is Output, if mVariableInfoById for this
    // type is not nullptr, this must be a builtin.
    const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];
    if (info)
    {
        ASSERT(info == &mBuiltinVariableInfo);
        return false;
    }

    // Copy the type declaration for modification.
    const size_t instructionOffset = copyInstruction(instruction, wordCount);

    const uint32_t newTypeId                                 = getNewId();
    (*mSpirvBlobOut)[instructionOffset + kIdIndex]           = newTypeId;
    (*mSpirvBlobOut)[instructionOffset + kStorageClassIndex] = spv::StorageClassPrivate;

    // Remember the id of the replacement.
    ASSERT(id < mTypePointerTransformedId.size());
    mTypePointerTransformedId[id] = newTypeId;

    // The original instruction should still be present as well.  At this point, we don't know
    // whether we will need the Output or Private type.
    return false;
}

bool SpirvTransformer::transformVariable(const uint32_t *instruction, size_t wordCount)
{
    // SPIR-V 1.0 Section 3.32 Instructions, OpVariable
    constexpr size_t kTypeIdIndex       = 1;
    constexpr size_t kIdIndex           = 2;
    constexpr size_t kStorageClassIndex = 3;

    const uint32_t id           = instruction[kIdIndex];
    const uint32_t typeId       = instruction[kTypeIdIndex];
    const uint32_t storageClass = instruction[kStorageClassIndex];

    const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];

    // If variable is not a shader interface variable that needs modification, there's nothing to
    // do.
    if (info == nullptr)
    {
        return false;
    }

    // Furthermore, if it's not an inactive varying output, there's nothing to do.  Note that
    // inactive varying inputs are already pruned by the translator.
    ASSERT(storageClass != spv::StorageClassInput || info->activeStages[mShaderType]);
    if (info->activeStages[mShaderType])
    {
        return false;
    }

    ASSERT(storageClass == spv::StorageClassOutput);

    // Copy the variable declaration for modification.  Change its type to the corresponding type
    // with the Private storage class, as well as changing the storage class respecified in this
    // instruction.
    const size_t instructionOffset = copyInstruction(instruction, wordCount);

    ASSERT(typeId < mTypePointerTransformedId.size());
    ASSERT(mTypePointerTransformedId[typeId] != 0);

    (*mSpirvBlobOut)[instructionOffset + kTypeIdIndex]       = mTypePointerTransformedId[typeId];
    (*mSpirvBlobOut)[instructionOffset + kStorageClassIndex] = spv::StorageClassPrivate;

    return true;
}

bool SpirvTransformer::transformAccessChain(const uint32_t *instruction, size_t wordCount)
{
    // SPIR-V 1.0 Section 3.32 Instructions, OpAccessChain, OpInBoundsAccessChain, OpPtrAccessChain,
    // OpInBoundsPtrAccessChain
    constexpr size_t kTypeIdIndex = 1;
    constexpr size_t kBaseIdIndex = 3;

    const uint32_t typeId = instruction[kTypeIdIndex];
    const uint32_t baseId = instruction[kBaseIdIndex];

    // If not accessing an inactive output varying, nothing to do.
    const ShaderInterfaceVariableInfo *info = mVariableInfoById[baseId];
    if (info == nullptr || info->activeStages[mShaderType])
    {
        return false;
    }

    // Copy the instruction for modification.
    const size_t instructionOffset = copyInstruction(instruction, wordCount);

    ASSERT(typeId < mTypePointerTransformedId.size());
    ASSERT(mTypePointerTransformedId[typeId] != 0);

    (*mSpirvBlobOut)[instructionOffset + kTypeIdIndex] = mTypePointerTransformedId[typeId];

    return true;
}

size_t SpirvTransformer::copyInstruction(const uint32_t *instruction, size_t wordCount)
{
    size_t instructionOffset = mSpirvBlobOut->size();
    mSpirvBlobOut->insert(mSpirvBlobOut->end(), instruction, instruction + wordCount);
    return instructionOffset;
}

uint32_t SpirvTransformer::getNewId()
{
    return (*mSpirvBlobOut)[kHeaderIndexIndexBound]++;
}
}  // anonymous namespace

const uint32_t ShaderInterfaceVariableInfo::kInvalid;

ShaderInterfaceVariableInfo::ShaderInterfaceVariableInfo()
{
    location.fill(kInvalid);
    component.fill(kInvalid);
}

void GlslangInitialize()
{
    int result = ShInitialize();
    ASSERT(result != 0);
}

void GlslangRelease()
{
    int result = ShFinalize();
    ASSERT(result != 0);
}

std::string GlslangGetMappedSamplerName(const std::string &originalName)
{
    std::string samplerName = originalName;

    // Samplers in structs are extracted.
    std::replace(samplerName.begin(), samplerName.end(), '.', '_');

    // Remove array elements
    auto out = samplerName.begin();
    for (auto in = samplerName.begin(); in != samplerName.end(); in++)
    {
        if (*in == '[')
        {
            while (*in != ']')
            {
                in++;
                ASSERT(in != samplerName.end());
            }
        }
        else
        {
            *out++ = *in;
        }
    }

    samplerName.erase(out, samplerName.end());

    if (MappedSamplerNameNeedsUserDefinedPrefix(originalName))
    {
        samplerName = sh::kUserDefinedNamePrefix + samplerName;
    }

    return samplerName;
}

void GlslangGetShaderSource(const GlslangSourceOptions &options,
                            const gl::ProgramState &programState,
                            const gl::ProgramLinkedResources &resources,
                            gl::ShaderMap<std::string> *shaderSourcesOut,
                            ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    variableInfoMapOut->clear();

    uint32_t locationsUsedForXfbExtension = 0;

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        gl::Shader *glShader            = programState.getAttachedShader(shaderType);
        (*shaderSourcesOut)[shaderType] = glShader ? glShader->getTranslatedSource() : "";
    }

    std::string *vertexSource         = &(*shaderSourcesOut)[gl::ShaderType::Vertex];
    const std::string &fragmentSource = (*shaderSourcesOut)[gl::ShaderType::Fragment];
    const std::string &computeSource  = (*shaderSourcesOut)[gl::ShaderType::Compute];

    // Write transform feedback output code.
    if (!vertexSource->empty())
    {
        if (programState.getLinkedTransformFeedbackVaryings().empty())
        {
            *vertexSource = SubstituteTransformFeedbackMarkers(*vertexSource, "", "");
        }
        else
        {
            if (options.supportsTransformFeedbackExtension)
            {
                GenerateTransformFeedbackExtensionOutputs(programState, resources, vertexSource,
                                                          variableInfoMapOut,
                                                          &locationsUsedForXfbExtension);
            }
            else if (options.emulateTransformFeedback)
            {
                GenerateTransformFeedbackEmulationOutputs(options, programState, vertexSource,
                                                          variableInfoMapOut);
            }
        }
    }

    // Assign outputs to the fragment shader, if any.
    if (!fragmentSource.empty())
    {
        AssignOutputLocations(programState, variableInfoMapOut);
    }

    // Assign attributes to the vertex shader, if any.
    if (!vertexSource->empty())
    {
        AssignAttributeLocations(programState, variableInfoMapOut);
    }

    if (computeSource.empty())
    {
        // Assign varying locations.
        AssignVaryingLocations(options, programState, resources, locationsUsedForXfbExtension,
                               variableInfoMapOut);

        if (!programState.getLinkedTransformFeedbackVaryings().empty() &&
            options.supportsTransformFeedbackExtension)
        {
            AssignTransformFeedbackExtensionQualifiers(
                programState, resources, locationsUsedForXfbExtension, variableInfoMapOut);
        }
    }

    AssignUniformBindings(options, shaderSourcesOut, variableInfoMapOut);
    AssignTextureBindings(options, programState, variableInfoMapOut);
    AssignNonTextureBindings(options, programState, variableInfoMapOut);
}

angle::Result GlslangGetShaderSpirvCode(GlslangErrorCallback callback,
                                        const gl::Caps &glCaps,
                                        const gl::ShaderMap<std::string> &shaderSources,
                                        const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                        gl::ShaderMap<SpirvBlob> *spirvBlobsOut)
{
    gl::ShaderMap<std::vector<uint32_t>> initialSpirvBlobs;
    ANGLE_TRY(GetShaderSpirvCode(callback, glCaps, shaderSources, &initialSpirvBlobs));

    // Transform the SPIR-V code by assigning location/set/binding values.
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const std::vector<uint32_t> initialSpirvBlob = initialSpirvBlobs[shaderType];

        if (initialSpirvBlob.empty())
        {
            continue;
        }

        SpirvBlob *spirvBlob = &(*spirvBlobsOut)[shaderType];

        SpirvTransformer transformer(initialSpirvBlob, variableInfoMap, shaderType, spirvBlob);
        ANGLE_GLSLANG_CHECK(callback, transformer.transform(), GlslangError::InvalidSpirv);

        ASSERT(ValidateSpirv(*spirvBlob));
    }

    return angle::Result::Continue;
}
}  // namespace rx

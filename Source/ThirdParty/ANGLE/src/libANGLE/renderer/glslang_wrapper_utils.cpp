//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Wrapper for Khronos glslang compiler.
//

#include "libANGLE/renderer/glslang_wrapper_utils.h"

#include <array>
#include <cctype>
#include <numeric>

#include "common/FixedVector.h"
#include "common/spirv/spirv_instruction_builder_autogen.h"
#include "common/spirv/spirv_instruction_parser_autogen.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/renderer/ShaderInterfaceVariableInfoMap.h"
#include "libANGLE/trace.h"

namespace spirv = angle::spirv;

namespace rx
{
namespace
{
template <size_t N>
constexpr size_t ConstStrLen(const char (&)[N])
{
    static_assert(N > 0, "C++ shouldn't allow N to be zero");

    // The length of a string defined as a char array is the size of the array minus 1 (the
    // terminating '\0').
    return N - 1;
}

// Test if there are non-zero indices in the uniform name, returning false in that case.  This
// happens for multi-dimensional arrays, where a uniform is created for every possible index of the
// array (except for the innermost dimension).  When assigning decorations (set/binding/etc), only
// the indices corresponding to the first element of the array should be specified.  This function
// is used to skip the other indices.
bool UniformNameIsIndexZero(const std::string &name)
{
    size_t lastBracketClose = 0;

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

bool MappedSamplerNameNeedsUserDefinedPrefix(const std::string &originalName)
{
    return originalName.find('.') == std::string::npos;
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

ShaderInterfaceVariableInfo *AddResourceInfoToAllStages(ShaderInterfaceVariableInfoMap *infoMap,
                                                        gl::ShaderType shaderType,
                                                        ShaderVariableType variableType,
                                                        const std::string &varName,
                                                        uint32_t descriptorSet,
                                                        uint32_t binding)
{
    gl::ShaderBitSet allStages;
    allStages.set();

    ShaderInterfaceVariableInfo &info = infoMap->add(shaderType, variableType, varName);
    info.descriptorSet                = descriptorSet;
    info.binding                      = binding;
    info.activeStages                 = allStages;
    return &info;
}

ShaderInterfaceVariableInfo *AddResourceInfo(ShaderInterfaceVariableInfoMap *infoMap,
                                             gl::ShaderBitSet stages,
                                             gl::ShaderType shaderType,
                                             ShaderVariableType variableType,
                                             const std::string &varName,
                                             uint32_t descriptorSet,
                                             uint32_t binding)
{
    ShaderInterfaceVariableInfo &info = infoMap->add(shaderType, variableType, varName);
    info.descriptorSet                = descriptorSet;
    info.binding                      = binding;
    info.activeStages                 = stages;
    return &info;
}

// Add location information for an in/out variable.
ShaderInterfaceVariableInfo *AddLocationInfo(ShaderInterfaceVariableInfoMap *infoMap,
                                             gl::ShaderType shaderType,
                                             ShaderVariableType variableType,
                                             const std::string &varName,
                                             uint32_t location,
                                             uint32_t component,
                                             uint8_t attributeComponentCount,
                                             uint8_t attributeLocationCount)
{
    // The info map for this name may or may not exist already.  This function merges the
    // location/component information.
    ShaderInterfaceVariableInfo &info = infoMap->addOrGet(shaderType, variableType, varName);

    ASSERT(info.descriptorSet == ShaderInterfaceVariableInfo::kInvalid);
    ASSERT(info.binding == ShaderInterfaceVariableInfo::kInvalid);
    if (info.location != ShaderInterfaceVariableInfo::kInvalid)
    {
        // TODO: Correctly support in and out interface variables with identical name.
        // anglebug.com/7220
        ASSERT(info.location == location);
        ASSERT(info.component == component);
    }
    ASSERT(info.component == ShaderInterfaceVariableInfo::kInvalid);

    info.location  = location;
    info.component = component;
    info.activeStages.set(shaderType);
    info.attributeComponentCount = attributeComponentCount;
    info.attributeLocationCount  = attributeLocationCount;

    return &info;
}

// Add location information for an in/out variable
void AddVaryingLocationInfo(ShaderInterfaceVariableInfoMap *infoMap,
                            const gl::VaryingInShaderRef &ref,
                            const bool isStructField,
                            const uint32_t location,
                            const uint32_t component)
{
    const std::string &name = isStructField ? ref.parentStructMappedName : ref.varying->mappedName;
    AddLocationInfo(infoMap, ref.stage, ShaderVariableType::Varying, name, location, component, 0,
                    0);
}

// Modify an existing out variable and add transform feedback information.
ShaderInterfaceVariableInfo *SetXfbInfo(ShaderInterfaceVariableInfoMap *infoMap,
                                        gl::ShaderType shaderType,
                                        const std::string &varName,
                                        int fieldIndex,
                                        uint32_t xfbBuffer,
                                        uint32_t xfbOffset,
                                        uint32_t xfbStride,
                                        uint32_t arraySize,
                                        uint32_t columnCount,
                                        uint32_t rowCount,
                                        uint32_t arrayIndex,
                                        GLenum componentType)
{
    ShaderInterfaceVariableInfo &info =
        infoMap->getMutable(shaderType, ShaderVariableType::Varying, varName);
    ShaderInterfaceVariableXfbInfo *xfb = &info.xfb;

    if (fieldIndex >= 0)
    {
        if (info.fieldXfb.size() <= static_cast<size_t>(fieldIndex))
        {
            info.fieldXfb.resize(fieldIndex + 1);
        }
        xfb = &info.fieldXfb[fieldIndex];
    }

    ASSERT(xfb->buffer == ShaderInterfaceVariableXfbInfo::kInvalid);
    ASSERT(xfb->offset == ShaderInterfaceVariableXfbInfo::kInvalid);
    ASSERT(xfb->stride == ShaderInterfaceVariableXfbInfo::kInvalid);

    if (arrayIndex != ShaderInterfaceVariableXfbInfo::kInvalid)
    {
        xfb->arrayElements.emplace_back();
        xfb = &xfb->arrayElements.back();
    }

    xfb->buffer        = xfbBuffer;
    xfb->offset        = xfbOffset;
    xfb->stride        = xfbStride;
    xfb->arraySize     = arraySize;
    xfb->columnCount   = columnCount;
    xfb->rowCount      = rowCount;
    xfb->arrayIndex    = arrayIndex;
    xfb->componentType = componentType;

    return &info;
}

void AssignTransformFeedbackEmulationBindings(gl::ShaderType shaderType,
                                              const gl::ProgramExecutable &programExecutable,
                                              bool isTransformFeedbackStage,
                                              GlslangProgramInterfaceInfo *programInterfaceInfo,
                                              ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    size_t bufferCount = 0;
    if (isTransformFeedbackStage)
    {
        ASSERT(!programExecutable.getLinkedTransformFeedbackVaryings().empty());
        const bool isInterleaved =
            programExecutable.getTransformFeedbackBufferMode() == GL_INTERLEAVED_ATTRIBS;
        bufferCount =
            isInterleaved ? 1 : programExecutable.getLinkedTransformFeedbackVaryings().size();
    }

    // Add entries for the transform feedback buffers to the info map, so they can have correct
    // set/binding.
    for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        AddResourceInfo(variableInfoMapOut, gl::ShaderBitSet().set(shaderType), shaderType,
                        ShaderVariableType::TransformFeedback, GetXfbBufferName(bufferIndex),
                        programInterfaceInfo->uniformsAndXfbDescriptorSetIndex,
                        programInterfaceInfo->currentUniformBindingIndex);
        ++programInterfaceInfo->currentUniformBindingIndex;
    }

    // Remove inactive transform feedback buffers.
    for (uint32_t bufferIndex = static_cast<uint32_t>(bufferCount);
         bufferIndex < gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; ++bufferIndex)
    {
        variableInfoMapOut->add(shaderType, ShaderVariableType::TransformFeedback,
                                GetXfbBufferName(bufferIndex));
    }
}

bool IsFirstRegisterOfVarying(const gl::PackedVaryingRegister &varyingReg,
                              bool allowFields,
                              uint32_t expectArrayIndex)
{
    const gl::PackedVarying &varying = *varyingReg.packedVarying;

    // In Vulkan GLSL, struct fields are not allowed to have location assignments.  The varying of a
    // struct type is thus given a location equal to the one assigned to its first field.  With I/O
    // blocks, transform feedback can capture an arbitrary field.  In that case, we need to look at
    // every field, not just the first one.
    if (!allowFields && varying.isStructField() &&
        (varying.fieldIndex > 0 || varying.secondaryFieldIndex > 0))
    {
        return false;
    }

    // Similarly, assign array varying locations to the assigned location of the first element.
    // Transform feedback may capture array elements, so if a specific non-zero element is
    // requested, accept that only.
    if (varyingReg.varyingArrayIndex != expectArrayIndex ||
        (varying.arrayIndex != GL_INVALID_INDEX && varying.arrayIndex != expectArrayIndex))
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

void AssignAttributeLocations(const gl::ProgramExecutable &programExecutable,
                              gl::ShaderType shaderType,
                              ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    // Assign attribute locations for the vertex shader.
    for (const sh::ShaderVariable &attribute : programExecutable.getProgramInputs())
    {
        ASSERT(attribute.active);

        const uint8_t colCount = static_cast<uint8_t>(gl::VariableColumnCount(attribute.type));
        const uint8_t rowCount = static_cast<uint8_t>(gl::VariableRowCount(attribute.type));
        const bool isMatrix    = colCount > 1 && rowCount > 1;

        const uint8_t componentCount = isMatrix ? rowCount : colCount;
        const uint8_t locationCount  = isMatrix ? colCount : rowCount;

        AddLocationInfo(variableInfoMapOut, shaderType, ShaderVariableType::Attribute,
                        attribute.mappedName, attribute.location,
                        ShaderInterfaceVariableInfo::kInvalid, componentCount, locationCount);
    }
}

void AssignSecondaryOutputLocations(const gl::ProgramExecutable &programExecutable,
                                    ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    const auto &secondaryOutputLocations = programExecutable.getSecondaryOutputLocations();
    const auto &outputVariables          = programExecutable.getOutputVariables();

    // Handle EXT_blend_func_extended secondary outputs (ones with index=1)
    for (const gl::VariableLocation &outputLocation : secondaryOutputLocations)
    {
        if (outputLocation.arrayIndex == 0 && outputLocation.used() && !outputLocation.ignored)
        {
            const sh::ShaderVariable &outputVar = outputVariables[outputLocation.index];

            uint32_t location = 0;
            if (outputVar.location != -1)
            {
                location = outputVar.location;
            }

            ShaderInterfaceVariableInfo *info = AddLocationInfo(
                variableInfoMapOut, gl::ShaderType::Fragment, ShaderVariableType::SecondaryOutput,
                outputVar.mappedName, location, ShaderInterfaceVariableInfo::kInvalid, 0, 0);

            // If the shader source has not specified the index, specify it here.
            if (outputVar.index == -1)
            {
                // Index 1 is used to specify that the color be used as the second color input to
                // the blend equation
                info->index = 1;
            }
        }
    }
    // Handle secondary outputs for ESSL version less than 3.00
    if (programExecutable.hasLinkedShaderStage(gl::ShaderType::Fragment) &&
        programExecutable.getLinkedShaderVersion(gl::ShaderType::Fragment) == 100)
    {
        const std::vector<sh::ShaderVariable> &shaderOutputs =
            programExecutable.getOutputVariables();
        for (const sh::ShaderVariable &outputVar : shaderOutputs)
        {
            if (outputVar.name == "gl_SecondaryFragColorEXT")
            {
                AddLocationInfo(variableInfoMapOut, gl::ShaderType::Fragment,
                                ShaderVariableType::SecondaryOutput, "webgl_SecondaryFragColor", 0,
                                ShaderInterfaceVariableInfo::kInvalid, 0, 0);
            }
            else if (outputVar.name == "gl_SecondaryFragDataEXT")
            {
                AddLocationInfo(variableInfoMapOut, gl::ShaderType::Fragment,
                                ShaderVariableType::SecondaryOutput, "webgl_SecondaryFragData", 0,
                                ShaderInterfaceVariableInfo::kInvalid, 0, 0);
            }
        }
    }
}

void AssignOutputLocations(const gl::ProgramExecutable &programExecutable,
                           const gl::ShaderType shaderType,
                           ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    // Assign output locations for the fragment shader.
    ASSERT(shaderType == gl::ShaderType::Fragment);

    const auto &outputLocations                      = programExecutable.getOutputLocations();
    const auto &outputVariables                      = programExecutable.getOutputVariables();
    const std::array<std::string, 3> implicitOutputs = {"gl_FragDepth", "gl_SampleMask",
                                                        "gl_FragStencilRefARB"};

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

            AddLocationInfo(variableInfoMapOut, shaderType, ShaderVariableType::Output,
                            outputVar.mappedName, location, ShaderInterfaceVariableInfo::kInvalid,
                            0, 0);
        }
    }

    AssignSecondaryOutputLocations(programExecutable, variableInfoMapOut);

    // When no fragment output is specified by the shader, the translator outputs webgl_FragColor or
    // webgl_FragData.  Add an entry for these.  Even though the translator is already assigning
    // location 0 to these entries, adding an entry for them here allows us to ASSERT that every
    // shader interface variable is processed during the SPIR-V transformation.  This is done when
    // iterating the ids provided by OpEntryPoint.
    AddLocationInfo(variableInfoMapOut, shaderType, ShaderVariableType::Output, "webgl_FragColor",
                    0, 0, 0, 0);
    AddLocationInfo(variableInfoMapOut, shaderType, ShaderVariableType::Output, "webgl_FragData", 0,
                    0, 0, 0);
}

void AssignVaryingLocations(const GlslangSourceOptions &options,
                            const gl::VaryingPacking &varyingPacking,
                            const gl::ShaderType shaderType,
                            const gl::ShaderType frontShaderType,
                            GlslangProgramInterfaceInfo *programInterfaceInfo,
                            ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    uint32_t locationsUsedForEmulation = programInterfaceInfo->locationsUsedForXfbExtension;

    // Assign varying locations.
    for (const gl::PackedVaryingRegister &varyingReg : varyingPacking.getRegisterList())
    {
        if (!IsFirstRegisterOfVarying(varyingReg, false, 0))
        {
            continue;
        }

        const gl::PackedVarying &varying = *varyingReg.packedVarying;

        uint32_t location  = varyingReg.registerRow + locationsUsedForEmulation;
        uint32_t component = ShaderInterfaceVariableInfo::kInvalid;
        if (varyingReg.registerColumn > 0)
        {
            ASSERT(!varying.varying().isStruct());
            ASSERT(!gl::IsMatrixType(varying.varying().type));
            component = varyingReg.registerColumn;
        }

        // In the following:
        //
        //     struct S { vec4 field; };
        //     out S varStruct;
        //
        // "_uvarStruct" is found through |parentStructMappedName|, with |varying->mappedName|
        // being "_ufield".  In such a case, use |parentStructMappedName|.
        if (varying.frontVarying.varying && (varying.frontVarying.stage == shaderType))
        {
            AddVaryingLocationInfo(variableInfoMapOut, varying.frontVarying,
                                   varying.isStructField(), location, component);
        }

        if (varying.backVarying.varying && (varying.backVarying.stage == shaderType))
        {
            AddVaryingLocationInfo(variableInfoMapOut, varying.backVarying, varying.isStructField(),
                                   location, component);
        }
    }

    // Add an entry for inactive varyings.
    const gl::ShaderMap<std::vector<std::string>> &inactiveVaryingMappedNames =
        varyingPacking.getInactiveVaryingMappedNames();
    for (const std::string &varyingName : inactiveVaryingMappedNames[shaderType])
    {
        ASSERT(!gl::IsBuiltInName(varyingName));

        // If name is already in the map, it will automatically have marked all other stages
        // inactive.
        if (variableInfoMapOut->hasVariable(shaderType, varyingName))
        {
            continue;
        }

        // Otherwise, add an entry for it with all locations inactive.
        ShaderInterfaceVariableInfo &info =
            variableInfoMapOut->addOrGet(shaderType, ShaderVariableType::Varying, varyingName);
        ASSERT(info.location == ShaderInterfaceVariableInfo::kInvalid);
    }

    // Add an entry for active builtins varyings.  This will allow inactive builtins, such as
    // gl_PointSize, gl_ClipDistance etc to be removed.
    const gl::ShaderMap<std::vector<std::string>> &activeOutputBuiltIns =
        varyingPacking.getActiveOutputBuiltInNames();
    for (const std::string &builtInName : activeOutputBuiltIns[shaderType])
    {
        ASSERT(gl::IsBuiltInName(builtInName));

        ShaderInterfaceVariableInfo &info =
            variableInfoMapOut->addOrGet(shaderType, ShaderVariableType::Varying, builtInName);
        info.activeStages.set(shaderType);
        info.builtinIsOutput = true;
    }

    // If an output builtin is active in the previous stage, assume it's active in the input of the
    // current stage as well.
    if (frontShaderType != gl::ShaderType::InvalidEnum)
    {
        for (const std::string &builtInName : activeOutputBuiltIns[frontShaderType])
        {
            ASSERT(gl::IsBuiltInName(builtInName));

            ShaderInterfaceVariableInfo &info =
                variableInfoMapOut->addOrGet(shaderType, ShaderVariableType::Varying, builtInName);
            info.activeStages.set(shaderType);
            info.builtinIsInput = true;
        }
    }

    // Add an entry for gl_PerVertex, for use with transform feedback capture of built-ins.
    ShaderInterfaceVariableInfo &info =
        variableInfoMapOut->addOrGet(shaderType, ShaderVariableType::Varying, "gl_PerVertex");
    info.activeStages.set(shaderType);
}

// Calculates XFB layout qualifier arguments for each transform feedback varying. Stores calculated
// values for the SPIR-V transformation.
void AssignTransformFeedbackQualifiers(const gl::ProgramExecutable &programExecutable,
                                       const gl::VaryingPacking &varyingPacking,
                                       const gl::ShaderType shaderType,
                                       bool usesExtension,
                                       ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    const std::vector<gl::TransformFeedbackVarying> &tfVaryings =
        programExecutable.getLinkedTransformFeedbackVaryings();
    const std::vector<GLsizei> &varyingStrides = programExecutable.getTransformFeedbackStrides();
    const bool isInterleaved =
        programExecutable.getTransformFeedbackBufferMode() == GL_INTERLEAVED_ATTRIBS;

    uint32_t currentOffset = 0;
    uint32_t currentStride = 0;
    uint32_t bufferIndex   = 0;

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
        const gl::UniformTypeInfo &uniformInfo        = gl::GetUniformTypeInfo(tfVarying.type);
        const uint32_t varyingSize =
            tfVarying.isArray() ? tfVarying.size() : ShaderInterfaceVariableXfbInfo::kInvalid;

        if (tfVarying.isBuiltIn())
        {
            if (usesExtension && tfVarying.name == "gl_Position")
            {
                // With the extension, gl_Position is captured via a special varying.
                SetXfbInfo(variableInfoMapOut, shaderType, sh::vk::kXfbExtensionPositionOutName, -1,
                           bufferIndex, currentOffset, currentStride, varyingSize,
                           uniformInfo.columnCount, uniformInfo.rowCount,
                           ShaderInterfaceVariableXfbInfo::kInvalid, uniformInfo.componentType);
            }
            else
            {
                // gl_PerVertex is always defined as:
                //
                //    Field 0: gl_Position
                //    Field 1: gl_PointSize
                //    Field 2: gl_ClipDistance
                //    Field 3: gl_CullDistance
                //
                // With the extension, all fields except gl_Position can be captured directly by
                // decorating gl_PerVertex fields.
                int fieldIndex                                                              = -1;
                constexpr int kPerVertexMemberCount                                         = 4;
                constexpr std::array<const char *, kPerVertexMemberCount> kPerVertexMembers = {
                    "gl_Position",
                    "gl_PointSize",
                    "gl_ClipDistance",
                    "gl_CullDistance",
                };
                for (int index = 0; index < kPerVertexMemberCount; ++index)
                {
                    if (tfVarying.name == kPerVertexMembers[index])
                    {
                        fieldIndex = index;
                        break;
                    }
                }
                ASSERT(fieldIndex != -1);
                ASSERT(!usesExtension || fieldIndex > 0);

                SetXfbInfo(variableInfoMapOut, shaderType, "gl_PerVertex", fieldIndex, bufferIndex,
                           currentOffset, currentStride, varyingSize, uniformInfo.columnCount,
                           uniformInfo.rowCount, ShaderInterfaceVariableXfbInfo::kInvalid,
                           uniformInfo.componentType);
            }

            continue;
        }
        // Note: capturing individual array elements using the Vulkan transform feedback extension
        // is currently not supported due to limitations in the extension.
        // ANGLE supports capturing the whole array.
        // http://anglebug.com/4140
        if (usesExtension && tfVarying.isArray() && tfVarying.arrayIndex != GL_INVALID_INDEX)
        {
            continue;
        }

        // Find the varying with this name.  If a struct is captured, we would be iterating over its
        // fields, and the name of the varying is found through parentStructMappedName.  This should
        // only be done for the first field of the struct.  For I/O blocks on the other hand, we
        // need to decorate the exact member that is captured (as whole-block capture is not
        // supported).
        const gl::PackedVarying *originalVarying = nullptr;
        for (const gl::PackedVaryingRegister &varyingReg : varyingPacking.getRegisterList())
        {
            const uint32_t arrayIndex =
                tfVarying.arrayIndex == GL_INVALID_INDEX ? 0 : tfVarying.arrayIndex;
            if (!IsFirstRegisterOfVarying(varyingReg, tfVarying.isShaderIOBlock, arrayIndex))
            {
                continue;
            }

            const gl::PackedVarying *varying = varyingReg.packedVarying;

            if (tfVarying.isShaderIOBlock)
            {
                if (varying->frontVarying.parentStructName == tfVarying.structOrBlockName)
                {
                    size_t pos = tfVarying.name.find_first_of(".");
                    std::string fieldName =
                        pos == std::string::npos ? tfVarying.name : tfVarying.name.substr(pos + 1);

                    if (fieldName == varying->frontVarying.varying->name.c_str())
                    {
                        originalVarying = varying;
                        break;
                    }
                }
            }
            else if (varying->frontVarying.varying->name == tfVarying.name)
            {
                originalVarying = varying;
                break;
            }
        }

        if (originalVarying)
        {
            const std::string &mappedName =
                originalVarying->isStructField()
                    ? originalVarying->frontVarying.parentStructMappedName
                    : originalVarying->frontVarying.varying->mappedName;

            const int fieldIndex = tfVarying.isShaderIOBlock ? originalVarying->fieldIndex : -1;
            const uint32_t arrayIndex = tfVarying.arrayIndex == GL_INVALID_INDEX
                                            ? ShaderInterfaceVariableXfbInfo::kInvalid
                                            : tfVarying.arrayIndex;

            // Set xfb info for this varying.  AssignVaryingLocations should have already added
            // location information for these varyings.
            SetXfbInfo(variableInfoMapOut, shaderType, mappedName, fieldIndex, bufferIndex,
                       currentOffset, currentStride, varyingSize, uniformInfo.columnCount,
                       uniformInfo.rowCount, arrayIndex, uniformInfo.componentType);
        }
    }
}

void AssignUniformBindings(const GlslangSourceOptions &options,
                           const gl::ProgramExecutable &programExecutable,
                           const gl::ShaderType shaderType,
                           GlslangProgramInterfaceInfo *programInterfaceInfo,
                           ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    if (programExecutable.hasLinkedShaderStage(shaderType))
    {
        AddResourceInfo(variableInfoMapOut, gl::ShaderBitSet().set(shaderType), shaderType,
                        ShaderVariableType::DefaultUniform, kDefaultUniformNames[shaderType],
                        programInterfaceInfo->uniformsAndXfbDescriptorSetIndex,
                        programInterfaceInfo->currentUniformBindingIndex);
        ++programInterfaceInfo->currentUniformBindingIndex;

        // Assign binding to the driver uniforms block
        AddResourceInfoToAllStages(variableInfoMapOut, shaderType,
                                   ShaderVariableType::DriverUniform,
                                   sh::vk::kDriverUniformsBlockName,
                                   programInterfaceInfo->driverUniformsDescriptorSetIndex, 0);
    }
}

bool InsertIfAbsent(UniformBindingIndexMap *uniformBindingIndexMapOut,
                    const std::string &name,
                    const uint32_t bindingIndex,
                    const gl::ShaderType shaderType)
{
    if (uniformBindingIndexMapOut->count(name) == 0)
    {
        (*uniformBindingIndexMapOut)[name] =
            UniformBindingInfo(bindingIndex, gl::ShaderBitSet(), shaderType);
        return true;
    }
    return false;
}

void AddAndUpdateResourceMaps(const gl::ShaderType shaderType,
                              ShaderVariableType variableType,
                              std::string name,
                              uint32_t *binding,
                              bool updateBinding,
                              bool updateFrontShaderType,
                              const uint32_t descriptorSetIndex,
                              UniformBindingIndexMap *uniformBindingIndexMapOut,
                              ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    ASSERT(binding);
    bool isUniqueName = InsertIfAbsent(uniformBindingIndexMapOut, name, *binding, shaderType);
    if (updateBinding && isUniqueName)
    {
        ++(*binding);
    }
    UniformBindingInfo &uniformBindingInfo = (*uniformBindingIndexMapOut)[name];
    uniformBindingInfo.shaderBitSet.set(shaderType);
    AddResourceInfo(variableInfoMapOut, uniformBindingInfo.shaderBitSet, shaderType, variableType,
                    name, descriptorSetIndex, uniformBindingInfo.bindingIndex);
    if (!isUniqueName)
    {
        if (updateFrontShaderType)
        {
            uniformBindingInfo.frontShaderType = shaderType;
        }
        else
        {
            variableInfoMapOut->markAsDuplicate(shaderType, variableType, name);
        }
    }

    variableInfoMapOut->setActiveStages(uniformBindingInfo.frontShaderType, variableType, name,
                                        uniformBindingInfo.shaderBitSet);
}

void AssignInputAttachmentBindings(const GlslangSourceOptions &options,
                                   const gl::ProgramExecutable &programExecutable,
                                   const std::vector<gl::LinkedUniform> &uniforms,
                                   const gl::RangeUI &inputAttachmentUniformRange,
                                   const gl::ShaderType shaderType,
                                   GlslangProgramInterfaceInfo *programInterfaceInfo,
                                   UniformBindingIndexMap *uniformBindingIndexMapOut,
                                   ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    const uint32_t baseInputAttachmentBindingIndex =
        programInterfaceInfo->currentShaderResourceBindingIndex;

    bool hasFragmentInOutVars = false;

    for (unsigned int uniformIndex : inputAttachmentUniformRange)
    {
        std::string mappedInputAttachmentName;
        const gl::LinkedUniform &inputAttachmentUniform = uniforms[uniformIndex];
        mappedInputAttachmentName                       = inputAttachmentUniform.mappedName;

        if (programExecutable.hasLinkedShaderStage(shaderType) &&
            inputAttachmentUniform.isActive(shaderType))
        {
            uint32_t inputAttachmentBindingIndex =
                baseInputAttachmentBindingIndex + inputAttachmentUniform.location;
            AddAndUpdateResourceMaps(shaderType, ShaderVariableType::FramebufferFetch,
                                     mappedInputAttachmentName, &(inputAttachmentBindingIndex),
                                     false, false,
                                     programInterfaceInfo->shaderResourceDescriptorSetIndex,
                                     uniformBindingIndexMapOut, variableInfoMapOut);
            hasFragmentInOutVars = true;
        }
    }

    if (hasFragmentInOutVars)
    {
        // For input attachment uniform, the descriptor set binding indices are allocated as much as
        // the maximum draw buffers.
        programInterfaceInfo->currentShaderResourceBindingIndex +=
            gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
    }
}

void AssignInterfaceBlockBindings(const GlslangSourceOptions &options,
                                  const gl::ProgramExecutable &programExecutable,
                                  const std::vector<gl::InterfaceBlock> &blocks,
                                  const gl::ShaderType shaderType,
                                  ShaderVariableType variableType,
                                  GlslangProgramInterfaceInfo *programInterfaceInfo,
                                  UniformBindingIndexMap *uniformBindingIndexMapOut,
                                  ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    for (uint32_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex)
    {
        const gl::InterfaceBlock &block = blocks[blockIndex];
        // TODO: http://anglebug.com/4523: All blocks should be active
        if (programExecutable.hasLinkedShaderStage(shaderType) && block.isActive(shaderType))
        {
            if (!block.isArray || block.arrayElement == 0)
            {
                AddAndUpdateResourceMaps(shaderType, variableType, block.mappedName,
                                         &(programInterfaceInfo->currentShaderResourceBindingIndex),
                                         true, false,
                                         programInterfaceInfo->shaderResourceDescriptorSetIndex,
                                         uniformBindingIndexMapOut, variableInfoMapOut);
            }
            variableInfoMapOut->mapIndexedResourceByName(shaderType, variableType, blockIndex,
                                                         block.mappedName);
        }
    }
}

void AssignAtomicCounterBufferBindings(const GlslangSourceOptions &options,
                                       const gl::ProgramExecutable &programExecutable,
                                       const std::vector<gl::AtomicCounterBuffer> &buffers,
                                       const gl::ShaderType shaderType,
                                       GlslangProgramInterfaceInfo *programInterfaceInfo,
                                       UniformBindingIndexMap *uniformBindingIndexMapOut,
                                       ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    if (buffers.size() == 0)
    {
        return;
    }

    if (programExecutable.hasLinkedShaderStage(shaderType))
    {
        AddAndUpdateResourceMaps(shaderType, ShaderVariableType::AtomicCounter,
                                 sh::vk::kAtomicCountersBlockName,
                                 &(programInterfaceInfo->currentShaderResourceBindingIndex), true,
                                 false, programInterfaceInfo->shaderResourceDescriptorSetIndex,
                                 uniformBindingIndexMapOut, variableInfoMapOut);
    }
}

void AssignImageBindings(const GlslangSourceOptions &options,
                         const gl::ProgramExecutable &programExecutable,
                         const std::vector<gl::LinkedUniform> &uniforms,
                         const gl::RangeUI &imageUniformRange,
                         const gl::ShaderType shaderType,
                         GlslangProgramInterfaceInfo *programInterfaceInfo,
                         UniformBindingIndexMap *uniformBindingIndexMapOut,
                         ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    for (unsigned int uniformIndex : imageUniformRange)
    {
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];
        if (programExecutable.hasLinkedShaderStage(shaderType))
        {
            std::string name = imageUniform.mappedName;
            if (GetImageNameWithoutIndices(&name))
            {
                bool updateFrontShaderType = false;
                if ((*uniformBindingIndexMapOut).count(name) > 0)
                {
                    UniformBindingInfo &uniformBindingInfo = (*uniformBindingIndexMapOut)[name];
                    updateFrontShaderType =
                        !imageUniform.isActive(uniformBindingInfo.frontShaderType);
                }
                AddAndUpdateResourceMaps(shaderType, ShaderVariableType::Image, name,
                                         &(programInterfaceInfo->currentShaderResourceBindingIndex),
                                         true, updateFrontShaderType,
                                         programInterfaceInfo->shaderResourceDescriptorSetIndex,
                                         uniformBindingIndexMapOut, variableInfoMapOut);
            }
            uint32_t imageIndex = uniformIndex - imageUniformRange.low();
            variableInfoMapOut->mapIndexedResourceByName(shaderType, ShaderVariableType::Image,
                                                         imageIndex, name);
        }
    }
}

void AssignNonTextureBindings(const GlslangSourceOptions &options,
                              const gl::ProgramExecutable &programExecutable,
                              const gl::ShaderType shaderType,
                              GlslangProgramInterfaceInfo *programInterfaceInfo,
                              UniformBindingIndexMap *uniformBindingIndexMapOut,
                              ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    const std::vector<gl::LinkedUniform> &uniforms = programExecutable.getUniforms();
    const gl::RangeUI &inputAttachmentUniformRange = programExecutable.getFragmentInoutRange();
    AssignInputAttachmentBindings(options, programExecutable, uniforms, inputAttachmentUniformRange,
                                  shaderType, programInterfaceInfo, uniformBindingIndexMapOut,
                                  variableInfoMapOut);

    const std::vector<gl::InterfaceBlock> &uniformBlocks = programExecutable.getUniformBlocks();
    AssignInterfaceBlockBindings(options, programExecutable, uniformBlocks, shaderType,
                                 ShaderVariableType::UniformBuffer, programInterfaceInfo,
                                 uniformBindingIndexMapOut, variableInfoMapOut);

    const std::vector<gl::InterfaceBlock> &storageBlocks =
        programExecutable.getShaderStorageBlocks();
    AssignInterfaceBlockBindings(options, programExecutable, storageBlocks, shaderType,
                                 ShaderVariableType::ShaderStorageBuffer, programInterfaceInfo,
                                 uniformBindingIndexMapOut, variableInfoMapOut);

    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers =
        programExecutable.getAtomicCounterBuffers();
    AssignAtomicCounterBufferBindings(options, programExecutable, atomicCounterBuffers, shaderType,
                                      programInterfaceInfo, uniformBindingIndexMapOut,
                                      variableInfoMapOut);

    const gl::RangeUI &imageUniformRange = programExecutable.getImageUniformRange();
    AssignImageBindings(options, programExecutable, uniforms, imageUniformRange, shaderType,
                        programInterfaceInfo, uniformBindingIndexMapOut, variableInfoMapOut);
}

void AssignTextureBindings(const GlslangSourceOptions &options,
                           const gl::ProgramExecutable &programExecutable,
                           const gl::ShaderType shaderType,
                           GlslangProgramInterfaceInfo *programInterfaceInfo,
                           UniformBindingIndexMap *uniformBindingIndexMapOut,
                           ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    // Assign textures to a descriptor set and binding.
    const std::vector<gl::LinkedUniform> &uniforms = programExecutable.getUniforms();

    for (unsigned int uniformIndex : programExecutable.getSamplerUniformRange())
    {
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

        // TODO: http://anglebug.com/4523: All uniforms should be active
        if (!programExecutable.hasLinkedShaderStage(shaderType) ||
            !samplerUniform.isActive(shaderType))
        {
            continue;
        }

        // Samplers in structs are extracted and renamed.
        const std::string samplerName = GlslangGetMappedSamplerName(samplerUniform.name);
        if (!gl::SamplerNameContainsNonZeroArrayElement(samplerUniform.name))
        {
            ASSERT(UniformNameIsIndexZero(samplerUniform.name));
            AddAndUpdateResourceMaps(shaderType, ShaderVariableType::Texture, samplerName,
                                     &(programInterfaceInfo->currentTextureBindingIndex), true,
                                     false, programInterfaceInfo->textureDescriptorSetIndex,
                                     uniformBindingIndexMapOut, variableInfoMapOut);
        }

        uint32_t textureIndex = uniformIndex - programExecutable.getSamplerUniformRange().low();
        variableInfoMapOut->mapIndexedResourceByName(shaderType, ShaderVariableType::Texture,
                                                     textureIndex, samplerName);
    }
}

// Base class for SPIR-V transformations.
class SpirvTransformerBase : angle::NonCopyable
{
  public:
    SpirvTransformerBase(const spirv::Blob &spirvBlobIn,
                         const ShaderInterfaceVariableInfoMap &variableInfoMap,
                         spirv::Blob *spirvBlobOut)
        : mSpirvBlobIn(spirvBlobIn), mVariableInfoMap(variableInfoMap), mSpirvBlobOut(spirvBlobOut)
    {
        gl::ShaderBitSet allStages;
        allStages.set();
        mBuiltinVariableInfo.activeStages = allStages;
    }

    std::vector<const ShaderInterfaceVariableInfo *> &getVariableInfoByIdMap()
    {
        return mVariableInfoById;
    }

    static spirv::IdRef GetNewId(spirv::Blob *blob);
    spirv::IdRef getNewId();

  protected:
    // Common utilities
    void onTransformBegin();
    const uint32_t *getCurrentInstruction(spv::Op *opCodeOut, uint32_t *wordCountOut) const;
    void copyInstruction(const uint32_t *instruction, size_t wordCount);

    // SPIR-V to transform:
    const spirv::Blob &mSpirvBlobIn;

    // Input shader variable info map:
    const ShaderInterfaceVariableInfoMap &mVariableInfoMap;

    // Transformed SPIR-V:
    spirv::Blob *mSpirvBlobOut;

    // Traversal state:
    size_t mCurrentWord       = 0;
    bool mIsInFunctionSection = false;

    // Transformation state:

    // Shader variable info per id, if id is a shader variable.
    std::vector<const ShaderInterfaceVariableInfo *> mVariableInfoById;
    ShaderInterfaceVariableInfo mBuiltinVariableInfo;
};

void SpirvTransformerBase::onTransformBegin()
{
    // Glslang succeeded in outputting SPIR-V, so we assume it's valid.
    ASSERT(mSpirvBlobIn.size() >= spirv::kHeaderIndexInstructions);
    // Since SPIR-V comes from a local call to glslang, it necessarily has the same endianness as
    // the running architecture, so no byte-swapping is necessary.
    ASSERT(mSpirvBlobIn[spirv::kHeaderIndexMagic] == spv::MagicNumber);

    // Make sure the transformer is not reused to avoid having to reinitialize it here.
    ASSERT(mCurrentWord == 0);
    ASSERT(mIsInFunctionSection == false);

    // Make sure the spirv::Blob is not reused.
    ASSERT(mSpirvBlobOut->empty());

    // Copy the header to SPIR-V blob, we need that to be defined for SpirvTransformerBase::getNewId
    // to work.
    mSpirvBlobOut->assign(mSpirvBlobIn.begin(),
                          mSpirvBlobIn.begin() + spirv::kHeaderIndexInstructions);

    mCurrentWord = spirv::kHeaderIndexInstructions;
}

const uint32_t *SpirvTransformerBase::getCurrentInstruction(spv::Op *opCodeOut,
                                                            uint32_t *wordCountOut) const
{
    ASSERT(mCurrentWord < mSpirvBlobIn.size());
    const uint32_t *instruction = &mSpirvBlobIn[mCurrentWord];

    spirv::GetInstructionOpAndLength(instruction, opCodeOut, wordCountOut);

    // Since glslang succeeded in producing SPIR-V, we assume it to be valid.
    ASSERT(mCurrentWord + *wordCountOut <= mSpirvBlobIn.size());

    return instruction;
}

void SpirvTransformerBase::copyInstruction(const uint32_t *instruction, size_t wordCount)
{
    mSpirvBlobOut->insert(mSpirvBlobOut->end(), instruction, instruction + wordCount);
}

spirv::IdRef SpirvTransformerBase::GetNewId(spirv::Blob *blob)
{
    return spirv::IdRef((*blob)[spirv::kHeaderIndexIndexBound]++);
}

spirv::IdRef SpirvTransformerBase::getNewId()
{
    return GetNewId(mSpirvBlobOut);
}

enum class SpirvVariableType
{
    InterfaceVariable,
    BuiltIn,
    Other,
};

enum class TransformationState
{
    Transformed,
    Unchanged,
};

// Helper class that gathers IDs of interest.  This class would be largely unnecessary when the
// translator generates SPIR-V directly, as it could communicate these IDs directly.
class SpirvIDDiscoverer final : angle::NonCopyable
{
  public:
    SpirvIDDiscoverer() : mOutputPerVertex{}, mInputPerVertex{} {}

    void init(size_t indexBound);

    // Instructions:
    void visitDecorate(spirv::IdRef id, spv::Decoration decoration);
    void visitName(spirv::IdRef id, const spirv::LiteralString &name);
    void visitMemberName(const ShaderInterfaceVariableInfo &info,
                         spirv::IdRef id,
                         spirv::LiteralInteger member,
                         const spirv::LiteralString &name);
    void visitTypeArray(spirv::IdResult id, spirv::IdRef elementType, spirv::IdRef length);
    void visitTypeFloat(spirv::IdResult id, spirv::LiteralInteger width);
    void visitTypeInt(spirv::IdResult id,
                      spirv::LiteralInteger width,
                      spirv::LiteralInteger signedness);
    void visitTypePointer(spirv::IdResult id, spv::StorageClass storageClass, spirv::IdRef typeId);
    void visitTypeVector(spirv::IdResult id,
                         spirv::IdRef componentId,
                         spirv::LiteralInteger componentCount);
    SpirvVariableType visitVariable(spirv::IdResultType typeId,
                                    spirv::IdResult id,
                                    spv::StorageClass storageClass,
                                    spirv::LiteralString *nameOut);

    // Helpers:
    void visitTypeHelper(spirv::IdResult id, spirv::IdRef typeId);
    void writePendingDeclarations(spirv::Blob *blobOut);

    // Getters:
    const spirv::LiteralString &getName(spirv::IdRef id) const { return mNamesById[id]; }
    bool isIOBlock(spirv::IdRef id) const { return mIsIOBlockById[id]; }
    bool isPerVertex(spirv::IdRef typeId) const
    {
        return typeId == mOutputPerVertex.typeId || typeId == mInputPerVertex.typeId;
    }
    uint32_t getPerVertexMaxActiveMember(spirv::IdRef typeId) const
    {
        ASSERT(isPerVertex(typeId));
        return typeId == mOutputPerVertex.typeId ? mOutputPerVertex.maxActiveMember
                                                 : mInputPerVertex.maxActiveMember;
    }

    spirv::IdRef floatId() const { return mFloatId; }
    spirv::IdRef vec4Id() const { return mVec4Id; }
    spirv::IdRef vec4OutTypePointerId() const { return mVec4OutTypePointerId; }
    spirv::IdRef intId() const { return mIntId; }
    spirv::IdRef ivec4Id() const { return mIvec4Id; }
    spirv::IdRef uintId() const { return mUintId; }
    spirv::IdRef int0Id() const { return mInt0Id; }
    spirv::IdRef floatHalfId() const { return mFloatHalfId; }
    spirv::IdRef outputPerVertexTypePointerId() const { return mOutputPerVertexTypePointerId; }
    spirv::IdRef outputPerVertexId() const { return mOutputPerVertexId; }

  private:
    // Names associated with ids through OpName.  The same name may be assigned to multiple ids, but
    // not all names are interesting (for example function arguments).  When the variable
    // declaration is met (OpVariable), the variable info is matched with the corresponding id's
    // name based on the Storage Class.
    std::vector<spirv::LiteralString> mNamesById;

    // Tracks whether a given type is an I/O block.  I/O blocks are identified by their type name
    // instead of variable name, but otherwise look like varyings of struct type (which are
    // identified by their instance name).  To disambiguate them, the `OpDecorate %N Block`
    // instruction is used which decorates I/O block types.
    std::vector<bool> mIsIOBlockById;

    // gl_PerVertex is unique in that it's the only builtin of struct type.  This struct is pruned
    // by removing trailing inactive members.  We therefore need to keep track of what's its type id
    // as well as which is the last active member. In the case of gl_PerVertex being used in an
    // array, we also need to keep track of the array's id. Note that intermediate stages, i.e.
    // geometry and tessellation have two gl_PerVertex declarations, one for input and one for
    // output.
    struct PerVertexData
    {
        spirv::IdRef typeId;
        spirv::IdRef arrayId;
        uint32_t maxActiveMember;
    };
    PerVertexData mOutputPerVertex;
    PerVertexData mInputPerVertex;

    // A handful of ids that are used to generate gl_Position transformation code (for pre-rotation
    // or depth correction).  These IDs are used to load/store gl_Position and apply modifications
    // and swizzles.
    //
    // - mFloatId: id of OpTypeFloat 32
    // - mVec4Id: id of OpTypeVector %mFloatId 4
    // - mVec4OutTypePointerId: id of OpTypePointer Output %mVec4Id
    // - mIntId: id of OpTypeInt 32 1
    // - mIvecId: id of OpTypeVector %mIntId 4
    // - mUintId: id of OpTypeInt 32 0
    // - mInt0Id: id of OpConstant %mIntId 0
    // - mFloatHalfId: id of OpConstant %mFloatId 0.5f
    // - mOutputPerVertexTypePointerId: id of OpTypePointer Output %mOutputPerVertex.typeId
    // - mOutputPerVertexId: id of OpVariable %mOutputPerVertexTypePointerId Output
    //
    spirv::IdRef mFloatId;
    spirv::IdRef mVec4Id;
    spirv::IdRef mVec4OutTypePointerId;
    spirv::IdRef mIntId;
    spirv::IdRef mIvec4Id;
    spirv::IdRef mUintId;
    spirv::IdRef mInt0Id;
    spirv::IdRef mFloatHalfId;
    spirv::IdRef mOutputPerVertexTypePointerId;
    spirv::IdRef mOutputPerVertexId;
};

void SpirvIDDiscoverer::init(size_t indexBound)
{
    // Allocate storage for id-to-name map.  Used to associate ShaderInterfaceVariableInfo with ids
    // based on name, but only when it's determined that the name corresponds to a shader interface
    // variable.
    mNamesById.resize(indexBound, nullptr);

    // Allocate storage for id-to-flag map.  Used to disambiguate I/O blocks instances from varyings
    // of struct type.
    mIsIOBlockById.resize(indexBound, false);
}

void SpirvIDDiscoverer::visitDecorate(spirv::IdRef id, spv::Decoration decoration)
{
    mIsIOBlockById[id] = decoration == spv::DecorationBlock;
}

void SpirvIDDiscoverer::visitName(spirv::IdRef id, const spirv::LiteralString &name)
{
    // The names and ids are unique
    ASSERT(id < mNamesById.size());
    ASSERT(mNamesById[id] == nullptr);

    mNamesById[id] = name;
}

void SpirvIDDiscoverer::visitMemberName(const ShaderInterfaceVariableInfo &info,
                                        spirv::IdRef id,
                                        spirv::LiteralInteger member,
                                        const spirv::LiteralString &name)
{
    // The names and ids are unique
    ASSERT(id < mNamesById.size());
    ASSERT(mNamesById[id] != nullptr);

    if (strcmp(mNamesById[id], "gl_PerVertex") != 0)
    {
        return;
    }

    // Assume output gl_PerVertex is encountered first.  When the storage class of these types are
    // determined, the variables can be swapped if this assumption was incorrect.
    if (!mOutputPerVertex.typeId.valid() || id == mOutputPerVertex.typeId)
    {
        mOutputPerVertex.typeId = id;

        // Keep track of the range of members that are active.
        if (info.builtinIsOutput && member > mOutputPerVertex.maxActiveMember)
        {
            mOutputPerVertex.maxActiveMember = member;
        }
    }
    else if (!mInputPerVertex.typeId.valid() || id == mInputPerVertex.typeId)
    {
        mInputPerVertex.typeId = id;

        // Keep track of the range of members that are active.
        if (info.builtinIsInput && member > mInputPerVertex.maxActiveMember)
        {
            mInputPerVertex.maxActiveMember = member;
        }
    }
    else
    {
        UNREACHABLE();
    }
}

void SpirvIDDiscoverer::visitTypeHelper(spirv::IdResult id, spirv::IdRef typeId)
{
    // Every type id is declared only once.
    ASSERT(id < mNamesById.size());
    ASSERT(mNamesById[id] == nullptr);
    ASSERT(id < mIsIOBlockById.size());
    ASSERT(!mIsIOBlockById[id]);

    // Carry the name forward from the base type.  This is only necessary for interface blocks,
    // as the variable info is associated with the block name instead of the variable name (to
    // support nameless interface blocks).  When the variable declaration is met, either the
    // type name or the variable name is used to associate with info based on the variable's
    // storage class.
    ASSERT(typeId < mNamesById.size());
    mNamesById[id] = mNamesById[typeId];

    // Similarly, carry forward the information regarding whether this type is an I/O block.
    ASSERT(typeId < mIsIOBlockById.size());
    mIsIOBlockById[id] = mIsIOBlockById[typeId];
}

void SpirvIDDiscoverer::visitTypeArray(spirv::IdResult id,
                                       spirv::IdRef elementType,
                                       spirv::IdRef length)
{
    visitTypeHelper(id, elementType);
    // In the case of a gl_PerVertex block being used in an array (gl_in/gl_out), save the id of the
    // array
    if (elementType == mOutputPerVertex.typeId)
    {
        mOutputPerVertex.arrayId = id;
    }
    else if (elementType == mInputPerVertex.typeId)
    {
        mInputPerVertex.arrayId = id;
    }
}

void SpirvIDDiscoverer::visitTypeFloat(spirv::IdResult id, spirv::LiteralInteger width)
{
    // Only interested in OpTypeFloat 32.
    if (width == 32)
    {
        ASSERT(!mFloatId.valid());
        mFloatId = id;
    }
}

void SpirvIDDiscoverer::visitTypeInt(spirv::IdResult id,
                                     spirv::LiteralInteger width,
                                     spirv::LiteralInteger signedness)
{
    // Only interested in OpTypeInt 32 *.
    if (width != 32)
    {
        return;
    }

    if (signedness == 0)
    {
        ASSERT(!mUintId.valid());
        mUintId = id;
    }
    else
    {
        ASSERT(!mIntId.valid());
        mIntId = id;
    }
}

void SpirvIDDiscoverer::visitTypePointer(spirv::IdResult id,
                                         spv::StorageClass storageClass,
                                         spirv::IdRef typeId)
{
    visitTypeHelper(id, typeId);

    // Check if the type is a gl_PerVertex block or an array of gl_PerVertex blocks
    bool isOutputPerVertex =
        (typeId == mOutputPerVertex.typeId || typeId == mOutputPerVertex.arrayId);
    bool isInputPerVertex = (typeId == mInputPerVertex.typeId || typeId == mInputPerVertex.arrayId);

    // Verify that the ids associated with input and output gl_PerVertex are correct.
    if (isOutputPerVertex || isInputPerVertex)
    {
        // If assumption about the first gl_PerVertex encountered being Output is wrong, swap the
        // two ids.
        if ((isOutputPerVertex && storageClass == spv::StorageClassInput) ||
            (isInputPerVertex && storageClass == spv::StorageClassOutput))
        {
            std::swap(mOutputPerVertex.typeId, mInputPerVertex.typeId);
            std::swap(mOutputPerVertex.arrayId, mInputPerVertex.arrayId);
        }

        // Remember type pointer of output gl_PerVertex for gl_Position transformations
        if (typeId == mOutputPerVertex.typeId)
        {
            mOutputPerVertexTypePointerId = id;
        }
    }

    // If OpTypePointer Output %mVec4ID was encountered, remember that.  Otherwise we'll have to
    // generate one.
    if (typeId == mVec4Id && storageClass == spv::StorageClassOutput)
    {
        mVec4OutTypePointerId = id;
    }
}

void SpirvIDDiscoverer::visitTypeVector(spirv::IdResult id,
                                        spirv::IdRef componentId,
                                        spirv::LiteralInteger componentCount)
{
    // Only interested in OpTypeVector %mFloatId 4 and OpTypeVector %mIntId 4
    if (componentId == mFloatId && componentCount == 4)
    {
        ASSERT(!mVec4Id.valid());
        mVec4Id = id;
    }
    if (componentId == mIntId && componentCount == 4)
    {
        ASSERT(!mIvec4Id.valid());
        mIvec4Id = id;
    }
}

SpirvVariableType SpirvIDDiscoverer::visitVariable(spirv::IdResultType typeId,
                                                   spirv::IdResult id,
                                                   spv::StorageClass storageClass,
                                                   spirv::LiteralString *nameOut)
{
    ASSERT(typeId < mNamesById.size());
    ASSERT(id < mNamesById.size());
    ASSERT(typeId < mIsIOBlockById.size());

    // If storage class indicates that this is not a shader interface variable, ignore it.
    const bool isInterfaceBlockVariable =
        storageClass == spv::StorageClassUniform || storageClass == spv::StorageClassStorageBuffer;
    const bool isOpaqueUniform = storageClass == spv::StorageClassUniformConstant;
    const bool isInOut =
        storageClass == spv::StorageClassInput || storageClass == spv::StorageClassOutput;

    if (!isInterfaceBlockVariable && !isOpaqueUniform && !isInOut)
    {
        return SpirvVariableType::Other;
    }

    // For interface block variables, the name that's used to associate info is the block name
    // rather than the variable name.
    const bool isIOBlock = mIsIOBlockById[typeId];
    *nameOut             = mNamesById[isInterfaceBlockVariable || isIOBlock ? typeId : id];

    ASSERT(*nameOut != nullptr);

    // Handle builtins, which all start with "gl_".  The variable name could be an indication of a
    // builtin variable (such as with gl_FragCoord).  gl_PerVertex is the only builtin whose "type"
    // name starts with gl_.  However, gl_PerVertex has its own entry in the info map for its
    // potential use with transform feedback.
    const bool isNameBuiltin = isInOut && !isIOBlock && gl::IsBuiltInName(*nameOut);
    if (isNameBuiltin)
    {
        return SpirvVariableType::BuiltIn;
    }

    if (typeId == mOutputPerVertexTypePointerId)
    {
        // If this is the output gl_PerVertex variable, remember its id for gl_Position
        // transformations.
        ASSERT(storageClass == spv::StorageClassOutput && isIOBlock &&
               strcmp(*nameOut, "gl_PerVertex") == 0);
        mOutputPerVertexId = id;
    }

    return SpirvVariableType::InterfaceVariable;
}

void SpirvIDDiscoverer::writePendingDeclarations(spirv::Blob *blobOut)
{
    if (!mFloatId.valid())
    {
        mFloatId = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteTypeFloat(blobOut, mFloatId, spirv::LiteralInteger(32));
    }

    if (!mVec4Id.valid())
    {
        mVec4Id = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteTypeVector(blobOut, mVec4Id, mFloatId, spirv::LiteralInteger(4));
    }

    if (!mVec4OutTypePointerId.valid())
    {
        mVec4OutTypePointerId = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteTypePointer(blobOut, mVec4OutTypePointerId, spv::StorageClassOutput, mVec4Id);
    }

    if (!mIntId.valid())
    {
        mIntId = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteTypeInt(blobOut, mIntId, spirv::LiteralInteger(32), spirv::LiteralInteger(1));
    }

    if (!mIvec4Id.valid())
    {
        mIvec4Id = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteTypeVector(blobOut, mIvec4Id, mIntId, spirv::LiteralInteger(4));
    }

    ASSERT(!mInt0Id.valid());
    mInt0Id = SpirvTransformerBase::GetNewId(blobOut);
    spirv::WriteConstant(blobOut, mIntId, mInt0Id, spirv::LiteralContextDependentNumber(0));

    constexpr uint32_t kFloatHalfAsUint = 0x3F00'0000;

    ASSERT(!mFloatHalfId.valid());
    mFloatHalfId = SpirvTransformerBase::GetNewId(blobOut);
    spirv::WriteConstant(blobOut, mFloatId, mFloatHalfId,
                         spirv::LiteralContextDependentNumber(kFloatHalfAsUint));
}

// Helper class that trims input and output gl_PerVertex declarations to remove inactive builtins.
class SpirvPerVertexTrimmer final : angle::NonCopyable
{
  public:
    SpirvPerVertexTrimmer() {}

    TransformationState transformMemberDecorate(const SpirvIDDiscoverer &ids,
                                                spirv::IdRef typeId,
                                                spirv::LiteralInteger member,
                                                spv::Decoration decoration);
    TransformationState transformMemberName(const SpirvIDDiscoverer &ids,
                                            spirv::IdRef id,
                                            spirv::LiteralInteger member,
                                            const spirv::LiteralString &name);
    TransformationState transformTypeStruct(const SpirvIDDiscoverer &ids,
                                            spirv::IdResult id,
                                            spirv::IdRefList *memberList,
                                            spirv::Blob *blobOut);
};

TransformationState SpirvPerVertexTrimmer::transformMemberDecorate(const SpirvIDDiscoverer &ids,
                                                                   spirv::IdRef typeId,
                                                                   spirv::LiteralInteger member,
                                                                   spv::Decoration decoration)
{
    // Transform the following:
    //
    // - OpMemberDecorate %gl_PerVertex N BuiltIn B
    // - OpMemberDecorate %gl_PerVertex N Invariant
    // - OpMemberDecorate %gl_PerVertex N RelaxedPrecision
    if (!ids.isPerVertex(typeId) ||
        (decoration != spv::DecorationBuiltIn && decoration != spv::DecorationInvariant &&
         decoration != spv::DecorationRelaxedPrecision))
    {
        return TransformationState::Unchanged;
    }

    // Drop stripped fields.
    return member > ids.getPerVertexMaxActiveMember(typeId) ? TransformationState::Transformed
                                                            : TransformationState::Unchanged;
}

TransformationState SpirvPerVertexTrimmer::transformMemberName(const SpirvIDDiscoverer &ids,
                                                               spirv::IdRef id,
                                                               spirv::LiteralInteger member,
                                                               const spirv::LiteralString &name)
{
    // Remove the instruction if it's a stripped member of gl_PerVertex.
    return ids.isPerVertex(id) && member > ids.getPerVertexMaxActiveMember(id)
               ? TransformationState::Transformed
               : TransformationState::Unchanged;
}

TransformationState SpirvPerVertexTrimmer::transformTypeStruct(const SpirvIDDiscoverer &ids,
                                                               spirv::IdResult id,
                                                               spirv::IdRefList *memberList,
                                                               spirv::Blob *blobOut)
{
    if (!ids.isPerVertex(id))
    {
        return TransformationState::Unchanged;
    }

    const uint32_t maxMembers = ids.getPerVertexMaxActiveMember(id);

    // Change the definition of the gl_PerVertex struct by stripping unused fields at the end.
    const uint32_t memberCount = maxMembers + 1;
    memberList->resize(memberCount);

    spirv::WriteTypeStruct(blobOut, id, *memberList);

    return TransformationState::Transformed;
}

// Helper class that removes inactive varyings and replaces them with Private variables.
class SpirvInactiveVaryingRemover final : angle::NonCopyable
{
  public:
    SpirvInactiveVaryingRemover() {}

    void init(size_t indexCount);

    TransformationState transformAccessChain(spirv::IdResultType typeId,
                                             spirv::IdResult id,
                                             spirv::IdRef baseId,
                                             const spirv::IdRefList &indexList,
                                             spirv::Blob *blobOut);
    TransformationState transformDecorate(const ShaderInterfaceVariableInfo &info,
                                          gl::ShaderType shaderType,
                                          spirv::IdRef id,
                                          spv::Decoration decoration,
                                          const spirv::LiteralIntegerList &decorationValues,
                                          spirv::Blob *blobOut);
    TransformationState transformTypePointer(const SpirvIDDiscoverer &ids,
                                             spirv::IdResult id,
                                             spv::StorageClass storageClass,
                                             spirv::IdRef typeId,
                                             spirv::Blob *blobOut);
    TransformationState transformVariable(spirv::IdResultType typeId,
                                          spirv::IdResult id,
                                          spv::StorageClass storageClass,
                                          spirv::Blob *blobOut);

    void modifyEntryPointInterfaceList(
        const std::vector<const ShaderInterfaceVariableInfo *> &variableInfoById,
        gl::ShaderType shaderType,
        spirv::IdRefList *interfaceList);

    bool isInactive(spirv::IdRef id) const { return mIsInactiveById[id]; }

  private:
    // Each OpTypePointer instruction that defines a type with the Output storage class is
    // duplicated with a similar instruction but which defines a type with the Private storage
    // class.  If inactive varyings are encountered, its type is changed to the Private one.  The
    // following vector maps the Output type id to the corresponding Private one.
    std::vector<spirv::IdRef> mTypePointerTransformedId;

    // Whether a variable has been marked inactive.
    std::vector<bool> mIsInactiveById;
};

void SpirvInactiveVaryingRemover::init(size_t indexBound)
{
    // Allocate storage for Output type pointer map.  At index i, this vector holds the identical
    // type as %i except for its storage class turned to Private.
    mTypePointerTransformedId.resize(indexBound);
    mIsInactiveById.resize(indexBound, false);
}

TransformationState SpirvInactiveVaryingRemover::transformAccessChain(
    spirv::IdResultType typeId,
    spirv::IdResult id,
    spirv::IdRef baseId,
    const spirv::IdRefList &indexList,
    spirv::Blob *blobOut)
{
    // Modifiy the instruction to use the private type.
    ASSERT(typeId < mTypePointerTransformedId.size());
    ASSERT(mTypePointerTransformedId[typeId].valid());

    spirv::WriteAccessChain(blobOut, mTypePointerTransformedId[typeId], id, baseId, indexList);

    return TransformationState::Transformed;
}

TransformationState SpirvInactiveVaryingRemover::transformDecorate(
    const ShaderInterfaceVariableInfo &info,
    gl::ShaderType shaderType,
    spirv::IdRef id,
    spv::Decoration decoration,
    const spirv::LiteralIntegerList &decorationValues,
    spirv::Blob *blobOut)
{
    // If it's an inactive varying, remove the decoration altogether.
    return info.activeStages[shaderType] ? TransformationState::Unchanged
                                         : TransformationState::Transformed;
}

void SpirvInactiveVaryingRemover::modifyEntryPointInterfaceList(
    const std::vector<const ShaderInterfaceVariableInfo *> &variableInfoById,
    gl::ShaderType shaderType,
    spirv::IdRefList *interfaceList)
{
    // Filter out inactive varyings from entry point interface declaration.
    size_t writeIndex = 0;
    for (size_t index = 0; index < interfaceList->size(); ++index)
    {
        spirv::IdRef id((*interfaceList)[index]);
        const ShaderInterfaceVariableInfo *info = variableInfoById[id];

        ASSERT(info);

        if (!info->activeStages[shaderType])
        {
            continue;
        }

        (*interfaceList)[writeIndex] = id;
        ++writeIndex;
    }

    // Update the number of interface variables.
    interfaceList->resize(writeIndex);
}

TransformationState SpirvInactiveVaryingRemover::transformTypePointer(
    const SpirvIDDiscoverer &ids,
    spirv::IdResult id,
    spv::StorageClass storageClass,
    spirv::IdRef typeId,
    spirv::Blob *blobOut)
{
    // If the storage class is output, this may be used to create a variable corresponding to an
    // inactive varying, or if that varying is a struct, an Op*AccessChain retrieving a field of
    // that inactive varying.
    //
    // SPIR-V specifies the storage class both on the type and the variable declaration.  Otherwise
    // it would have been sufficient to modify the OpVariable instruction. For simplicity, duplicate
    // every "OpTypePointer Output" and "OpTypePointer Input" instruction except with the Private
    // storage class, in case it may be necessary later.

    // Cannot create a Private type declaration from builtins such as gl_PerVertex.
    if (ids.getName(typeId) != nullptr && gl::IsBuiltInName(ids.getName(typeId)))
    {
        return TransformationState::Unchanged;
    }

    if (storageClass != spv::StorageClassOutput && storageClass != spv::StorageClassInput)
    {
        return TransformationState::Unchanged;
    }

    const spirv::IdRef newPrivateTypeId(SpirvTransformerBase::GetNewId(blobOut));

    // Write OpTypePointer for the new PrivateType.
    spirv::WriteTypePointer(blobOut, newPrivateTypeId, spv::StorageClassPrivate, typeId);

    // Remember the id of the replacement.
    ASSERT(id < mTypePointerTransformedId.size());
    mTypePointerTransformedId[id] = newPrivateTypeId;

    // The original instruction should still be present as well.  At this point, we don't know
    // whether we will need the original or Private type.
    return TransformationState::Unchanged;
}

TransformationState SpirvInactiveVaryingRemover::transformVariable(spirv::IdResultType typeId,
                                                                   spirv::IdResult id,
                                                                   spv::StorageClass storageClass,
                                                                   spirv::Blob *blobOut)
{
    ASSERT(storageClass == spv::StorageClassOutput || storageClass == spv::StorageClassInput);

    ASSERT(typeId < mTypePointerTransformedId.size());
    ASSERT(mTypePointerTransformedId[typeId].valid());
    spirv::WriteVariable(blobOut, mTypePointerTransformedId[typeId], id, spv::StorageClassPrivate,
                         nullptr);

    mIsInactiveById[id] = true;

    return TransformationState::Transformed;
}

// Helper class that generates code for transform feedback
class SpirvTransformFeedbackCodeGenerator final : angle::NonCopyable
{
  public:
    SpirvTransformFeedbackCodeGenerator(bool isEmulated)
        : mIsEmulated(isEmulated), mHasTransformFeedbackOutput(false)
    {}

    void visitName(spirv::IdRef id, const spirv::LiteralString &name);
    void visitTypeVector(const SpirvIDDiscoverer &ids,
                         spirv::IdResult id,
                         spirv::IdRef componentId,
                         spirv::LiteralInteger componentCount);
    void visitTypePointer(spirv::IdResult id, spv::StorageClass storageClass, spirv::IdRef typeId);
    void visitVariable(const ShaderInterfaceVariableInfo &info,
                       gl::ShaderType shaderType,
                       const spirv::LiteralString &name,
                       spirv::IdResultType typeId,
                       spirv::IdResult id,
                       spv::StorageClass storageClass);

    TransformationState transformCapability(spv::Capability capability, spirv::Blob *blobOut);
    TransformationState transformName(spirv::IdRef id, spirv::LiteralString name);
    TransformationState transformVariable(const ShaderInterfaceVariableInfo &info,
                                          const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                          gl::ShaderType shaderType,
                                          spirv::IdResultType typeId,
                                          spirv::IdResult id,
                                          spv::StorageClass storageClass);

    void writePendingDeclarations(
        const std::vector<const ShaderInterfaceVariableInfo *> &variableInfoById,
        const SpirvIDDiscoverer &ids,
        spirv::Blob *blobOut);
    void writeTransformFeedbackExtensionOutput(const SpirvIDDiscoverer &ids,
                                               spirv::IdRef positionId,
                                               spirv::Blob *blobOut);
    void writeTransformFeedbackEmulationOutput(
        const SpirvIDDiscoverer &ids,
        const SpirvInactiveVaryingRemover &inactiveVaryingRemover,
        spirv::IdRef currentFunctionId,
        spirv::Blob *blobOut);
    void addExecutionMode(spirv::IdRef entryPointId, spirv::Blob *blobOut);
    void addMemberDecorate(const ShaderInterfaceVariableInfo &info,
                           spirv::IdRef id,
                           spirv::Blob *blobOut);
    void addDecorate(const ShaderInterfaceVariableInfo &info,
                     spirv::IdRef id,
                     spirv::Blob *blobOut);

  private:
    void gatherXfbVaryings(const ShaderInterfaceVariableInfo &info, spirv::IdRef id);
    void visitXfbVarying(const ShaderInterfaceVariableXfbInfo &xfb,
                         spirv::IdRef baseId,
                         uint32_t fieldIndex);
    void writeIntConstant(const SpirvIDDiscoverer &ids,
                          uint32_t value,
                          spirv::IdRef intId,
                          spirv::Blob *blobOut);
    void getVaryingTypeIds(const SpirvIDDiscoverer &ids,
                           GLenum componentType,
                           bool isPrivate,
                           spirv::IdRef *typeIdOut,
                           spirv::IdRef *typePtrOut);
    void writeGetOffsetsCall(spirv::IdRef xfbOffsets, spirv::Blob *blobOut);
    void writeComponentCapture(const SpirvIDDiscoverer &ids,
                               uint32_t bufferIndex,
                               spirv::IdRef xfbOffset,
                               spirv::IdRef varyingTypeId,
                               spirv::IdRef varyingTypePtr,
                               spirv::IdRef varyingBaseId,
                               const spirv::IdRefList &accessChainIndices,
                               GLenum componentType,
                               spirv::Blob *blobOut);

    static constexpr size_t kXfbDecorationCount                           = 3;
    static constexpr spv::Decoration kXfbDecorations[kXfbDecorationCount] = {
        spv::DecorationXfbBuffer,
        spv::DecorationXfbStride,
        spv::DecorationOffset,
    };

    bool mIsEmulated;
    bool mHasTransformFeedbackOutput;

    // Ids needed to generate transform feedback support code.
    spirv::IdRef mTransformFeedbackExtensionPositionId;
    spirv::IdRef mGetXfbOffsetsFuncId;
    spirv::IdRef mXfbCaptureFuncId;
    gl::TransformFeedbackBuffersArray<spirv::IdRef> mXfbBuffers;
    gl::TransformFeedbackBuffersArray<spirv::IdRef> mBufferStrides;
    spirv::IdRef mBufferStridesCompositeId;

    // Type and constant ids:
    //
    // - mIVec4Id: id of OpTypeVector %mIntId 4
    //
    // - mFloatOutputPointerId: id of OpTypePointer Output %mFloatId
    // - mIntOutputPointerId: id of OpTypePointer Output %mIntId
    // - mUintOutputPointerId: id of OpTypePointer Output %mUintId
    // - mFloatPrivatePointerId, mIntPrivatePointerId, mUintPrivatePointerId: identical to the
    //   above, but with the Private storage class.  Used to load from varyings that have been
    //   replaced as part of precision mismatch fixup.
    // - mFloatUniformPointerId: id of OpTypePointer Uniform %mFloatId
    // - mIVec4FuncPointerId: id of OpTypePointer Function %mIVec4Id
    //
    // - mIntNIds[n]: id of OpConstant %mIntId n
    spirv::IdRef mIVec4Id;
    spirv::IdRef mFloatOutputPointerId;
    spirv::IdRef mIntOutputPointerId;
    spirv::IdRef mUintOutputPointerId;
    spirv::IdRef mFloatPrivatePointerId;
    spirv::IdRef mIntPrivatePointerId;
    spirv::IdRef mUintPrivatePointerId;
    spirv::IdRef mFloatUniformPointerId;
    spirv::IdRef mIVec4FuncPointerId;
    // Id of constants such as row, column and array index.  Integers 0, 1, 2 and 3 are always
    // defined due to the ubiquity of usage.
    angle::FastVector<spirv::IdRef, 4> mIntNIds;

    // For transform feedback emulation, the captured elements are gathered in a list and sorted.
    // This allows the output generation code to always use offset += 1, thus relying on only one
    // constant (1).
    struct XfbVarying
    {
        // The varyings are sorted by info.offset.
        const ShaderInterfaceVariableXfbInfo *info;
        // Id of the base variable.
        spirv::IdRef baseId;
        // The field index, if a member of an I/O blocks
        uint32_t fieldIndex;
    };
    gl::TransformFeedbackBuffersArray<std::vector<XfbVarying>> mXfbVaryings;
};

constexpr size_t SpirvTransformFeedbackCodeGenerator::kXfbDecorationCount;
constexpr spv::Decoration SpirvTransformFeedbackCodeGenerator::kXfbDecorations[kXfbDecorationCount];

void SpirvTransformFeedbackCodeGenerator::visitName(spirv::IdRef id,
                                                    const spirv::LiteralString &name)
{
    if (!mIsEmulated)
    {
        return;
    }

    const size_t bufferNameBaseLength = strlen(sh::vk::kXfbEmulationBufferName);

    if (angle::BeginsWith(name, sh::vk::kXfbEmulationGetOffsetsFunctionName))
    {
        ASSERT(!mGetXfbOffsetsFuncId.valid());
        mGetXfbOffsetsFuncId = id;
    }
    else if (angle::BeginsWith(name, sh::vk::kXfbEmulationCaptureFunctionName))
    {
        ASSERT(!mXfbCaptureFuncId.valid());
        mXfbCaptureFuncId = id;
    }
    else if (angle::BeginsWith(name, sh::vk::kXfbEmulationBufferName) &&
             std::isdigit(name[bufferNameBaseLength]))
    {
        static_assert(gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS < 10,
                      "Parsing the xfb buffer index below must be adjusted");
        uint32_t xfbBuffer     = name[bufferNameBaseLength] - '0';
        mXfbBuffers[xfbBuffer] = id;
    }
}

void SpirvTransformFeedbackCodeGenerator::visitTypeVector(const SpirvIDDiscoverer &ids,
                                                          spirv::IdResult id,
                                                          spirv::IdRef componentId,
                                                          spirv::LiteralInteger componentCount)
{
    // Only interested in OpTypeVector %mIntId 4
    if (componentId == ids.intId() && componentCount == 4)
    {
        ASSERT(!mIVec4Id.valid());
        mIVec4Id = id;
    }
}

void SpirvTransformFeedbackCodeGenerator::visitTypePointer(spirv::IdResult id,
                                                           spv::StorageClass storageClass,
                                                           spirv::IdRef typeId)
{
    if (typeId == mIVec4Id && storageClass == spv::StorageClassFunction)
    {
        ASSERT(!mIVec4FuncPointerId.valid());
        mIVec4FuncPointerId = id;
    }
}

void SpirvTransformFeedbackCodeGenerator::visitVariable(const ShaderInterfaceVariableInfo &info,
                                                        gl::ShaderType shaderType,
                                                        const spirv::LiteralString &name,
                                                        spirv::IdResultType typeId,
                                                        spirv::IdResult id,
                                                        spv::StorageClass storageClass)
{
    if (mIsEmulated)
    {
        gatherXfbVaryings(info, id);
        return;
    }

    // Note if the variable is captured by transform feedback.  In that case, the TransformFeedback
    // capability needs to be added.
    if ((info.xfb.buffer != ShaderInterfaceVariableInfo::kInvalid || !info.fieldXfb.empty()) &&
        info.activeStages[shaderType])
    {
        mHasTransformFeedbackOutput = true;

        // If this is the special ANGLEXfbPosition variable, remember its id to be used for the
        // ANGLEXfbPosition = gl_Position; assignment code generation.
        if (strcmp(name, sh::vk::kXfbExtensionPositionOutName) == 0)
        {
            mTransformFeedbackExtensionPositionId = id;
        }
    }
}

TransformationState SpirvTransformFeedbackCodeGenerator::transformCapability(
    spv::Capability capability,
    spirv::Blob *blobOut)
{
    if (!mHasTransformFeedbackOutput || mIsEmulated)
    {
        return TransformationState::Unchanged;
    }

    // Transform feedback capability shouldn't have already been specified.
    ASSERT(capability != spv::CapabilityTransformFeedback);

    // Vulkan shaders have either Shader, Geometry or Tessellation capability.  We find this
    // capability, and add the TransformFeedback capability right before it.
    if (capability != spv::CapabilityShader && capability != spv::CapabilityGeometry &&
        capability != spv::CapabilityTessellation)
    {
        return TransformationState::Unchanged;
    }

    // Write the TransformFeedback capability declaration.
    spirv::WriteCapability(blobOut, spv::CapabilityTransformFeedback);

    // The original capability is retained.
    return TransformationState::Unchanged;
}

TransformationState SpirvTransformFeedbackCodeGenerator::transformName(spirv::IdRef id,
                                                                       spirv::LiteralString name)
{
    // In the case of ANGLEXfbN, unconditionally remove the variable names.  If transform
    // feedback is not active, the corresponding variables will be removed.
    return angle::BeginsWith(name, sh::vk::kXfbEmulationBufferName)
               ? TransformationState::Transformed
               : TransformationState::Unchanged;
}

TransformationState SpirvTransformFeedbackCodeGenerator::transformVariable(
    const ShaderInterfaceVariableInfo &info,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    gl::ShaderType shaderType,
    spirv::IdResultType typeId,
    spirv::IdResult id,
    spv::StorageClass storageClass)
{
    // This function is currently called for inactive variables.
    ASSERT(!info.activeStages[shaderType]);

    if (shaderType == gl::ShaderType::Vertex && storageClass == spv::StorageClassUniform)
    {
        // The ANGLEXfbN variables are unconditionally generated and may be inactive.  Remove these
        // variables in that case.
        ASSERT(&info == &variableInfoMap.getVariableByName(shaderType, GetXfbBufferName(0)) ||
               &info == &variableInfoMap.getVariableByName(shaderType, GetXfbBufferName(1)) ||
               &info == &variableInfoMap.getVariableByName(shaderType, GetXfbBufferName(2)) ||
               &info == &variableInfoMap.getVariableByName(shaderType, GetXfbBufferName(3)));

        // Drop the declaration.
        return TransformationState::Transformed;
    }

    return TransformationState::Unchanged;
}

void SpirvTransformFeedbackCodeGenerator::gatherXfbVaryings(const ShaderInterfaceVariableInfo &info,
                                                            spirv::IdRef id)
{
    visitXfbVarying(info.xfb, id, ShaderInterfaceVariableXfbInfo::kInvalid);

    for (size_t fieldIndex = 0; fieldIndex < info.fieldXfb.size(); ++fieldIndex)
    {
        visitXfbVarying(info.fieldXfb[fieldIndex], id, static_cast<uint32_t>(fieldIndex));
    }
}

void SpirvTransformFeedbackCodeGenerator::visitXfbVarying(const ShaderInterfaceVariableXfbInfo &xfb,
                                                          spirv::IdRef baseId,
                                                          uint32_t fieldIndex)
{
    for (const ShaderInterfaceVariableXfbInfo &arrayElement : xfb.arrayElements)
    {
        visitXfbVarying(arrayElement, baseId, fieldIndex);
    }

    if (xfb.buffer == ShaderInterfaceVariableXfbInfo::kInvalid)
    {
        return;
    }

    // Varyings captured to the same buffer have the same stride.
    ASSERT(mXfbVaryings[xfb.buffer].empty() ||
           mXfbVaryings[xfb.buffer][0].info->stride == xfb.stride);

    mXfbVaryings[xfb.buffer].push_back({&xfb, baseId, fieldIndex});
}

void SpirvTransformFeedbackCodeGenerator::writeIntConstant(const SpirvIDDiscoverer &ids,
                                                           uint32_t value,
                                                           spirv::IdRef intId,
                                                           spirv::Blob *blobOut)
{
    if (value == ShaderInterfaceVariableXfbInfo::kInvalid)
    {
        return;
    }

    if (mIntNIds.size() <= value)
    {
        mIntNIds.resize(value + 1);
    }
    else if (mIntNIds[value].valid())
    {
        return;
    }

    mIntNIds[value] = SpirvTransformerBase::GetNewId(blobOut);
    spirv::WriteConstant(blobOut, ids.intId(), mIntNIds[value],
                         spirv::LiteralContextDependentNumber(value));
}

void SpirvTransformFeedbackCodeGenerator::writePendingDeclarations(
    const std::vector<const ShaderInterfaceVariableInfo *> &variableInfoById,
    const SpirvIDDiscoverer &ids,
    spirv::Blob *blobOut)
{
    if (!mIsEmulated)
    {
        return;
    }

    ASSERT(mIVec4Id.valid());

    mFloatOutputPointerId  = SpirvTransformerBase::GetNewId(blobOut);
    mFloatPrivatePointerId = SpirvTransformerBase::GetNewId(blobOut);
    spirv::WriteTypePointer(blobOut, mFloatOutputPointerId, spv::StorageClassOutput, ids.floatId());
    spirv::WriteTypePointer(blobOut, mFloatPrivatePointerId, spv::StorageClassPrivate,
                            ids.floatId());

    if (ids.intId().valid())
    {
        mIntOutputPointerId  = SpirvTransformerBase::GetNewId(blobOut);
        mIntPrivatePointerId = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteTypePointer(blobOut, mIntOutputPointerId, spv::StorageClassOutput, ids.intId());
        spirv::WriteTypePointer(blobOut, mIntPrivatePointerId, spv::StorageClassPrivate,
                                ids.intId());
    }

    if (ids.uintId().valid())
    {
        mUintOutputPointerId  = SpirvTransformerBase::GetNewId(blobOut);
        mUintPrivatePointerId = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteTypePointer(blobOut, mUintOutputPointerId, spv::StorageClassOutput,
                                ids.uintId());
        spirv::WriteTypePointer(blobOut, mUintPrivatePointerId, spv::StorageClassPrivate,
                                ids.uintId());
    }

    mFloatUniformPointerId = SpirvTransformerBase::GetNewId(blobOut);
    spirv::WriteTypePointer(blobOut, mFloatUniformPointerId, spv::StorageClassUniform,
                            ids.floatId());

    if (!mIVec4FuncPointerId.valid())
    {
        mIVec4FuncPointerId = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteTypePointer(blobOut, mIVec4FuncPointerId, spv::StorageClassFunction,
                                ids.ivec4Id());
    }

    mIntNIds.resize(4);
    mIntNIds[0] = ids.int0Id();
    for (int n = 1; n < 4; ++n)
    {
        writeIntConstant(ids, n, ids.intId(), blobOut);
    }

    spirv::IdRefList compositeIds;
    for (const std::vector<XfbVarying> &varyings : mXfbVaryings)
    {
        if (varyings.empty())
        {
            compositeIds.push_back(ids.int0Id());
            continue;
        }

        const ShaderInterfaceVariableXfbInfo *info0 = varyings[0].info;

        // Define the buffer stride constant
        ASSERT(info0->stride % sizeof(float) == 0);
        uint32_t stride = info0->stride / sizeof(float);

        mBufferStrides[info0->buffer] = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteConstant(blobOut, ids.intId(), mBufferStrides[info0->buffer],
                             spirv::LiteralContextDependentNumber(stride));

        compositeIds.push_back(mBufferStrides[info0->buffer]);

        // Define all the constants that would be necessary to load the components of the varying.
        for (const XfbVarying &varying : varyings)
        {
            writeIntConstant(ids, varying.fieldIndex, ids.intId(), blobOut);
            const ShaderInterfaceVariableXfbInfo *info = varying.info;
            if (info->arraySize == ShaderInterfaceVariableXfbInfo::kInvalid)
            {
                continue;
            }

            uint32_t arrayIndexStart =
                varying.info->arrayIndex != ShaderInterfaceVariableXfbInfo::kInvalid
                    ? varying.info->arrayIndex
                    : 0;
            uint32_t arrayIndexEnd = arrayIndexStart + info->arraySize;

            for (uint32_t arrayIndex = arrayIndexStart; arrayIndex < arrayIndexEnd; ++arrayIndex)
            {
                writeIntConstant(ids, arrayIndex, ids.intId(), blobOut);
            }
        }
    }

    mBufferStridesCompositeId = SpirvTransformerBase::GetNewId(blobOut);
    spirv::WriteConstantComposite(blobOut, mIVec4Id, mBufferStridesCompositeId, compositeIds);
}

void SpirvTransformFeedbackCodeGenerator::writeTransformFeedbackExtensionOutput(
    const SpirvIDDiscoverer &ids,
    spirv::IdRef positionId,
    spirv::Blob *blobOut)
{
    if (mIsEmulated)
    {
        return;
    }

    if (mTransformFeedbackExtensionPositionId.valid())
    {
        spirv::WriteStore(blobOut, mTransformFeedbackExtensionPositionId, positionId, nullptr);
    }
}

class AccessChainIndexListAppend final : angle::NonCopyable
{
  public:
    AccessChainIndexListAppend(bool condition,
                               angle::FastVector<spirv::IdRef, 4> intNIds,
                               uint32_t index,
                               spirv::IdRefList *indexList)
        : mCondition(condition), mIndexList(indexList)
    {
        if (mCondition)
        {
            mIndexList->push_back(intNIds[index]);
        }
    }
    ~AccessChainIndexListAppend()
    {
        if (mCondition)
        {
            mIndexList->pop_back();
        }
    }

  private:
    bool mCondition;
    spirv::IdRefList *mIndexList;
};

void SpirvTransformFeedbackCodeGenerator::writeTransformFeedbackEmulationOutput(
    const SpirvIDDiscoverer &ids,
    const SpirvInactiveVaryingRemover &inactiveVaryingRemover,
    spirv::IdRef currentFunctionId,
    spirv::Blob *blobOut)
{
    if (!mIsEmulated || !mXfbCaptureFuncId.valid() || currentFunctionId != mXfbCaptureFuncId)
    {
        return;
    }

    // First, sort the varyings by offset, to simplify calculation of the output offset.
    for (std::vector<XfbVarying> &varyings : mXfbVaryings)
    {
        std::sort(varyings.begin(), varyings.end(),
                  [](const XfbVarying &first, const XfbVarying &second) {
                      return first.info->offset < second.info->offset;
                  });
    }

    // The following code is generated for transform feedback emulation:
    //
    //     ivec4 xfbOffsets = ANGLEGetXfbOffsets(ivec4(stride0, stride1, stride2, stride3));
    //     // For buffer N:
    //     int xfbOffset = xfbOffsets[N]
    //     ANGLEXfbN.xfbOut[xfbOffset] = tfVarying0.field[index][row][col]
    //     xfbOffset += 1;
    //     ANGLEXfbN.xfbOut[xfbOffset] = tfVarying0.field[index][row][col + 1]
    //     xfbOffset += 1;
    //     ...
    //
    // The following pieces of SPIR-V code are generated according to the above:
    //
    // - For the initial offsets calculation:
    //
    //    %xfbOffsetsResult = OpFunctionCall %ivec4 %ANGLEGetXfbOffsets %stridesComposite
    //       %xfbOffsetsVar = OpVariable %mIVec4FuncPointerId Function
    //                        OpStore %xfbOffsetsVar %xfbOffsetsResult
    //          %xfbOffsets = OpLoad %ivec4 %xfbOffsetsVar
    //
    // - Initial code for each buffer N:
    //
    //           %xfbOffset = OpCompositeExtract %int %xfbOffsets N
    //
    // - For each varying being captured:
    //
    //                        // Load the component
    //        %componentPtr = OpAccessChain %floatOutputPtr %baseId %field %arrayIndex %row %col
    //           %component = OpLoad %float %componentPtr
    //                        // Store in xfb output
    //           %xfbOutPtr = OpAccessChain %floatUniformPtr %xfbBufferN %int0 %xfbOffset
    //                        OpStore %xfbOutPtr %component
    //                        // Increment offset
    //           %xfbOffset = OpIAdd %int %xfbOffset %int1
    //
    //   Note that if the varying being captured is integer, the first two instructions above would
    //   use the intger equivalent types, and the following instruction would bitcast it to float
    //   for storage:
    //
    //             %asFloat = OpBitcast %float %component
    //

    const spirv::IdRef xfbOffsets(SpirvTransformerBase::GetNewId(blobOut));

    // ivec4 xfbOffsets = ANGLEGetXfbOffsets(ivec4(stride0, stride1, stride2, stride3));
    writeGetOffsetsCall(xfbOffsets, blobOut);

    // Go over the buffers one by one and capture the varyings.
    for (uint32_t bufferIndex = 0; bufferIndex < mXfbVaryings.size(); ++bufferIndex)
    {
        spirv::IdRef xfbOffset(SpirvTransformerBase::GetNewId(blobOut));

        // Get the offset corresponding to this buffer:
        //
        //     int xfbOffset = xfbOffsets[N]
        spirv::WriteCompositeExtract(blobOut, ids.intId(), xfbOffset, xfbOffsets,
                                     {spirv::LiteralInteger(bufferIndex)});

        // Track offsets for verification.
        uint32_t offsetForVerification = 0;

        // Go over the varyings of this buffer in order.
        const std::vector<XfbVarying> &varyings = mXfbVaryings[bufferIndex];
        for (size_t varyingIndex = 0; varyingIndex < varyings.size(); ++varyingIndex)
        {
            const XfbVarying &varying                  = varyings[varyingIndex];
            const ShaderInterfaceVariableXfbInfo *info = varying.info;
            ASSERT(info->buffer == bufferIndex);

            // Each component of the varying being captured is loaded one by one.  This uses the
            // OpAccessChain instruction that takes a chain of "indices" to end up with the
            // component starting from the base variable.  For example:
            //
            //     var.member[3][2][0]
            //
            // where member is field number 4 in var and is a mat4, the access chain would be:
            //
            //     4 3 2 0
            //     ^ ^ ^ ^
            //     | | | |
            //     | | | row 0
            //     | | column 2
            //     | array element 3
            //     field 4
            //
            // The following tracks the access chain as the field, array elements, columns and rows
            // are looped over.
            spirv::IdRefList indexList;
            AccessChainIndexListAppend appendField(
                varying.fieldIndex != ShaderInterfaceVariableXfbInfo::kInvalid, mIntNIds,
                varying.fieldIndex, &indexList);

            // The varying being captured is either:
            //
            // - Not an array: In this case, no entry is added in the access chain
            // - An element of the array
            // - The whole array
            //
            uint32_t arrayIndexStart = 0;
            uint32_t arrayIndexEnd   = info->arraySize;
            const bool isArray       = info->arraySize != ShaderInterfaceVariableXfbInfo::kInvalid;
            if (varying.info->arrayIndex != ShaderInterfaceVariableXfbInfo::kInvalid)
            {
                // Capturing a single element.
                arrayIndexStart = varying.info->arrayIndex;
                arrayIndexEnd   = arrayIndexStart + 1;
            }
            else if (!isArray)
            {
                // Not an array.
                arrayIndexEnd = 1;
            }

            // Sorting the varyings should have ensured that offsets are in order and that writing
            // to the output buffer sequentially ends up using the correct offsets.
            ASSERT(info->offset == offsetForVerification);
            offsetForVerification += (arrayIndexEnd - arrayIndexStart) * info->rowCount *
                                     info->columnCount * sizeof(float);

            // Determine the type of the component being captured.  OpBitcast is used (the
            // implementation of intBitsToFloat() and uintBitsToFloat() for non-float types).
            spirv::IdRef varyingTypeId;
            spirv::IdRef varyingTypePtr;
            const bool isPrivate = inactiveVaryingRemover.isInactive(varying.baseId);
            getVaryingTypeIds(ids, info->componentType, isPrivate, &varyingTypeId, &varyingTypePtr);

            for (uint32_t arrayIndex = arrayIndexStart; arrayIndex < arrayIndexEnd; ++arrayIndex)
            {
                AccessChainIndexListAppend appendArrayIndex(isArray, mIntNIds, arrayIndex,
                                                            &indexList);
                for (uint32_t col = 0; col < info->columnCount; ++col)
                {
                    AccessChainIndexListAppend appendColumn(info->columnCount > 1, mIntNIds, col,
                                                            &indexList);
                    for (uint32_t row = 0; row < info->rowCount; ++row)
                    {
                        AccessChainIndexListAppend appendRow(info->rowCount > 1, mIntNIds, row,
                                                             &indexList);

                        // Generate the code to capture a single component of the varying:
                        //
                        //     ANGLEXfbN.xfbOut[xfbOffset] = tfVarying0.field[index][row][col]
                        writeComponentCapture(ids, bufferIndex, xfbOffset, varyingTypeId,
                                              varyingTypePtr, varying.baseId, indexList,
                                              info->componentType, blobOut);

                        // Increment the offset:
                        //
                        //     xfbOffset += 1;
                        //
                        // which translates to:
                        //
                        //     %newOffsetId = OpIAdd %int %currentOffsetId %int1
                        spirv::IdRef nextOffset(SpirvTransformerBase::GetNewId(blobOut));
                        spirv::WriteIAdd(blobOut, ids.intId(), nextOffset, xfbOffset, mIntNIds[1]);
                        xfbOffset = nextOffset;
                    }
                }
            }
        }
    }
}

void SpirvTransformFeedbackCodeGenerator::getVaryingTypeIds(const SpirvIDDiscoverer &ids,
                                                            GLenum componentType,
                                                            bool isPrivate,
                                                            spirv::IdRef *typeIdOut,
                                                            spirv::IdRef *typePtrOut)
{
    switch (componentType)
    {
        case GL_INT:
            *typeIdOut  = ids.intId();
            *typePtrOut = isPrivate ? mIntPrivatePointerId : mIntOutputPointerId;
            break;
        case GL_UNSIGNED_INT:
            *typeIdOut  = ids.uintId();
            *typePtrOut = isPrivate ? mUintPrivatePointerId : mUintOutputPointerId;
            break;
        case GL_FLOAT:
            *typeIdOut  = ids.floatId();
            *typePtrOut = isPrivate ? mFloatPrivatePointerId : mFloatOutputPointerId;
            break;
        default:
            UNREACHABLE();
    }

    ASSERT(typeIdOut->valid());
    ASSERT(typePtrOut->valid());
}

void SpirvTransformFeedbackCodeGenerator::writeGetOffsetsCall(spirv::IdRef xfbOffsets,
                                                              spirv::Blob *blobOut)
{
    const spirv::IdRef xfbOffsetsResult(SpirvTransformerBase::GetNewId(blobOut));
    const spirv::IdRef xfbOffsetsVar(SpirvTransformerBase::GetNewId(blobOut));

    // Generate code for the following:
    //
    //     ivec4 xfbOffsets = ANGLEGetXfbOffsets(ivec4(stride0, stride1, stride2, stride3));

    // Create a variable to hold the result.
    spirv::WriteVariable(blobOut, mIVec4FuncPointerId, xfbOffsetsVar, spv::StorageClassFunction,
                         nullptr);
    // Call a helper function generated by the translator to calculate the offsets for the current
    // vertex.
    spirv::WriteFunctionCall(blobOut, mIVec4Id, xfbOffsetsResult, mGetXfbOffsetsFuncId,
                             {mBufferStridesCompositeId});
    // Store the results.
    spirv::WriteStore(blobOut, xfbOffsetsVar, xfbOffsetsResult, nullptr);
    // Load from the variable for use in expressions.
    spirv::WriteLoad(blobOut, mIVec4Id, xfbOffsets, xfbOffsetsVar, nullptr);
}

void SpirvTransformFeedbackCodeGenerator::writeComponentCapture(
    const SpirvIDDiscoverer &ids,
    uint32_t bufferIndex,
    spirv::IdRef xfbOffset,
    spirv::IdRef varyingTypeId,
    spirv::IdRef varyingTypePtr,
    spirv::IdRef varyingBaseId,
    const spirv::IdRefList &accessChainIndices,
    GLenum componentType,
    spirv::Blob *blobOut)
{
    spirv::IdRef component(SpirvTransformerBase::GetNewId(blobOut));
    spirv::IdRef xfbOutPtr(SpirvTransformerBase::GetNewId(blobOut));

    // Generate code for the following:
    //
    //     ANGLEXfbN.xfbOut[xfbOffset] = tfVarying0.field[index][row][col]

    // Load from the component traversing the base variable with the given indices.  If there are no
    // indices, the variable can be loaded directly.
    spirv::IdRef loadPtr = varyingBaseId;
    if (!accessChainIndices.empty())
    {
        loadPtr = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteAccessChain(blobOut, varyingTypePtr, loadPtr, varyingBaseId,
                                accessChainIndices);
    }
    spirv::WriteLoad(blobOut, varyingTypeId, component, loadPtr, nullptr);

    // If the varying is int or uint, bitcast it to float to store in the float[] array used to
    // capture transform feedback output.
    spirv::IdRef asFloat = component;
    if (componentType != GL_FLOAT)
    {
        asFloat = SpirvTransformerBase::GetNewId(blobOut);
        spirv::WriteBitcast(blobOut, ids.floatId(), asFloat, component);
    }

    // Store into the transform feedback capture buffer at the current offset.  Note that this
    // buffer has only one field (xfbOut), hence ANGLEXfbN.xfbOut[xfbOffset] translates to ANGLEXfbN
    // with access chain {0, xfbOffset}.
    spirv::WriteAccessChain(blobOut, mFloatUniformPointerId, xfbOutPtr, mXfbBuffers[bufferIndex],
                            {mIntNIds[0], xfbOffset});
    spirv::WriteStore(blobOut, xfbOutPtr, asFloat, nullptr);
}

void SpirvTransformFeedbackCodeGenerator::addExecutionMode(spirv::IdRef entryPointId,
                                                           spirv::Blob *blobOut)
{
    if (mIsEmulated)
    {
        return;
    }

    if (mHasTransformFeedbackOutput)
    {
        spirv::WriteExecutionMode(blobOut, entryPointId, spv::ExecutionModeXfb, {});
    }
}

void SpirvTransformFeedbackCodeGenerator::addMemberDecorate(const ShaderInterfaceVariableInfo &info,
                                                            spirv::IdRef id,
                                                            spirv::Blob *blobOut)
{
    if (mIsEmulated || info.fieldXfb.empty())
    {
        return;
    }

    for (uint32_t fieldIndex = 0; fieldIndex < info.fieldXfb.size(); ++fieldIndex)
    {
        const ShaderInterfaceVariableXfbInfo &xfb = info.fieldXfb[fieldIndex];

        if (xfb.buffer == ShaderInterfaceVariableXfbInfo::kInvalid)
        {
            continue;
        }

        ASSERT(xfb.stride != ShaderInterfaceVariableXfbInfo::kInvalid);
        ASSERT(xfb.offset != ShaderInterfaceVariableXfbInfo::kInvalid);

        const uint32_t xfbDecorationValues[kXfbDecorationCount] = {
            xfb.buffer,
            xfb.stride,
            xfb.offset,
        };

        // Generate the following three instructions:
        //
        //     OpMemberDecorate %id fieldIndex XfbBuffer xfb.buffer
        //     OpMemberDecorate %id fieldIndex XfbStride xfb.stride
        //     OpMemberDecorate %id fieldIndex Offset xfb.offset
        for (size_t i = 0; i < kXfbDecorationCount; ++i)
        {
            spirv::WriteMemberDecorate(blobOut, id, spirv::LiteralInteger(fieldIndex),
                                       kXfbDecorations[i],
                                       {spirv::LiteralInteger(xfbDecorationValues[i])});
        }
    }
}

void SpirvTransformFeedbackCodeGenerator::addDecorate(const ShaderInterfaceVariableInfo &info,
                                                      spirv::IdRef id,
                                                      spirv::Blob *blobOut)
{
    if (mIsEmulated || info.xfb.buffer == ShaderInterfaceVariableXfbInfo::kInvalid)
    {
        return;
    }

    ASSERT(info.xfb.stride != ShaderInterfaceVariableXfbInfo::kInvalid);
    ASSERT(info.xfb.offset != ShaderInterfaceVariableXfbInfo::kInvalid);

    const uint32_t xfbDecorationValues[kXfbDecorationCount] = {
        info.xfb.buffer,
        info.xfb.stride,
        info.xfb.offset,
    };

    // Generate the following three instructions:
    //
    //     OpDecorate %id XfbBuffer xfb.buffer
    //     OpDecorate %id XfbStride xfb.stride
    //     OpDecorate %id Offset xfb.offset
    for (size_t i = 0; i < kXfbDecorationCount; ++i)
    {
        spirv::WriteDecorate(blobOut, id, kXfbDecorations[i],
                             {spirv::LiteralInteger(xfbDecorationValues[i])});
    }
}

// Helper class that generates code for gl_Position transformations
class SpirvPositionTransformer final : angle::NonCopyable
{
  public:
    SpirvPositionTransformer(const GlslangSpirvOptions &options) : mOptions(options) {}

    void visitName(spirv::IdRef id, const spirv::LiteralString &name);

    void writePositionTransformation(const SpirvIDDiscoverer &ids,
                                     spirv::IdRef positionPointerId,
                                     spirv::IdRef positionId,
                                     spirv::Blob *blobOut);

  private:
    GlslangSpirvOptions mOptions;

    spirv::IdRef mTransformPositionFuncId;
};

void SpirvPositionTransformer::visitName(spirv::IdRef id, const spirv::LiteralString &name)
{
    if (angle::BeginsWith(name, sh::vk::kTransformPositionFunctionName))
    {
        ASSERT(!mTransformPositionFuncId.valid());
        mTransformPositionFuncId = id;
    }
}

void SpirvPositionTransformer::writePositionTransformation(const SpirvIDDiscoverer &ids,
                                                           spirv::IdRef positionPointerId,
                                                           spirv::IdRef positionId,
                                                           spirv::Blob *blobOut)
{
    // Generate the following SPIR-V for prerotation and depth transformation:
    //
    //     // Transform position based on uniforms by making a call to the ANGLETransformPosition
    //     // function that the translator has already provided.
    //     %transformed = OpFunctionCall %mVec4Id %mTransformPositionFuncId %position
    //
    //     // Store the results back in gl_Position
    //     OpStore %PositionPointer %transformedPosition
    //
    const spirv::IdRef transformedPositionId(SpirvTransformerBase::GetNewId(blobOut));

    spirv::WriteFunctionCall(blobOut, ids.vec4Id(), transformedPositionId, mTransformPositionFuncId,
                             {positionId});
    spirv::WriteStore(blobOut, positionPointerId, transformedPositionId, nullptr);
}

class SpirvMultiSampleTransformer final : angle::NonCopyable
{
  public:
    SpirvMultiSampleTransformer(const GlslangSpirvOptions &options)
        : mOptions(options),
          mIsSampleRateShadingCapabilityEnabled(false),
          mSampleIDExists(false),
          mAnyImageTypesModified(false)
    {}
    ~SpirvMultiSampleTransformer()
    {
        ASSERT(!mOptions.isMultisampledFramebufferFetch || mAnyImageTypesModified);
    }

    void visitCapability(const uint32_t *instruction);

    void visitDecorate(const spirv::IdRef &id,
                       const spv::Decoration decoration,
                       const spirv::LiteralIntegerList &valueList);

    void visitEntryPoint(const uint32_t *instruction);

    TransformationState transformCapability(const spv::Capability capability, spirv::Blob *blobOut);

    TransformationState transformTypeImage(const uint32_t *instruction, spirv::Blob *blobOut);

    void modifyEntryPointInterfaceList(spirv::IdRefList *interfaceList, spirv::Blob *blobOut);

    void writePendingDeclarations(
        const std::vector<const ShaderInterfaceVariableInfo *> &variableInfoById,
        SpirvIDDiscoverer &ids,
        spirv::Blob *blobOut);

    TransformationState transformDecoration(const spv::Decoration &decoration,
                                            spirv::Blob *blobOut);

    TransformationState transformImageRead(const uint32_t *instruction,
                                           const SpirvIDDiscoverer &ids,
                                           spirv::Blob *blobOut);

  private:
    GlslangSpirvOptions mOptions;
    spirv::IdRef mBuiltInGLSampleID;
    spirv::IdRef mIntInputPointerId;
    bool mIsSampleRateShadingCapabilityEnabled;
    bool mSampleIDExists;
    // Used to assert that the transformation is not unnecessarily run.
    bool mAnyImageTypesModified;
};

TransformationState SpirvMultiSampleTransformer::transformImageRead(const uint32_t *instruction,
                                                                    const SpirvIDDiscoverer &ids,
                                                                    spirv::Blob *blobOut)
{
    // Transform the following:
    // %21 = OpImageRead %v4float %13 %20
    // to
    // %21 = OpImageRead %v4float %13 %20 Sample %17
    // where
    // %17 = OpLoad %int %gl_SampleID

    if (!mOptions.isMultisampledFramebufferFetch)
    {
        return TransformationState::Unchanged;
    }

    ASSERT(mBuiltInGLSampleID.valid());

    spirv::IdResultType idResultType;
    spirv::IdResult idResult;
    spirv::IdRef image;
    spirv::IdRef coordinate;
    spv::ImageOperandsMask imageOperands;
    spirv::IdRefList imageOperandIdsList;

    spirv::ParseImageRead(instruction, &idResultType, &idResult, &image, &coordinate,
                          &imageOperands, &imageOperandIdsList);

    ASSERT(ids.intId().valid());

    spirv::IdRef builtInSampleIDOpLoad = SpirvTransformerBase::GetNewId(blobOut);

    spirv::WriteLoad(blobOut, ids.intId(), builtInSampleIDOpLoad, mBuiltInGLSampleID, nullptr);

    imageOperands = spv::ImageOperandsMask::ImageOperandsSampleMask;
    imageOperandIdsList.push_back(builtInSampleIDOpLoad);
    spirv::WriteImageRead(blobOut, idResultType, idResult, image, coordinate, &imageOperands,
                          imageOperandIdsList);
    return TransformationState::Transformed;
}

void SpirvMultiSampleTransformer::writePendingDeclarations(
    const std::vector<const ShaderInterfaceVariableInfo *> &variableInfoById,
    SpirvIDDiscoverer &ids,
    spirv::Blob *blobOut)
{
    // Add following declarations if they are not available yet

    // %int = OpTypeInt 32 1
    // %_ptr_Input_int = OpTypePointer Input %int
    // %gl_SampleID = OpVariable %_ptr_Input_int Input

    if (!mOptions.isMultisampledFramebufferFetch)
    {
        return;
    }

    if (mSampleIDExists)
    {
        return;
    }

    if (!ids.intId().valid())
    {
        spirv::IdResult id               = SpirvTransformerBase::GetNewId(blobOut);
        spirv::LiteralInteger width      = spirv::LiteralInteger(32);
        spirv::LiteralInteger signedness = spirv::LiteralInteger(1);
        ids.visitTypeInt(id, width, signedness);
        spirv::WriteTypeInt(blobOut, id, spirv::LiteralInteger(32), spirv::LiteralInteger(1));
    }
    ASSERT(ids.intId().valid());
    mIntInputPointerId = SpirvTransformerBase::GetNewId(blobOut);
    spirv::WriteTypePointer(blobOut, mIntInputPointerId, spv::StorageClassInput, ids.intId());
    ASSERT(mIntInputPointerId.valid());
    ASSERT(mBuiltInGLSampleID.valid());

    spirv::WriteVariable(blobOut, mIntInputPointerId, mBuiltInGLSampleID, spv::StorageClassInput,
                         nullptr);
}

TransformationState SpirvMultiSampleTransformer::transformTypeImage(const uint32_t *instruction,
                                                                    spirv::Blob *blobOut)
{
    // Transform the following
    // %10 = OpTypeImage %float SubpassData 0 0 0 2
    // To
    // %10 = OpTypeImage %float SubpassData 0 0 1 2

    if (!mOptions.isMultisampledFramebufferFetch)
    {
        return TransformationState::Unchanged;
    }

    spirv::IdResult idResult;
    spirv::IdRef sampledType;
    spv::Dim dim;
    spirv::LiteralInteger depth;
    spirv::LiteralInteger arrayed;
    spirv::LiteralInteger ms;
    spirv::LiteralInteger sampled;
    spv::ImageFormat imageFormat;
    spv::AccessQualifier accessQualifier;
    spirv::ParseTypeImage(instruction, &idResult, &sampledType, &dim, &depth, &arrayed, &ms,
                          &sampled, &imageFormat, &accessQualifier);

    // Only transform input attachment image types.
    if (dim != spv::DimSubpassData)
    {
        return TransformationState::Unchanged;
    }

    ms = spirv::LiteralInteger(1);
    spirv::WriteTypeImage(blobOut, idResult, sampledType, dim, depth, arrayed, ms, sampled,
                          imageFormat, nullptr);

    mAnyImageTypesModified = true;

    return TransformationState::Transformed;
}

namespace
{
bool verifyEntryPointsContainsID(const spirv::IdRefList &interfaceList, const spirv::IdRef &id)
{
    for (spirv::IdRef interfaceId : interfaceList)
    {
        if (interfaceId == id)
        {
            return true;
        }
    }
    return false;
}
}  // namespace

void SpirvMultiSampleTransformer::modifyEntryPointInterfaceList(spirv::IdRefList *interfaceList,
                                                                spirv::Blob *blobOut)
{
    // Append %gl_sampleID to OpEntryPoint
    // Transform the following
    // OpEntryPoint Fragment %main "main" %_uo_color
    // To
    // OpEntryPoint Fragment %main "main" %_uo_color %gl_SampleID

    if (!mOptions.isMultisampledFramebufferFetch)
    {
        return;
    }

    // First check if the shader as the gl_SampleID append to the EntryPoint
    if (mSampleIDExists)
    {
        ASSERT(verifyEntryPointsContainsID(*interfaceList, mBuiltInGLSampleID));
        return;
    }

    // If the pre transform SPIRV doesn't have an id for gl_SampleID, check that we haven't
    // generated an id for gl_SampleID
    ASSERT(!mBuiltInGLSampleID.valid());

    // Generate a new id for gl_SampleID
    mBuiltInGLSampleID = SpirvTransformerBase::GetNewId(blobOut);

    // Add the generated id to the interfaceList
    interfaceList->push_back(mBuiltInGLSampleID);
    return;
}

TransformationState SpirvMultiSampleTransformer::transformCapability(
    const spv::Capability capability,
    spirv::Blob *blobOut)
{
    // Add a new OpCapability line: OpCapability SampleRateShading
    // right before the following instruction
    // OpCapability InputAttachment

    if (!mOptions.isMultisampledFramebufferFetch)
    {
        return TransformationState::Unchanged;
    }

    // If we already have this line "OpCapability SampleRateShading"
    // Do not add a duplicated line. Return.
    if (mIsSampleRateShadingCapabilityEnabled)
    {
        return TransformationState::Unchanged;
    }

    // Make sure no duplicates
    ASSERT(capability != spv::CapabilitySampleRateShading);

    // Make sure we only add the new line on top of "OpCapability InputAttachment"
    if (capability != spv::CapabilityInputAttachment)
    {
        return TransformationState::Unchanged;
    }

    spirv::WriteCapability(blobOut, spv::CapabilitySampleRateShading);
    mIsSampleRateShadingCapabilityEnabled = true;

    // Leave the next line "OpCapability InputAttachment untouched"
    return TransformationState::Unchanged;
}

TransformationState SpirvMultiSampleTransformer::transformDecoration(
    const spv::Decoration &decoration,
    spirv::Blob *blobOut)
{
    // Add the following declarations if they are not available yet:
    // OpDecorate %gl_SampleID RelaxedPrecision
    // OpDecorate %gl_SampleID Flat
    // OpDecorate %gl_SampleID BuiltIn SampleId

    if (!mOptions.isMultisampledFramebufferFetch ||
        decoration != spv::DecorationInputAttachmentIndex)
    {
        return TransformationState::Unchanged;
    }

    if (mSampleIDExists)
    {
        return TransformationState::Unchanged;
    }

    ASSERT(mBuiltInGLSampleID.valid());

    spirv::WriteDecorate(blobOut, mBuiltInGLSampleID, spv::DecorationRelaxedPrecision, {});
    spirv::WriteDecorate(blobOut, mBuiltInGLSampleID, spv::DecorationFlat, {});
    spirv::WriteDecorate(blobOut, mBuiltInGLSampleID, spv::DecorationBuiltIn,
                         {spirv::LiteralInteger(spv::BuiltIn::BuiltInSampleId)});
    return TransformationState::Unchanged;
}

void SpirvMultiSampleTransformer::visitDecorate(const spirv::IdRef &id,
                                                const spv::Decoration decoration,
                                                const spirv::LiteralIntegerList &valueList)
{
    if (!mOptions.isMultisampledFramebufferFetch)
    {
        return;
    }

    if (decoration == spv::DecorationBuiltIn)
    {
        if (valueList[0] == spv::BuiltInSampleId)
        {
            mBuiltInGLSampleID = id;
            mSampleIDExists    = true;
        }
    }
    return;
}

void SpirvMultiSampleTransformer::visitCapability(const uint32_t *instruction)
{
    if (!mOptions.isMultisampledFramebufferFetch)
    {
        return;
    }
    spv::Capability capability;
    spirv::ParseCapability(instruction, &capability);
    if (capability == spv::CapabilitySampleRateShading)
    {
        mIsSampleRateShadingCapabilityEnabled = true;
    }
}

// A SPIR-V transformer.  It walks the instructions and modifies them as necessary, for example to
// assign bindings or locations.
class SpirvTransformer final : public SpirvTransformerBase
{
  public:
    SpirvTransformer(const spirv::Blob &spirvBlobIn,
                     const GlslangSpirvOptions &options,
                     const ShaderInterfaceVariableInfoMap &variableInfoMap,
                     spirv::Blob *spirvBlobOut)
        : SpirvTransformerBase(spirvBlobIn, variableInfoMap, spirvBlobOut),
          mOptions(options),
          mXfbCodeGenerator(options.isTransformFeedbackEmulated),
          mPositionTransformer(options),
          mMultiSampleTransformer(options)
    {}

    void transform();

  private:
    // A prepass to resolve interesting ids:
    void resolveVariableIds();

    // Transform instructions:
    void transformInstruction();

    // Instructions that are purely informational:
    void visitDecorate(const uint32_t *instruction);
    void visitName(const uint32_t *instruction);
    void visitMemberName(const uint32_t *instruction);
    void visitTypeArray(const uint32_t *instruction);
    void visitTypeFloat(const uint32_t *instruction);
    void visitTypeInt(const uint32_t *instruction);
    void visitTypePointer(const uint32_t *instruction);
    void visitTypeVector(const uint32_t *instruction);
    void visitVariable(const uint32_t *instruction);
    void visitCapability(const uint32_t *instruction);

    // Instructions that potentially need transformation.  They return true if the instruction is
    // transformed.  If false is returned, the instruction should be copied as-is.
    TransformationState transformAccessChain(const uint32_t *instruction);
    TransformationState transformCapability(const uint32_t *instruction);
    TransformationState transformDebugInfo(const uint32_t *instruction, spv::Op op);
    TransformationState transformEmitVertex(const uint32_t *instruction);
    TransformationState transformEntryPoint(const uint32_t *instruction);
    TransformationState transformDecorate(const uint32_t *instruction);
    TransformationState transformMemberDecorate(const uint32_t *instruction);
    TransformationState transformTypePointer(const uint32_t *instruction);
    TransformationState transformTypeStruct(const uint32_t *instruction);
    TransformationState transformReturn(const uint32_t *instruction);
    TransformationState transformVariable(const uint32_t *instruction);
    TransformationState transformTypeImage(const uint32_t *instruction);
    TransformationState transformImageRead(const uint32_t *instruction);

    // Helpers:
    void visitTypeHelper(spirv::IdResult id, spirv::IdRef typeId);
    void writePendingDeclarations();
    void writeOutputPrologue();

    // Special flags:
    GlslangSpirvOptions mOptions;

    // Traversal state:
    spirv::IdRef mEntryPointId;
    spirv::IdRef mCurrentFunctionId;

    SpirvIDDiscoverer mIds;

    // Transformation state:

    SpirvPerVertexTrimmer mPerVertexTrimmer;
    SpirvInactiveVaryingRemover mInactiveVaryingRemover;
    SpirvTransformFeedbackCodeGenerator mXfbCodeGenerator;
    SpirvPositionTransformer mPositionTransformer;
    SpirvMultiSampleTransformer mMultiSampleTransformer;
};

void SpirvTransformer::transform()
{
    onTransformBegin();

    // First, find all necessary ids and associate them with the information required to transform
    // their decorations.
    resolveVariableIds();

    while (mCurrentWord < mSpirvBlobIn.size())
    {
        transformInstruction();
    }
}

void SpirvTransformer::resolveVariableIds()
{
    const size_t indexBound = mSpirvBlobIn[spirv::kHeaderIndexIndexBound];

    mIds.init(indexBound);
    mInactiveVaryingRemover.init(indexBound);

    // Allocate storage for id-to-info map.  If %i is the id of a name in mVariableInfoMap, index i
    // in this vector will hold a pointer to the ShaderInterfaceVariableInfo object associated with
    // that name in mVariableInfoMap.
    mVariableInfoById.resize(indexBound, nullptr);

    size_t currentWord = spirv::kHeaderIndexInstructions;

    while (currentWord < mSpirvBlobIn.size())
    {
        const uint32_t *instruction = &mSpirvBlobIn[currentWord];

        uint32_t wordCount;
        spv::Op opCode;
        spirv::GetInstructionOpAndLength(instruction, &opCode, &wordCount);

        switch (opCode)
        {
            case spv::OpCapability:
                visitCapability(instruction);
                break;
            case spv::OpDecorate:
                visitDecorate(instruction);
                break;
            case spv::OpName:
                visitName(instruction);
                break;
            case spv::OpMemberName:
                visitMemberName(instruction);
                break;
            case spv::OpTypeArray:
                visitTypeArray(instruction);
                break;
            case spv::OpTypeFloat:
                visitTypeFloat(instruction);
                break;
            case spv::OpTypeInt:
                visitTypeInt(instruction);
                break;
            case spv::OpTypePointer:
                visitTypePointer(instruction);
                break;
            case spv::OpTypeVector:
                visitTypeVector(instruction);
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
    UNREACHABLE();
}

void SpirvTransformer::transformInstruction()
{
    uint32_t wordCount;
    spv::Op opCode;
    const uint32_t *instruction = getCurrentInstruction(&opCode, &wordCount);

    if (opCode == spv::OpFunction)
    {
        spirv::IdResultType id;
        spv::FunctionControlMask functionControl;
        spirv::IdRef functionType;
        spirv::ParseFunction(instruction, &id, &mCurrentFunctionId, &functionControl,
                             &functionType);

        // SPIR-V is structured in sections.  Function declarations come last.  Only a few
        // instructions such as Op*Access* or OpEmitVertex opcodes inside functions need to be
        // inspected.
        //
        // If this is the first OpFunction instruction, this is also where the declaration section
        // finishes, so we need to declare anything that we need but didn't find there already right
        // now.
        if (!mIsInFunctionSection)
        {
            writePendingDeclarations();
        }
        mIsInFunctionSection = true;
    }

    // Only look at interesting instructions.
    TransformationState transformationState = TransformationState::Unchanged;

    if (mIsInFunctionSection)
    {
        // Look at in-function opcodes.
        switch (opCode)
        {
            case spv::OpAccessChain:
            case spv::OpInBoundsAccessChain:
            case spv::OpPtrAccessChain:
            case spv::OpInBoundsPtrAccessChain:
                transformationState = transformAccessChain(instruction);
                break;
            case spv::OpImageRead:
                transformationState = transformImageRead(instruction);
                break;
            case spv::OpEmitVertex:
                transformationState = transformEmitVertex(instruction);
                break;
            case spv::OpReturn:
                transformationState = transformReturn(instruction);
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
            case spv::OpName:
            case spv::OpMemberName:
            case spv::OpString:
            case spv::OpLine:
            case spv::OpNoLine:
            case spv::OpModuleProcessed:
                transformationState = transformDebugInfo(instruction, opCode);
                break;
            case spv::OpCapability:
                transformationState = transformCapability(instruction);
                break;
            case spv::OpEntryPoint:
                transformationState = transformEntryPoint(instruction);
                break;
            case spv::OpDecorate:
                transformationState = transformDecorate(instruction);
                break;
            case spv::OpMemberDecorate:
                transformationState = transformMemberDecorate(instruction);
                break;
            case spv::OpTypeImage:
                transformationState = transformTypeImage(instruction);
                break;
            case spv::OpTypePointer:
                transformationState = transformTypePointer(instruction);
                break;
            case spv::OpTypeStruct:
                transformationState = transformTypeStruct(instruction);
                break;
            case spv::OpVariable:
                transformationState = transformVariable(instruction);
                break;
            default:
                break;
        }
    }

    // If the instruction was not transformed, copy it to output as is.
    if (transformationState == TransformationState::Unchanged)
    {
        copyInstruction(instruction, wordCount);
    }

    // Advance to next instruction.
    mCurrentWord += wordCount;
}

// Called at the end of the declarations section.  Any declarations that are necessary but weren't
// present in the original shader need to be done here.
void SpirvTransformer::writePendingDeclarations()
{
    mMultiSampleTransformer.writePendingDeclarations(mVariableInfoById, mIds, mSpirvBlobOut);

    // Pre-rotation and transformation of depth to Vulkan clip space require declarations that may
    // not necessarily be in the shader.  Transform feedback emulation additionally requires a few
    // overlapping ids.
    if (!mOptions.isLastPreFragmentStage)
    {
        return;
    }

    mIds.writePendingDeclarations(mSpirvBlobOut);
    if (mOptions.isTransformFeedbackStage)
    {
        mXfbCodeGenerator.writePendingDeclarations(mVariableInfoById, mIds, mSpirvBlobOut);
    }
}

// Called by transformInstruction to insert necessary instructions for modifying gl_Position.
void SpirvTransformer::writeOutputPrologue()
{
    if (!mIds.outputPerVertexId().valid())
    {
        return;
    }

    // Whether gl_Position should be transformed to account for pre-rotation and Vulkan clip space.
    const bool transformPosition = mOptions.isLastPreFragmentStage;
    const bool isXfbExtensionStage =
        mOptions.isTransformFeedbackStage && !mOptions.isTransformFeedbackEmulated;
    if (!transformPosition && !isXfbExtensionStage)
    {
        return;
    }

    // Load gl_Position with the following SPIR-V:
    //
    //     // Create an access chain to gl_PerVertex.gl_Position, which is always at index 0.
    //     %PositionPointer = OpAccessChain %mVec4OutTypePointerId %mOutputPerVertexId %mInt0Id
    //     // Load gl_Position
    //     %Position = OpLoad %mVec4Id %PositionPointer
    //
    const spirv::IdRef positionPointerId(getNewId());
    const spirv::IdRef positionId(getNewId());

    spirv::WriteAccessChain(mSpirvBlobOut, mIds.vec4OutTypePointerId(), positionPointerId,
                            mIds.outputPerVertexId(), {mIds.int0Id()});
    spirv::WriteLoad(mSpirvBlobOut, mIds.vec4Id(), positionId, positionPointerId, nullptr);

    // Write transform feedback output before modifying gl_Position.
    if (isXfbExtensionStage)
    {
        mXfbCodeGenerator.writeTransformFeedbackExtensionOutput(mIds, positionId, mSpirvBlobOut);
    }

    if (transformPosition)
    {
        mPositionTransformer.writePositionTransformation(mIds, positionPointerId, positionId,
                                                         mSpirvBlobOut);
    }
}

void SpirvTransformer::visitCapability(const uint32_t *instruction)
{
    mMultiSampleTransformer.visitCapability(instruction);
}

void SpirvTransformer::visitDecorate(const uint32_t *instruction)
{
    spirv::IdRef id;
    spv::Decoration decoration;
    spirv::LiteralIntegerList valueList;
    spirv::ParseDecorate(instruction, &id, &decoration, &valueList);

    mIds.visitDecorate(id, decoration);

    if (mIds.isIOBlock(id))
    {
        // For I/O blocks, associate the type with the info, which is used to decorate its members
        // with transform feedback if any.
        spirv::LiteralString name = mIds.getName(id);
        ASSERT(name != nullptr);

        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.getVariableByName(mOptions.shaderType, name);
        mVariableInfoById[id] = &info;
    }

    mMultiSampleTransformer.visitDecorate(id, decoration, valueList);
}

void SpirvTransformer::visitName(const uint32_t *instruction)
{
    spirv::IdRef id;
    spirv::LiteralString name;
    spirv::ParseName(instruction, &id, &name);

    mIds.visitName(id, name);
    mXfbCodeGenerator.visitName(id, name);
    mPositionTransformer.visitName(id, name);
}

void SpirvTransformer::visitMemberName(const uint32_t *instruction)
{
    spirv::IdRef id;
    spirv::LiteralInteger member;
    spirv::LiteralString name;
    spirv::ParseMemberName(instruction, &id, &member, &name);

    if (!mVariableInfoMap.hasVariable(mOptions.shaderType, name))
    {
        return;
    }

    const ShaderInterfaceVariableInfo &info =
        mVariableInfoMap.getVariableByName(mOptions.shaderType, name);

    mIds.visitMemberName(info, id, member, name);
}

void SpirvTransformer::visitTypeArray(const uint32_t *instruction)
{
    spirv::IdResult id;
    spirv::IdRef elementType;
    spirv::IdRef length;
    spirv::ParseTypeArray(instruction, &id, &elementType, &length);

    mIds.visitTypeArray(id, elementType, length);
}

void SpirvTransformer::visitTypeFloat(const uint32_t *instruction)
{
    spirv::IdResult id;
    spirv::LiteralInteger width;
    spirv::ParseTypeFloat(instruction, &id, &width);

    mIds.visitTypeFloat(id, width);
}

void SpirvTransformer::visitTypeInt(const uint32_t *instruction)
{
    spirv::IdResult id;
    spirv::LiteralInteger width;
    spirv::LiteralInteger signedness;
    spirv::ParseTypeInt(instruction, &id, &width, &signedness);

    mIds.visitTypeInt(id, width, signedness);
}

void SpirvTransformer::visitTypePointer(const uint32_t *instruction)
{
    spirv::IdResult id;
    spv::StorageClass storageClass;
    spirv::IdRef typeId;
    spirv::ParseTypePointer(instruction, &id, &storageClass, &typeId);

    mIds.visitTypePointer(id, storageClass, typeId);
    mXfbCodeGenerator.visitTypePointer(id, storageClass, typeId);
}

void SpirvTransformer::visitTypeVector(const uint32_t *instruction)
{
    spirv::IdResult id;
    spirv::IdRef componentId;
    spirv::LiteralInteger componentCount;
    spirv::ParseTypeVector(instruction, &id, &componentId, &componentCount);

    mIds.visitTypeVector(id, componentId, componentCount);
    mXfbCodeGenerator.visitTypeVector(mIds, id, componentId, componentCount);
}

void SpirvTransformer::visitVariable(const uint32_t *instruction)
{
    spirv::IdResultType typeId;
    spirv::IdResult id;
    spv::StorageClass storageClass;
    spirv::ParseVariable(instruction, &typeId, &id, &storageClass, nullptr);

    spirv::LiteralString name;
    SpirvVariableType variableType = mIds.visitVariable(typeId, id, storageClass, &name);

    if (variableType == SpirvVariableType::Other)
    {
        return;
    }

    // The ids are unique.
    ASSERT(id < mVariableInfoById.size());
    ASSERT(mVariableInfoById[id] == nullptr);

    if (variableType == SpirvVariableType::BuiltIn)
    {
        // Make all builtins point to this no-op info.  Adding this entry allows us to ASSERT that
        // every shader interface variable is processed during the SPIR-V transformation.  This is
        // done when iterating the ids provided by OpEntryPoint.
        mVariableInfoById[id] = &mBuiltinVariableInfo;
        return;
    }

    // Every shader interface variable should have an associated data.
    const ShaderInterfaceVariableInfo &info =
        mVariableInfoMap.getVariableByName(mOptions.shaderType, name);

    // Associate the id of this name with its info.
    mVariableInfoById[id] = &info;
    if (mOptions.isTransformFeedbackStage)
    {
        mXfbCodeGenerator.visitVariable(info, mOptions.shaderType, name, typeId, id, storageClass);
    }
}

TransformationState SpirvTransformer::transformDecorate(const uint32_t *instruction)
{
    spirv::IdRef id;
    spv::Decoration decoration;
    spirv::LiteralIntegerList decorationValues;
    spirv::ParseDecorate(instruction, &id, &decoration, &decorationValues);

    ASSERT(id < mVariableInfoById.size());
    const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];

    // If variable is not a shader interface variable that needs modification, there's nothing to
    // do.
    if (info == nullptr)
    {
        return TransformationState::Unchanged;
    }

    mMultiSampleTransformer.transformDecoration(decoration, mSpirvBlobOut);

    if (mInactiveVaryingRemover.transformDecorate(*info, mOptions.shaderType, id, decoration,
                                                  decorationValues, mSpirvBlobOut) ==
        TransformationState::Transformed)
    {
        return TransformationState::Transformed;
    }

    uint32_t newDecorationValue = ShaderInterfaceVariableInfo::kInvalid;

    switch (decoration)
    {
        case spv::DecorationLocation:
            newDecorationValue = info->location;
            break;
        case spv::DecorationBinding:
            newDecorationValue = info->binding;
            break;
        case spv::DecorationDescriptorSet:
            newDecorationValue = info->descriptorSet;
            break;
        case spv::DecorationBlock:
            // If this is the Block decoration of a shader I/O block, add the transform feedback
            // decorations to its members right away.
            if (mOptions.isTransformFeedbackStage)
            {
                mXfbCodeGenerator.addMemberDecorate(*info, id, mSpirvBlobOut);
            }
            break;
        case spv::DecorationInvariant:
            spirv::WriteDecorate(mSpirvBlobOut, id, spv::DecorationInvariant, {});
            return TransformationState::Transformed;
        default:
            break;
    }

    // If the decoration is not something we care about modifying, there's nothing to do.
    if (newDecorationValue == ShaderInterfaceVariableInfo::kInvalid)
    {
        return TransformationState::Unchanged;
    }

    // Modify the decoration value.
    ASSERT(decorationValues.size() == 1);
    spirv::WriteDecorate(mSpirvBlobOut, id, decoration,
                         {spirv::LiteralInteger(newDecorationValue)});

    // If there are decorations to be added, add them right after the Location decoration is
    // encountered.
    if (decoration != spv::DecorationLocation)
    {
        return TransformationState::Transformed;
    }

    // Add component decoration, if any.
    if (info->component != ShaderInterfaceVariableInfo::kInvalid)
    {
        spirv::WriteDecorate(mSpirvBlobOut, id, spv::DecorationComponent,
                             {spirv::LiteralInteger(info->component)});
    }

    // Add index decoration, if any.
    if (info->index != ShaderInterfaceVariableInfo::kInvalid)
    {
        spirv::WriteDecorate(mSpirvBlobOut, id, spv::DecorationIndex,
                             {spirv::LiteralInteger(info->index)});
    }

    // Add Xfb decorations, if any.
    if (mOptions.isTransformFeedbackStage)
    {
        mXfbCodeGenerator.addDecorate(*info, id, mSpirvBlobOut);
    }

    return TransformationState::Transformed;
}

TransformationState SpirvTransformer::transformMemberDecorate(const uint32_t *instruction)
{
    spirv::IdRef typeId;
    spirv::LiteralInteger member;
    spv::Decoration decoration;
    spirv::ParseMemberDecorate(instruction, &typeId, &member, &decoration, nullptr);

    return mPerVertexTrimmer.transformMemberDecorate(mIds, typeId, member, decoration);
}

TransformationState SpirvTransformer::transformCapability(const uint32_t *instruction)
{
    spv::Capability capability;
    spirv::ParseCapability(instruction, &capability);

    TransformationState xfbTransformState =
        mXfbCodeGenerator.transformCapability(capability, mSpirvBlobOut);
    ASSERT(xfbTransformState == TransformationState::Unchanged);

    TransformationState multiSampleTransformState =
        mMultiSampleTransformer.transformCapability(capability, mSpirvBlobOut);
    ASSERT(multiSampleTransformState == TransformationState::Unchanged);

    return TransformationState::Unchanged;
}

TransformationState SpirvTransformer::transformDebugInfo(const uint32_t *instruction, spv::Op op)
{
    if (mOptions.removeDebugInfo)
    {
        // Strip debug info to reduce binary size.
        return TransformationState::Transformed;
    }

    // In the case of OpMemberName, unconditionally remove stripped gl_PerVertex members.
    if (op == spv::OpMemberName)
    {
        spirv::IdRef id;
        spirv::LiteralInteger member;
        spirv::LiteralString name;
        spirv::ParseMemberName(instruction, &id, &member, &name);

        return mPerVertexTrimmer.transformMemberName(mIds, id, member, name);
    }

    if (op == spv::OpName)
    {
        spirv::IdRef id;
        spirv::LiteralString name;
        spirv::ParseName(instruction, &id, &name);

        return mXfbCodeGenerator.transformName(id, name);
    }

    return TransformationState::Unchanged;
}

TransformationState SpirvTransformer::transformEmitVertex(const uint32_t *instruction)
{
    // This is only possible in geometry shaders.
    ASSERT(mOptions.shaderType == gl::ShaderType::Geometry);

    // Write the temporary variables that hold varyings data before EmitVertex().
    writeOutputPrologue();

    return TransformationState::Unchanged;
}

TransformationState SpirvTransformer::transformEntryPoint(const uint32_t *instruction)
{
    // Should only have one EntryPoint
    ASSERT(!mEntryPointId.valid());

    spv::ExecutionModel executionModel;
    spirv::LiteralString name;
    spirv::IdRefList interfaceList;
    spirv::ParseEntryPoint(instruction, &executionModel, &mEntryPointId, &name, &interfaceList);

    mInactiveVaryingRemover.modifyEntryPointInterfaceList(mVariableInfoById, mOptions.shaderType,
                                                          &interfaceList);

    mMultiSampleTransformer.modifyEntryPointInterfaceList(&interfaceList, mSpirvBlobOut);

    // Write the entry point with the inactive interface variables removed.
    spirv::WriteEntryPoint(mSpirvBlobOut, executionModel, mEntryPointId, name, interfaceList);

    // Add an OpExecutionMode Xfb instruction if necessary.
    mXfbCodeGenerator.addExecutionMode(mEntryPointId, mSpirvBlobOut);

    return TransformationState::Transformed;
}

TransformationState SpirvTransformer::transformTypePointer(const uint32_t *instruction)
{
    spirv::IdResult id;
    spv::StorageClass storageClass;
    spirv::IdRef typeId;
    spirv::ParseTypePointer(instruction, &id, &storageClass, &typeId);

    return mInactiveVaryingRemover.transformTypePointer(mIds, id, storageClass, typeId,
                                                        mSpirvBlobOut);
}

TransformationState SpirvTransformer::transformTypeStruct(const uint32_t *instruction)
{
    spirv::IdResult id;
    spirv::IdRefList memberList;
    ParseTypeStruct(instruction, &id, &memberList);

    return mPerVertexTrimmer.transformTypeStruct(mIds, id, &memberList, mSpirvBlobOut);
}

TransformationState SpirvTransformer::transformReturn(const uint32_t *instruction)
{
    if (mCurrentFunctionId != mEntryPointId)
    {
        if (mOptions.isTransformFeedbackStage)
        {
            // Transform feedback emulation is written to a designated function.  Allow its code to
            // be generated if this is the right function.
            mXfbCodeGenerator.writeTransformFeedbackEmulationOutput(
                mIds, mInactiveVaryingRemover, mCurrentFunctionId, mSpirvBlobOut);
        }

        // We only need to process the precision info when returning from the entry point function
        return TransformationState::Unchanged;
    }

    // For geometry shaders, this operations is done before every EmitVertex() instead.
    // Additionally, this transformation (which affects output varyings) doesn't apply to fragment
    // shaders.
    if (mOptions.shaderType == gl::ShaderType::Geometry ||
        mOptions.shaderType == gl::ShaderType::Fragment)
    {
        return TransformationState::Unchanged;
    }

    writeOutputPrologue();

    return TransformationState::Unchanged;
}

TransformationState SpirvTransformer::transformVariable(const uint32_t *instruction)
{
    spirv::IdResultType typeId;
    spirv::IdResult id;
    spv::StorageClass storageClass;
    spirv::ParseVariable(instruction, &typeId, &id, &storageClass, nullptr);

    const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];

    // If variable is not a shader interface variable that needs modification, there's nothing to
    // do.
    if (info == nullptr)
    {
        return TransformationState::Unchanged;
    }

    // Furthermore, if it's not an inactive varying output, there's nothing to do.  Note that
    // inactive varying inputs are already pruned by the translator.
    // However, input or output storage class for interface block will not be pruned when a shader
    // is compiled separately.
    if (info->activeStages[mOptions.shaderType])
    {
        return TransformationState::Unchanged;
    }

    if (mXfbCodeGenerator.transformVariable(*info, mVariableInfoMap, mOptions.shaderType, typeId,
                                            id, storageClass) == TransformationState::Transformed)
    {
        return TransformationState::Transformed;
    }

    // The variable is inactive.  Output a modified variable declaration, where the type is the
    // corresponding type with the Private storage class.
    return mInactiveVaryingRemover.transformVariable(typeId, id, storageClass, mSpirvBlobOut);
}

TransformationState SpirvTransformer::transformTypeImage(const uint32_t *instruction)
{
    return mMultiSampleTransformer.transformTypeImage(instruction, mSpirvBlobOut);
}

TransformationState SpirvTransformer::transformImageRead(const uint32_t *instruction)
{
    return mMultiSampleTransformer.transformImageRead(instruction, mIds, mSpirvBlobOut);
}

TransformationState SpirvTransformer::transformAccessChain(const uint32_t *instruction)
{
    spirv::IdResultType typeId;
    spirv::IdResult id;
    spirv::IdRef baseId;
    spirv::IdRefList indexList;
    spirv::ParseAccessChain(instruction, &typeId, &id, &baseId, &indexList);

    // If not accessing an inactive output varying, nothing to do.
    const ShaderInterfaceVariableInfo *info = mVariableInfoById[baseId];
    if (info == nullptr)
    {
        return TransformationState::Unchanged;
    }

    if (info->activeStages[mOptions.shaderType])
    {
        return TransformationState::Unchanged;
    }

    return mInactiveVaryingRemover.transformAccessChain(typeId, id, baseId, indexList,
                                                        mSpirvBlobOut);
}

struct AliasingAttributeMap
{
    // The SPIR-V id of the aliasing attribute with the most components.  This attribute will be
    // used to read from this location instead of every aliasing one.
    spirv::IdRef attribute;

    // SPIR-V ids of aliasing attributes.
    std::vector<spirv::IdRef> aliasingAttributes;
};

void ValidateShaderInterfaceVariableIsAttribute(const ShaderInterfaceVariableInfo *info)
{
    ASSERT(info);
    ASSERT(info->activeStages[gl::ShaderType::Vertex]);
    ASSERT(info->attributeComponentCount > 0);
    ASSERT(info->attributeLocationCount > 0);
    ASSERT(info->location != ShaderInterfaceVariableInfo::kInvalid);
}

void ValidateIsAliasingAttribute(const AliasingAttributeMap *aliasingMap, uint32_t id)
{
    ASSERT(id != aliasingMap->attribute);
    ASSERT(std::find(aliasingMap->aliasingAttributes.begin(), aliasingMap->aliasingAttributes.end(),
                     id) != aliasingMap->aliasingAttributes.end());
}

// A transformation that resolves vertex attribute aliases.  Note that vertex attribute aliasing is
// only allowed in GLSL ES 100, where the attribute types can only be one of float, vec2, vec3,
// vec4, mat2, mat3, and mat4.  Matrix attributes are handled by expanding them to multiple vector
// attributes, each occupying one location.
class SpirvVertexAttributeAliasingTransformer final : public SpirvTransformerBase
{
  public:
    SpirvVertexAttributeAliasingTransformer(
        const spirv::Blob &spirvBlobIn,
        const ShaderInterfaceVariableInfoMap &variableInfoMap,
        std::vector<const ShaderInterfaceVariableInfo *> &&variableInfoById,
        spirv::Blob *spirvBlobOut)
        : SpirvTransformerBase(spirvBlobIn, variableInfoMap, spirvBlobOut)
    {
        mVariableInfoById = std::move(variableInfoById);
    }

    void transform();

  private:
    // Preprocess aliasing attributes in preparation for their removal.
    void preprocessAliasingAttributes();

    // Transform instructions:
    void transformInstruction();

    // Helpers:
    spirv::IdRef getAliasingAttributeReplacementId(spirv::IdRef aliasingId, uint32_t offset) const;
    bool isMatrixAttribute(spirv::IdRef id) const;

    // Instructions that are purely informational:
    void visitTypeFloat(const uint32_t *instruction);
    void visitTypeVector(const uint32_t *instruction);
    void visitTypeMatrix(const uint32_t *instruction);
    void visitTypePointer(const uint32_t *instruction);

    // Instructions that potentially need transformation.  They return true if the instruction is
    // transformed.  If false is returned, the instruction should be copied as-is.
    TransformationState transformEntryPoint(const uint32_t *instruction);
    TransformationState transformName(const uint32_t *instruction);
    TransformationState transformDecorate(const uint32_t *instruction);
    TransformationState transformVariable(const uint32_t *instruction);
    TransformationState transformAccessChain(const uint32_t *instruction);
    void transformLoadHelper(spirv::IdRef pointerId,
                             spirv::IdRef typeId,
                             spirv::IdRef replacementId,
                             spirv::IdRef resultId);
    TransformationState transformLoad(const uint32_t *instruction);

    void declareExpandedMatrixVectors();
    void writeExpandedMatrixInitialization();

    // Transformation state:

    // Map of aliasing attributes per location.
    gl::AttribArray<AliasingAttributeMap> mAliasingAttributeMap;

    // For each id, this map indicates whether it refers to an aliasing attribute that needs to be
    // removed.
    std::vector<bool> mIsAliasingAttributeById;

    // Matrix attributes are split into vectors, each occupying one location.  The SPIR-V
    // declaration would need to change from:
    //
    //     %type = OpTypeMatrix %vectorType N
    //     %matrixType = OpTypePointer Input %type
    //     %matrix = OpVariable %matrixType Input
    //
    // to:
    //
    //     %matrixType = OpTypePointer Private %type
    //     %matrix = OpVariable %matrixType Private
    //
    //     %vecType = OpTypePointer Input %vectorType
    //
    //     %vec0 = OpVariable %vecType Input
    //     ...
    //     %vecN-1 = OpVariable %vecType Input
    //
    // For each id %matrix (which corresponds to a matrix attribute), this map contains %vec0.  The
    // ids of the split vectors are consecutive, so %veci == %vec0 + i.  %veciType is taken from
    // mInputTypePointers.
    std::vector<spirv::IdRef> mExpandedMatrixFirstVectorIdById;
    // Whether the expanded matrix OpVariables are generated.
    bool mHaveMatricesExpanded = false;
    // Whether initialization of the matrix attributes should be written at the beginning of the
    // current function.
    bool mWriteExpandedMatrixInitialization = false;
    spirv::IdRef mEntryPointId;

    // Id of attribute types; float and veci.  This array is one-based, and [0] is unused.
    //
    // [1]: id of OpTypeFloat 32
    // [N]: id of OpTypeVector %[1] N, N = {2, 3, 4}
    //
    // In other words, index of the array corresponds to the number of components in the type.
    std::array<spirv::IdRef, 5> mFloatTypes;

    // Corresponding to mFloatTypes, [i]: id of OpMatrix %mFloatTypes[i] i.  Note that only square
    // matrices are possible as attributes in GLSL ES 1.00.  [0] and [1] are unused.
    std::array<spirv::IdRef, 5> mMatrixTypes;

    // Corresponding to mFloatTypes, [i]: id of OpTypePointer Input %mFloatTypes[i].  [0] is unused.
    std::array<spirv::IdRef, 5> mInputTypePointers;

    // Corresponding to mFloatTypes, [i]: id of OpTypePointer Private %mFloatTypes[i].  [0] is
    // unused.
    std::array<spirv::IdRef, 5> mPrivateFloatTypePointers;

    // Corresponding to mMatrixTypes, [i]: id of OpTypePointer Private %mMatrixTypes[i].  [0] and
    // [1] are unused.
    std::array<spirv::IdRef, 5> mPrivateMatrixTypePointers;
};

void SpirvVertexAttributeAliasingTransformer::transform()
{
    onTransformBegin();

    preprocessAliasingAttributes();

    while (mCurrentWord < mSpirvBlobIn.size())
    {
        transformInstruction();
    }
}

void SpirvVertexAttributeAliasingTransformer::preprocessAliasingAttributes()
{
    const uint32_t indexBound = mSpirvBlobIn[spirv::kHeaderIndexIndexBound];

    mVariableInfoById.resize(indexBound, nullptr);
    mIsAliasingAttributeById.resize(indexBound, false);
    mExpandedMatrixFirstVectorIdById.resize(indexBound);

    // Go through attributes and find out which alias which.
    for (uint32_t idIndex = spirv::kMinValidId; idIndex < indexBound; ++idIndex)
    {
        const spirv::IdRef id(idIndex);

        const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];

        // Ignore non attribute ids.
        if (info == nullptr || info->attributeComponentCount == 0)
        {
            continue;
        }

        ASSERT(info->activeStages[gl::ShaderType::Vertex]);
        ASSERT(info->location != ShaderInterfaceVariableInfo::kInvalid);

        const bool isMatrixAttribute = info->attributeLocationCount > 1;

        for (uint32_t offset = 0; offset < info->attributeLocationCount; ++offset)
        {
            uint32_t location = info->location + offset;
            ASSERT(location < mAliasingAttributeMap.size());

            spirv::IdRef attributeId(id);

            // If this is a matrix attribute, expand it to vectors.
            if (isMatrixAttribute)
            {
                const spirv::IdRef matrixId(id);

                // Get a new id for this location and associate it with the matrix.
                attributeId = getNewId();
                if (offset == 0)
                {
                    mExpandedMatrixFirstVectorIdById[matrixId] = attributeId;
                }
                // The ids are consecutive.
                ASSERT(attributeId == mExpandedMatrixFirstVectorIdById[matrixId] + offset);

                mIsAliasingAttributeById.resize(attributeId + 1, false);
                mVariableInfoById.resize(attributeId + 1, nullptr);
                mVariableInfoById[attributeId] = info;
            }

            AliasingAttributeMap *aliasingMap = &mAliasingAttributeMap[location];

            // If this is the first attribute in this location, remember it.
            if (!aliasingMap->attribute.valid())
            {
                aliasingMap->attribute = attributeId;
                continue;
            }

            // Otherwise, either add it to the list of aliasing attributes, or replace the main
            // attribute (and add that to the list of aliasing attributes).  The one with the
            // largest number of components is used as the main attribute.
            const ShaderInterfaceVariableInfo *curMainAttribute =
                mVariableInfoById[aliasingMap->attribute];
            ASSERT(curMainAttribute != nullptr && curMainAttribute->attributeComponentCount > 0);

            spirv::IdRef aliasingId;
            if (info->attributeComponentCount > curMainAttribute->attributeComponentCount)
            {
                aliasingId             = aliasingMap->attribute;
                aliasingMap->attribute = attributeId;
            }
            else
            {
                aliasingId = attributeId;
            }

            aliasingMap->aliasingAttributes.push_back(aliasingId);
            ASSERT(!mIsAliasingAttributeById[aliasingId]);
            mIsAliasingAttributeById[aliasingId] = true;
        }
    }
}

void SpirvVertexAttributeAliasingTransformer::transformInstruction()
{
    uint32_t wordCount;
    spv::Op opCode;
    const uint32_t *instruction = getCurrentInstruction(&opCode, &wordCount);

    if (opCode == spv::OpFunction)
    {
        // Declare the expanded matrix variables right before the first function declaration.
        if (!mHaveMatricesExpanded)
        {
            declareExpandedMatrixVectors();
            mHaveMatricesExpanded = true;
        }

        // SPIR-V is structured in sections.  Function declarations come last.
        mIsInFunctionSection = true;

        // The matrix attribute declarations have been changed to have Private storage class, and
        // they are initialized from the expanded (and potentially aliased) Input vectors.  This is
        // done at the beginning of the entry point.

        spirv::IdResultType id;
        spirv::IdResult functionId;
        spv::FunctionControlMask functionControl;
        spirv::IdRef functionType;
        spirv::ParseFunction(instruction, &id, &functionId, &functionControl, &functionType);

        mWriteExpandedMatrixInitialization = functionId == mEntryPointId;
    }

    // Only look at interesting instructions.
    TransformationState transformationState = TransformationState::Unchanged;

    if (mIsInFunctionSection)
    {
        // Write expanded matrix initialization right after the entry point's OpFunction and any
        // instruction that must come immediately after it.
        if (mWriteExpandedMatrixInitialization && opCode != spv::OpFunction &&
            opCode != spv::OpFunctionParameter && opCode != spv::OpLabel &&
            opCode != spv::OpVariable)
        {
            writeExpandedMatrixInitialization();
            mWriteExpandedMatrixInitialization = false;
        }

        // Look at in-function opcodes.
        switch (opCode)
        {
            case spv::OpAccessChain:
            case spv::OpInBoundsAccessChain:
                transformationState = transformAccessChain(instruction);
                break;
            case spv::OpLoad:
                transformationState = transformLoad(instruction);
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
            // Informational instructions:
            case spv::OpTypeFloat:
                visitTypeFloat(instruction);
                break;
            case spv::OpTypeVector:
                visitTypeVector(instruction);
                break;
            case spv::OpTypeMatrix:
                visitTypeMatrix(instruction);
                break;
            case spv::OpTypePointer:
                visitTypePointer(instruction);
                break;
            // Instructions that may need transformation:
            case spv::OpEntryPoint:
                transformationState = transformEntryPoint(instruction);
                break;
            case spv::OpName:
                transformationState = transformName(instruction);
                break;
            case spv::OpDecorate:
                transformationState = transformDecorate(instruction);
                break;
            case spv::OpVariable:
                transformationState = transformVariable(instruction);
                break;
            default:
                break;
        }
    }

    // If the instruction was not transformed, copy it to output as is.
    if (transformationState == TransformationState::Unchanged)
    {
        copyInstruction(instruction, wordCount);
    }

    // Advance to next instruction.
    mCurrentWord += wordCount;
}

spirv::IdRef SpirvVertexAttributeAliasingTransformer::getAliasingAttributeReplacementId(
    spirv::IdRef aliasingId,
    uint32_t offset) const
{
    // Get variable info corresponding to the aliasing attribute.
    const ShaderInterfaceVariableInfo *aliasingInfo = mVariableInfoById[aliasingId];
    ValidateShaderInterfaceVariableIsAttribute(aliasingInfo);

    // Find the replacement attribute.
    const AliasingAttributeMap *aliasingMap =
        &mAliasingAttributeMap[aliasingInfo->location + offset];
    ValidateIsAliasingAttribute(aliasingMap, aliasingId);

    const spirv::IdRef replacementId(aliasingMap->attribute);
    ASSERT(replacementId.valid() && replacementId < mIsAliasingAttributeById.size());
    ASSERT(!mIsAliasingAttributeById[replacementId]);

    return replacementId;
}

bool SpirvVertexAttributeAliasingTransformer::isMatrixAttribute(spirv::IdRef id) const
{
    return mExpandedMatrixFirstVectorIdById[id].valid();
}

void SpirvVertexAttributeAliasingTransformer::visitTypeFloat(const uint32_t *instruction)
{
    spirv::IdResult id;
    spirv::LiteralInteger width;
    spirv::ParseTypeFloat(instruction, &id, &width);

    // Only interested in OpTypeFloat 32.
    if (width == 32)
    {
        ASSERT(!mFloatTypes[1].valid());
        mFloatTypes[1] = id;
    }
}

void SpirvVertexAttributeAliasingTransformer::visitTypeVector(const uint32_t *instruction)
{
    spirv::IdResult id;
    spirv::IdRef componentId;
    spirv::LiteralInteger componentCount;
    spirv::ParseTypeVector(instruction, &id, &componentId, &componentCount);

    // Only interested in OpTypeVector %f32 N, where %f32 is the id of OpTypeFloat 32.
    if (componentId == mFloatTypes[1])
    {
        ASSERT(componentCount >= 2 && componentCount <= 4);
        ASSERT(!mFloatTypes[componentCount].valid());
        mFloatTypes[componentCount] = id;
    }
}

void SpirvVertexAttributeAliasingTransformer::visitTypeMatrix(const uint32_t *instruction)
{
    spirv::IdResult id;
    spirv::IdRef columnType;
    spirv::LiteralInteger columnCount;
    spirv::ParseTypeMatrix(instruction, &id, &columnType, &columnCount);

    // Only interested in OpTypeMatrix %vecN, where %vecN is the id of OpTypeVector %f32 N.
    // This is only for square matN types (as allowed by GLSL ES 1.00), so columnCount is the same
    // as rowCount.
    if (columnType == mFloatTypes[columnCount])
    {
        ASSERT(!mMatrixTypes[columnCount].valid());
        mMatrixTypes[columnCount] = id;
    }
}

void SpirvVertexAttributeAliasingTransformer::visitTypePointer(const uint32_t *instruction)
{
    spirv::IdResult id;
    spv::StorageClass storageClass;
    spirv::IdRef typeId;
    spirv::ParseTypePointer(instruction, &id, &storageClass, &typeId);

    // Only interested in OpTypePointer Input %vecN, where %vecN is the id of OpTypeVector %f32 N,
    // as well as OpTypePointer Private %matN, where %matN is the id of OpTypeMatrix %vecN N.
    // This is only for matN types (as allowed by GLSL ES 1.00), so N >= 2.
    if (storageClass == spv::StorageClassInput)
    {
        for (size_t n = 2; n < mFloatTypes.size(); ++n)
        {
            if (typeId == mFloatTypes[n])
            {
                ASSERT(!mInputTypePointers[n].valid());
                mInputTypePointers[n] = id;
                break;
            }
        }
    }
    else if (storageClass == spv::StorageClassPrivate)
    {
        ASSERT(mFloatTypes.size() == mMatrixTypes.size());
        for (size_t n = 2; n < mMatrixTypes.size(); ++n)
        {
            // Note that Private types may not be unique, as the previous transformation can
            // generate duplicates.
            if (typeId == mFloatTypes[n])
            {
                mPrivateFloatTypePointers[n] = id;
                break;
            }
            if (typeId == mMatrixTypes[n])
            {
                mPrivateMatrixTypePointers[n] = id;
                break;
            }
        }
    }
}

TransformationState SpirvVertexAttributeAliasingTransformer::transformEntryPoint(
    const uint32_t *instruction)
{
    // Should only have one EntryPoint
    ASSERT(!mEntryPointId.valid());

    // Remove aliasing attributes from the shader interface declaration.
    spv::ExecutionModel executionModel;
    spirv::LiteralString name;
    spirv::IdRefList interfaceList;
    spirv::ParseEntryPoint(instruction, &executionModel, &mEntryPointId, &name, &interfaceList);

    // As a first pass, filter out matrix attributes and append their replacement vectors.
    size_t originalInterfaceListSize = interfaceList.size();
    for (size_t index = 0; index < originalInterfaceListSize; ++index)
    {
        const spirv::IdRef matrixId(interfaceList[index]);

        if (!mExpandedMatrixFirstVectorIdById[matrixId].valid())
        {
            continue;
        }

        const ShaderInterfaceVariableInfo *info = mVariableInfoById[matrixId];
        ValidateShaderInterfaceVariableIsAttribute(info);

        // Replace the matrix id with its first vector id.
        const spirv::IdRef vec0Id(mExpandedMatrixFirstVectorIdById[matrixId]);
        interfaceList[index] = vec0Id;

        // Append the rest of the vectors to the entry point.
        for (uint32_t offset = 1; offset < info->attributeLocationCount; ++offset)
        {
            const spirv::IdRef vecId(vec0Id + offset);
            interfaceList.push_back(vecId);
        }
    }

    // Filter out aliasing attributes from entry point interface declaration.
    size_t writeIndex = 0;
    for (size_t index = 0; index < interfaceList.size(); ++index)
    {
        const spirv::IdRef id(interfaceList[index]);

        // If this is an attribute that's aliasing another one in the same location, remove it.
        if (mIsAliasingAttributeById[id])
        {
            const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];
            ValidateShaderInterfaceVariableIsAttribute(info);

            // The following assertion is only valid for non-matrix attributes.
            if (info->attributeLocationCount == 1)
            {
                const AliasingAttributeMap *aliasingMap = &mAliasingAttributeMap[info->location];
                ValidateIsAliasingAttribute(aliasingMap, id);
            }

            continue;
        }

        interfaceList[writeIndex] = id;
        ++writeIndex;
    }

    // Update the number of interface variables.
    interfaceList.resize(writeIndex);

    // Write the entry point with the aliasing attributes removed.
    spirv::WriteEntryPoint(mSpirvBlobOut, executionModel, mEntryPointId, name, interfaceList);

    return TransformationState::Transformed;
}

TransformationState SpirvVertexAttributeAliasingTransformer::transformName(
    const uint32_t *instruction)
{
    spirv::IdRef id;
    spirv::LiteralString name;
    spirv::ParseName(instruction, &id, &name);

    // If id is not that of an aliasing attribute, there's nothing to do.
    ASSERT(id < mIsAliasingAttributeById.size());
    if (!mIsAliasingAttributeById[id])
    {
        return TransformationState::Unchanged;
    }

    // Drop debug annotations for this id.
    return TransformationState::Transformed;
}

TransformationState SpirvVertexAttributeAliasingTransformer::transformDecorate(
    const uint32_t *instruction)
{
    spirv::IdRef id;
    spv::Decoration decoration;
    spirv::ParseDecorate(instruction, &id, &decoration, nullptr);

    if (isMatrixAttribute(id))
    {
        // If it's a matrix attribute, it's expanded to multiple vectors.  Insert the Location
        // decorations for these vectors here.

        // Keep all decorations except for Location.
        if (decoration != spv::DecorationLocation)
        {
            return TransformationState::Unchanged;
        }

        const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];
        ValidateShaderInterfaceVariableIsAttribute(info);

        const spirv::IdRef vec0Id(mExpandedMatrixFirstVectorIdById[id]);
        ASSERT(vec0Id.valid());

        for (uint32_t offset = 0; offset < info->attributeLocationCount; ++offset)
        {
            const spirv::IdRef vecId(vec0Id + offset);
            if (mIsAliasingAttributeById[vecId])
            {
                continue;
            }

            spirv::WriteDecorate(mSpirvBlobOut, vecId, decoration,
                                 {spirv::LiteralInteger(info->location + offset)});
        }
    }
    else
    {
        // If id is not that of an active attribute, there's nothing to do.
        const ShaderInterfaceVariableInfo *info = mVariableInfoById[id];
        if (info == nullptr || info->attributeComponentCount == 0 ||
            !info->activeStages[gl::ShaderType::Vertex])
        {
            return TransformationState::Unchanged;
        }

        // Always drop RelaxedPrecision from input attributes.  The temporary variable the attribute
        // is loaded into has RelaxedPrecision and will implicitly convert.
        if (decoration == spv::DecorationRelaxedPrecision)
        {
            return TransformationState::Transformed;
        }

        // If id is not that of an aliasing attribute, there's nothing else to do.
        ASSERT(id < mIsAliasingAttributeById.size());
        if (!mIsAliasingAttributeById[id])
        {
            return TransformationState::Unchanged;
        }
    }

    // Drop every decoration for this id.
    return TransformationState::Transformed;
}

TransformationState SpirvVertexAttributeAliasingTransformer::transformVariable(
    const uint32_t *instruction)
{
    spirv::IdResultType typeId;
    spirv::IdResult id;
    spv::StorageClass storageClass;
    spirv::ParseVariable(instruction, &typeId, &id, &storageClass, nullptr);

    if (!isMatrixAttribute(id))
    {
        // If id is not that of an aliasing attribute, there's nothing to do.  Note that matrix
        // declarations are always replaced.
        ASSERT(id < mIsAliasingAttributeById.size());
        if (!mIsAliasingAttributeById[id])
        {
            return TransformationState::Unchanged;
        }
    }

    ASSERT(storageClass == spv::StorageClassInput);

    // Drop the declaration.
    return TransformationState::Transformed;
}

TransformationState SpirvVertexAttributeAliasingTransformer::transformAccessChain(
    const uint32_t *instruction)
{
    spirv::IdResultType typeId;
    spirv::IdResult id;
    spirv::IdRef baseId;
    spirv::IdRefList indexList;
    spirv::ParseAccessChain(instruction, &typeId, &id, &baseId, &indexList);

    if (isMatrixAttribute(baseId))
    {
        // Write a modified OpAccessChain instruction.  Only modification is that the %type is
        // replaced with the Private version of it.  If there is one %index, that would be a vector
        // type, and if there are two %index'es, it's a float type.
        spirv::IdRef replacementTypeId;

        if (indexList.size() == 1)
        {
            // If indexed once, it uses a vector type.
            const ShaderInterfaceVariableInfo *info = mVariableInfoById[baseId];
            ValidateShaderInterfaceVariableIsAttribute(info);

            const uint32_t componentCount = info->attributeComponentCount;

            // %type must have been the Input vector type with the matrice's component size.
            ASSERT(typeId == mInputTypePointers[componentCount]);

            // Replace the type with the corresponding Private one.
            replacementTypeId = mPrivateFloatTypePointers[componentCount];
        }
        else
        {
            // If indexed twice, it uses the float type.
            ASSERT(indexList.size() == 2);

            // Replace the type with the Private pointer to float32.
            replacementTypeId = mPrivateFloatTypePointers[1];
        }

        spirv::WriteAccessChain(mSpirvBlobOut, replacementTypeId, id, baseId, indexList);
    }
    else
    {
        // If base id is not that of an aliasing attribute, there's nothing to do.
        ASSERT(baseId < mIsAliasingAttributeById.size());
        if (!mIsAliasingAttributeById[baseId])
        {
            return TransformationState::Unchanged;
        }

        // Find the replacement attribute for the aliasing one.
        const spirv::IdRef replacementId(getAliasingAttributeReplacementId(baseId, 0));

        // Get variable info corresponding to the replacement attribute.
        const ShaderInterfaceVariableInfo *replacementInfo = mVariableInfoById[replacementId];
        ValidateShaderInterfaceVariableIsAttribute(replacementInfo);

        // Write a modified OpAccessChain instruction.  Currently, the instruction is:
        //
        //     %id = OpAccessChain %type %base %index
        //
        // This is modified to:
        //
        //     %id = OpAccessChain %type %replacement %index
        //
        // Note that the replacement has at least as many components as the aliasing attribute,
        // and both attributes start at component 0 (GLSL ES restriction).  So, indexing the
        // replacement attribute with the same index yields the same result and type.
        spirv::WriteAccessChain(mSpirvBlobOut, typeId, id, replacementId, indexList);
    }

    return TransformationState::Transformed;
}

void SpirvVertexAttributeAliasingTransformer::transformLoadHelper(spirv::IdRef pointerId,
                                                                  spirv::IdRef typeId,
                                                                  spirv::IdRef replacementId,
                                                                  spirv::IdRef resultId)
{
    // Get variable info corresponding to the replacement attribute.
    const ShaderInterfaceVariableInfo *replacementInfo = mVariableInfoById[replacementId];
    ValidateShaderInterfaceVariableIsAttribute(replacementInfo);

    // Currently, the instruction is:
    //
    //     %id = OpLoad %type %pointer
    //
    // This is modified to:
    //
    //     %newId = OpLoad %replacementType %replacement
    //
    const spirv::IdRef loadResultId(getNewId());
    const spirv::IdRef replacementTypeId(mFloatTypes[replacementInfo->attributeComponentCount]);
    ASSERT(replacementTypeId.valid());

    spirv::WriteLoad(mSpirvBlobOut, replacementTypeId, loadResultId, replacementId, nullptr);

    // If swizzle is not necessary, assign %newId to %resultId.
    const ShaderInterfaceVariableInfo *aliasingInfo = mVariableInfoById[pointerId];
    if (aliasingInfo->attributeComponentCount == replacementInfo->attributeComponentCount)
    {
        spirv::WriteCopyObject(mSpirvBlobOut, typeId, resultId, loadResultId);
        return;
    }

    // Take as many components from the replacement as the aliasing attribute wanted.  This is done
    // by either of the following instructions:
    //
    // - If aliasing attribute has only one component:
    //
    //     %resultId = OpCompositeExtract %floatType %newId 0
    //
    // - If aliasing attribute has more than one component:
    //
    //     %resultId = OpVectorShuffle %vecType %newId %newId 0 1 ...
    //
    ASSERT(aliasingInfo->attributeComponentCount < replacementInfo->attributeComponentCount);
    ASSERT(mFloatTypes[aliasingInfo->attributeComponentCount] == typeId);

    if (aliasingInfo->attributeComponentCount == 1)
    {
        spirv::WriteCompositeExtract(mSpirvBlobOut, typeId, resultId, loadResultId,
                                     {spirv::LiteralInteger(0)});
    }
    else
    {
        spirv::LiteralIntegerList swizzle = {spirv::LiteralInteger(0), spirv::LiteralInteger(1),
                                             spirv::LiteralInteger(2), spirv::LiteralInteger(3)};
        swizzle.resize(aliasingInfo->attributeComponentCount);

        spirv::WriteVectorShuffle(mSpirvBlobOut, typeId, resultId, loadResultId, loadResultId,
                                  swizzle);
    }
}

TransformationState SpirvVertexAttributeAliasingTransformer::transformLoad(
    const uint32_t *instruction)
{
    spirv::IdResultType typeId;
    spirv::IdResult id;
    spirv::IdRef pointerId;
    ParseLoad(instruction, &typeId, &id, &pointerId, nullptr);

    // Currently, the instruction is:
    //
    //     %id = OpLoad %type %pointer
    //
    // If non-matrix, this is modifed to load from the aliasing vector instead if aliasing.
    //
    // If matrix, this is modified such that %type points to the Private version of it.
    //
    if (isMatrixAttribute(pointerId))
    {
        const ShaderInterfaceVariableInfo *info = mVariableInfoById[pointerId];
        ValidateShaderInterfaceVariableIsAttribute(info);

        const spirv::IdRef replacementTypeId(mMatrixTypes[info->attributeLocationCount]);

        spirv::WriteLoad(mSpirvBlobOut, replacementTypeId, id, pointerId, nullptr);
    }
    else
    {
        // If pointer id is not that of an aliasing attribute, there's nothing to do.
        ASSERT(pointerId < mIsAliasingAttributeById.size());
        if (!mIsAliasingAttributeById[pointerId])
        {
            return TransformationState::Unchanged;
        }

        // Find the replacement attribute for the aliasing one.
        const spirv::IdRef replacementId(getAliasingAttributeReplacementId(pointerId, 0));

        // Replace the load instruction by a load from the replacement id.
        transformLoadHelper(pointerId, typeId, replacementId, id);
    }

    return TransformationState::Transformed;
}

void SpirvVertexAttributeAliasingTransformer::declareExpandedMatrixVectors()
{
    // Go through matrix attributes and expand them.
    for (uint32_t matrixIdIndex = spirv::kMinValidId;
         matrixIdIndex < mExpandedMatrixFirstVectorIdById.size(); ++matrixIdIndex)
    {
        const spirv::IdRef matrixId(matrixIdIndex);

        if (!mExpandedMatrixFirstVectorIdById[matrixId].valid())
        {
            continue;
        }

        const spirv::IdRef vec0Id(mExpandedMatrixFirstVectorIdById[matrixId]);

        const ShaderInterfaceVariableInfo *info = mVariableInfoById[matrixId];
        ValidateShaderInterfaceVariableIsAttribute(info);

        // Need to generate the following:
        //
        //     %privateType = OpTypePointer Private %matrixType
        //     %id = OpVariable %privateType Private
        //     %vecType = OpTypePointer %vecType Input
        //     %vec0 = OpVariable %vecType Input
        //     ...
        //     %vecN-1 = OpVariable %vecType Input
        const uint32_t componentCount = info->attributeComponentCount;
        const uint32_t locationCount  = info->attributeLocationCount;
        ASSERT(componentCount == locationCount);
        ASSERT(mMatrixTypes[locationCount].valid());

        // OpTypePointer Private %matrixType
        spirv::IdRef privateType(mPrivateMatrixTypePointers[locationCount]);
        if (!privateType.valid())
        {
            privateType                               = getNewId();
            mPrivateMatrixTypePointers[locationCount] = privateType;
            spirv::WriteTypePointer(mSpirvBlobOut, privateType, spv::StorageClassPrivate,
                                    mMatrixTypes[locationCount]);
        }

        // OpVariable %privateType Private
        spirv::WriteVariable(mSpirvBlobOut, privateType, matrixId, spv::StorageClassPrivate,
                             nullptr);

        // If the OpTypePointer is not declared for the vector type corresponding to each location,
        // declare it now.
        //
        //     %vecType = OpTypePointer %vecType Input
        spirv::IdRef inputType(mInputTypePointers[componentCount]);
        if (!inputType.valid())
        {
            inputType                          = getNewId();
            mInputTypePointers[componentCount] = inputType;
            spirv::WriteTypePointer(mSpirvBlobOut, inputType, spv::StorageClassInput,
                                    mFloatTypes[componentCount]);
        }

        // Declare a vector for each column of the matrix.
        for (uint32_t offset = 0; offset < info->attributeLocationCount; ++offset)
        {
            const spirv::IdRef vecId(vec0Id + offset);
            if (!mIsAliasingAttributeById[vecId])
            {
                spirv::WriteVariable(mSpirvBlobOut, inputType, vecId, spv::StorageClassInput,
                                     nullptr);
            }
        }
    }

    // Additionally, declare OpTypePointer Private %mFloatTypes[i] in case needed (used in
    // Op*AccessChain instructions, if any).
    for (size_t n = 1; n < mFloatTypes.size(); ++n)
    {
        if (mFloatTypes[n].valid() && !mPrivateFloatTypePointers[n].valid())
        {
            const spirv::IdRef privateType(getNewId());
            mPrivateFloatTypePointers[n] = privateType;
            spirv::WriteTypePointer(mSpirvBlobOut, privateType, spv::StorageClassPrivate,
                                    mFloatTypes[n]);
        }
    }
}

void SpirvVertexAttributeAliasingTransformer::writeExpandedMatrixInitialization()
{
    // Go through matrix attributes and initialize them.  Note that their declaration is replaced
    // with a Private storage class, but otherwise has the same id.
    for (uint32_t matrixIdIndex = spirv::kMinValidId;
         matrixIdIndex < mExpandedMatrixFirstVectorIdById.size(); ++matrixIdIndex)
    {
        const spirv::IdRef matrixId(matrixIdIndex);

        if (!mExpandedMatrixFirstVectorIdById[matrixId].valid())
        {
            continue;
        }

        const spirv::IdRef vec0Id(mExpandedMatrixFirstVectorIdById[matrixId]);

        // For every matrix, need to generate the following:
        //
        //     %vec0Id = OpLoad %vecType %vec0Pointer
        //     ...
        //     %vecN-1Id = OpLoad %vecType %vecN-1Pointer
        //     %mat = OpCompositeConstruct %matrixType %vec0 ... %vecN-1
        //     OpStore %matrixId %mat

        const ShaderInterfaceVariableInfo *info = mVariableInfoById[matrixId];
        ValidateShaderInterfaceVariableIsAttribute(info);

        spirv::IdRefList vecLoadIds;
        const uint32_t locationCount = info->attributeLocationCount;
        for (uint32_t offset = 0; offset < locationCount; ++offset)
        {
            const spirv::IdRef vecId(vec0Id + offset);

            // Load into temporary, potentially through an aliasing vector.
            spirv::IdRef replacementId(vecId);
            ASSERT(vecId < mIsAliasingAttributeById.size());
            if (mIsAliasingAttributeById[vecId])
            {
                replacementId = getAliasingAttributeReplacementId(vecId, offset);
            }

            // Write a load instruction from the replacement id.
            vecLoadIds.push_back(getNewId());
            transformLoadHelper(matrixId, mFloatTypes[info->attributeComponentCount], replacementId,
                                vecLoadIds.back());
        }

        // Aggregate the vector loads into a matrix.
        ASSERT(mMatrixTypes[locationCount].valid());
        const spirv::IdRef compositeId(getNewId());
        spirv::WriteCompositeConstruct(mSpirvBlobOut, mMatrixTypes[locationCount], compositeId,
                                       vecLoadIds);

        // Store it in the private variable.
        spirv::WriteStore(mSpirvBlobOut, matrixId, compositeId, nullptr);
    }
}

bool HasAliasingAttributes(const ShaderInterfaceVariableInfoMap &variableInfoMap)
{
    gl::AttributesMask isLocationAssigned;

    for (const ShaderInterfaceVariableInfo &info : variableInfoMap.getAttributes())
    {
        ASSERT(info.activeStages[gl::ShaderType::Vertex]);
        ASSERT(info.location != ShaderInterfaceVariableInfo::kInvalid);
        ASSERT(info.attributeComponentCount > 0);
        ASSERT(info.attributeLocationCount > 0);

        for (uint8_t offset = 0; offset < info.attributeLocationCount; ++offset)
        {
            uint32_t location = info.location + offset;

            // If there's aliasing, return immediately.
            if (isLocationAssigned.test(location))
            {
                return true;
            }

            isLocationAssigned.set(location);
        }
    }

    return false;
}
}  // anonymous namespace

UniformBindingInfo::UniformBindingInfo(uint32_t bindingIndex,
                                       gl::ShaderBitSet shaderBitSet,
                                       gl::ShaderType frontShaderType)
    : bindingIndex(bindingIndex), shaderBitSet(shaderBitSet), frontShaderType(frontShaderType)
{}

UniformBindingInfo::UniformBindingInfo() {}

// Strip indices from the name.  If there are non-zero indices, return false to indicate that this
// image uniform doesn't require set/binding.  That is done on index 0.
bool GetImageNameWithoutIndices(std::string *name)
{
    if (name->back() != ']')
    {
        return true;
    }

    bool isIndexZero = UniformNameIsIndexZero(*name);

    // Strip all indices
    *name = name->substr(0, name->find('['));

    return isIndexZero;
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

std::string GetXfbBufferName(const uint32_t bufferIndex)
{
    return sh::vk::kXfbEmulationBufferBlockName + Str(bufferIndex);
}

void GlslangAssignLocations(const GlslangSourceOptions &options,
                            const gl::ProgramExecutable &programExecutable,
                            const gl::ProgramVaryingPacking &varyingPacking,
                            const gl::ShaderType shaderType,
                            const gl::ShaderType frontShaderType,
                            bool isTransformFeedbackStage,
                            GlslangProgramInterfaceInfo *programInterfaceInfo,
                            UniformBindingIndexMap *uniformBindingIndexMapOut,
                            ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    // Assign outputs to the fragment shader, if any.
    if ((shaderType == gl::ShaderType::Fragment) &&
        programExecutable.hasLinkedShaderStage(gl::ShaderType::Fragment))
    {
        AssignOutputLocations(programExecutable, gl::ShaderType::Fragment, variableInfoMapOut);
    }

    // Assign attributes to the vertex shader, if any.
    if ((shaderType == gl::ShaderType::Vertex) &&
        programExecutable.hasLinkedShaderStage(gl::ShaderType::Vertex))
    {
        AssignAttributeLocations(programExecutable, gl::ShaderType::Vertex, variableInfoMapOut);
    }

    if (programExecutable.hasLinkedGraphicsShader())
    {
        const gl::VaryingPacking &inputPacking  = varyingPacking.getInputPacking(shaderType);
        const gl::VaryingPacking &outputPacking = varyingPacking.getOutputPacking(shaderType);

        // Assign varying locations.
        if (shaderType != gl::ShaderType::Vertex)
        {
            AssignVaryingLocations(options, inputPacking, shaderType, frontShaderType,
                                   programInterfaceInfo, variableInfoMapOut);
        }
        if (shaderType != gl::ShaderType::Fragment)
        {
            AssignVaryingLocations(options, outputPacking, shaderType, frontShaderType,
                                   programInterfaceInfo, variableInfoMapOut);
        }

        // Assign qualifiers to all varyings captured by transform feedback
        if (!programExecutable.getLinkedTransformFeedbackVaryings().empty() &&
            shaderType == programExecutable.getLinkedTransformFeedbackStage())
        {
            AssignTransformFeedbackQualifiers(programExecutable, outputPacking, shaderType,
                                              options.supportsTransformFeedbackExtension,
                                              variableInfoMapOut);
        }
    }

    AssignUniformBindings(options, programExecutable, shaderType, programInterfaceInfo,
                          variableInfoMapOut);
    AssignTextureBindings(options, programExecutable, shaderType, programInterfaceInfo,
                          uniformBindingIndexMapOut, variableInfoMapOut);
    AssignNonTextureBindings(options, programExecutable, shaderType, programInterfaceInfo,
                             uniformBindingIndexMapOut, variableInfoMapOut);

    if (options.supportsTransformFeedbackEmulation &&
        gl::ShaderTypeSupportsTransformFeedback(shaderType))
    {
        // If transform feedback emulation is not enabled, mark all transform feedback output
        // buffers as inactive.
        isTransformFeedbackStage =
            isTransformFeedbackStage && options.enableTransformFeedbackEmulation;

        AssignTransformFeedbackEmulationBindings(shaderType, programExecutable,
                                                 isTransformFeedbackStage, programInterfaceInfo,
                                                 variableInfoMapOut);
    }
}

void GlslangAssignTransformFeedbackLocations(gl::ShaderType shaderType,
                                             const gl::ProgramExecutable &programExecutable,
                                             bool isTransformFeedbackStage,
                                             GlslangProgramInterfaceInfo *programInterfaceInfo,
                                             ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    // The only varying that requires additional resources is gl_Position, as it's indirectly
    // captured through ANGLEXfbPosition.

    const std::vector<gl::TransformFeedbackVarying> &tfVaryings =
        programExecutable.getLinkedTransformFeedbackVaryings();

    bool capturesPosition = false;

    if (isTransformFeedbackStage)
    {
        for (uint32_t varyingIndex = 0; varyingIndex < tfVaryings.size(); ++varyingIndex)
        {
            const gl::TransformFeedbackVarying &tfVarying = tfVaryings[varyingIndex];
            const std::string &tfVaryingName              = tfVarying.mappedName;

            if (tfVaryingName == "gl_Position")
            {
                ASSERT(tfVarying.isBuiltIn());
                capturesPosition = true;
                break;
            }
        }
    }

    if (capturesPosition)
    {
        AddLocationInfo(variableInfoMapOut, shaderType, ShaderVariableType::Varying,
                        sh::vk::kXfbExtensionPositionOutName,
                        programInterfaceInfo->locationsUsedForXfbExtension, 0, 0, 0);
        ++programInterfaceInfo->locationsUsedForXfbExtension;
    }
    else
    {
        // Make sure this varying is removed from the other stages, or if position is not captured
        // at all.
        variableInfoMapOut->add(shaderType, ShaderVariableType::Varying,
                                sh::vk::kXfbExtensionPositionOutName);
    }
}

void GlslangGetShaderSpirvCode(const gl::Context *context,
                               const GlslangSourceOptions &options,
                               const gl::ProgramState &programState,
                               const gl::ProgramLinkedResources &resources,
                               GlslangProgramInterfaceInfo *programInterfaceInfo,
                               gl::ShaderMap<const spirv::Blob *> *spirvBlobsOut,
                               ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        gl::Shader *glShader         = programState.getAttachedShader(shaderType);
        (*spirvBlobsOut)[shaderType] = glShader ? &glShader->getCompiledBinary(context) : nullptr;
    }

    const gl::ProgramExecutable &programExecutable = programState.getExecutable();
    gl::ShaderType xfbStage        = programState.getAttachedTransformFeedbackStage();
    gl::ShaderType frontShaderType = gl::ShaderType::InvalidEnum;

    // This should be done before assigning varying location. Otherwise, We can encounter shader
    // interface mismatching problem in case the transformFeedback stage is not Vertex stage.
    for (const gl::ShaderType shaderType : programExecutable.getLinkedShaderStages())
    {
        // Assign location to varyings generated for transform feedback capture
        const bool isXfbStage = shaderType == xfbStage &&
                                !programExecutable.getLinkedTransformFeedbackVaryings().empty();
        if (options.supportsTransformFeedbackExtension &&
            gl::ShaderTypeSupportsTransformFeedback(shaderType))
        {
            GlslangAssignTransformFeedbackLocations(shaderType, programExecutable, isXfbStage,
                                                    programInterfaceInfo, variableInfoMapOut);
        }
    }
    UniformBindingIndexMap uniformBindingIndexMap;
    for (const gl::ShaderType shaderType : programExecutable.getLinkedShaderStages())
    {
        const bool isXfbStage = shaderType == xfbStage &&
                                !programExecutable.getLinkedTransformFeedbackVaryings().empty();
        GlslangAssignLocations(options, programExecutable, resources.varyingPacking, shaderType,
                               frontShaderType, isXfbStage, programInterfaceInfo,
                               &uniformBindingIndexMap, variableInfoMapOut);

        frontShaderType = shaderType;
    }
}

angle::Result GlslangTransformSpirvCode(const GlslangSpirvOptions &options,
                                        const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                        const spirv::Blob &initialSpirvBlob,
                                        spirv::Blob *spirvBlobOut)
{
    if (initialSpirvBlob.empty())
    {
        return angle::Result::Continue;
    }

    // Transform the SPIR-V code by assigning location/set/binding values.
    SpirvTransformer transformer(initialSpirvBlob, options, variableInfoMap, spirvBlobOut);
    transformer.transform();

    // If there are aliasing vertex attributes, transform the SPIR-V again to remove them.
    if (options.shaderType == gl::ShaderType::Vertex && HasAliasingAttributes(variableInfoMap))
    {
        spirv::Blob preTransformBlob = std::move(*spirvBlobOut);
        SpirvVertexAttributeAliasingTransformer aliasingTransformer(
            preTransformBlob, variableInfoMap, std::move(transformer.getVariableInfoByIdMap()),
            spirvBlobOut);
        aliasingTransformer.transform();
    }

    spirvBlobOut->shrink_to_fit();

    if (options.validate)
    {
        ASSERT(spirv::Validate(*spirvBlobOut));
    }

    return angle::Result::Continue;
}
}  // namespace rx

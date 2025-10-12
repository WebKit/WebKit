//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/wgpu/wgpu_wgsl_util.h"

#include <sstream>
#include <string>

#include "common/PackedEnums.h"
#include "common/PackedGLEnums_autogen.h"
#include "common/log_utils.h"
#include "common/utilities.h"
#include "compiler/translator/wgsl/OutputUniformBlocks.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramExecutable.h"

namespace rx
{
namespace webgpu
{

namespace
{
const bool kOutputReplacements = false;

void ReplaceFoundMarker(const std::string &shaderSource,
                        std::string &newSource,
                        const std::map<std::string, int> &varNameToLocation,
                        const char *marker,
                        const char *markerReplacement,
                        const char *replacementEnd,
                        size_t nextMarker,
                        size_t &currPos)
{
    const char *endOfName = " : ";

    // Copy up to the next marker
    newSource.append(shaderSource, currPos, nextMarker - currPos);

    // Extract name from something like `@location(@@@@@@) NAME : TYPE`.
    size_t startOfNamePos = nextMarker + strlen(marker);
    size_t endOfNamePos   = shaderSource.find(endOfName, startOfNamePos, strlen(endOfName));
    std::string name      = shaderSource.substr(startOfNamePos, endOfNamePos - startOfNamePos);

    // Use the shader variable's name to get the assigned location
    auto locationIter = varNameToLocation.find(name);
    if (locationIter == varNameToLocation.end())
    {
        if (kOutputReplacements)
        {
            std::cout << "Didn't find " << name << ", so skipping to next semicolon" << std::endl;
        }
        // This should be an ignored sampler, so just delete it.
        size_t endOfLine = shaderSource.find(";\n", nextMarker);
        currPos          = endOfLine + 2;
        return;
    }

    // TODO(anglebug.com/42267100): if the GLSL input is a matrix there should be multiple
    // WGSL input variables (multiple vectors representing the columns of the matrix).
    int location = locationIter->second;
    std::ostringstream locationReplacementStream;
    locationReplacementStream << markerReplacement << location << replacementEnd << " " << name;

    if (kOutputReplacements)
    {
        std::cout << "Replace \"" << marker << name << "\" with \""
                  << locationReplacementStream.str() << "\"" << std::endl;
    }

    // Append the new `@location(N) name` and then continue from the ` : type`.
    newSource.append(locationReplacementStream.str());
    currPos = endOfNamePos;
}

std::string WgslReplaceMarkers(const std::string &shaderSource,
                               const std::map<std::string, int> &varNameToLocation)
{
    const char *locationMarker = "@location(@@@@@@) ";
    const char *bindingMarker  = "@group(1) @binding(@@@@@@) var ";

    std::string newSource;
    newSource.reserve(shaderSource.size());

    size_t currPos = 0;
    while (true)
    {
        size_t nextMarker = shaderSource.find(locationMarker, currPos, strlen(locationMarker));
        if (nextMarker != std::string::npos)
        {
            ReplaceFoundMarker(shaderSource, newSource, varNameToLocation, locationMarker,
                               "@location(", ")", nextMarker, currPos);
        }
        else if ((nextMarker = shaderSource.find(bindingMarker, currPos, strlen(bindingMarker))) !=
                 std::string::npos)
        {

            ReplaceFoundMarker(shaderSource, newSource, varNameToLocation, bindingMarker,
                               "@group(1) @binding(", ") var", nextMarker, currPos);
        }
        else
        {
            // Copy the rest of the shader and end the loop.
            newSource.append(shaderSource, currPos);
            break;
        }
    }
    return newSource;
}

void AddShaderVarLocation(std::map<std::string, int> &varNameToLocation,
                          std::string varName,
                          int &startLoc,
                          GLenum varType,
                          unsigned int arraySize)
{
    ASSERT(!gl::IsSamplerType(varType));

    if (!arraySize && !gl::IsMatrixType(varType))
    {
        ASSERT(varNameToLocation.find(varName) == varNameToLocation.end());
        varNameToLocation[varName] = startLoc++;
        return;
    }

    if (arraySize)
    {
        // TODO(anglebug.com/42267100): need to support arrays (of scalars, vectors, and matrices).
        UNIMPLEMENTED();
        return;
    }

    if (gl::IsMatrixType(varType))
    {
        // Split into column vectors.
        for (int i = 0; i < gl::VariableColumnCount(varType); i++)
        {
            std::string fullVarName = varName + "_col" + std::to_string(i);
            ASSERT(varNameToLocation.find(fullVarName) == varNameToLocation.end());
            varNameToLocation[fullVarName] = startLoc++;
        }
    }
}

}  // namespace

template <typename T>
std::string WgslAssignLocationsAndSamplerBindings(const gl::ProgramExecutable &executable,
                                                  const std::string &shaderSource,
                                                  const std::vector<T> shaderVars,
                                                  const gl::ProgramMergedVaryings &mergedVaryings,
                                                  gl::ShaderType shaderType)
{
    std::map<std::string, int> varNameToLocation;
    for (const T &shaderVar : shaderVars)
    {
        if (shaderVar.isBuiltIn())
        {
            continue;
        }
        int loc = shaderVar.getLocation();
        AddShaderVarLocation(varNameToLocation, shaderVar.name, loc, shaderVar.getType(),
                             shaderVar.isArray() ? shaderVar.getBasicTypeElementCount() : 0u);
    }

    int currLocMarker = 0;
    for (const gl::ProgramVaryingRef &linkedVarying : mergedVaryings)
    {
        gl::ShaderBitSet supportedShaderStages =
            gl::ShaderBitSet({gl::ShaderType::Vertex, gl::ShaderType::Fragment});
        ASSERT(linkedVarying.frontShaderStage == gl::ShaderType::InvalidEnum ||
               supportedShaderStages.test(linkedVarying.frontShaderStage));
        ASSERT(linkedVarying.backShaderStage == gl::ShaderType::InvalidEnum ||
               supportedShaderStages.test(linkedVarying.backShaderStage));
        if (!linkedVarying.frontShader && !linkedVarying.backShader)
        {
            continue;
        }
        const sh::ShaderVariable *shaderVar = shaderType == gl::ShaderType::Vertex
                                                  ? linkedVarying.frontShader
                                                  : linkedVarying.backShader;
        if (shaderVar)
        {
            if (shaderVar->isBuiltIn())
            {
                continue;
            }
            AddShaderVarLocation(varNameToLocation, shaderVar->name, currLocMarker, shaderVar->type,
                                 shaderVar->isArray() ? shaderVar->getBasicTypeElementCount() : 0u);
        }
        else
        {
            const sh::ShaderVariable *otherShaderVar = shaderType == gl::ShaderType::Vertex
                                                           ? linkedVarying.backShader
                                                           : linkedVarying.frontShader;
            if (!otherShaderVar->isBuiltIn())
            {
                // Increment `currLockMarker` to keep locations in sync with the WGSL source
                // generated for the other shader stage, which will also have incremented
                // `currLocMarker` when seeing this variable.
                currLocMarker++;
            }
        }
    }

    const std::vector<gl::SamplerBinding> &samplerBindings = executable.getSamplerBindings();

    // GLSL samplers are split into WGSL samplers/textures and need to be assigned consecutive
    // locations, alternating between a sampler and its corresponding texture.
    // The WGPU backend will read the same metadata and lay out its bind groups in the same
    // alternating fashion.
    for (uint32_t textureIndex = 0; textureIndex < samplerBindings.size(); ++textureIndex)
    {
        if (samplerBindings[textureIndex].textureUnitsCount != 1)
        {
            // TODO(anglebug.com/389145696): implement sampler arrays.
            UNIMPLEMENTED();
            continue;
        }
        // Get the name of the sampler variable from the uniform metadata.
        uint32_t uniformIndex          = executable.getUniformIndexFromSamplerIndex(textureIndex);
        const std::string &uniformName = executable.getUniformNames()[uniformIndex];
        std::string mappedSamplerName  = sh::WGSLGetMappedSamplerName(uniformName);

        varNameToLocation[std::string(sh::kAngleSamplerPrefix) + mappedSamplerName] =
            textureIndex * 2;
        varNameToLocation[std::string(sh::kAngleTexturePrefix) + mappedSamplerName] =
            textureIndex * 2 + 1;
    }

    return WgslReplaceMarkers(shaderSource, varNameToLocation);
}

template std::string WgslAssignLocationsAndSamplerBindings<gl::ProgramInput>(
    const gl::ProgramExecutable &executable,
    const std::string &shaderSource,
    const std::vector<gl::ProgramInput> shaderVars,
    const gl::ProgramMergedVaryings &mergedVaryings,
    gl::ShaderType shaderType);

template std::string WgslAssignLocationsAndSamplerBindings<gl::ProgramOutput>(
    const gl::ProgramExecutable &executable,
    const std::string &shaderSource,
    const std::vector<gl::ProgramOutput> shaderVars,
    const gl::ProgramMergedVaryings &mergedVaryings,
    gl::ShaderType shaderType);

}  // namespace webgpu
}  // namespace rx

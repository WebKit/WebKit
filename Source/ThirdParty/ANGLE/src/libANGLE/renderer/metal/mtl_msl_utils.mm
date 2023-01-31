//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_msl_utils.h: Utilities to manipulate MSL.
//

#import <Foundation/Foundation.h>

#include "common/utilities.h"
#include "compiler/translator/TranslatorMetalDirect.h"
#include "libANGLE/renderer/metal/ShaderMtl.h"
#include "libANGLE/renderer/metal/mtl_msl_utils.h"
namespace rx
{
namespace
{
constexpr char kXfbBindingsMarker[]     = "@@XFB-Bindings@@";
constexpr char kXfbOutMarker[]          = "ANGLE_@@XFB-OUT@@";
constexpr char kUserDefinedNamePrefix[] = "_u";  // Defined in GLSLANG/ShaderLang.h

template <size_t N>
constexpr size_t ConstStrLen(const char (&)[N])
{
    static_assert(N > 0, "C++ shouldn't allow N to be zero");
    
    // The length of a string defined as a char array is the size of the array minus 1 (the
    // terminating '\0').
    return N - 1;
}

std::string GetXfbBufferNameMtl(const uint32_t bufferIndex)
{
    return "xfbBuffer" + Str(bufferIndex);
}

}  // namespace

namespace mtl
{

void TranslatedShaderInfo::reset()
{
    metalShaderSource.clear();
    metalLibrary         = nil;
    hasUBOArgumentBuffer = false;
    for (mtl::SamplerBinding &binding : actualSamplerBindings)
    {
        binding.textureBinding = mtl::kMaxShaderSamplers;
        binding.samplerBinding = 0;
    }
    for (int &rwTextureBinding : actualImageBindings)
    {
        rwTextureBinding = -1;
    }
    for (uint32_t &binding : actualUBOBindings)
    {
        binding = mtl::kMaxShaderBuffers;
    }

    for (uint32_t &binding : actualXFBBindings)
    {
        binding = mtl::kMaxShaderBuffers;
    }
}

// Original mapping of front end from sampler name to multiple sampler slots (in form of
// slot:count pair)
using OriginalSamplerBindingMap =
    std::unordered_map<std::string, std::vector<std::pair<uint32_t, uint32_t>>>;

bool MappedSamplerNameNeedsUserDefinedPrefix(const std::string &originalName)
{
    return originalName.find('.') == std::string::npos;
}

static std::string MSLGetMappedSamplerName(const std::string &originalName)
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

void MSLGetShaderSource(const gl::Context *context,
                        const gl::ProgramState &programState,
                        const gl::ProgramLinkedResources &resources,
                        gl::ShaderMap<std::string> *shaderSourcesOut)
{
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        gl::Shader *glShader            = programState.getAttachedShader(shaderType);
        (*shaderSourcesOut)[shaderType] = glShader ? glShader->getTranslatedSource(context) : "";
    }
}

void GetAssignedSamplerBindings(const sh::TranslatorMetalReflection *reflection,
                                const OriginalSamplerBindingMap &originalBindings,
                                std::unordered_set<std::string> &structSamplers,
                                std::array<SamplerBinding, mtl::kMaxGLSamplerBindings> *bindings)
{
    for (auto &sampler : reflection->getSamplerBindings())
    {
        const std::string &name          = sampler.first;
        const uint32_t actualSamplerSlot = (uint32_t)reflection->getSamplerBinding(name);
        const uint32_t actualTextureSlot = (uint32_t)reflection->getTextureBinding(name);

        // Assign sequential index for subsequent array elements
        const bool structSampler = structSamplers.find(name) != structSamplers.end();
        const std::string mappedName =
            structSampler ? name : MSLGetMappedSamplerName(sh::kUserDefinedNamePrefix + name);
        auto original = originalBindings.find(mappedName);
        if (original != originalBindings.end())
        {
            const std::vector<std::pair<uint32_t, uint32_t>> &resOrignalBindings =
                originalBindings.at(mappedName);
            uint32_t currentTextureSlot = actualTextureSlot;
            uint32_t currentSamplerSlot = actualSamplerSlot;
            for (const std::pair<uint32_t, uint32_t> &originalBindingRange : resOrignalBindings)
            {
                SamplerBinding &actualBinding = bindings->at(originalBindingRange.first);
                actualBinding.textureBinding  = currentTextureSlot;
                actualBinding.samplerBinding  = currentSamplerSlot;

                currentTextureSlot += originalBindingRange.second;
                currentSamplerSlot += originalBindingRange.second;
            }
        }
    }
}

sh::TranslatorMetalReflection *getReflectionFromShader(gl::Shader *shader)
{
    ShaderMtl *shaderInstance = static_cast<ShaderMtl *>(shader->getImplementation());
    return shaderInstance->getTranslatorMetalReflection();
}

std::string updateShaderAttributes(std::string shaderSourceIn, const gl::ProgramState &programState)
{
    // Build string to attrib map.
    const auto &programAttributes = programState.getProgramInputs();
    std::ostringstream stream;
    std::unordered_map<std::string, uint32_t> attributeBindings;
    for (auto &attribute : programAttributes)
    {
        const int regs = gl::VariableRegisterCount(attribute.type);
        if (regs > 1)
        {
            for (int i = 0; i < regs; i++)
            {
                stream.str("");
                stream << " " << kUserDefinedNamePrefix << attribute.name << "_"
                       << std::to_string(i) << sh::kUnassignedAttributeString;
                attributeBindings.insert({std::string(stream.str()), i + attribute.location});
            }
        }
        else
        {
            stream.str("");
            stream << " " << kUserDefinedNamePrefix << attribute.name
                   << sh::kUnassignedAttributeString;
            attributeBindings.insert({std::string(stream.str()), attribute.location});
            stream.str("");
        }
    }
    // Rewrite attributes
    std::string outputSource = shaderSourceIn;
    for (auto it = attributeBindings.begin(); it != attributeBindings.end(); ++it)
    {
        std::size_t attribFound = outputSource.find(it->first);
        if (attribFound != std::string::npos)
        {
            stream.str("");
            stream << "[[attribute(" << it->second << ")]]";
            outputSource = outputSource.replace(
                attribFound + it->first.length() - strlen(sh::kUnassignedAttributeString),
                strlen(sh::kUnassignedAttributeString), stream.str());
        }
    }
    return outputSource;
}

std::string SubstituteTransformFeedbackMarkers(const std::string &originalSource,
                                               const std::string &xfbBindings,
                                               const std::string &xfbOut)
{
    const size_t xfbBindingsMarkerStart = originalSource.find(kXfbBindingsMarker);
    bool hasBindingsMarker              = xfbBindingsMarkerStart != std::string::npos;
    const size_t xfbBindingsMarkerEnd   = xfbBindingsMarkerStart + ConstStrLen(kXfbBindingsMarker);

    const size_t xfbOutMarkerStart = originalSource.find(kXfbOutMarker, xfbBindingsMarkerStart);
    bool hasOutMarker              = xfbOutMarkerStart != std::string::npos;
    const size_t xfbOutMarkerEnd   = xfbOutMarkerStart + ConstStrLen(kXfbOutMarker);

    // The shader is the following form:
    //
    // ..part1..
    // @@ XFB-BINDINGS @@
    // ..part2..
    // @@ XFB-OUT @@;
    // ..part3..
    //
    // Construct the string by concatenating these five pieces, replacing the markers with the given
    // values.
    std::string result;
    if (hasBindingsMarker && hasOutMarker)
    {
        result.append(&originalSource[0], &originalSource[xfbBindingsMarkerStart]);
        result.append(xfbBindings);
        result.append(&originalSource[xfbBindingsMarkerEnd], &originalSource[xfbOutMarkerStart]);
        result.append(xfbOut);
        result.append(&originalSource[xfbOutMarkerEnd], &originalSource[originalSource.size()]);
        return result;
    }
    return originalSource;
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
                result << "        ";
                result << "ANGLE_"
                       << "xfbBuffer" << bufferIndex << "["
                       << "ANGLE_" << std::string(sh::kUniformsVar) << ".ANGLE_xfbBufferOffsets["
                       << bufferIndex
                       << "] + (gl_VertexID + (ANGLE_instanceIdMod - ANGLE_baseInstance) * "
                       << "ANGLE_" << std::string(sh::kUniformsVar)
                       << ".ANGLE_xfbVerticesPerInstance) * " << stride << " + " << offset << "] = "
                       << "as_type<float>"
                       << "("
                       << "ANGLE_vertexOut.";
                if (!varying.isBuiltIn())
                {
                    result << kUserDefinedNamePrefix;
                }
                result << varying.name;

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

void GenerateTransformFeedbackEmulationOutputs(
    const gl::ProgramState &programState,
    std::string *vertexShader,
    std::array<uint32_t, kMaxShaderXFBs> *xfbBindingRemapOut)
{
    const std::vector<gl::TransformFeedbackVarying> &varyings =
        programState.getLinkedTransformFeedbackVaryings();
    const std::vector<GLsizei> &bufferStrides = programState.getTransformFeedbackStrides();
    const bool isInterleaved =
        programState.getTransformFeedbackBufferMode() == GL_INTERLEAVED_ATTRIBS;
    const size_t bufferCount = isInterleaved ? 1 : varyings.size();

    std::vector<std::string> xfbIndices(bufferCount);

    std::string xfbBindings;

    for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
    {
        const std::string xfbBinding = Str(0);
        xfbIndices[bufferIndex]      = Str(bufferIndex);

        std::string bufferName = GetXfbBufferNameMtl(bufferIndex);

        xfbBindings += ", ";
        // TODO: offset from last used buffer binding from front end
        // XFB buffer is allocated slot starting from last discrete Metal buffer slot.
        uint32_t bindingPoint               = kMaxShaderBuffers - 1 - bufferIndex;
        xfbBindingRemapOut->at(bufferIndex) = bindingPoint;
        xfbBindings +=
            "device float* ANGLE_" + bufferName + " [[buffer(" + Str(bindingPoint) + ")]]";
    }

    std::string xfbOut  = "#if TRANSFORM_FEEDBACK_ENABLED\n    {\n";
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
    xfbOut += "    }\n#endif\n";

    *vertexShader = SubstituteTransformFeedbackMarkers(*vertexShader, xfbBindings, xfbOut);
}

angle::Result MTLGetMSL(const gl::Context *glContext,
                        const gl::ProgramState &programState,
                        const gl::Caps &glCaps,
                        const gl::ShaderMap<std::string> &shaderSources,
                        gl::ShaderMap<TranslatedShaderInfo> *mslShaderInfoOut,
                        gl::ShaderMap<std::string> *mslCodeOut,
                        size_t xfbBufferCount)
{
    // Retrieve original uniform buffer bindings generated by front end. We will need to do a remap.
    std::unordered_map<std::string, uint32_t> uboOriginalBindings;
    const std::vector<gl::InterfaceBlock> &blocks = programState.getUniformBlocks();
    for (uint32_t bufferIdx = 0; bufferIdx < blocks.size(); ++bufferIdx)
    {
        const gl::InterfaceBlock &block = blocks[bufferIdx];
        if (!uboOriginalBindings.count(block.name))
        {
            uboOriginalBindings[block.name] = bufferIdx;
        }
    }
    // Retrieve original sampler bindings produced by front end.
    OriginalSamplerBindingMap originalSamplerBindings;
    const std::vector<gl::SamplerBinding> &samplerBindings = programState.getSamplerBindings();
    const std::vector<gl::LinkedUniform> &uniforms         = programState.getUniforms();

    std::unordered_set<std::string> structSamplers = {};

    for (uint32_t textureIndex = 0; textureIndex < samplerBindings.size(); ++textureIndex)
    {
        const gl::SamplerBinding &samplerBinding = samplerBindings[textureIndex];
        uint32_t uniformIndex = programState.getUniformIndexFromSamplerIndex(textureIndex);
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];
        bool isSamplerInStruct        = samplerUniform.name.find('.') != std::string::npos;
        std::string mappedSamplerName = isSamplerInStruct
                                            ? MSLGetMappedSamplerName(samplerUniform.name)
                                            : MSLGetMappedSamplerName(samplerUniform.mappedName);
        // These need to be prefixed later seperately
        if (isSamplerInStruct)
            structSamplers.insert(mappedSamplerName);
        originalSamplerBindings[mappedSamplerName].push_back(
            {textureIndex, static_cast<uint32_t>(samplerBinding.boundTextureUnits.size())});
    }
    for (gl::ShaderType type : {gl::ShaderType::Vertex, gl::ShaderType::Fragment})
    {
        std::string source;
        if (type == gl::ShaderType::Vertex)
        {
            source = updateShaderAttributes(shaderSources[type], programState);
            // Write transform feedback output code.
            if (!source.empty())
            {
                if (programState.getLinkedTransformFeedbackVaryings().empty())
                {
                    source = SubstituteTransformFeedbackMarkers(source, "", "");
                }
                else
                {
                    GenerateTransformFeedbackEmulationOutputs(
                        programState, &source, &(*mslShaderInfoOut)[type].actualXFBBindings);
                }
            }
        }
        else
        {
            source = shaderSources[type];
        }
        (*mslCodeOut)[type]                             = source;
        (*mslShaderInfoOut)[type].metalShaderSource     = source;
        gl::Shader *shader                              = programState.getAttachedShader(type);
        const sh::TranslatorMetalReflection *reflection = getReflectionFromShader(shader);
        if (reflection->hasUBOs)
        {
            (*mslShaderInfoOut)[type].hasUBOArgumentBuffer = true;

            for (auto &uboBinding : reflection->getUniformBufferBindings())
            {
                const std::string &uboName         = uboBinding.first;
                const sh::UBOBindingInfo &bindInfo = uboBinding.second;
                const uint32_t uboBindIndex        = static_cast<uint32_t>(bindInfo.bindIndex);
                const uint32_t uboArraySize        = static_cast<uint32_t>(bindInfo.arraySize);
                const uint32_t originalBinding     = uboOriginalBindings.at(uboName);
                uint32_t currentSlot               = static_cast<uint>(uboBindIndex);
                for (uint32_t i = 0; i < uboArraySize; ++i)
                {
                    // Use consecutive slot for member in array
                    (*mslShaderInfoOut)[type].actualUBOBindings[originalBinding + i] =
                        currentSlot + i;
                }
            }
        }
        // Retrieve automatic texture slot assignments
        if (originalSamplerBindings.size() > 0)
        {
            GetAssignedSamplerBindings(reflection, originalSamplerBindings, structSamplers,
                                       &mslShaderInfoOut->at(type).actualSamplerBindings);
        }
        for (uint32_t i = 0; i < kMaxShaderImages; ++i)
        {
            mslShaderInfoOut->at(type).actualImageBindings[i] = reflection->getRWTextureBinding(i);
        }
        (*mslShaderInfoOut)[type].hasInvariantOrAtan =
            reflection->hasAtan || reflection->hasInvariance;
    }
    return angle::Result::Continue;
}

uint MslGetShaderShadowCompareMode(GLenum mode, GLenum func)
{
    // See SpirvToMslCompiler::emit_header()
    if (mode == GL_NONE)
    {
        return 0;
    }
    else
    {
        switch (func)
        {
            case GL_LESS:
                return 1;
            case GL_LEQUAL:
                return 2;
            case GL_GREATER:
                return 3;
            case GL_GEQUAL:
                return 4;
            case GL_NEVER:
                return 5;
            case GL_ALWAYS:
                return 6;
            case GL_EQUAL:
                return 7;
            case GL_NOTEQUAL:
                return 8;
            default:
                UNREACHABLE();
                return 1;
        }
    }
}

}  // namespace mtl
}  // namespace rx

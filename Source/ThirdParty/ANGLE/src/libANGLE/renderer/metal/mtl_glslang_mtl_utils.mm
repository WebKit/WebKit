//
//  mgl_glslang_mtl_utils.m
//  ANGLE (static)
//
//  Created by Kyle Piddington on 11/10/20.
//

#import <Foundation/Foundation.h>

#include "compiler/translator/TranslatorMetalDirect.h"
#include "libANGLE/renderer/metal/ShaderMtl.h"
#include "libANGLE/renderer/metal/mtl_glslang_mtl_utils.h"

namespace rx
{
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

void MSLGetShaderSource(const gl::ProgramState &programState,
                        const gl::ProgramLinkedResources &resources,
                        gl::ShaderMap<std::string> *shaderSourcesOut,
                        ShaderMapInterfaceVariableInfoMap *variableInfoMapOut)
{
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        gl::Shader *glShader            = programState.getAttachedShader(shaderType);
        (*shaderSourcesOut)[shaderType] = glShader ? glShader->getTranslatedSource() : "";
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

sh::TranslatorMetalReflection *getReflectionFromCompiler(gl::Compiler *compiler,
                                                         gl::ShaderType type)
{
    auto compilerInstance      = compiler->getInstance(type);
    sh::TShHandleBase *base    = static_cast<sh::TShHandleBase *>(compilerInstance.getHandle());
    auto translatorMetalDirect = base->getAsTranslatorMetalDirect();
    compiler->putInstance(std::move(compilerInstance));
    return translatorMetalDirect->getTranslatorMetalReflection();
}

angle::Result GlslangGetMSL(const gl::Context *glContext,
                            const gl::ProgramState &programState,
                            const gl::Caps &glCaps,
                            const gl::ShaderMap<std::string> &shaderSources,
                            const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
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
        if (!uboOriginalBindings.count(block.mappedName))
        {
            uboOriginalBindings[block.mappedName] = bufferIdx;
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
        (*mslCodeOut)[type]                         = shaderSources[type];
        (*mslShaderInfoOut)[type].metalShaderSource = shaderSources[type];
        gl::Shader *shader                        = programState.getAttachedShader(type);
        sh::TranslatorMetalReflection *reflection = getReflectionFromShader(shader);
        // Retrieve automatic texture slot assignments
        if (originalSamplerBindings.size() > 0)
        {
            GetAssignedSamplerBindings(reflection, originalSamplerBindings, structSamplers,
                                       &mslShaderInfoOut->at(type).actualSamplerBindings);
        }
        reflection->reset();
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

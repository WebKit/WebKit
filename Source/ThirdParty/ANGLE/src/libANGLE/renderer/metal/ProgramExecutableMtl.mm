//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutableMtl.cpp: Implementation of ProgramExecutableMtl.

#include "libANGLE/renderer/metal/ProgramExecutableMtl.h"

#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/blocklayoutMetal.h"

namespace rx
{
namespace
{
bool CompareBlockInfo(const sh::BlockMemberInfo &a, const sh::BlockMemberInfo &b)
{
    return a.offset < b.offset;
}

size_t GetAlignmentOfUniformGroup(sh::BlockLayoutMap *blockLayoutMap)
{
    size_t align = 1;
    for (auto layoutIter = blockLayoutMap->begin(); layoutIter != blockLayoutMap->end();
         ++layoutIter)
    {
        align = std::max(mtl::GetMetalAlignmentForGLType(layoutIter->second.type), align);
    }
    return align;
}

void InitDefaultUniformBlock(const std::vector<sh::Uniform> &uniforms,
                             sh::BlockLayoutMap *blockLayoutMapOut,
                             size_t *blockSizeOut)
{
    if (uniforms.empty())
    {
        *blockSizeOut = 0;
        return;
    }

    mtl::BlockLayoutEncoderMTL blockEncoder;
    sh::GetActiveUniformBlockInfo(uniforms, "", &blockEncoder, blockLayoutMapOut);
    size_t blockAlign = GetAlignmentOfUniformGroup(blockLayoutMapOut);
    size_t blockSize  = roundUp(blockEncoder.getCurrentOffset(), blockAlign);

    // TODO(jmadill): I think we still need a valid block for the pipeline even if zero sized.
    if (blockSize == 0)
    {
        *blockSizeOut = 0;
        return;
    }

    *blockSizeOut = blockSize;
    return;
}

class Std140BlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override { return new sh::Std140BlockEncoder(); }
};

class Std430BlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override { return new sh::Std430BlockEncoder(); }
};

class StdMTLBLockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override { return new mtl::BlockLayoutEncoderMTL(); }
};
}  // anonymous namespace
DefaultUniformBlockMtl::DefaultUniformBlockMtl() {}

DefaultUniformBlockMtl::~DefaultUniformBlockMtl() = default;

ProgramExecutableMtl::ProgramExecutableMtl(const gl::ProgramExecutable *executable)
    : ProgramExecutableImpl(executable), mProgramHasFlatAttributes(false)
{}

ProgramExecutableMtl::~ProgramExecutableMtl() {}

void ProgramExecutableMtl::destroy(const gl::Context *context)
{
    reset(mtl::GetImpl(context));
}

void ProgramExecutableMtl::reset(ContextMtl *context)
{
    mProgramHasFlatAttributes = false;

    for (auto &block : mDefaultUniformBlocks)
    {
        block.uniformLayout.clear();
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mMslShaderTranslateInfo[shaderType].reset();
        mCurrentShaderVariants[shaderType] = nullptr;
    }
    mMslXfbOnlyVertexShaderInfo.reset();

    for (ProgramShaderObjVariantMtl &var : mVertexShaderVariants)
    {
        var.reset(context);
    }
    for (ProgramShaderObjVariantMtl &var : mFragmentShaderVariants)
    {
        var.reset(context);
    }
}

angle::Result ProgramExecutableMtl::load(ContextMtl *contextMtl, gl::BinaryInputStream *stream)
{
    loadTranslatedShaders(stream);
    loadShaderInternalInfo(stream);
    ANGLE_TRY(loadDefaultUniformBlocksInfo(contextMtl, stream));
    return loadInterfaceBlockInfo(stream);
}

void ProgramExecutableMtl::save(gl::BinaryOutputStream *stream)
{
    saveTranslatedShaders(stream);
    saveShaderInternalInfo(stream);
    saveDefaultUniformBlocksInfo(stream);
    saveInterfaceBlockInfo(stream);
}

void ProgramExecutableMtl::saveInterfaceBlockInfo(gl::BinaryOutputStream *stream)
{
    // Serializes the uniformLayout data of mDefaultUniformBlocks
    // First, save the number of Ib's to process
    stream->writeInt<unsigned int>((unsigned int)mUniformBlockConversions.size());
    // Next, iterate through all of the conversions.
    for (auto conversion : mUniformBlockConversions)
    {
        // Write the name of the conversion
        stream->writeString(conversion.first);
        // Write the number of entries in the conversion
        const UBOConversionInfo &conversionInfo = conversion.second;
        unsigned int numEntries                 = (unsigned int)(conversionInfo.stdInfo().size());
        stream->writeInt<unsigned int>(numEntries);
        for (unsigned int i = 0; i < numEntries; ++i)
        {
            gl::WriteBlockMemberInfo(stream, conversionInfo.stdInfo()[i]);
        }
        for (unsigned int i = 0; i < numEntries; ++i)
        {
            gl::WriteBlockMemberInfo(stream, conversionInfo.metalInfo()[i]);
        }
        stream->writeInt<size_t>(conversionInfo.stdSize());
        stream->writeInt<size_t>(conversionInfo.metalSize());
    }
}

angle::Result ProgramExecutableMtl::loadInterfaceBlockInfo(gl::BinaryInputStream *stream)
{
    mUniformBlockConversions.clear();
    // First, load the number of Ib's to process
    uint32_t numBlocks = stream->readInt<uint32_t>();
    // Next, iterate through all of the conversions.
    for (uint32_t nBlocks = 0; nBlocks < numBlocks; ++nBlocks)
    {
        // Read the name of the conversion
        std::string blockName = stream->readString();
        // Read the number of entries in the conversion
        std::vector<sh::BlockMemberInfo> stdInfo, metalInfo;
        uint32_t numEntries = stream->readInt<uint32_t>();
        stdInfo.reserve(numEntries);
        metalInfo.reserve(numEntries);
        for (uint32_t i = 0; i < numEntries; ++i)
        {
            stdInfo.push_back(sh::BlockMemberInfo());
            gl::LoadBlockMemberInfo(stream, &(stdInfo[i]));
        }
        for (uint32_t i = 0; i < numEntries; ++i)
        {
            metalInfo.push_back(sh::BlockMemberInfo());
            gl::LoadBlockMemberInfo(stream, &(metalInfo[i]));
        }
        size_t stdSize   = stream->readInt<size_t>();
        size_t metalSize = stream->readInt<size_t>();
        mUniformBlockConversions.insert(
            {blockName, UBOConversionInfo(stdInfo, metalInfo, stdSize, metalSize)});
    }
    return angle::Result::Continue;
}

void ProgramExecutableMtl::saveDefaultUniformBlocksInfo(gl::BinaryOutputStream *stream)
{
    // Serializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = mDefaultUniformBlocks[shaderType].uniformLayout.size();
        stream->writeInt<size_t>(uniformCount);
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo &blockInfo =
                mDefaultUniformBlocks[shaderType].uniformLayout[uniformIndex];
            gl::WriteBlockMemberInfo(stream, blockInfo);
        }
    }

    // Serializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mDefaultUniformBlocks[shaderType].uniformData.size());
    }
}

angle::Result ProgramExecutableMtl::loadDefaultUniformBlocksInfo(mtl::Context *context,
                                                                 gl::BinaryInputStream *stream)
{
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);
    // Deserializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = stream->readInt<size_t>();
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo blockInfo;
            gl::LoadBlockMemberInfo(stream, &blockInfo);
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(blockInfo);
        }
    }

    // Deserializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        requiredBufferSize[shaderType] = stream->readInt<size_t>();
    }

    return resizeDefaultUniformBlocksMemory(context, requiredBufferSize);
}

void ProgramExecutableMtl::saveShaderInternalInfo(gl::BinaryOutputStream *stream)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt<int>(mMslShaderTranslateInfo[shaderType].hasUBOArgumentBuffer);
        for (const mtl::SamplerBinding &binding :
             mMslShaderTranslateInfo[shaderType].actualSamplerBindings)
        {
            stream->writeInt<uint32_t>(binding.textureBinding);
            stream->writeInt<uint32_t>(binding.samplerBinding);
        }
        for (int rwTextureBinding : mMslShaderTranslateInfo[shaderType].actualImageBindings)
        {
            stream->writeInt<int>(rwTextureBinding);
        }

        for (uint32_t uboBinding : mMslShaderTranslateInfo[shaderType].actualUBOBindings)
        {
            stream->writeInt<uint32_t>(uboBinding);
        }
        stream->writeBool(mMslShaderTranslateInfo[shaderType].hasInvariant);
    }
    for (size_t xfbBindIndex = 0; xfbBindIndex < mtl::kMaxShaderXFBs; xfbBindIndex++)
    {
        stream->writeInt(
            mMslShaderTranslateInfo[gl::ShaderType::Vertex].actualXFBBindings[xfbBindIndex]);
    }

    // Write out XFB info.
    {
        stream->writeInt<int>(mMslXfbOnlyVertexShaderInfo.hasUBOArgumentBuffer);
        for (mtl::SamplerBinding &binding : mMslXfbOnlyVertexShaderInfo.actualSamplerBindings)
        {
            stream->writeInt<uint32_t>(binding.textureBinding);
            stream->writeInt<uint32_t>(binding.samplerBinding);
        }
        for (int rwTextureBinding : mMslXfbOnlyVertexShaderInfo.actualImageBindings)
        {
            stream->writeInt<int>(rwTextureBinding);
        }

        for (uint32_t &uboBinding : mMslXfbOnlyVertexShaderInfo.actualUBOBindings)
        {
            stream->writeInt<uint32_t>(uboBinding);
        }
        for (size_t xfbBindIndex = 0; xfbBindIndex < mtl::kMaxShaderXFBs; xfbBindIndex++)
        {
            stream->writeInt(mMslXfbOnlyVertexShaderInfo.actualXFBBindings[xfbBindIndex]);
        }
    }

    stream->writeBool(mProgramHasFlatAttributes);
}

void ProgramExecutableMtl::loadShaderInternalInfo(gl::BinaryInputStream *stream)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mMslShaderTranslateInfo[shaderType].hasUBOArgumentBuffer = stream->readInt<int>() != 0;
        for (mtl::SamplerBinding &binding :
             mMslShaderTranslateInfo[shaderType].actualSamplerBindings)
        {
            binding.textureBinding = stream->readInt<uint32_t>();
            binding.samplerBinding = stream->readInt<uint32_t>();
        }
        for (int &rwTextureBinding : mMslShaderTranslateInfo[shaderType].actualImageBindings)
        {
            rwTextureBinding = stream->readInt<int>();
        }

        for (uint32_t &uboBinding : mMslShaderTranslateInfo[shaderType].actualUBOBindings)
        {
            uboBinding = stream->readInt<uint32_t>();
        }
        mMslShaderTranslateInfo[shaderType].hasInvariant = stream->readBool();
    }

    for (size_t xfbBindIndex = 0; xfbBindIndex < mtl::kMaxShaderXFBs; xfbBindIndex++)
    {
        stream->readInt(
            &mMslShaderTranslateInfo[gl::ShaderType::Vertex].actualXFBBindings[xfbBindIndex]);
    }
    // Load Transform Feedback info
    {
        mMslXfbOnlyVertexShaderInfo.hasUBOArgumentBuffer = stream->readInt<int>() != 0;
        for (mtl::SamplerBinding &binding : mMslXfbOnlyVertexShaderInfo.actualSamplerBindings)
        {
            binding.textureBinding = stream->readInt<uint32_t>();
            binding.samplerBinding = stream->readInt<uint32_t>();
        }
        for (int &rwTextureBinding : mMslXfbOnlyVertexShaderInfo.actualImageBindings)
        {
            rwTextureBinding = stream->readInt<int>();
        }

        for (uint32_t &uboBinding : mMslXfbOnlyVertexShaderInfo.actualUBOBindings)
        {
            uboBinding = stream->readInt<uint32_t>();
        }
        for (size_t xfbBindIndex = 0; xfbBindIndex < mtl::kMaxShaderXFBs; xfbBindIndex++)
        {
            stream->readInt(&mMslXfbOnlyVertexShaderInfo.actualXFBBindings[xfbBindIndex]);
        }
        mMslXfbOnlyVertexShaderInfo.metalLibrary = nullptr;
    }

    mProgramHasFlatAttributes = stream->readBool();
}

void ProgramExecutableMtl::saveTranslatedShaders(gl::BinaryOutputStream *stream)
{
    auto writeTranslatedSource = [](gl::BinaryOutputStream *stream,
                                    const mtl::TranslatedShaderInfo &shaderInfo) {
        const std::string &source =
            shaderInfo.metalShaderSource ? *shaderInfo.metalShaderSource : std::string();
        stream->writeString(source);
    };

    // Write out shader sources for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        writeTranslatedSource(stream, mMslShaderTranslateInfo[shaderType]);
    }
    writeTranslatedSource(stream, mMslXfbOnlyVertexShaderInfo);
}

void ProgramExecutableMtl::loadTranslatedShaders(gl::BinaryInputStream *stream)
{
    auto readTranslatedSource = [](gl::BinaryInputStream *stream,
                                   mtl::TranslatedShaderInfo &shaderInfo) {
        std::string source = stream->readString();
        if (!source.empty())
        {
            shaderInfo.metalShaderSource = std::make_shared<const std::string>(std::move(source));
        }
        else
        {
            shaderInfo.metalShaderSource = nullptr;
        }
    };

    // Read in shader sources for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        readTranslatedSource(stream, mMslShaderTranslateInfo[shaderType]);
    }
    readTranslatedSource(stream, mMslXfbOnlyVertexShaderInfo);
}

void ProgramExecutableMtl::linkUpdateHasFlatAttributes(
    const gl::SharedCompiledShaderState &vertexShader)
{
    mProgramHasFlatAttributes = false;

    const auto &programInputs = mExecutable->getProgramInputs();
    for (auto &attribute : programInputs)
    {
        if (attribute.getInterpolation() == sh::INTERPOLATION_FLAT)
        {
            mProgramHasFlatAttributes = true;
            return;
        }
    }

    const auto &flatVaryings = vertexShader->outputVaryings;
    for (auto &attribute : flatVaryings)
    {
        if (attribute.interpolation == sh::INTERPOLATION_FLAT)
        {
            mProgramHasFlatAttributes = true;
            return;
        }
    }
}

angle::Result ProgramExecutableMtl::initDefaultUniformBlocks(
    mtl::Context *context,
    const gl::ShaderMap<gl::SharedCompiledShaderState> &shaders)
{
    // Process vertex and fragment uniforms into std140 packing.
    gl::ShaderMap<sh::BlockLayoutMap> layoutMap;
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        const gl::SharedCompiledShaderState &shader = shaders[shaderType];
        if (shader)
        {
            const std::vector<sh::Uniform> &uniforms = shader->uniforms;
            InitDefaultUniformBlock(uniforms, &layoutMap[shaderType],
                                    &requiredBufferSize[shaderType]);
            // Set up block conversion buffer
            initUniformBlocksRemapper(shader);
        }
    }

    // Init the default block layout info.
    const auto &uniforms         = mExecutable->getUniforms();
    const auto &uniformNames     = mExecutable->getUniformNames();
    const auto &uniformLocations = mExecutable->getUniformLocations();
    for (size_t locSlot = 0; locSlot < uniformLocations.size(); ++locSlot)
    {
        const gl::VariableLocation &location = uniformLocations[locSlot];
        gl::ShaderMap<sh::BlockMemberInfo> layoutInfo;

        if (location.used() && !location.ignored)
        {
            const gl::LinkedUniform &uniform = uniforms[location.index];
            if (uniform.isInDefaultBlock() && !uniform.isSampler() && !uniform.isImage())
            {
                std::string uniformName = uniformNames[location.index];
                if (uniform.isArray())
                {
                    // Gets the uniform name without the [0] at the end.
                    uniformName = gl::ParseResourceName(uniformName, nullptr);
                }

                bool found = false;

                for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
                {
                    auto it = layoutMap[shaderType].find(uniformName);
                    if (it != layoutMap[shaderType].end())
                    {
                        found                  = true;
                        layoutInfo[shaderType] = it->second;
                    }
                }

                ASSERT(found);
            }
        }

        for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
        {
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(layoutInfo[shaderType]);
        }
    }

    return resizeDefaultUniformBlocksMemory(context, requiredBufferSize);
}

angle::Result ProgramExecutableMtl::resizeDefaultUniformBlocksMemory(
    mtl::Context *context,
    const gl::ShaderMap<size_t> &requiredBufferSize)
{
    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if (requiredBufferSize[shaderType] > 0)
        {
            ASSERT(requiredBufferSize[shaderType] <= mtl::kDefaultUniformsMaxSize);

            if (!mDefaultUniformBlocks[shaderType].uniformData.resize(
                    requiredBufferSize[shaderType]))
            {
                ANGLE_MTL_CHECK(context, false, GL_OUT_OF_MEMORY);
            }

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType].uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    return angle::Result::Continue;
}

// TODO(angleproject:7979) Upgrade ANGLE Uniform buffer remapper to compute shaders
void ProgramExecutableMtl::initUniformBlocksRemapper(const gl::SharedCompiledShaderState &shader)
{
    std::unordered_map<std::string, UBOConversionInfo> conversionMap;
    const std::vector<sh::InterfaceBlock> ibs = shader->uniformBlocks;
    for (size_t i = 0; i < ibs.size(); ++i)
    {

        const sh::InterfaceBlock &ib = ibs[i];
        if (mUniformBlockConversions.find(ib.name) == mUniformBlockConversions.end())
        {
            mtl::BlockLayoutEncoderMTL metalEncoder;
            sh::BlockLayoutEncoder *encoder;
            switch (ib.layout)
            {
                case sh::BLOCKLAYOUT_PACKED:
                case sh::BLOCKLAYOUT_SHARED:
                case sh::BLOCKLAYOUT_STD140:
                {
                    Std140BlockLayoutEncoderFactory factory;
                    encoder = factory.makeEncoder();
                }
                break;
                case sh::BLOCKLAYOUT_STD430:
                {
                    Std430BlockLayoutEncoderFactory factory;
                    encoder = factory.makeEncoder();
                }
                break;
            }
            sh::BlockLayoutMap blockLayoutMapOut, stdMapOut;

            sh::GetInterfaceBlockInfo(ib.fields, "", &metalEncoder, &blockLayoutMapOut);
            sh::GetInterfaceBlockInfo(ib.fields, "", encoder, &stdMapOut);

            auto stdIterator = stdMapOut.begin();
            auto mtlIterator = blockLayoutMapOut.begin();

            std::vector<sh::BlockMemberInfo> stdConversions, mtlConversions;
            while (stdIterator != stdMapOut.end())
            {
                stdConversions.push_back(stdIterator->second);
                mtlConversions.push_back(mtlIterator->second);
                stdIterator++;
                mtlIterator++;
            }
            std::sort(stdConversions.begin(), stdConversions.end(), CompareBlockInfo);
            std::sort(mtlConversions.begin(), mtlConversions.end(), CompareBlockInfo);

            size_t stdSize    = encoder->getCurrentOffset();
            size_t metalAlign = GetAlignmentOfUniformGroup(&blockLayoutMapOut);
            size_t metalSize  = roundUp(metalEncoder.getCurrentOffset(), metalAlign);

            conversionMap.insert(
                {ib.name, UBOConversionInfo(stdConversions, mtlConversions, stdSize, metalSize)});
            SafeDelete(encoder);
        }
    }
    mUniformBlockConversions.insert(conversionMap.begin(), conversionMap.end());
}

}  // namespace rx

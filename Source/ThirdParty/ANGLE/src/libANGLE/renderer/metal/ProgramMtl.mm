//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramMtl.mm:
//    Implements the class methods for ProgramMtl.
//

#include "libANGLE/renderer/metal/ProgramMtl.h"

#include <TargetConditionals.h>

#include <sstream>

#include "common/debug.h"
#include "common/system_utils.h"

#include "libANGLE/Context.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/CompilerMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/TextureMtl.h"
#include "libANGLE/renderer/metal/blocklayoutMetal.h"
#include "libANGLE/renderer/metal/mtl_msl_utils.h"
#include "libANGLE/renderer/metal/mtl_utils.h"
#include "libANGLE/renderer/metal/renderermtl_utils.h"
#include "libANGLE/renderer/renderer_utils.h"
namespace rx
{

namespace
{

// Block Encoder Information

#define SHADER_ENTRY_NAME @"main0"
template <typename T>
class [[nodiscard]] ScopedAutoClearVector
{
  public:
    ScopedAutoClearVector(std::vector<T> *array) : mArray(*array) {}
    ~ScopedAutoClearVector() { mArray.clear(); }

  private:
    std::vector<T> &mArray;
};

// Copy matrix one column at a time
inline void copy_matrix(void *dst, const void *src, size_t srcStride, size_t dstStride, GLenum type)
{
    size_t elemSize      = mtl::GetMetalSizeForGLType(gl::VariableComponentType(type));
    const size_t dstRows = gl::VariableRowCount(type);
    const size_t dstCols = gl::VariableColumnCount(type);

    for (size_t col = 0; col < dstCols; col++)
    {
        size_t srcOffset = col * srcStride;
        memcpy(((uint8_t *)dst) + dstStride * col, (const uint8_t *)src + srcOffset,
               elemSize * dstRows);
    }
}

// Copy matrix one element at a time to transpose.
inline void copy_matrix_row_major(void *dst,
                                  const void *src,
                                  size_t srcStride,
                                  size_t dstStride,
                                  GLenum type)
{
    size_t elemSize      = mtl::GetMetalSizeForGLType(gl::VariableComponentType(type));
    const size_t dstRows = gl::VariableRowCount(type);
    const size_t dstCols = gl::VariableColumnCount(type);

    for (size_t col = 0; col < dstCols; col++)
    {
        for (size_t row = 0; row < dstRows; row++)
        {
            size_t srcOffset = row * srcStride + col * elemSize;
            memcpy((uint8_t *)dst + dstStride * col + row * elemSize,
                   (const uint8_t *)src + srcOffset, elemSize);
        }
    }
}
bool compareBlockInfo(const sh::BlockMemberInfo &a, const sh::BlockMemberInfo &b)
{
    return a.offset < b.offset;
}

// TODO(angleproject:7979) Upgrade ANGLE Uniform buffer remapper to compute shaders
angle::Result ConvertUniformBufferData(ContextMtl *contextMtl,
                                       const UBOConversionInfo &blockConversionInfo,
                                       mtl::BufferPool *dynamicBuffer,
                                       const uint8_t *sourceData,
                                       size_t sizeToCopy,
                                       mtl::BufferRef *bufferOut,
                                       size_t *bufferOffsetOut)
{
    uint8_t *dst = nullptr;
    dynamicBuffer->releaseInFlightBuffers(contextMtl);

    // When converting a UBO buffer, we convert all of the data
    // supplied in a buffer at once (sizeToCopy = bufferMtl->size() - initial offset).
    // It's possible that a buffer could represent multiple instances of
    // a uniform block, so we loop over the number of block conversions we intend
    // to do.
    size_t numBlocksToCopy =
        (sizeToCopy + blockConversionInfo.stdSize() - 1) / blockConversionInfo.stdSize();
    size_t bytesToAllocate = numBlocksToCopy * blockConversionInfo.metalSize();
    ANGLE_TRY(dynamicBuffer->allocate(contextMtl, bytesToAllocate, &dst, bufferOut, bufferOffsetOut,
                                      nullptr));

    const std::vector<sh::BlockMemberInfo> &stdConversions = blockConversionInfo.stdInfo();
    const std::vector<sh::BlockMemberInfo> &mtlConversions = blockConversionInfo.metalInfo();
    for (size_t i = 0; i < numBlocksToCopy; ++i)
    {
        auto stdIterator = stdConversions.begin();
        auto mtlIterator = mtlConversions.begin();

        while (stdIterator != stdConversions.end())
        {
            for (int arraySize = 0; arraySize < stdIterator->arraySize; ++arraySize)
            {
                // For every entry in an array, calculate the offset based off of the
                // array element size.

                // Offset of a single entry is
                // blockIndex*blockSize + arrayOffset*arraySize + offset of field in base struct.
                // Fields are copied per block, per member, per array entry of member.

                size_t stdArrayOffset = stdIterator->arrayStride * arraySize;
                size_t mtlArrayOffset = mtlIterator->arrayStride * arraySize;

                if (gl::IsMatrixType(mtlIterator->type))
                {

                    void *dstMat = dst + mtlIterator->offset + mtlArrayOffset +
                                   blockConversionInfo.metalSize() * i;
                    const void *srcMat = sourceData + stdIterator->offset + stdArrayOffset +
                                         blockConversionInfo.stdSize() * i;
                    // Transpose matricies into column major order, if they're row major encoded.
                    if (stdIterator->isRowMajorMatrix)
                    {
                        copy_matrix_row_major(dstMat, srcMat, stdIterator->matrixStride,
                                              mtlIterator->matrixStride, mtlIterator->type);
                    }
                    else
                    {
                        copy_matrix(dstMat, srcMat, stdIterator->matrixStride,
                                    mtlIterator->matrixStride, mtlIterator->type);
                    }
                }
                // Compress bool from four bytes to one byte because bool values in GLSL
                // are uint-sized: ES 3.0 Section 2.12.6.3 "Uniform Buffer Object Storage".
                // Bools in metal are byte-sized. (Metal shading language spec Table 2.2)
                else if (gl::VariableComponentType(mtlIterator->type) == GL_BOOL)
                {
                    for (int boolCol = 0; boolCol < gl::VariableComponentCount(mtlIterator->type);
                         boolCol++)
                    {
                        const uint8_t *srcOffset =
                            (sourceData + stdIterator->offset + stdArrayOffset +
                             blockConversionInfo.stdSize() * i +
                             gl::VariableComponentSize(GL_BOOL) * boolCol);
                        unsigned int srcValue = *((unsigned int *)(srcOffset));
                        bool boolVal          = bool(srcValue);
                        memcpy(dst + mtlIterator->offset + mtlArrayOffset +
                                   blockConversionInfo.metalSize() * i + sizeof(bool) * boolCol,
                               &boolVal, sizeof(bool));
                    }
                }
                else
                {
                    memcpy(dst + mtlIterator->offset + mtlArrayOffset +
                               blockConversionInfo.metalSize() * i,
                           sourceData + stdIterator->offset + stdArrayOffset +
                               blockConversionInfo.stdSize() * i,
                           mtl::GetMetalSizeForGLType(mtlIterator->type));
                }
            }
            ++stdIterator;
            ++mtlIterator;
        }
    }

    ANGLE_TRY(dynamicBuffer->commit(contextMtl));
    return angle::Result::Continue;
}

void InitDefaultUniformBlock(const std::vector<sh::Uniform> &uniforms,
                             gl::Shader *shader,
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

    size_t blockSize = blockEncoder.getCurrentOffset();

    // TODO(jmadill): I think we still need a valid block for the pipeline even if zero sized.
    if (blockSize == 0)
    {
        *blockSizeOut = 0;
        return;
    }

    *blockSizeOut = blockSize;
    return;
}

inline NSDictionary<NSString *, NSObject *> *getDefaultSubstitutionDictionary()
{
    return @{};
}

template <typename T>
void UpdateDefaultUniformBlock(GLsizei count,
                               uint32_t arrayIndex,
                               int componentCount,
                               const T *v,
                               const sh::BlockMemberInfo &layoutInfo,
                               angle::MemoryBuffer *uniformData)
{
    UpdateDefaultUniformBlockWithElementSize(count, arrayIndex, componentCount, v, sizeof(T),
                                             layoutInfo, uniformData);
}

template <typename T>
void UpdateDefaultUniformBlockWithElementSize(GLsizei count,
                                              uint32_t arrayIndex,
                                              int componentCount,
                                              const T *v,
                                              size_t baseElementSize,
                                              const sh::BlockMemberInfo &layoutInfo,
                                              angle::MemoryBuffer *uniformData)
{
    const int elementSize = (int)(baseElementSize * componentCount);

    uint8_t *dst = uniformData->data() + layoutInfo.offset;
    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        uint32_t arrayOffset = arrayIndex * layoutInfo.arrayStride;
        uint8_t *writePtr    = dst + arrayOffset;
        ASSERT(writePtr + (elementSize * count) <= uniformData->data() + uniformData->size());
        memcpy(writePtr, v, elementSize * count);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        int maxIndex = arrayIndex + count;
        for (int writeIndex = arrayIndex, readIndex = 0; writeIndex < maxIndex;
             writeIndex++, readIndex++)
        {
            const int arrayOffset = writeIndex * layoutInfo.arrayStride;
            uint8_t *writePtr     = dst + arrayOffset;
            const T *readPtr      = v + (readIndex * componentCount);
            ASSERT(writePtr + elementSize <= uniformData->data() + uniformData->size());
            memcpy(writePtr, readPtr, elementSize);
        }
    }
}
template <typename T>
void ReadFromDefaultUniformBlock(int componentCount,
                                 uint32_t arrayIndex,
                                 T *dst,
                                 size_t elementSize,
                                 const sh::BlockMemberInfo &layoutInfo,
                                 const angle::MemoryBuffer *uniformData)
{
    ReadFromDefaultUniformBlockWithElementSize(componentCount, arrayIndex, dst, sizeof(T),
                                               layoutInfo, uniformData);
}

void ReadFromDefaultUniformBlockWithElementSize(int componentCount,
                                                uint32_t arrayIndex,
                                                void *dst,
                                                size_t baseElementSize,
                                                const sh::BlockMemberInfo &layoutInfo,
                                                const angle::MemoryBuffer *uniformData)
{
    ASSERT(layoutInfo.offset != -1);

    const size_t elementSize = (baseElementSize * componentCount);
    const uint8_t *source    = uniformData->data() + layoutInfo.offset;

    if (layoutInfo.arrayStride == 0 || (size_t)layoutInfo.arrayStride == elementSize)
    {
        const uint8_t *readPtr = source + arrayIndex * layoutInfo.arrayStride;
        memcpy(dst, readPtr, elementSize);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        const int arrayOffset  = arrayIndex * layoutInfo.arrayStride;
        const uint8_t *readPtr = source + arrayOffset;
        memcpy(dst, readPtr, elementSize);
    }
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

void InitArgumentBufferEncoder(mtl::Context *context,
                               id<MTLFunction> function,
                               uint32_t bufferIndex,
                               ProgramArgumentBufferEncoderMtl *encoder)
{
    encoder->metalArgBufferEncoder =
        mtl::adoptObjCObj([function newArgumentEncoderWithBufferIndex:bufferIndex]);
    if (encoder->metalArgBufferEncoder)
    {
        encoder->bufferPool.initialize(context, encoder->metalArgBufferEncoder.get().encodedLength,
                                       mtl::kArgumentBufferOffsetAlignment, 0);
    }
}

}  // namespace

// TODO(angleproject:7979) Upgrade ANGLE Uniform buffer remapper to compute shaders
void ProgramMtl::initUniformBlocksRemapper(gl::Shader *shader, const gl::Context *glContext)
{
    std::unordered_map<std::string, UBOConversionInfo> conversionMap;
    const std::vector<sh::InterfaceBlock> ibs = shader->getUniformBlocks(glContext);
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
            std::sort(stdConversions.begin(), stdConversions.end(), compareBlockInfo);
            std::sort(mtlConversions.begin(), mtlConversions.end(), compareBlockInfo);

            size_t stdSize   = encoder->getCurrentOffset();
            size_t metalSize = metalEncoder.getCurrentOffset();

            conversionMap.insert(
                {ib.name, UBOConversionInfo(stdConversions, mtlConversions, stdSize, metalSize)});
            SafeDelete(encoder);
        }
    }
    mUniformBlockConversions.insert(conversionMap.begin(), conversionMap.end());
}

// ProgramArgumentBufferEncoderMtl implementation
void ProgramArgumentBufferEncoderMtl::reset(ContextMtl *contextMtl)
{
    metalArgBufferEncoder = nil;
    bufferPool.destroy(contextMtl);
}

// ProgramShaderObjVariantMtl implementation
void ProgramShaderObjVariantMtl::reset(ContextMtl *contextMtl)
{
    metalShader = nil;

    uboArgBufferEncoder.reset(contextMtl);

    translatedSrcInfo = nullptr;
}

// ProgramMtl implementation
ProgramMtl::DefaultUniformBlock::DefaultUniformBlock() {}

ProgramMtl::DefaultUniformBlock::~DefaultUniformBlock() = default;

ProgramMtl::ProgramMtl(const gl::ProgramState &state)
    : ProgramImpl(state),
      mProgramHasFlatAttributes(false),
      mShadowCompareModes(),
      mMetalRenderPipelineCache(this),
      mAuxBufferPool(nullptr)
{}

ProgramMtl::~ProgramMtl() {}

void ProgramMtl::destroy(const gl::Context *context)
{
    auto contextMtl = mtl::GetImpl(context);
    if (mAuxBufferPool)
    {
        mAuxBufferPool->destroy(contextMtl);
        delete mAuxBufferPool;
        mAuxBufferPool = nullptr;
    }
    reset(contextMtl);
}

void ProgramMtl::reset(ContextMtl *context)
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
    if (mAuxBufferPool)
    {
        if (mAuxBufferPool->reset(context, mtl::kDefaultUniformsMaxSize * 2,
                                  mtl::kUniformBufferSettingOffsetMinAlignment,
                                  3) != angle::Result::Continue)
        {
            mAuxBufferPool->destroy(context);
            delete mAuxBufferPool;
            mAuxBufferPool = nullptr;
        }
    }
    mMetalRenderPipelineCache.clear();
}

void ProgramMtl::saveTranslatedShaders(gl::BinaryOutputStream *stream)
{
    // Write out shader sources for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeString(mMslShaderTranslateInfo[shaderType].metalShaderSource);
    }
    stream->writeString(mMslXfbOnlyVertexShaderInfo.metalShaderSource);
}

void ProgramMtl::loadTranslatedShaders(gl::BinaryInputStream *stream)
{
    // Read in shader sources for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mMslShaderTranslateInfo[shaderType].metalShaderSource = stream->readString();
    }
    mMslXfbOnlyVertexShaderInfo.metalShaderSource = stream->readString();
}

std::unique_ptr<rx::LinkEvent> ProgramMtl::load(const gl::Context *context,
                                                gl::BinaryInputStream *stream,
                                                gl::InfoLog &infoLog)
{

    return std::make_unique<LinkEventDone>(linkTranslatedShaders(context, stream, infoLog));
}

void ProgramMtl::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    saveTranslatedShaders(stream);
    saveShaderInternalInfo(stream);
    saveDefaultUniformBlocksInfo(stream);
    saveInterfaceBlockInfo(stream);
}

void ProgramMtl::setBinaryRetrievableHint(bool retrievable) {}

void ProgramMtl::setSeparable(bool separable)
{
    UNIMPLEMENTED();
}

std::unique_ptr<LinkEvent> ProgramMtl::link(const gl::Context *context,
                                            const gl::ProgramLinkedResources &resources,
                                            gl::InfoLog &infoLog,
                                            const gl::ProgramMergedVaryings &mergedVaryings)
{
    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources(context, resources);

    // NOTE(hqle): Parallelize linking.
    return std::make_unique<LinkEventDone>(linkImpl(context, resources, infoLog));
}

angle::Result ProgramMtl::linkImplDirect(const gl::Context *glContext,
                                         const gl::ProgramLinkedResources &resources,
                                         gl::InfoLog &infoLog)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);

    reset(contextMtl);
    ANGLE_TRY(initDefaultUniformBlocks(glContext));

    gl::ShaderMap<std::string> shaderSources;
    gl::ShaderMap<std::string> translatedMslShaders;
    mtl::MSLGetShaderSource(glContext, mState, resources, &shaderSources);

    ANGLE_TRY(mtl::MTLGetMSL(glContext, mState, contextMtl->getCaps(), shaderSources,
                             &mMslShaderTranslateInfo, &translatedMslShaders,
                             mState.getExecutable().getTransformFeedbackBufferCount()));
    mMslXfbOnlyVertexShaderInfo = mMslShaderTranslateInfo[gl::ShaderType::Vertex];
    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        // Create actual Metal shader
        ANGLE_TRY(createMslShaderLib(contextMtl, shaderType, infoLog,
                                     &mMslShaderTranslateInfo[shaderType],
                                     getDefaultSubstitutionDictionary()));
    }
    return angle::Result::Continue;
}

void ProgramMtl::linkUpdateHasFlatAttributes(const gl::Context *context)
{
    mProgramHasFlatAttributes = false;

    const auto &programInputs = mState.getProgramInputs();
    for (auto &attribute : programInputs)
    {
        if (attribute.interpolation == sh::INTERPOLATION_FLAT)
        {
            mProgramHasFlatAttributes = true;
            return;
        }
    }

    const auto &flatVaryings =
        mState.getAttachedShader(gl::ShaderType::Vertex)->getOutputVaryings(context);
    for (auto &attribute : flatVaryings)
    {
        if (attribute.interpolation == sh::INTERPOLATION_FLAT)
        {
            mProgramHasFlatAttributes = true;
            return;
        }
    }
}

angle::Result ProgramMtl::linkImpl(const gl::Context *glContext,
                                   const gl::ProgramLinkedResources &resources,
                                   gl::InfoLog &infoLog)
{
    ANGLE_TRY(linkImplDirect(glContext, resources, infoLog));
    linkUpdateHasFlatAttributes(glContext);
    return angle::Result::Continue;
}

angle::Result ProgramMtl::linkTranslatedShaders(const gl::Context *glContext,
                                                gl::BinaryInputStream *stream,
                                                gl::InfoLog &infoLog)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);
    // NOTE(hqle): No transform feedbacks for now, since we only support ES 2.0 atm

    reset(contextMtl);

    loadTranslatedShaders(stream);
    loadShaderInternalInfo(stream);
    ANGLE_TRY(loadDefaultUniformBlocksInfo(glContext, stream));
    ANGLE_TRY(loadInterfaceBlockInfo(glContext, stream));
    ANGLE_TRY(createMslShaderLib(contextMtl, gl::ShaderType::Vertex, infoLog,
                                 &mMslShaderTranslateInfo[gl::ShaderType::Vertex],
                                 getDefaultSubstitutionDictionary()));
    ANGLE_TRY(createMslShaderLib(contextMtl, gl::ShaderType::Fragment, infoLog,
                                 &mMslShaderTranslateInfo[gl::ShaderType::Fragment],
                                 getDefaultSubstitutionDictionary()));

    return angle::Result::Continue;
}

mtl::BufferPool *ProgramMtl::getBufferPool(ContextMtl *context)
{
    if (mAuxBufferPool == nullptr)
    {
        mAuxBufferPool = new mtl::BufferPool(true);
        mAuxBufferPool->initialize(context, mtl::kDefaultUniformsMaxSize * 2,
                                   mtl::kUniformBufferSettingOffsetMinAlignment, 3);
    }
    return mAuxBufferPool;
}
void ProgramMtl::linkResources(const gl::Context *context,
                               const gl::ProgramLinkedResources &resources)
{
    Std140BlockLayoutEncoderFactory std140EncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&std140EncoderFactory);

    linker.linkResources(context, mState, resources);
}

angle::Result ProgramMtl::initDefaultUniformBlocks(const gl::Context *glContext)
{
    // Process vertex and fragment uniforms into std140 packing.
    gl::ShaderMap<sh::BlockLayoutMap> layoutMap;
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        gl::Shader *shader = mState.getAttachedShader(shaderType);
        if (shader)
        {
            const std::vector<sh::Uniform> &uniforms = shader->getUniforms(glContext);
            InitDefaultUniformBlock(uniforms, shader, &layoutMap[shaderType],
                                    &requiredBufferSize[shaderType]);
            // Set up block conversion buffer
            initUniformBlocksRemapper(shader, glContext);
        }
    }

    // Init the default block layout info.
    const auto &uniforms         = mState.getUniforms();
    const auto &uniformLocations = mState.getUniformLocations();
    for (size_t locSlot = 0; locSlot < uniformLocations.size(); ++locSlot)
    {
        const gl::VariableLocation &location = uniformLocations[locSlot];
        gl::ShaderMap<sh::BlockMemberInfo> layoutInfo;

        if (location.used() && !location.ignored)
        {
            const gl::LinkedUniform &uniform = uniforms[location.index];
            if (uniform.isInDefaultBlock() && !uniform.isSampler() && !uniform.isImage())
            {
                std::string uniformName = uniform.name;
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

    return resizeDefaultUniformBlocksMemory(glContext, requiredBufferSize);
}

angle::Result ProgramMtl::resizeDefaultUniformBlocksMemory(
    const gl::Context *glContext,
    const gl::ShaderMap<size_t> &requiredBufferSize)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if (requiredBufferSize[shaderType] > 0)
        {
            ASSERT(requiredBufferSize[shaderType] <= mtl::kDefaultUniformsMaxSize);

            if (!mDefaultUniformBlocks[shaderType].uniformData.resize(
                    requiredBufferSize[shaderType]))
            {
                ANGLE_MTL_CHECK(contextMtl, false, GL_OUT_OF_MEMORY);
            }

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType].uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    return angle::Result::Continue;
}

angle::Result ProgramMtl::getSpecializedShader(ContextMtl *context,
                                               gl::ShaderType shaderType,
                                               const mtl::RenderPipelineDesc &renderPipelineDesc,
                                               id<MTLFunction> *shaderOut)
{
    static_assert(YES == 1, "YES should have value of 1");

    mtl::TranslatedShaderInfo *translatedMslInfo = &mMslShaderTranslateInfo[shaderType];
    ProgramShaderObjVariantMtl *shaderVariant;
    mtl::AutoObjCObj<MTLFunctionConstantValues> funcConstants;

    if (shaderType == gl::ShaderType::Vertex)
    {
        // For vertex shader, we need to create 3 variants, one with emulated rasterization
        // discard, one with true rasterization discard and one without.
        shaderVariant = &mVertexShaderVariants[renderPipelineDesc.rasterizationType];
        if (shaderVariant->metalShader)
        {
            // Already created.
            *shaderOut = shaderVariant->metalShader;
            return angle::Result::Continue;
        }

        if (renderPipelineDesc.rasterizationType == mtl::RenderPipelineRasterization::Disabled)
        {
            // Special case: XFB output only vertex shader.
            ASSERT(!mState.getLinkedTransformFeedbackVaryings().empty());
            translatedMslInfo = &mMslXfbOnlyVertexShaderInfo;
            if (!translatedMslInfo->metalLibrary)
            {
                // Lazily compile XFB only shader
                gl::InfoLog infoLog;
                ANGLE_TRY(createMslShaderLib(context, shaderType, infoLog,
                                             &mMslXfbOnlyVertexShaderInfo,
                                             @{@"TRANSFORM_FEEDBACK_ENABLED" : @"1"}));
                translatedMslInfo->metalLibrary.get().label = @"TransformFeedback";
            }
        }

        ANGLE_MTL_OBJC_SCOPE
        {
            BOOL emulateDiscard = renderPipelineDesc.rasterizationType ==
                                  mtl::RenderPipelineRasterization::EmulatedDiscard;

            NSString *discardEnabledStr =
                [NSString stringWithUTF8String:sh::mtl::kRasterizerDiscardEnabledConstName];

            funcConstants = mtl::adoptObjCObj([[MTLFunctionConstantValues alloc] init]);
            [funcConstants setConstantValue:&emulateDiscard
                                       type:MTLDataTypeBool
                                   withName:discardEnabledStr];
        }
    }  // if (shaderType == gl::ShaderType::Vertex)
    else if (shaderType == gl::ShaderType::Fragment)
    {
        // For fragment shader, we need to create 2 variants, one with sample coverage mask
        // disabled, one with the mask enabled.
        BOOL emulateCoverageMask = renderPipelineDesc.emulateCoverageMask;
        shaderVariant            = &mFragmentShaderVariants[emulateCoverageMask];
        if (shaderVariant->metalShader)
        {
            // Already created.
            *shaderOut = shaderVariant->metalShader;
            return angle::Result::Continue;
        }

        ANGLE_MTL_OBJC_SCOPE
        {
            NSString *coverageMaskEnabledStr =
                [NSString stringWithUTF8String:sh::mtl::kCoverageMaskEnabledConstName];

            NSString *depthWriteEnabledStr =
                [NSString stringWithUTF8String:sh::mtl::kDepthWriteEnabledConstName];

            funcConstants = mtl::adoptObjCObj([[MTLFunctionConstantValues alloc] init]);
            [funcConstants setConstantValue:&emulateCoverageMask
                                       type:MTLDataTypeBool
                                   withName:coverageMaskEnabledStr];
            
            MTLPixelFormat depthPixelFormat =
                (MTLPixelFormat)renderPipelineDesc.outputDescriptor.depthAttachmentPixelFormat;
            BOOL fragDepthWriteEnabled = depthPixelFormat != MTLPixelFormatInvalid;
            [funcConstants setConstantValue:&fragDepthWriteEnabled
                                       type:MTLDataTypeBool
                                   withName:depthWriteEnabledStr];
        }

    }  // gl::ShaderType::Fragment
    else
    {
        UNREACHABLE();
        return angle::Result::Stop;
    }
    [funcConstants
        setConstantValue:&(context->getDisplay()->getFeatures().allowSamplerCompareGradient.enabled)
                    type:MTLDataTypeBool
                withName:@"ANGLEUseSampleCompareGradient"];
    [funcConstants
        setConstantValue:&(context->getDisplay()->getFeatures().allowSamplerCompareLod.enabled)
                    type:MTLDataTypeBool
                withName:@"ANGLEUseSampleCompareLod"];
    // Create Metal shader object
    ANGLE_MTL_OBJC_SCOPE
    {
        ANGLE_TRY(CreateMslShader(context, translatedMslInfo->metalLibrary, SHADER_ENTRY_NAME,
                                  funcConstants.get(), &shaderVariant->metalShader));
    }

    // Store reference to the translated source for easily querying mapped bindings later.
    shaderVariant->translatedSrcInfo = translatedMslInfo;

    // Initialize argument buffer encoder if required
    if (translatedMslInfo->hasUBOArgumentBuffer)
    {
        InitArgumentBufferEncoder(context, shaderVariant->metalShader,
                                  mtl::kUBOArgumentBufferBindingIndex,
                                  &shaderVariant->uboArgBufferEncoder);
    }

    *shaderOut = shaderVariant->metalShader;

    return angle::Result::Continue;
}

bool ProgramMtl::hasSpecializedShader(gl::ShaderType shaderType,
                                      const mtl::RenderPipelineDesc &renderPipelineDesc)
{
    return true;
}

angle::Result ProgramMtl::createMslShaderLib(
    ContextMtl *context,
    gl::ShaderType shaderType,
    gl::InfoLog &infoLog,
    mtl::TranslatedShaderInfo *translatedMslInfo,
    NSDictionary<NSString *, NSObject *> *substitutionMacros)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        const mtl::ContextDevice &metalDevice = context->getMetalDevice();

        // Convert to actual binary shader
        mtl::AutoObjCPtr<NSError *> err = nil;
        bool disableFastMath = (context->getDisplay()->getFeatures().intelDisableFastMath.enabled &&
                                translatedMslInfo->hasInvariantOrAtan);
        translatedMslInfo->metalLibrary =
            mtl::CreateShaderLibrary(metalDevice, translatedMslInfo->metalShaderSource,
                                     substitutionMacros, !disableFastMath, &err);
        if (err && !translatedMslInfo->metalLibrary)
        {
            std::ostringstream ss;
            ss << "Internal error compiling shader with Metal backend.\n";
            ss << err.get().localizedDescription.UTF8String << "\n";
            ss << "-----\n";
            ss << translatedMslInfo->metalShaderSource;
            ss << "-----\n";

            // ERR() << ss.str();
            infoLog << ss.str();

            ANGLE_MTL_HANDLE_ERROR(context, ss.str().c_str(), GL_INVALID_OPERATION);
            return angle::Result::Stop;
        }

        return angle::Result::Continue;
    }
}

void ProgramMtl::saveInterfaceBlockInfo(gl::BinaryOutputStream *stream)
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

angle::Result ProgramMtl::loadInterfaceBlockInfo(const gl::Context *glContext,
                                                 gl::BinaryInputStream *stream)
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

void ProgramMtl::saveDefaultUniformBlocksInfo(gl::BinaryOutputStream *stream)
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

angle::Result ProgramMtl::loadDefaultUniformBlocksInfo(const gl::Context *glContext,
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

    return resizeDefaultUniformBlocksMemory(glContext, requiredBufferSize);
}

void ProgramMtl::saveShaderInternalInfo(gl::BinaryOutputStream *stream)
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
        stream->writeBool(mMslShaderTranslateInfo[shaderType].hasInvariantOrAtan);
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

void ProgramMtl::loadShaderInternalInfo(gl::BinaryInputStream *stream)
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
        mMslShaderTranslateInfo[shaderType].hasInvariantOrAtan = stream->readBool();
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

GLboolean ProgramMtl::validate(const gl::Caps &caps, gl::InfoLog *infoLog)
{
    // No-op. The spec is very vague about the behavior of validation.
    return GL_TRUE;
}

template <typename T>
void ProgramMtl::setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType)
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    if (linkedUniform.isSampler())
    {
        // Sampler binding has changed.
        mSamplerBindingsDirty.set();
        return;
    }

    if (linkedUniform.typeInfo->type == entryPointType)
    {
        for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
        {
            DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount    = (GLint)linkedUniform.typeInfo->componentCount;
            const GLint baseComponentSize = (GLint)mtl::GetMetalSizeForGLType(
                gl::VariableComponentType(linkedUniform.typeInfo->type));
            UpdateDefaultUniformBlockWithElementSize(count, locationInfo.arrayIndex, componentCount,
                                                     v, baseComponentSize, layoutInfo,
                                                     &uniformBlock.uniformData);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
    else
    {
        for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
        {
            DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.typeInfo->componentCount;

            ASSERT(linkedUniform.typeInfo->type == gl::VariableBoolVectorType(entryPointType));

            GLint initialArrayOffset =
                locationInfo.arrayIndex * layoutInfo.arrayStride + layoutInfo.offset;
            for (GLint i = 0; i < count; i++)
            {
                GLint elementOffset = i * layoutInfo.arrayStride + initialArrayOffset;
                bool *dest =
                    reinterpret_cast<bool *>(uniformBlock.uniformData.data() + elementOffset);
                const T *source = v + i * componentCount;

                for (int c = 0; c < componentCount; c++)
                {
                    dest[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
                }
            }

            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

template <typename T>
void ProgramMtl::getUniformImpl(GLint location, T *v, GLenum entryPointType) const
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    ASSERT(!linkedUniform.isSampler());

    const gl::ShaderType shaderType = linkedUniform.getFirstShaderTypeWhereActive();
    ASSERT(shaderType != gl::ShaderType::InvalidEnum);

    const DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];
    const sh::BlockMemberInfo &layoutInfo   = uniformBlock.uniformLayout[location];

    ASSERT(linkedUniform.typeInfo->componentType == entryPointType ||
           linkedUniform.typeInfo->componentType == gl::VariableBoolVectorType(entryPointType));
    const GLint baseComponentSize =
        (GLint)mtl::GetMetalSizeForGLType(gl::VariableComponentType(linkedUniform.typeInfo->type));

    if (gl::IsMatrixType(linkedUniform.type))
    {
        const uint8_t *ptrToElement = uniformBlock.uniformData.data() + layoutInfo.offset +
                                      (locationInfo.arrayIndex * layoutInfo.arrayStride);
        mtl::GetMatrixUniformMetal(linkedUniform.type, v, reinterpret_cast<const T *>(ptrToElement),
                                   false);
    }
    // Decompress bool from one byte to four bytes because bool values in GLSL
    // are uint-sized: ES 3.0 Section 2.12.6.3 "Uniform Buffer Object Storage".
    else if (gl::VariableComponentType(linkedUniform.type) == GL_BOOL)
    {
        bool bVals[4] = {0};
        ReadFromDefaultUniformBlockWithElementSize(
            linkedUniform.typeInfo->componentCount, locationInfo.arrayIndex, bVals,
            baseComponentSize, layoutInfo, &uniformBlock.uniformData);
        for (int bCol = 0; bCol < linkedUniform.typeInfo->componentCount; ++bCol)
        {
            unsigned int data = bVals[bCol];
            *(v + bCol)       = static_cast<T>(data);
        }
    }
    else
    {

        assert(baseComponentSize == sizeof(T));
        ReadFromDefaultUniformBlockWithElementSize(linkedUniform.typeInfo->componentCount,
                                                   locationInfo.arrayIndex, v, baseComponentSize,
                                                   layoutInfo, &uniformBlock.uniformData);
    }
}

void ProgramMtl::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT);
}

void ProgramMtl::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC2);
}

void ProgramMtl::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC3);
}

void ProgramMtl::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC4);
}

void ProgramMtl::setUniform1iv(GLint startLocation, GLsizei count, const GLint *v)
{
    setUniformImpl(startLocation, count, v, GL_INT);
}

void ProgramMtl::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC2);
}

void ProgramMtl::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC3);
}

void ProgramMtl::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC4);
}

void ProgramMtl::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT);
}

void ProgramMtl::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC2);
}

void ProgramMtl::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC3);
}

void ProgramMtl::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC4);
}

template <int cols, int rows>
void ProgramMtl::setUniformMatrixfv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
        const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

        // Assume an offset of -1 means the block is unused.
        if (layoutInfo.offset == -1)
        {
            continue;
        }

        mtl::SetFloatUniformMatrixMetal<cols, rows>::Run(
            locationInfo.arrayIndex, linkedUniform.getArraySizeProduct(), count, transpose, value,
            uniformBlock.uniformData.data() + layoutInfo.offset);

        mDefaultUniformBlocksDirty.set(shaderType);
    }
}

void ProgramMtl::setUniformMatrix2fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<2, 2>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix3fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<3, 3>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix4fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfv<4, 4>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix2x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<2, 3>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix3x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<3, 2>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix2x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<2, 4>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix4x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<4, 2>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix3x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<3, 4>(location, count, transpose, value);
}

void ProgramMtl::setUniformMatrix4x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfv<4, 3>(location, count, transpose, value);
}

void ProgramMtl::getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const
{
    getUniformImpl(location, params, GL_FLOAT);
}

void ProgramMtl::getUniformiv(const gl::Context *context, GLint location, GLint *params) const
{
    getUniformImpl(location, params, GL_INT);
}

void ProgramMtl::getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const
{
    getUniformImpl(location, params, GL_UNSIGNED_INT);
}

angle::Result ProgramMtl::setupDraw(const gl::Context *glContext,
                                    mtl::RenderCommandEncoder *cmdEncoder,
                                    const mtl::RenderPipelineDesc &pipelineDesc,
                                    bool pipelineDescChanged,
                                    bool forceTexturesSetting,
                                    bool uniformBuffersDirty)
{
    ContextMtl *context = mtl::GetImpl(glContext);
    if (pipelineDescChanged || !cmdEncoder->hasPipelineState())
    {
        // Render pipeline state needs to be changed
        id<MTLRenderPipelineState> pipelineState =
            mMetalRenderPipelineCache.getRenderPipelineState(context, pipelineDesc);
        if (!pipelineState)
        {
            // Error already logged inside getRenderPipelineState()
            return angle::Result::Stop;
        }
        cmdEncoder->setRenderPipelineState(pipelineState);

        // We need to rebind uniform buffers & textures also
        mDefaultUniformBlocksDirty.set();
        mSamplerBindingsDirty.set();

        // Cache current shader variant references for easier querying.
        mCurrentShaderVariants[gl::ShaderType::Vertex] =
            &mVertexShaderVariants[pipelineDesc.rasterizationType];
        mCurrentShaderVariants[gl::ShaderType::Fragment] =
            pipelineDesc.rasterizationEnabled()
                ? &mFragmentShaderVariants[pipelineDesc.emulateCoverageMask]
                : nullptr;
    }

    ANGLE_TRY(commitUniforms(context, cmdEncoder));
    ANGLE_TRY(updateTextures(glContext, cmdEncoder, forceTexturesSetting));

    if (uniformBuffersDirty || pipelineDescChanged)
    {
        ANGLE_TRY(updateUniformBuffers(context, cmdEncoder, pipelineDesc));
    }

    if (pipelineDescChanged)
    {
        ANGLE_TRY(updateXfbBuffers(context, cmdEncoder, pipelineDesc));
    }

    return angle::Result::Continue;
}

angle::Result ProgramMtl::commitUniforms(ContextMtl *context, mtl::RenderCommandEncoder *cmdEncoder)
{
    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if (!mDefaultUniformBlocksDirty[shaderType] || !mCurrentShaderVariants[shaderType])
        {
            continue;
        }
        DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];

        if (!uniformBlock.uniformData.size())
        {
            continue;
        }
        if (mAuxBufferPool)
        {
            mAuxBufferPool->releaseInFlightBuffers(context);
        }
        // If we exceed the default inline max size, try to allocate a buffer
        bool needsCommitUniform = true;
        if (needsCommitUniform && uniformBlock.uniformData.size() <= mtl::kInlineConstDataMaxSize)
        {
            ASSERT(uniformBlock.uniformData.size() <= mtl::kInlineConstDataMaxSize);
            cmdEncoder->setBytes(shaderType, uniformBlock.uniformData.data(),
                                 uniformBlock.uniformData.size(),
                                 mtl::kDefaultUniformsBindingIndex);
        }
        else if (needsCommitUniform)
        {
            ASSERT(uniformBlock.uniformData.size() <= mtl::kDefaultUniformsMaxSize);
            mtl::BufferRef mtlBufferOut;
            size_t offsetOut;
            uint8_t *ptrOut;
            // Allocate a new Uniform buffer
            ANGLE_TRY(getBufferPool(context)->allocate(context, uniformBlock.uniformData.size(),
                                                       &ptrOut, &mtlBufferOut, &offsetOut));
            // Copy the uniform result
            memcpy(ptrOut, uniformBlock.uniformData.data(), uniformBlock.uniformData.size());
            // Commit
            ANGLE_TRY(getBufferPool(context)->commit(context));
            // Set buffer
            cmdEncoder->setBuffer(shaderType, mtlBufferOut, (uint32_t)offsetOut,
                                  mtl::kDefaultUniformsBindingIndex);
        }

        mDefaultUniformBlocksDirty.reset(shaderType);
    }
    return angle::Result::Continue;
}

angle::Result ProgramMtl::updateTextures(const gl::Context *glContext,
                                         mtl::RenderCommandEncoder *cmdEncoder,
                                         bool forceUpdate)
{
    ContextMtl *contextMtl                          = mtl::GetImpl(glContext);
    const auto &glState                             = glContext->getState();
    const gl::ActiveTexturesCache &completeTextures = glState.getActiveTexturesCache();

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if ((!mSamplerBindingsDirty[shaderType] && !forceUpdate) ||
            !mCurrentShaderVariants[shaderType])
        {
            continue;
        }

        const mtl::TranslatedShaderInfo &shaderInfo =
            mCurrentShaderVariants[shaderType]->translatedSrcInfo
                ? *mCurrentShaderVariants[shaderType]->translatedSrcInfo
                : mMslShaderTranslateInfo[shaderType];
        bool hasDepthSampler = false;

        for (uint32_t textureIndex = 0; textureIndex < mState.getSamplerBindings().size();
             ++textureIndex)
        {
            const gl::SamplerBinding &samplerBinding = mState.getSamplerBindings()[textureIndex];
            const mtl::SamplerBinding &mslBinding = shaderInfo.actualSamplerBindings[textureIndex];
            if (mslBinding.textureBinding >= mtl::kMaxShaderSamplers)
            {
                // No binding assigned
                continue;
            }

            gl::TextureType textureType = samplerBinding.textureType;

            for (uint32_t arrayElement = 0; arrayElement < samplerBinding.boundTextureUnits.size();
                 ++arrayElement)
            {
                GLuint textureUnit   = samplerBinding.boundTextureUnits[arrayElement];
                gl::Texture *texture = completeTextures[textureUnit];
                gl::Sampler *sampler = contextMtl->getState().getSampler(textureUnit);
                uint32_t textureSlot = mslBinding.textureBinding + arrayElement;
                uint32_t samplerSlot = mslBinding.samplerBinding + arrayElement;
                if (!texture)
                {
                    ANGLE_TRY(contextMtl->getIncompleteTexture(glContext, textureType, &texture));
                }
                const gl::SamplerState *samplerState =
                    sampler ? &sampler->getSamplerState() : &texture->getSamplerState();
                TextureMtl *textureMtl = mtl::GetImpl(texture);
                if (samplerBinding.format == gl::SamplerFormat::Shadow)
                {
                    hasDepthSampler                  = true;
                    mShadowCompareModes[textureSlot] = mtl::MslGetShaderShadowCompareMode(
                        samplerState->getCompareMode(), samplerState->getCompareFunc());
                }
                ANGLE_TRY(textureMtl->bindToShader(glContext, cmdEncoder, shaderType, sampler,
                                                   textureSlot, samplerSlot));
            }  // for array elements
        }      // for sampler bindings

        if (hasDepthSampler)
        {
            cmdEncoder->setData(shaderType, mShadowCompareModes,
                                mtl::kShadowSamplerCompareModesBindingIndex);
        }

        for (const gl::ImageBinding &imageBinding : mState.getImageBindings())
        {
            if (imageBinding.boundImageUnits.size() != 1)
            {
                UNIMPLEMENTED();
                continue;
            }

            int glslImageBinding    = imageBinding.boundImageUnits[0];
            int mtlRWTextureBinding = shaderInfo.actualImageBindings[glslImageBinding];
            ASSERT(mtlRWTextureBinding < static_cast<int>(mtl::kMaxShaderImages));
            if (mtlRWTextureBinding < 0)
            {
                continue;  // The program does not have an image bound at this unit.
            }

            const gl::ImageUnit &imageUnit = glState.getImageUnit(glslImageBinding);
            TextureMtl *textureMtl         = mtl::GetImpl(imageUnit.texture.get());
            ANGLE_TRY(textureMtl->bindToShaderImage(
                glContext, cmdEncoder, shaderType, static_cast<uint32_t>(mtlRWTextureBinding),
                imageUnit.level, imageUnit.layer, imageUnit.format));
        }
    }  // for shader types

    return angle::Result::Continue;
}

angle::Result ProgramMtl::updateUniformBuffers(ContextMtl *context,
                                               mtl::RenderCommandEncoder *cmdEncoder,
                                               const mtl::RenderPipelineDesc &pipelineDesc)
{
    const std::vector<gl::InterfaceBlock> &blocks = mState.getUniformBlocks();
    if (blocks.empty())
    {
        return angle::Result::Continue;
    }

    // This array is only used inside this function and its callees.
    ScopedAutoClearVector<uint32_t> scopeArrayClear(&mArgumentBufferRenderStageUsages);
    ScopedAutoClearVector<std::pair<mtl::BufferRef, uint32_t>> scopeArrayClear2(
        &mLegalizedOffsetedUniformBuffers);
    mArgumentBufferRenderStageUsages.resize(blocks.size());
    mLegalizedOffsetedUniformBuffers.resize(blocks.size());

    ANGLE_TRY(legalizeUniformBufferOffsets(context, blocks));

    const gl::State &glState = context->getState();

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        if (!mCurrentShaderVariants[shaderType])
        {
            continue;
        }

        if (mCurrentShaderVariants[shaderType]->translatedSrcInfo->hasUBOArgumentBuffer)
        {
            ANGLE_TRY(
                encodeUniformBuffersInfoArgumentBuffer(context, cmdEncoder, blocks, shaderType));
        }
        else
        {
            ANGLE_TRY(bindUniformBuffersToDiscreteSlots(context, cmdEncoder, blocks, shaderType));
        }
    }  // for shader types

    // After encode the uniform buffers into an argument buffer, we need to tell Metal that
    // the buffers are being used by what shader stages.
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(block.binding);
        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        // Remove any other stages other than vertex and fragment.
        uint32_t stages = mArgumentBufferRenderStageUsages[bufferIndex] &
                          (mtl::kRenderStageVertex | mtl::kRenderStageFragment);

        if (stages == 0)
        {
            continue;
        }

        cmdEncoder->useResource(mLegalizedOffsetedUniformBuffers[bufferIndex].first,
                                MTLResourceUsageRead, static_cast<mtl::RenderStages>(stages));
    }

    return angle::Result::Continue;
}

angle::Result ProgramMtl::legalizeUniformBufferOffsets(
    ContextMtl *context,
    const std::vector<gl::InterfaceBlock> &blocks)
{
    const gl::State &glState = context->getState();

    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(block.binding);

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        BufferMtl *bufferMtl = mtl::GetImpl(bufferBinding.get());
        size_t srcOffset     = std::min<size_t>(bufferBinding.getOffset(), bufferMtl->size());
        assert(mUniformBlockConversions.find(block.name) != mUniformBlockConversions.end());
        const UBOConversionInfo &conversionInfo = mUniformBlockConversions.at(block.name);
        if (conversionInfo.needsConversion())
        {

            UniformConversionBufferMtl *conversion =
                (UniformConversionBufferMtl *)bufferMtl->getUniformConversionBuffer(
                    context, std::pair<size_t, size_t>(bufferIndex, srcOffset),
                    conversionInfo.stdSize());
            // Has the content of the buffer has changed since last conversion?
            if (conversion->dirty)
            {
                const uint8_t *srcBytes = bufferMtl->getBufferDataReadOnly(context);
                srcBytes += srcOffset;
                size_t sizeToCopy = bufferMtl->size() - srcOffset;

                ANGLE_TRY(ConvertUniformBufferData(
                    context, conversionInfo, &conversion->data, srcBytes, sizeToCopy,
                    &conversion->convertedBuffer, &conversion->convertedOffset));

                conversion->dirty = false;
            }
            // Calculate offset in new block.
            size_t dstOffsetSource = srcOffset - conversion->initialSrcOffset();
            assert(dstOffsetSource % conversionInfo.stdSize() == 0);
            unsigned int numBlocksToOffset =
                (unsigned int)(dstOffsetSource / conversionInfo.stdSize());
            size_t bytesToOffset = numBlocksToOffset * conversionInfo.metalSize();

            mLegalizedOffsetedUniformBuffers[bufferIndex].first = conversion->convertedBuffer;
            mLegalizedOffsetedUniformBuffers[bufferIndex].second =
                static_cast<uint32_t>(conversion->convertedOffset + bytesToOffset);
        }
        else
        {
            mLegalizedOffsetedUniformBuffers[bufferIndex].first = bufferMtl->getCurrentBuffer();
            mLegalizedOffsetedUniformBuffers[bufferIndex].second =
                static_cast<uint32_t>(bufferBinding.getOffset());
        }
    }
    return angle::Result::Continue;
}

angle::Result ProgramMtl::bindUniformBuffersToDiscreteSlots(
    ContextMtl *context,
    mtl::RenderCommandEncoder *cmdEncoder,
    const std::vector<gl::InterfaceBlock> &blocks,
    gl::ShaderType shaderType)
{
    const gl::State &glState = context->getState();
    const mtl::TranslatedShaderInfo &shaderInfo =
        *mCurrentShaderVariants[shaderType]->translatedSrcInfo;
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(block.binding);

        if (bufferBinding.get() == nullptr || !block.activeShaders().test(shaderType))
        {
            continue;
        }

        uint32_t actualBufferIdx = shaderInfo.actualUBOBindings[bufferIndex];

        if (actualBufferIdx >= mtl::kMaxShaderBuffers)
        {
            continue;
        }

        mtl::BufferRef mtlBuffer = mLegalizedOffsetedUniformBuffers[bufferIndex].first;
        uint32_t offset          = mLegalizedOffsetedUniformBuffers[bufferIndex].second;
        cmdEncoder->setBuffer(shaderType, mtlBuffer, offset, actualBufferIdx);
    }
    return angle::Result::Continue;
}
angle::Result ProgramMtl::encodeUniformBuffersInfoArgumentBuffer(
    ContextMtl *context,
    mtl::RenderCommandEncoder *cmdEncoder,
    const std::vector<gl::InterfaceBlock> &blocks,
    gl::ShaderType shaderType)
{
    const gl::State &glState = context->getState();
    ASSERT(mCurrentShaderVariants[shaderType]->translatedSrcInfo);
    const mtl::TranslatedShaderInfo &shaderInfo =
        *mCurrentShaderVariants[shaderType]->translatedSrcInfo;

    // Encode all uniform buffers into an argument buffer.
    ProgramArgumentBufferEncoderMtl &bufferEncoder =
        mCurrentShaderVariants[shaderType]->uboArgBufferEncoder;

    mtl::BufferRef argumentBuffer;
    size_t argumentBufferOffset;
    bufferEncoder.bufferPool.releaseInFlightBuffers(context);
    ANGLE_TRY(bufferEncoder.bufferPool.allocate(
        context, bufferEncoder.metalArgBufferEncoder.get().encodedLength, nullptr, &argumentBuffer,
        &argumentBufferOffset));

    [bufferEncoder.metalArgBufferEncoder setArgumentBuffer:argumentBuffer->get()
                                                    offset:argumentBufferOffset];

    constexpr gl::ShaderMap<MTLRenderStages> kShaderStageMap = {
        {gl::ShaderType::Vertex, mtl::kRenderStageVertex},
        {gl::ShaderType::Fragment, mtl::kRenderStageFragment},
    };

    auto mtlRenderStage = kShaderStageMap[shaderType];

    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(block.binding);

        if (bufferBinding.get() == nullptr || !block.activeShaders().test(shaderType))
        {
            continue;
        }

        mArgumentBufferRenderStageUsages[bufferIndex] |= mtlRenderStage;

        uint32_t actualBufferIdx = shaderInfo.actualUBOBindings[bufferIndex];
        if (actualBufferIdx >= mtl::kMaxShaderBuffers)
        {
            continue;
        }

        mtl::BufferRef mtlBuffer = mLegalizedOffsetedUniformBuffers[bufferIndex].first;
        uint32_t offset          = mLegalizedOffsetedUniformBuffers[bufferIndex].second;
        [bufferEncoder.metalArgBufferEncoder setBuffer:mtlBuffer->get()
                                                offset:offset
                                               atIndex:actualBufferIdx];
    }

    ANGLE_TRY(bufferEncoder.bufferPool.commit(context));

    cmdEncoder->setBuffer(shaderType, argumentBuffer, static_cast<uint32_t>(argumentBufferOffset),
                          mtl::kUBOArgumentBufferBindingIndex);
    return angle::Result::Continue;
}

angle::Result ProgramMtl::updateXfbBuffers(ContextMtl *context,
                                           mtl::RenderCommandEncoder *cmdEncoder,
                                           const mtl::RenderPipelineDesc &pipelineDesc)
{
    const gl::State &glState                 = context->getState();
    gl::TransformFeedback *transformFeedback = glState.getCurrentTransformFeedback();

    if (pipelineDesc.rasterizationEnabled() || !glState.isTransformFeedbackActiveUnpaused() ||
        ANGLE_UNLIKELY(!transformFeedback))
    {
        // XFB output can only be used with rasterization disabled.
        return angle::Result::Continue;
    }

    size_t xfbBufferCount = glState.getProgramExecutable()->getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);
    ASSERT(mState.getTransformFeedbackBufferMode() != GL_INTERLEAVED_ATTRIBS ||
           xfbBufferCount == 1);

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        uint32_t actualBufferIdx = mMslXfbOnlyVertexShaderInfo.actualXFBBindings[bufferIndex];

        if (actualBufferIdx >= mtl::kMaxShaderBuffers)
        {
            continue;
        }

        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            transformFeedback->getIndexedBuffer(bufferIndex);
        gl::Buffer *buffer = bufferBinding.get();
        ASSERT((bufferBinding.getOffset() % 4) == 0);
        ASSERT(buffer != nullptr);

        BufferMtl *bufferMtl = mtl::GetImpl(buffer);

        // Use offset=0, actual offset will be set in Driver Uniform inside ContextMtl.
        cmdEncoder->setBufferForWrite(gl::ShaderType::Vertex, bufferMtl->getCurrentBuffer(), 0,
                                      actualBufferIdx);
    }

    return angle::Result::Continue;
}

}  // namespace rx

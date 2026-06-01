//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLContextVk.h: Defines the class interface for CLContextVk, implementing CLContextImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_

#include "common/PackedEnums.h"
#include "common/SimpleMutex.h"
#include "libANGLE/renderer/vulkan/CLPlatformVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/renderer/CLContextImpl.h"

#include <libANGLE/CLContext.h>
#include "libANGLE/CLDevice.h"

namespace rx
{

class CLKernelVk;

class CLContextVk : public CLContextImpl, public vk::Context
{
  public:
    CLContextVk(const cl::Context &context, const cl::DevicePtrs devicePtrs);

    ~CLContextVk() override;

    void handleError(VkResult errorCode,
                     const char *file,
                     const char *function,
                     unsigned int line) override;

    bool hasMemory(cl_mem memory) const;

    angle::Result getDevices(cl::DevicePtrs *devicePtrsOut) const override;

    angle::Result createCommandQueue(const cl::CommandQueue &commandQueue,
                                     CLCommandQueueImpl::Ptr *commandQueueOut) override;

    angle::Result createBuffer(const cl::Buffer &buffer,
                               void *hostPtr,
                               CLMemoryImpl::Ptr *bufferOut) override;

    angle::Result createImage(const cl::Image &image,
                              void *hostPtr,
                              CLMemoryImpl::Ptr *imageOut) override;

    angle::Result getSupportedImageFormats(cl::MemFlags flags,
                                           cl::MemObjectType imageType,
                                           cl_uint numEntries,
                                           cl_image_format *imageFormats,
                                           cl_uint *numImageFormats) override;

    angle::Result createSampler(const cl::Sampler &sampler,
                                CLSamplerImpl::Ptr *samplerOut) override;

    angle::Result createProgramWithSource(const cl::Program &program,
                                          const std::string &source,
                                          CLProgramImpl::Ptr *programOut) override;

    angle::Result createProgramWithIL(const cl::Program &program,
                                      const void *il,
                                      size_t length,
                                      CLProgramImpl::Ptr *programOut) override;

    angle::Result createProgramWithBinary(const cl::Program &program,
                                          const size_t *lengths,
                                          const unsigned char **binaries,
                                          cl_int *binaryStatus,
                                          CLProgramImpl::Ptr *programOut) override;

    angle::Result createProgramWithBuiltInKernels(const cl::Program &program,
                                                  const char *kernel_names,
                                                  CLProgramImpl::Ptr *programOut) override;

    angle::Result linkProgram(const cl::Program &program,
                              const cl::DevicePtrs &devices,
                              const char *options,
                              const cl::ProgramPtrs &inputPrograms,
                              cl::Program *notify,
                              CLProgramImpl::Ptr *programOut) override;

    angle::Result createUserEvent(const cl::Event &event, CLEventImpl::Ptr *eventOut) override;

    angle::Result waitForEvents(const cl::EventPtrs &events) override;

    CLPlatformVk *getPlatform() { return &mContext.getPlatform().getImpl<CLPlatformVk>(); }

    cl::Context &getFrontendObject() { return const_cast<cl::Context &>(mContext); }

    DescriptorSetLayoutCache *getDescriptorSetLayoutCache() { return &mDescriptorSetLayoutCache; }
    PipelineLayoutCache *getPipelineLayoutCache() { return &mPipelineLayoutCache; }

    vk::MetaDescriptorPool &getMetaDescriptorPool() { return mMetaDescriptorPool; }

    angle::Result allocateDescriptorSet(
        CLKernelVk *kernelVk,
        DescriptorSetIndex index,
        angle::EnumIterator<DescriptorSetIndex> layoutIndex,
        vk::OutsideRenderPassCommandBufferHelper *computePassCommands);
    angle::Result initializeDescriptorPools(CLKernelVk *kernelVk);

    void addCommandBufferDiagnostics(const std::string &commandBufferDiagnostics);
    void dumpCommandStreamDiagnostics();

  private:
    void handleDeviceLost() const;

    // mutex to synchronize the descriptor set allocations
    angle::SimpleMutex mDescriptorSetMutex;

    // Caches for DescriptorSetLayout and PipelineLayout
    DescriptorSetLayoutCache mDescriptorSetLayoutCache;
    PipelineLayoutCache mPipelineLayoutCache;

    vk::MetaDescriptorPool mMetaDescriptorPool;

    // Have the CL Context keep tabs on associated CL objects
    struct Mutable
    {
        std::unordered_set<const _cl_mem *> mMemories;
    };
    using MutableData = angle::SynchronizedValue<Mutable>;
    MutableData mAssociatedObjects;

    const cl::DevicePtrs mAssociatedDevices;

    std::vector<std::string> mCommandBufferDiagnostics;

    // Table of minimum required image formats for OpenCL
    static constexpr cl_image_format kMinSupportedFormatsReadOrWrite[11] = {
        {CL_RGBA, CL_UNORM_INT8},  {CL_RGBA, CL_UNSIGNED_INT8},  {CL_RGBA, CL_SIGNED_INT8},
        {CL_RGBA, CL_UNORM_INT16}, {CL_RGBA, CL_UNSIGNED_INT16}, {CL_RGBA, CL_SIGNED_INT16},
        {CL_RGBA, CL_HALF_FLOAT},  {CL_RGBA, CL_UNSIGNED_INT32}, {CL_RGBA, CL_SIGNED_INT32},
        {CL_RGBA, CL_FLOAT},       {CL_BGRA, CL_UNORM_INT8}};

    static constexpr cl_image_format kMinSupportedFormatsReadAndWrite[18] = {
        {CL_R, CL_UNORM_INT8},        {CL_R, CL_UNSIGNED_INT8},    {CL_R, CL_SIGNED_INT8},
        {CL_R, CL_UNSIGNED_INT16},    {CL_R, CL_SIGNED_INT16},     {CL_R, CL_HALF_FLOAT},
        {CL_R, CL_UNSIGNED_INT32},    {CL_R, CL_SIGNED_INT32},     {CL_R, CL_FLOAT},
        {CL_RGBA, CL_UNORM_INT8},     {CL_RGBA, CL_UNSIGNED_INT8}, {CL_RGBA, CL_SIGNED_INT8},
        {CL_RGBA, CL_UNSIGNED_INT16}, {CL_RGBA, CL_SIGNED_INT16},  {CL_RGBA, CL_HALF_FLOAT},
        {CL_RGBA, CL_UNSIGNED_INT32}, {CL_RGBA, CL_SIGNED_INT32},  {CL_RGBA, CL_FLOAT}};

    // TODO: Keep minimum supported fmt table (so we can bail early)
    // http://anglebug.com/488270265
    static constexpr cl_image_format kSupportedFormats[] = {
        // R
        {CL_R, CL_SNORM_INT8},
        {CL_R, CL_SNORM_INT16},
        {CL_R, CL_UNORM_INT8},
        {CL_R, CL_UNORM_INT16},

        {CL_R, CL_SIGNED_INT8},
        {CL_R, CL_SIGNED_INT16},
        {CL_R, CL_SIGNED_INT32},
        {CL_R, CL_UNSIGNED_INT8},
        {CL_R, CL_UNSIGNED_INT16},
        {CL_R, CL_UNSIGNED_INT32},

        {CL_R, CL_HALF_FLOAT},
        {CL_R, CL_FLOAT},

        // A
        {CL_A, CL_SNORM_INT8},
        {CL_A, CL_SNORM_INT16},
        {CL_A, CL_UNORM_INT8},
        {CL_A, CL_UNORM_INT16},

        {CL_A, CL_SIGNED_INT8},
        {CL_A, CL_SIGNED_INT16},
        {CL_A, CL_SIGNED_INT32},
        {CL_A, CL_UNSIGNED_INT8},
        {CL_A, CL_UNSIGNED_INT16},
        {CL_A, CL_UNSIGNED_INT32},

        {CL_A, CL_HALF_FLOAT},
        {CL_A, CL_FLOAT},

        // RG
        {CL_RG, CL_SNORM_INT8},
        {CL_RG, CL_SNORM_INT16},
        {CL_RG, CL_UNORM_INT8},
        {CL_RG, CL_UNORM_INT16},

        {CL_RG, CL_SIGNED_INT8},
        {CL_RG, CL_SIGNED_INT16},
        {CL_RG, CL_SIGNED_INT32},
        {CL_RG, CL_UNSIGNED_INT8},
        {CL_RG, CL_UNSIGNED_INT16},
        {CL_RG, CL_UNSIGNED_INT32},

        {CL_RG, CL_HALF_FLOAT},
        {CL_RG, CL_FLOAT},

        // RGBA
        {CL_RGBA, CL_SNORM_INT8},
        {CL_RGBA, CL_SNORM_INT16},
        {CL_RGBA, CL_UNORM_INT8},
        {CL_RGBA, CL_UNORM_INT16},

        {CL_RGBA, CL_SIGNED_INT8},
        {CL_RGBA, CL_SIGNED_INT16},
        {CL_RGBA, CL_SIGNED_INT32},
        {CL_RGBA, CL_UNSIGNED_INT8},
        {CL_RGBA, CL_UNSIGNED_INT16},
        {CL_RGBA, CL_UNSIGNED_INT32},

        {CL_RGBA, CL_HALF_FLOAT},
        {CL_RGBA, CL_FLOAT},

        // ARGB
        {CL_ARGB, CL_SNORM_INT8},
        {CL_ARGB, CL_UNORM_INT8},
        {CL_ARGB, CL_SIGNED_INT8},
        {CL_ARGB, CL_UNSIGNED_INT8},

        // BGRA
        {CL_BGRA, CL_SNORM_INT8},
        {CL_BGRA, CL_UNORM_INT8},
        {CL_BGRA, CL_SIGNED_INT8},
        {CL_BGRA, CL_UNSIGNED_INT8},

        // LUMINANCE
        {CL_LUMINANCE, CL_SNORM_INT8},
        {CL_LUMINANCE, CL_SNORM_INT16},
        {CL_LUMINANCE, CL_UNORM_INT8},
        {CL_LUMINANCE, CL_UNORM_INT16},

        {CL_LUMINANCE, CL_SIGNED_INT8},
        {CL_LUMINANCE, CL_SIGNED_INT16},
        {CL_LUMINANCE, CL_SIGNED_INT32},
        {CL_LUMINANCE, CL_UNSIGNED_INT8},
        {CL_LUMINANCE, CL_UNSIGNED_INT16},
        {CL_LUMINANCE, CL_UNSIGNED_INT32},

        {CL_LUMINANCE, CL_HALF_FLOAT},
        {CL_LUMINANCE, CL_FLOAT},

        // INTENSITY
        {CL_INTENSITY, CL_SNORM_INT8},
        {CL_INTENSITY, CL_SNORM_INT16},
        {CL_INTENSITY, CL_UNORM_INT8},
        {CL_INTENSITY, CL_UNORM_INT16},

        {CL_INTENSITY, CL_SIGNED_INT8},
        {CL_INTENSITY, CL_SIGNED_INT16},
        {CL_INTENSITY, CL_SIGNED_INT32},
        {CL_INTENSITY, CL_UNSIGNED_INT8},
        {CL_INTENSITY, CL_UNSIGNED_INT16},
        {CL_INTENSITY, CL_UNSIGNED_INT32},

        {CL_INTENSITY, CL_HALF_FLOAT},
        {CL_INTENSITY, CL_FLOAT},

        // RGB
        // TODO: Add support for this format
        // http://anglebug.com/488270265
        {CL_RGB, CL_UNORM_INT_101010},

        // DEPTH
        {CL_DEPTH, CL_UNORM_INT16},
        {CL_DEPTH, CL_FLOAT},
    };

    friend class CLMemoryVk;
};

inline bool CLContextVk::hasMemory(cl_mem memory) const
{
    const auto data = mAssociatedObjects.synchronize();
    return data->mMemories.find(memory) != data->mMemories.cend();
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLCONTEXTVK_H_

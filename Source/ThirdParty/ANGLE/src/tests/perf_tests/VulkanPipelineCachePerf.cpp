//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanPipelineCachePerf:
//   Performance benchmark for the Vulkan Pipeline cache.

#include "ANGLEPerfTest.h"

#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "util/random_utils.h"

using namespace rx;

namespace
{
constexpr unsigned int kIterationsPerStep = 100;

class VulkanPipelineCachePerfTest : public ANGLEPerfTest
{
  public:
    VulkanPipelineCachePerfTest();
    ~VulkanPipelineCachePerfTest();

    void SetUp() override;
    void step() override;

    GraphicsPipelineCache mCache;
    angle::RNG mRNG;

    std::vector<vk::GraphicsPipelineDesc> mCacheHits;
    std::vector<vk::GraphicsPipelineDesc> mCacheMisses;
    size_t mMissIndex = 0;

    bool mWithDynamicState;

  private:
    void randomizeDesc(vk::GraphicsPipelineDesc *desc);
};

VulkanPipelineCachePerfTest::VulkanPipelineCachePerfTest()
    : ANGLEPerfTest("VulkanPipelineCachePerf", "", "", kIterationsPerStep), mWithDynamicState(false)
{}

VulkanPipelineCachePerfTest::~VulkanPipelineCachePerfTest()
{
    mCache.reset();
}

void VulkanPipelineCachePerfTest::SetUp()
{
    // Insert a number of random pipeline states.
    for (int pipelineCount = 0; pipelineCount < 100; ++pipelineCount)
    {
        vk::Pipeline pipeline;
        vk::GraphicsPipelineDesc desc;
        randomizeDesc(&desc);

        if (pipelineCount < 10)
        {
            mCacheHits.push_back(desc);
        }
        mCache.populate(desc, std::move(pipeline));
    }

    for (int missCount = 0; missCount < 10000; ++missCount)
    {
        vk::GraphicsPipelineDesc desc;
        randomizeDesc(&desc);
        mCacheMisses.push_back(desc);
    }
}

void VulkanPipelineCachePerfTest::randomizeDesc(vk::GraphicsPipelineDesc *desc)
{
    std::vector<uint8_t> bytes(sizeof(vk::GraphicsPipelineDesc));
    FillVectorWithRandomUBytes(&mRNG, &bytes);
    memcpy(desc, bytes.data(), sizeof(vk::GraphicsPipelineDesc));

    desc->setSupportsDynamicStateForTest(mWithDynamicState);
}

void VulkanPipelineCachePerfTest::step()
{
    vk::RenderPass rp;
    vk::PipelineLayout pl;
    vk::PipelineCache pc;
    PipelineCacheAccess spc;
    vk::RefCounted<vk::ShaderAndSerial> vsAndSerial;
    vk::RefCounted<vk::ShaderAndSerial> fsAndSerial;
    vk::ShaderAndSerialMap ssm;
    const vk::GraphicsPipelineDesc *desc = nullptr;
    vk::PipelineHelper *result           = nullptr;
    gl::AttributesMask am;
    gl::ComponentTypeMask ctm;
    gl::DrawBufferMask dbm;

    // The Vulkan handle types are difficult to cast to without #ifdefs.
    VkShaderModule vs = (VkShaderModule)1;
    VkShaderModule fs = (VkShaderModule)2;

    vsAndSerial.get().get().setHandle(vs);
    fsAndSerial.get().get().setHandle(fs);

    ssm[gl::ShaderType::Vertex].set(&vsAndSerial);
    ssm[gl::ShaderType::Fragment].set(&fsAndSerial);

    spc.init(&pc, nullptr);

    vk::SpecializationConstants defaultSpecConsts{};

    for (unsigned int iteration = 0; iteration < kIterationsPerStep; ++iteration)
    {
        for (const auto &hit : mCacheHits)
        {
            (void)mCache.getPipeline(VK_NULL_HANDLE, &spc, rp, pl, am, ctm, dbm, ssm,
                                     defaultSpecConsts, PipelineSource::Draw, hit, &desc, &result);
        }
    }

    for (int missCount = 0; missCount < 20 && mMissIndex < mCacheMisses.size();
         ++missCount, ++mMissIndex)
    {
        const auto &miss = mCacheMisses[mMissIndex];
        (void)mCache.getPipeline(VK_NULL_HANDLE, &spc, rp, pl, am, ctm, dbm, ssm, defaultSpecConsts,
                                 PipelineSource::Draw, miss, &desc, &result);
    }

    vsAndSerial.get().get().setHandle(VK_NULL_HANDLE);
    fsAndSerial.get().get().setHandle(VK_NULL_HANDLE);
}

}  // anonymous namespace

TEST_F(VulkanPipelineCachePerfTest, Run)
{
    run();
}

TEST_F(VulkanPipelineCachePerfTest, Run_WithDynamicState)
{
    mWithDynamicState = true;
    run();
}

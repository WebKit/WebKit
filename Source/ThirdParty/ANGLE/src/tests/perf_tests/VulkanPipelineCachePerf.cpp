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

struct Params
{
    bool withDynamicState = false;
};

class VulkanPipelineCachePerfTest : public ANGLEPerfTest,
                                    public ::testing::WithParamInterface<Params>
{
  public:
    VulkanPipelineCachePerfTest();
    ~VulkanPipelineCachePerfTest();

    void SetUp() override;
    void step() override;

    GraphicsPipelineCache<GraphicsPipelineDescCompleteHash> mCache;
    angle::RNG mRNG;

    std::vector<vk::GraphicsPipelineDesc> mCacheHits;
    std::vector<vk::GraphicsPipelineDesc> mCacheMisses;
    size_t mMissIndex = 0;

  private:
    void randomizeDesc(vk::GraphicsPipelineDesc *desc);
};

VulkanPipelineCachePerfTest::VulkanPipelineCachePerfTest()
    : ANGLEPerfTest("VulkanPipelineCachePerf", "", "", kIterationsPerStep), mRNG(0x12345678u)
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

    desc->setSupportsDynamicStateForTest(GetParam().withDynamicState);
}

void VulkanPipelineCachePerfTest::step()
{
    vk::RenderPass rp;
    vk::PipelineLayout pl;
    vk::PipelineCache pc;
    PipelineCacheAccess spc;
    vk::RefCounted<vk::ShaderModule> vsRefCounted;
    vk::RefCounted<vk::ShaderModule> fsRefCounted;
    vk::ShaderModuleMap ssm;
    const vk::GraphicsPipelineDesc *desc = nullptr;
    vk::PipelineHelper *result           = nullptr;

    // The Vulkan handle types are difficult to cast to without #ifdefs.
    VkShaderModule vs = (VkShaderModule)1;
    VkShaderModule fs = (VkShaderModule)2;

    vsRefCounted.get().setHandle(vs);
    fsRefCounted.get().setHandle(fs);

    ssm[gl::ShaderType::Vertex].set(&vsRefCounted);
    ssm[gl::ShaderType::Fragment].set(&fsRefCounted);

    spc.init(&pc, nullptr);

    vk::SpecializationConstants defaultSpecConsts{};

    for (unsigned int iteration = 0; iteration < kIterationsPerStep; ++iteration)
    {
        for (const auto &hit : mCacheHits)
        {
            if (!mCache.getPipeline(hit, &desc, &result))
            {
                (void)mCache.createPipeline(VK_NULL_HANDLE, &spc, rp, pl, ssm, defaultSpecConsts,
                                            PipelineSource::Draw, hit, &desc, &result);
            }
        }
    }

    for (int missCount = 0; missCount < 20 && mMissIndex < mCacheMisses.size();
         ++missCount, ++mMissIndex)
    {
        const auto &miss = mCacheMisses[mMissIndex];
        if (!mCache.getPipeline(miss, &desc, &result))
        {
            (void)mCache.createPipeline(VK_NULL_HANDLE, &spc, rp, pl, ssm, defaultSpecConsts,
                                        PipelineSource::Draw, miss, &desc, &result);
        }
    }

    vsRefCounted.get().setHandle(VK_NULL_HANDLE);
    fsRefCounted.get().setHandle(VK_NULL_HANDLE);
}

}  // anonymous namespace

// Test performance of pipeline hash and look up in Vulkan
TEST_P(VulkanPipelineCachePerfTest, Run)
{
    run();
}

INSTANTIATE_TEST_SUITE_P(,
                         VulkanPipelineCachePerfTest,
                         ::testing::ValuesIn(std::vector<Params>{{Params{false}, Params{true}}}));

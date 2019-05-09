//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GPUTestExpectationsTest.cpp : Tests of the test_expectations library.

#include "test_expectations/GPUTestConfig.h"
#include "test_expectations/GPUTestExpectationsParser.h"
#include "test_utils/ANGLETest.h"

namespace angle
{

class GPUTestConfigTest : public ANGLETest
{
  protected:
    GPUTestConfigTest() {}

    // todo(jonahr): Eventually could add support for all conditions/operating
    // systems, but these are the ones in use for now
    void validateConfigBase(const GPUTestConfig &config)
    {
        EXPECT_EQ(IsWindows(), config.getConditions()[GPUTestConfig::kConditionWin]);
        EXPECT_EQ(IsOSX(), config.getConditions()[GPUTestConfig::kConditionMac]);
        EXPECT_EQ(IsLinux(), config.getConditions()[GPUTestConfig::kConditionLinux]);
        EXPECT_EQ(IsAndroid(), config.getConditions()[GPUTestConfig::kConditionAndroid]);
        EXPECT_EQ(IsNexus5X(), config.getConditions()[GPUTestConfig::kConditionNexus5X]);
        EXPECT_EQ(IsPixel2(), config.getConditions()[GPUTestConfig::kConditionPixel2]);
        EXPECT_EQ(IsIntel(), config.getConditions()[GPUTestConfig::kConditionIntel]);
        EXPECT_EQ(IsAMD(), config.getConditions()[GPUTestConfig::kConditionAMD]);
        EXPECT_EQ(IsNVIDIA(), config.getConditions()[GPUTestConfig::kConditionNVIDIA]);
        EXPECT_EQ(IsDebug(), config.getConditions()[GPUTestConfig::kConditionDebug]);
        EXPECT_EQ(IsRelease(), config.getConditions()[GPUTestConfig::kConditionRelease]);
    }

    void validateConfigAPI(const GPUTestConfig &config, const GPUTestConfig::API &api)
    {
        bool D3D9      = false;
        bool D3D11     = false;
        bool GLDesktop = false;
        bool GLES      = false;
        bool Vulkan    = false;
        switch (api)
        {
            case GPUTestConfig::kAPID3D9:
                D3D9 = true;
                break;
            case GPUTestConfig::kAPID3D11:
                D3D11 = true;
                break;
            case GPUTestConfig::kAPIGLDesktop:
                GLDesktop = true;
                break;
            case GPUTestConfig::kAPIGLES:
                GLES = true;
                break;
            case GPUTestConfig::kAPIVulkan:
                Vulkan = true;
                break;
            case GPUTestConfig::kAPIUnknown:
            default:
                break;
        }
        EXPECT_EQ(D3D9, config.getConditions()[GPUTestConfig::kConditionD3D9]);
        EXPECT_EQ(D3D11, config.getConditions()[GPUTestConfig::kConditionD3D11]);
        EXPECT_EQ(GLDesktop, config.getConditions()[GPUTestConfig::kConditionGLDesktop]);
        EXPECT_EQ(GLES, config.getConditions()[GPUTestConfig::kConditionGLES]);
        EXPECT_EQ(Vulkan, config.getConditions()[GPUTestConfig::kConditionVulkan]);
    }
};

// Create a new GPUTestConfig and make sure all the condition flags were set
// correctly based on the hardware.
TEST_P(GPUTestConfigTest, GPUTestConfigConditions)
{
    GPUTestConfig config;
    validateConfigBase(config);
}

// Create a new GPUTestConfig with each backend specified and validate the
// condition flags are set correctly.
TEST_P(GPUTestConfigTest, GPUTestConfigConditions_D3D9)
{
    GPUTestConfig config(GPUTestConfig::kAPID3D9);
    validateConfigAPI(config, GPUTestConfig::kAPID3D9);
}

TEST_P(GPUTestConfigTest, GPUTestConfigConditions_D3D11)
{
    GPUTestConfig config(GPUTestConfig::kAPID3D11);
    validateConfigAPI(config, GPUTestConfig::kAPID3D11);
}

TEST_P(GPUTestConfigTest, GPUTestConfigConditions_GLDesktop)
{
    GPUTestConfig config(GPUTestConfig::kAPIGLDesktop);
    validateConfigAPI(config, GPUTestConfig::kAPIGLDesktop);
}

TEST_P(GPUTestConfigTest, GPUTestConfigConditions_GLES)
{
    GPUTestConfig config(GPUTestConfig::kAPIGLES);
    validateConfigAPI(config, GPUTestConfig::kAPIGLES);
}

TEST_P(GPUTestConfigTest, GPUTestConfigConditions_Vulkan)
{
    GPUTestConfig config(GPUTestConfig::kAPIVulkan);
    validateConfigAPI(config, GPUTestConfig::kAPIVulkan);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(GPUTestConfigTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES2_VULKAN());

}  // namespace angle

//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef TEST_EXPECTATIONS_GPU_TEST_CONFIG_H_
#define TEST_EXPECTATIONS_GPU_TEST_CONFIG_H_

#include <common/bitset_utils.h>

namespace angle
{

struct GPUTestConfig
{
  public:
    enum API
    {
        kAPIUnknown = 0,
        kAPID3D9,
        kAPID3D11,
        kAPIGLDesktop,
        kAPIGLES,
        kAPIVulkan,
        kAPISwiftShader,
        kAPIMetal,
    };

    enum Condition
    {
        kConditionNone = 0,
        kConditionWinXP,
        kConditionWinVista,
        kConditionWin7,
        kConditionWin8,
        kConditionWin10,
        kConditionWin,
        kConditionMacLeopard,
        kConditionMacSnowLeopard,
        kConditionMacLion,
        kConditionMacMountainLion,
        kConditionMacMavericks,
        kConditionMacYosemite,
        kConditionMacElCapitan,
        kConditionMacSierra,
        kConditionMacHighSierra,
        kConditionMacMojave,
        kConditionMac,
        kConditionLinux,
        kConditionAndroid,
        kConditionNVIDIA,
        kConditionAMD,
        kConditionIntel,
        kConditionVMWare,
        kConditionRelease,
        kConditionDebug,
        kConditionD3D9,
        kConditionD3D11,
        kConditionGLDesktop,
        kConditionGLES,
        kConditionVulkan,
        kConditionMetal,
        kConditionNexus5X,
        kConditionPixel2OrXL,
        kConditionNVIDIAQuadroP400,
        kConditionSwiftShader,

        kNumberOfConditions,
    };

    using ConditionArray = angle::BitSet<GPUTestConfig::kNumberOfConditions>;

    GPUTestConfig();
    GPUTestConfig(const API &api);

    const GPUTestConfig::ConditionArray &getConditions() const;

  protected:
    GPUTestConfig::ConditionArray mConditions;
};

}  // namespace angle

#endif  // TEST_EXPECTATIONS_GPU_TEST_CONFIG_H_

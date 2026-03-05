//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLEComputeTestCL:
//   Base class for ANGLEComputeTestCL performance tests
//

#include "ANGLEComputeTestCL.h"

#include "ANGLEPerfTestArgs.h"
#include "common/system_utils.h"
#include "util/shader_utils.h"
#include "util/test_utils.h"

#include <string>

using namespace angle;

ANGLEComputeTestCL::ANGLEComputeTestCL(const std::string &name,
                                       const RenderTestParams &testParams,
                                       const char *units)
    : ANGLEPerfTest(name,
                    testParams.backend(),
                    testParams.story(),
                    OneFrame() ? 1 : testParams.iterationsPerStep,
                    units),
      mTestParams(testParams)
{
    // Force fast tests to make sure our slowest bots don't time out.
    if (OneFrame())
    {
        const_cast<RenderTestParams &>(testParams).iterationsPerStep = 1;
    }
}

ANGLEComputeTestCL::~ANGLEComputeTestCL() {}

void ANGLEComputeTestCL::step()
{
    drawBenchmark();

    // Sample system memory
    uint64_t processMemoryUsageKB = GetProcessMemoryUsageKB();
    if (processMemoryUsageKB)
    {
        mProcessMemoryUsageKBSamples.push_back(processMemoryUsageKB);
    }
}

void ANGLEComputeTestCL::SetUp()
{
    if (mSkipTest)
    {
        return;
    }

    // Set a consistent CPU core affinity and high priority.
    StabilizeCPUForBenchmarking();

    initializeBenchmark();
    ANGLEPerfTest::SetUp();
}

void ANGLEComputeTestCL::TearDown()
{
    ANGLEPerfTest::TearDown();
}

void ANGLEComputeTestCL::updatePerfCounters()
{
    if (mPerfCounterInfo.empty())
    {
        return;
    }

    std::vector<PerfMonitorTriplet> perfData = GetPerfMonitorTriplets();
    ASSERT(!perfData.empty());

    for (auto &iter : mPerfCounterInfo)
    {
        uint32_t counter               = iter.first;
        std::vector<GLuint64> &samples = iter.second.samples;
        ASSERT(perfData[counter].group == 0);
        ASSERT(perfData[counter].counter == counter);
        samples.push_back(perfData[counter].value);
    }
}

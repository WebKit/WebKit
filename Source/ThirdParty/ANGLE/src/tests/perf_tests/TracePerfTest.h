//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Header file for both TracePerfTest and TracePerfTestCL
//

#pragma once

#include <gtest/gtest.h>
#include "tests/perf_tests/ANGLEPerfTest.h"
#include "tests/perf_tests/ANGLEPerfTestArgs.h"
#include "tests/test_expectations/GPUTestExpectationsParser.h"
#include "util/capture/frame_capture_test_utils.h"
#include "util/test_utils.h"

namespace angle
{

constexpr char kTraceTestFolder[] = "src/tests/restricted_traces";
constexpr size_t kMaxPath         = 1024;

struct TracePerfParams final : public RenderTestParams
{
    // Common default options
    TracePerfParams(const TraceInfo &traceInfoIn,
                    GLESDriverType driverType,
                    EGLenum platformType,
                    EGLenum deviceType)
        : traceInfo(traceInfoIn)
    {
        majorVersion = traceInfo.contextClientMajorVersion;
        minorVersion = traceInfo.contextClientMinorVersion;
        windowWidth  = traceInfo.drawSurfaceWidth;
        windowHeight = traceInfo.drawSurfaceHeight;
        colorSpace   = traceInfo.drawSurfaceColorSpace;
        isCL         = traceInfo.isCL;

        // Display the frame after every drawBenchmark invocation
        iterationsPerStep = 1;

        trackGpuTime = gTrackGPUTime;

        driver                   = driverType;
        eglParameters.renderer   = platformType;
        eglParameters.deviceType = deviceType;

        ASSERT(!gOffscreen || !gVsync);

        if (gOffscreen)
        {
            surfaceType = SurfaceType::Offscreen;
        }
        if (gVsync)
        {
            surfaceType = SurfaceType::WindowWithVSync;
        }

        // Force on features if we're validating serialization.
        if (gTraceTestValidation)
        {
            // Enable limits when validating traces because we usually turn off capture.
            eglParameters.enable(Feature::EnableCaptureLimits);

            // This feature should also be enabled in capture to mirror the replay.
            eglParameters.enable(Feature::ForceInitShaderVariables);
        }
    }

    std::string story() const override
    {
        std::stringstream strstr;
        strstr << RenderTestParams::story() << "_" << traceInfo.name;
        return strstr.str();
    }

    TraceInfo traceInfo = {};
};

ANGLEPerfTest *CreateTracePerfTestCL(std::unique_ptr<const TracePerfParams> params);

}  // namespace angle

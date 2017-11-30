//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DrawCallPerfParams.h:
//   Parametrization for performance tests for ANGLE draw call overhead.
//

#ifndef TESTS_PERF_TESTS_DRAW_CALL_PERF_PARAMS_H_
#define TESTS_PERF_TESTS_DRAW_CALL_PERF_PARAMS_H_

#include <ostream>

#include "ANGLEPerfTest.h"

struct DrawCallPerfParams : public RenderTestParams
{
    // Common default options
    DrawCallPerfParams()
    {
        majorVersion = 2;
        minorVersion = 0;
        windowWidth  = 256;
        windowHeight = 256;
    }
    virtual ~DrawCallPerfParams() {}

    std::string suffix() const override;

    unsigned int iterations = 50;
    double runTimeSeconds   = 10.0;
    int numTris             = 1;
    bool useFBO             = false;
};

std::ostream &operator<<(std::ostream &os, const DrawCallPerfParams &params);

DrawCallPerfParams DrawCallPerfD3D11Params(bool useNullDevice, bool renderToTexture);

DrawCallPerfParams DrawCallPerfD3D9Params(bool useNullDevice, bool renderToTexture);

DrawCallPerfParams DrawCallPerfOpenGLOrGLESParams(bool useNullDevice, bool renderToTexture);

DrawCallPerfParams DrawCallPerfValidationOnly();

DrawCallPerfParams DrawCallPerfVulkanParams(bool renderToTexture);

#endif  // TESTS_PERF_TESTS_DRAW_CALL_PERF_PARAMS_H_

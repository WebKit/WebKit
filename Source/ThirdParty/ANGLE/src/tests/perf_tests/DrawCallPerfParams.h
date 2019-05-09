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
    DrawCallPerfParams();
    virtual ~DrawCallPerfParams();

    std::string suffix() const override;

    double runTimeSeconds;
    int numTris;
    bool useFBO;
};

DrawCallPerfParams DrawCallPerfD3D11Params(bool useNullDevice, bool renderToTexture);
DrawCallPerfParams DrawCallPerfD3D9Params(bool useNullDevice, bool renderToTexture);
DrawCallPerfParams DrawCallPerfOpenGLOrGLESParams(bool useNullDevice, bool renderToTexture);
DrawCallPerfParams DrawCallPerfValidationOnly();
DrawCallPerfParams DrawCallPerfVulkanParams(bool useNullDevice, bool renderToTexture);
DrawCallPerfParams DrawCallPerfWGLParams(bool renderToTexture);

#endif  // TESTS_PERF_TESTS_DRAW_CALL_PERF_PARAMS_H_

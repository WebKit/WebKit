//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
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

    std::string story() const override;

    double runTimeSeconds;
    int numTris;
    bool offscreen;
};

namespace params
{
DrawCallPerfParams DrawCallD3D11();
DrawCallPerfParams DrawCallD3D9();
DrawCallPerfParams DrawCallOpenGL();
DrawCallPerfParams DrawCallValidation();
DrawCallPerfParams DrawCallVulkan();
DrawCallPerfParams DrawCallWGL();
}  // namespace params

#endif  // TESTS_PERF_TESTS_DRAW_CALL_PERF_PARAMS_H_

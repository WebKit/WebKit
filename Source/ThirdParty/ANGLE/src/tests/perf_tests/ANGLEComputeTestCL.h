//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLEComputeTestCL:
//   Base class for ANGLEComputeTestCL performance tests
//

#include "ANGLEPerfTest.h"

class ANGLEComputeTestCL : public ANGLEPerfTest
{
  public:
    ANGLEComputeTestCL(const std::string &name,
                       const RenderTestParams &testParams,
                       const char *units = "ns");
    ~ANGLEComputeTestCL() override;

    virtual void initializeBenchmark() {}
    virtual void destroyBenchmark() {}

    virtual void drawBenchmark() = 0;

    std::mutex &getTraceEventMutex() { return mTraceEventMutex; }
    bool isRenderTest() const override { return true; }

  protected:
    const RenderTestParams &mTestParams;

    void updatePerfCounters();

  private:
    void SetUp() override;
    void TearDown() override;

    void step() override;

    std::mutex mTraceEventMutex;
};

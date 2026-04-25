//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLETestCL:
//   Implementation of ANGLE CL testing fixture.
//

#include "ANGLETest.h"

namespace angle
{

template <typename Params = angle::PlatformParameters>
class ANGLETestCL : public ::testing::TestWithParam<Params>
{
  protected:
    ANGLETestCL(const PlatformParameters &params);

    virtual void testSetUp() {}
    virtual void testTearDown() {}

    void recreateTestFixture()
    {
        TearDown();
        SetUp();
    }

  private:
    void SetUp() final;

    void TearDown() final;

    bool mSetUpCalled;
    bool mIsSetUp;
    bool mTearDownCalled;
    angle::PlatformParameters mCurrentParams;
    ConfigParameters mConfigParameters;

    RenderDoc mRenderDoc;

    std::map<angle::PlatformParameters, ConfigParameters> configParams;
};

}  // namespace angle

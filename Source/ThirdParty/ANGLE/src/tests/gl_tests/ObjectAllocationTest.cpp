//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ObjectAllocationTest
//   Tests for object allocations and lifetimes.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class ObjectAllocationTest : public ANGLETest
{
  protected:
    ObjectAllocationTest() {}
};

// Test that we don't re-allocate a bound framebuffer ID.
TEST_P(ObjectAllocationTest, BindFramebufferBeforeGen)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 1);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    EXPECT_NE(1u, fbo);
    glDeleteFramebuffers(1, &fbo);
    EXPECT_GL_NO_ERROR();
}

// Test that we don't re-allocate a bound framebuffer ID, other pattern.
TEST_P(ObjectAllocationTest, BindFramebufferAfterGen)
{
    GLuint firstFBO = 0;
    glGenFramebuffers(1, &firstFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, 1);
    glDeleteFramebuffers(1, &firstFBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 2);
    GLuint secondFBOs[2] = {0};
    glGenFramebuffers(2, secondFBOs);
    EXPECT_NE(2u, secondFBOs[0]);
    EXPECT_NE(2u, secondFBOs[1]);
    glDeleteFramebuffers(2, secondFBOs);

    EXPECT_GL_NO_ERROR();
}

}  // anonymous namespace

ANGLE_INSTANTIATE_TEST_ES3(ObjectAllocationTest);
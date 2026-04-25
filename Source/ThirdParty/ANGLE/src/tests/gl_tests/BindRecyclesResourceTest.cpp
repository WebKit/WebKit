//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BindRecyclesResourceTest.cpp : Tests of the GL_CHROMIUM_bind_generates_resource extension.

#include "test_utils/ANGLETest.h"

namespace angle
{

class BindRecyclesResourceTest : public ANGLETest<>
{
  protected:
    BindRecyclesResourceTest() { setBindGeneratesResource(false); }
};

// crbug.com/496284494
TEST_P(BindRecyclesResourceTest, BufferRecycling)
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint idA;
    glGenBuffers(1, &idA);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idA);

    // Unbind the VAO
    glBindVertexArray(0);

    glDeleteBuffers(1, &idA);

    GLuint idB;
    glGenBuffers(1, &idB);
    EXPECT_NE(idA, idB);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &idB);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES3_AND_ES31_AND_ES32(BindRecyclesResourceTest);

}  // namespace angle

//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CHROMIUMPathRenderingTest
//   Test CHROMIUM subset of NV_path_rendering
//   This extension allows to render geometric paths as first class GL objects.

#include "test_utils/ANGLETest.h"

#include "common/angleutils.h"
#include "util/shader_utils.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>

using namespace angle;

namespace
{

bool CheckPixels(GLint x,
                 GLint y,
                 GLsizei width,
                 GLsizei height,
                 GLint tolerance,
                 const angle::GLColor &color)
{
    for (GLint yy = 0; yy < height; ++yy)
    {
        for (GLint xx = 0; xx < width; ++xx)
        {
            const auto px = x + xx;
            const auto py = y + yy;
            EXPECT_PIXEL_NEAR(px, py, color.R, color.G, color.B, color.A, tolerance);
        }
    }

    return true;
}

void ExpectEqualMatrix(const GLfloat *expected, const GLfloat *actual)
{
    for (size_t i = 0; i < 16; ++i)
    {
        EXPECT_EQ(expected[i], actual[i]);
    }
}

void ExpectEqualMatrix(const GLfloat *expected, const GLint *actual)
{
    for (size_t i = 0; i < 16; ++i)
    {
        EXPECT_EQ(static_cast<GLint>(std::roundf(expected[i])), actual[i]);
    }
}

const int kResolution = 300;

class CHROMIUMPathRenderingTest : public ANGLETest
{
  protected:
    CHROMIUMPathRenderingTest()
    {
        setWindowWidth(kResolution);
        setWindowHeight(kResolution);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);
    }

    bool isApplicable() const { return IsGLExtensionEnabled("GL_CHROMIUM_path_rendering"); }

    void tryAllDrawFunctions(GLuint path, GLenum err)
    {
        glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F);
        EXPECT_GL_ERROR(err);

        glStencilFillPathCHROMIUM(path, GL_COUNT_DOWN_CHROMIUM, 0x7F);
        EXPECT_GL_ERROR(err);

        glStencilStrokePathCHROMIUM(path, 0x80, 0x80);
        EXPECT_GL_ERROR(err);

        glCoverFillPathCHROMIUM(path, GL_BOUNDING_BOX_CHROMIUM);
        EXPECT_GL_ERROR(err);

        glCoverStrokePathCHROMIUM(path, GL_BOUNDING_BOX_CHROMIUM);
        EXPECT_GL_ERROR(err);

        glStencilThenCoverStrokePathCHROMIUM(path, 0x80, 0x80, GL_BOUNDING_BOX_CHROMIUM);
        EXPECT_GL_ERROR(err);

        glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F,
                                           GL_BOUNDING_BOX_CHROMIUM);
        EXPECT_GL_ERROR(err);
    }
};

// Test setting and getting of path rendering matrices.
TEST_P(CHROMIUMPathRenderingTest, TestMatrix)
{
    if (!isApplicable())
        return;

    static const GLfloat kIdentityMatrix[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                                0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    static const GLfloat kSeqMatrix[16] = {0.5f,   -0.5f,  -0.1f,  -0.8f, 4.4f,   5.5f,
                                           6.6f,   7.7f,   8.8f,   9.9f,  10.11f, 11.22f,
                                           12.33f, 13.44f, 14.55f, 15.66f};

    static const GLenum kMatrixModes[] = {GL_PATH_MODELVIEW_CHROMIUM, GL_PATH_PROJECTION_CHROMIUM};

    static const GLenum kGetMatrixModes[] = {GL_PATH_MODELVIEW_MATRIX_CHROMIUM,
                                             GL_PATH_PROJECTION_MATRIX_CHROMIUM};

    for (size_t i = 0; i < 2; ++i)
    {
        GLfloat mf[16];
        GLint mi[16];
        std::memset(mf, 0, sizeof(mf));
        std::memset(mi, 0, sizeof(mi));
        glGetFloatv(kGetMatrixModes[i], mf);
        glGetIntegerv(kGetMatrixModes[i], mi);
        ExpectEqualMatrix(kIdentityMatrix, mf);
        ExpectEqualMatrix(kIdentityMatrix, mi);

        glMatrixLoadfCHROMIUM(kMatrixModes[i], kSeqMatrix);
        std::memset(mf, 0, sizeof(mf));
        std::memset(mi, 0, sizeof(mi));
        glGetFloatv(kGetMatrixModes[i], mf);
        glGetIntegerv(kGetMatrixModes[i], mi);
        ExpectEqualMatrix(kSeqMatrix, mf);
        ExpectEqualMatrix(kSeqMatrix, mi);

        glMatrixLoadIdentityCHROMIUM(kMatrixModes[i]);
        std::memset(mf, 0, sizeof(mf));
        std::memset(mi, 0, sizeof(mi));
        glGetFloatv(kGetMatrixModes[i], mf);
        glGetIntegerv(kGetMatrixModes[i], mi);
        ExpectEqualMatrix(kIdentityMatrix, mf);
        ExpectEqualMatrix(kIdentityMatrix, mi);

        ASSERT_GL_NO_ERROR();
    }
}

// Test that trying to set incorrect matrix target results
// in a GL error.
TEST_P(CHROMIUMPathRenderingTest, TestMatrixErrors)
{
    if (!isApplicable())
        return;

    GLfloat mf[16];
    std::memset(mf, 0, sizeof(mf));

    glMatrixLoadfCHROMIUM(GL_PATH_MODELVIEW_CHROMIUM, mf);
    ASSERT_GL_NO_ERROR();

    glMatrixLoadIdentityCHROMIUM(GL_PATH_PROJECTION_CHROMIUM);
    ASSERT_GL_NO_ERROR();

    // Test that invalid matrix targets fail.
    glMatrixLoadfCHROMIUM(GL_PATH_MODELVIEW_CHROMIUM - 1, mf);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Test that invalid matrix targets fail.
    glMatrixLoadIdentityCHROMIUM(GL_PATH_PROJECTION_CHROMIUM + 1);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Test basic path create and delete.
TEST_P(CHROMIUMPathRenderingTest, TestGenDelete)
{
    if (!isApplicable())
        return;

    // This is unspecified in NV_path_rendering.
    EXPECT_EQ(0u, glGenPathsCHROMIUM(0));
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    GLuint path = glGenPathsCHROMIUM(1);
    EXPECT_NE(0u, path);
    glDeletePathsCHROMIUM(path, 1);
    ASSERT_GL_NO_ERROR();

    GLuint first_path = glGenPathsCHROMIUM(5);
    EXPECT_NE(0u, first_path);
    glDeletePathsCHROMIUM(first_path, 5);
    ASSERT_GL_NO_ERROR();

    // Test deleting paths that are not actually allocated:
    // "unused names in /paths/ are silently ignored".
    first_path = glGenPathsCHROMIUM(5);
    EXPECT_NE(0u, first_path);
    glDeletePathsCHROMIUM(first_path, 6);
    ASSERT_GL_NO_ERROR();

    GLsizei big_range = 0xffff;
    first_path        = glGenPathsCHROMIUM(big_range);
    EXPECT_NE(0u, first_path);
    glDeletePathsCHROMIUM(first_path, big_range);
    ASSERT_GL_NO_ERROR();

    // Test glIsPathCHROMIUM(). A path object is not considered a path untill
    // it has actually been specified with a path data.

    path = glGenPathsCHROMIUM(1);
    ASSERT_TRUE(glIsPathCHROMIUM(path) == GL_FALSE);

    // specify the data.
    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[]   = {50.0f, 50.0f};
    glPathCommandsCHROMIUM(path, 2, commands, 2, GL_FLOAT, coords);
    ASSERT_TRUE(glIsPathCHROMIUM(path) == GL_TRUE);
    glDeletePathsCHROMIUM(path, 1);
    ASSERT_TRUE(glIsPathCHROMIUM(path) == GL_FALSE);
}

// Test incorrect path creation and deletion and expect GL errors.
TEST_P(CHROMIUMPathRenderingTest, TestGenDeleteErrors)
{
    if (!isApplicable())
        return;

    // GenPaths / DeletePaths tests.
    // std::numeric_limits<GLuint>::max() is wrong for GLsizei.
    GLuint first_path = glGenPathsCHROMIUM(std::numeric_limits<GLuint>::max());
    EXPECT_EQ(first_path, 0u);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    first_path = glGenPathsCHROMIUM(-1);
    EXPECT_EQ(0u, first_path);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glDeletePathsCHROMIUM(1, -5);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    first_path = glGenPathsCHROMIUM(-1);
    EXPECT_EQ(0u, first_path);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test setting and getting path parameters.
TEST_P(CHROMIUMPathRenderingTest, TestPathParameter)
{
    if (!isApplicable())
        return;

    GLuint path = glGenPathsCHROMIUM(1);

    // specify the data.
    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[]   = {50.0f, 50.0f};
    glPathCommandsCHROMIUM(path, 2, commands, 2, GL_FLOAT, coords);
    ASSERT_GL_NO_ERROR();
    EXPECT_GL_TRUE(glIsPathCHROMIUM(path));

    static const GLenum kEndCaps[] = {GL_FLAT_CHROMIUM, GL_SQUARE_CHROMIUM, GL_ROUND_CHROMIUM};
    for (std::size_t i = 0; i < 3; ++i)
    {
        GLint x;
        glPathParameteriCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, static_cast<GLenum>(kEndCaps[i]));
        ASSERT_GL_NO_ERROR();
        glGetPathParameterivCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, &x);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(kEndCaps[i], static_cast<GLenum>(x));

        GLfloat f;
        glPathParameterfCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM,
                                 static_cast<GLfloat>(kEndCaps[(i + 1) % 3]));
        glGetPathParameterfvCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, &f);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(kEndCaps[(i + 1) % 3], static_cast<GLenum>(f));
    }

    static const GLenum kJoinStyles[] = {GL_MITER_REVERT_CHROMIUM, GL_BEVEL_CHROMIUM,
                                         GL_ROUND_CHROMIUM};
    for (std::size_t i = 0; i < 3; ++i)
    {
        GLint x;
        glPathParameteriCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM,
                                 static_cast<GLenum>(kJoinStyles[i]));
        ASSERT_GL_NO_ERROR();
        glGetPathParameterivCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM, &x);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(kJoinStyles[i], static_cast<GLenum>(x));

        GLfloat f;
        glPathParameterfCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM,
                                 static_cast<GLfloat>(kJoinStyles[(i + 1) % 3]));
        ASSERT_GL_NO_ERROR();
        glGetPathParameterfvCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM, &f);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(kJoinStyles[(i + 1) % 3], static_cast<GLenum>(f));
    }

    {
        glPathParameterfCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM, 5.0f);
        ASSERT_GL_NO_ERROR();

        GLfloat f;
        glGetPathParameterfvCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM, &f);
        EXPECT_EQ(5.0f, f);
    }

    glDeletePathsCHROMIUM(path, 1);
}

// Test that setting incorrect path parameter generates GL error.
TEST_P(CHROMIUMPathRenderingTest, TestPathParameterErrors)
{
    if (!isApplicable())
        return;

    GLuint path = glGenPathsCHROMIUM(1);

    // PathParameter*: Wrong value for the pname should fail.
    glPathParameteriCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM, GL_FLAT_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glPathParameterfCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, GL_MITER_REVERT_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // PathParameter*: Wrong floating-point value should fail.
    glPathParameterfCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM, -0.1f);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // PathParameter*: Wrong pname should fail.
    glPathParameteriCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM - 1, 5);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glDeletePathsCHROMIUM(path, 1);
}

// Test expected path object state.
TEST_P(CHROMIUMPathRenderingTest, TestPathObjectState)
{
    if (!isApplicable())
        return;

    glViewport(0, 0, kResolution, kResolution);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glStencilMask(0xffffffff);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    ASSERT_GL_NO_ERROR();

    // Test that trying to draw non-existing paths does not produce errors or results.
    GLuint non_existing_paths[] = {0, 55, 74744};
    for (auto &p : non_existing_paths)
    {
        EXPECT_GL_FALSE(glIsPathCHROMIUM(p));
        ASSERT_GL_NO_ERROR();
        tryAllDrawFunctions(p, GL_NO_ERROR);
    }

    // Path name marked as used but without path object state causes
    // a GL error upon any draw command.
    GLuint path = glGenPathsCHROMIUM(1);
    EXPECT_GL_FALSE(glIsPathCHROMIUM(path));
    tryAllDrawFunctions(path, GL_INVALID_OPERATION);
    glDeletePathsCHROMIUM(path, 1);

    // Document a bit of an inconsistency: path name marked as used but without
    // path object state causes a GL error upon any draw command (tested above).
    // Path name that had path object state, but then was "cleared", still has a
    // path object state, even though the state is empty.
    path = glGenPathsCHROMIUM(1);
    EXPECT_GL_FALSE(glIsPathCHROMIUM(path));

    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[]   = {50.0f, 50.0f};
    glPathCommandsCHROMIUM(path, 2, commands, 2, GL_FLOAT, coords);
    EXPECT_GL_TRUE(glIsPathCHROMIUM(path));

    glPathCommandsCHROMIUM(path, 0, nullptr, 0, GL_FLOAT, nullptr);
    EXPECT_GL_TRUE(glIsPathCHROMIUM(path));  // The surprise.

    tryAllDrawFunctions(path, GL_NO_ERROR);
    glDeletePathsCHROMIUM(path, 1);

    // Make sure nothing got drawn by the drawing commands that should not produce
    // anything.
    const angle::GLColor black = {0, 0, 0, 0};
    EXPECT_TRUE(CheckPixels(0, 0, kResolution, kResolution, 0, black));
}

// Test that trying to use path object that doesn't exist generates
// a GL error.
TEST_P(CHROMIUMPathRenderingTest, TestUnnamedPathsErrors)
{
    if (!isApplicable())
        return;

    // Unnamed paths: Trying to create a path object with non-existing path name
    // produces error.  (Not a error in real NV_path_rendering).
    ASSERT_GL_NO_ERROR();
    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[]   = {50.0f, 50.0f};
    glPathCommandsCHROMIUM(555, 2, commands, 2, GL_FLOAT, coords);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // PathParameter*: Using non-existing path object produces error.
    ASSERT_GL_NO_ERROR();
    glPathParameterfCHROMIUM(555, GL_PATH_STROKE_WIDTH_CHROMIUM, 5.0f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    ASSERT_GL_NO_ERROR();
    glPathParameteriCHROMIUM(555, GL_PATH_JOIN_STYLE_CHROMIUM, GL_ROUND_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that setting incorrect path data generates a GL error.
TEST_P(CHROMIUMPathRenderingTest, TestPathCommandsErrors)
{
    if (!isApplicable())
        return;

    static const GLenum kInvalidCoordType = GL_NONE;

    GLuint path        = glGenPathsCHROMIUM(1);
    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[]   = {50.0f, 50.0f};

    glPathCommandsCHROMIUM(path, 2, commands, -4, GL_FLOAT, coords);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glPathCommandsCHROMIUM(path, -1, commands, 2, GL_FLOAT, coords);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glPathCommandsCHROMIUM(path, 2, commands, 2, kInvalidCoordType, coords);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // incorrect number of coordinates
    glPathCommandsCHROMIUM(path, 2, commands, std::numeric_limits<GLsizei>::max(), GL_FLOAT,
                           coords);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // This should fail due to cmd count + coord count * short size.
    glPathCommandsCHROMIUM(path, 2, commands, std::numeric_limits<GLsizei>::max(), GL_SHORT,
                           coords);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glDeletePathsCHROMIUM(path, 1);
}

// Test that trying to render a path with invalid arguments
// generates a GL error.
TEST_P(CHROMIUMPathRenderingTest, TestPathRenderingInvalidArgs)
{
    if (!isApplicable())
        return;

    GLuint path = glGenPathsCHROMIUM(1);
    glPathCommandsCHROMIUM(path, 0, nullptr, 0, GL_FLOAT, nullptr);

    // Verify that normal calls work.
    glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F);
    ASSERT_GL_NO_ERROR();
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F, GL_BOUNDING_BOX_CHROMIUM);
    ASSERT_GL_NO_ERROR();

    // Using invalid fill mode causes INVALID_ENUM.
    glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM - 1, 0x7F);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM - 1, 0x7F,
                                       GL_BOUNDING_BOX_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Using invalid cover mode causes INVALID_ENUM.
    glCoverFillPathCHROMIUM(path, GL_CONVEX_HULL_CHROMIUM - 1);
    EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F,
                                       GL_BOUNDING_BOX_CHROMIUM + 1);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Using mask+1 not being power of two causes INVALID_VALUE with up/down fill mode
    glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x40);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_DOWN_CHROMIUM, 12, GL_BOUNDING_BOX_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // check incorrect instance parameters.

    // CoverFillPathInstanced
    {
        glCoverFillPathInstancedCHROMIUM(-1, GL_UNSIGNED_INT, &path, 0, GL_CONVEX_HULL_CHROMIUM,
                                         GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glCoverFillPathInstancedCHROMIUM(1, GL_FLOAT, &path, 0, GL_CONVEX_HULL_CHROMIUM, GL_NONE,
                                         nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glCoverFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, nullptr, 0, GL_CONVEX_HULL_CHROMIUM,
                                         GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glCoverFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_UNSIGNED_INT, GL_NONE,
                                         nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glCoverFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_CONVEX_HULL_CHROMIUM,
                                         GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glCoverFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_CONVEX_HULL_CHROMIUM,
                                         GL_TRANSLATE_X_CHROMIUM, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }

    // CoverStrokePathInstanced
    {
        glCoverStrokePathInstancedCHROMIUM(-1, GL_UNSIGNED_INT, &path, 0, GL_CONVEX_HULL_CHROMIUM,
                                           GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glCoverStrokePathInstancedCHROMIUM(1, GL_FLOAT, &path, 0, GL_CONVEX_HULL_CHROMIUM, GL_NONE,
                                           nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glCoverStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, nullptr, 0, GL_CONVEX_HULL_CHROMIUM,
                                           GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glCoverStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_UNSIGNED_INT, GL_NONE,
                                           nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glCoverStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_CONVEX_HULL_CHROMIUM,
                                           GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glCoverStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_CONVEX_HULL_CHROMIUM,
                                           GL_TRANSLATE_X_CHROMIUM, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }

    // StencilFillPathInstanced
    {
        glStencilFillPathInstancedCHROMIUM(-1, GL_UNSIGNED_INT, &path, 0, GL_COUNT_UP_CHROMIUM, 0x0,
                                           GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilFillPathInstancedCHROMIUM(1, GL_FLOAT, &path, 0, GL_COUNT_UP_CHROMIUM, 0x0,
                                           GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, nullptr, 0, GL_COUNT_UP_CHROMIUM,
                                           0x0, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_UNSIGNED_INT, 0x0,
                                           GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_COUNT_UP_CHROMIUM, 0x2,
                                           GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_COUNT_UP_CHROMIUM, 0x0,
                                           GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_COUNT_UP_CHROMIUM, 0x0,
                                           GL_TRANSLATE_X_CHROMIUM, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }

    // StencilStrokePathInstanced
    {
        glStencilStrokePathInstancedCHROMIUM(-1, GL_UNSIGNED_INT, &path, 0, 0x00, 0x00, GL_NONE,
                                             nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilStrokePathInstancedCHROMIUM(1, GL_FLOAT, &path, 0, 0x00, 0x00, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, nullptr, 0, 0x00, 0x00, GL_NONE,
                                             nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, 0x00, 0x00,
                                             GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, 0x00, 0x00,
                                             GL_TRANSLATE_X_CHROMIUM, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }

    // StencilThenCoverFillPathInstanced
    {
        glStencilThenCoverFillPathInstancedCHROMIUM(-1, GL_UNSIGNED_INT, &path, 0,
                                                    GL_COUNT_UP_CHROMIUM, 0, GL_COUNT_UP_CHROMIUM,
                                                    GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilThenCoverFillPathInstancedCHROMIUM(1, GL_FLOAT, &path, 0, GL_CONVEX_HULL_CHROMIUM,
                                                    0, GL_COUNT_UP_CHROMIUM, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilThenCoverFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, nullptr, 0,
                                                    GL_CONVEX_HULL_CHROMIUM, 0,
                                                    GL_COUNT_UP_CHROMIUM, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilThenCoverFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, GL_UNSIGNED_INT,
                                                    0, GL_COUNT_UP_CHROMIUM, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilThenCoverFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0,
                                                    GL_CONVEX_HULL_CHROMIUM, 0, GL_UNSIGNED_INT,
                                                    GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilThenCoverFillPathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0,
                                                    GL_CONVEX_HULL_CHROMIUM, 0,
                                                    GL_COUNT_UP_CHROMIUM, GL_FLOAT, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilThenCoverFillPathInstancedCHROMIUM(
            1, GL_UNSIGNED_INT, &path, 0, GL_CONVEX_HULL_CHROMIUM, 0, GL_COUNT_UP_CHROMIUM,
            GL_TRANSLATE_X_CHROMIUM, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }

    // StencilThenCoverStrokePathInstanced
    {
        glStencilThenCoverStrokePathInstancedCHROMIUM(-1, GL_UNSIGNED_INT, &path, 0, 0x0, 0x0,
                                                      GL_CONVEX_HULL_CHROMIUM, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilThenCoverStrokePathInstancedCHROMIUM(1, GL_FLOAT, &path, 0, 0x0, 0x0,
                                                      GL_CONVEX_HULL_CHROMIUM, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilThenCoverStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, nullptr, 0, 0x0, 0x0,
                                                      GL_CONVEX_HULL_CHROMIUM, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);

        glStencilThenCoverStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, 0x0, 0x0,
                                                      GL_FLOAT, GL_NONE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilThenCoverStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, 0x0, 0x0,
                                                      GL_CONVEX_HULL_CHROMIUM, GL_FLOAT, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glStencilThenCoverStrokePathInstancedCHROMIUM(1, GL_UNSIGNED_INT, &path, 0, 0x0, 0x0,
                                                      GL_CONVEX_HULL_CHROMIUM,
                                                      GL_TRANSLATE_X_CHROMIUM, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }

    glDeletePathsCHROMIUM(path, 1);
}

const GLfloat kProjectionMatrix[16] = {2.0f / kResolution,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       2.0f / kResolution,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       -1.0f,
                                       0.0f,
                                       -1.0f,
                                       -1.0f,
                                       0.0f,
                                       1.0f};

class CHROMIUMPathRenderingDrawTest : public ANGLETest
{
  protected:
    CHROMIUMPathRenderingDrawTest()
    {
        setWindowWidth(kResolution);
        setWindowHeight(kResolution);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);
    }

    bool isApplicable() const { return IsGLExtensionEnabled("GL_CHROMIUM_path_rendering"); }

    void setupStateForTestPattern()
    {
        glViewport(0, 0, kResolution, kResolution);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glStencilMask(0xffffffff);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_STENCIL_TEST);

        constexpr char kVertexShaderSource[] =
            "void main() {\n"
            "  gl_Position = vec4(1.0); \n"
            "}";

        constexpr char kFragmentShaderSource[] =
            "precision mediump float;\n"
            "uniform vec4 color;\n"
            "void main() {\n"
            "  gl_FragColor = color;\n"
            "}";

        GLuint program = CompileProgram(kVertexShaderSource, kFragmentShaderSource);
        glUseProgram(program);
        mColorLoc = glGetUniformLocation(program, "color");
        glDeleteProgram(program);

        // Set up orthogonal projection with near/far plane distance of 2.
        glMatrixLoadfCHROMIUM(GL_PATH_PROJECTION_CHROMIUM, kProjectionMatrix);
        glMatrixLoadIdentityCHROMIUM(GL_PATH_MODELVIEW_CHROMIUM);

        ASSERT_GL_NO_ERROR();
    }

    void setupPathStateForTestPattern(GLuint path)
    {
        static const GLubyte kCommands[] = {GL_MOVE_TO_CHROMIUM, GL_LINE_TO_CHROMIUM,
                                            GL_QUADRATIC_CURVE_TO_CHROMIUM,
                                            GL_CUBIC_CURVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};

        static const GLfloat kCoords[] = {50.0f, 50.0f, 75.0f, 75.0f, 100.0f, 62.5f, 50.0f,
                                          25.5f, 0.0f,  62.5f, 50.0f, 50.0f,  25.0f, 75.0f};

        glPathCommandsCHROMIUM(path, 5, kCommands, 14, GL_FLOAT, kCoords);
        glPathParameterfCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM, 5.0f);
        glPathParameterfCHROMIUM(path, GL_PATH_MITER_LIMIT_CHROMIUM, 1.0f);
        glPathParameterfCHROMIUM(path, GL_PATH_STROKE_BOUND_CHROMIUM, .02f);
        glPathParameteriCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM, GL_ROUND_CHROMIUM);
        glPathParameteriCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, GL_SQUARE_CHROMIUM);
        ASSERT_GL_NO_ERROR();
    }

    void verifyTestPatternFill(GLfloat flx, GLfloat fly)
    {
        static const GLint kFillCoords[]  = {55, 54, 50, 28, 66, 63};
        static const angle::GLColor kBlue = {0, 0, 255, 255};

        GLint x = static_cast<GLint>(flx);
        GLint y = static_cast<GLint>(fly);

        for (size_t i = 0; i < 6; i += 2)
        {
            GLint fx = kFillCoords[i];
            GLint fy = kFillCoords[i + 1];
            EXPECT_TRUE(CheckPixels(x + fx, y + fy, 1, 1, 0, kBlue));
        }
    }
    void verifyTestPatternBg(GLfloat fx, GLfloat fy)
    {
        static const GLint kBackgroundCoords[]     = {80, 80, 20, 20, 90, 1};
        static const angle::GLColor kExpectedColor = {0, 0, 0, 0};

        GLint x = static_cast<GLint>(fx);
        GLint y = static_cast<GLint>(fy);

        for (size_t i = 0; i < 6; i += 2)
        {
            GLint bx = kBackgroundCoords[i];
            GLint by = kBackgroundCoords[i + 1];
            EXPECT_TRUE(CheckPixels(x + bx, y + by, 1, 1, 0, kExpectedColor));
        }
    }

    void verifyTestPatternStroke(GLfloat fx, GLfloat fy)
    {
        GLint x = static_cast<GLint>(fx);
        GLint y = static_cast<GLint>(fy);

        // Inside the stroke we should have green.
        static const angle::GLColor kGreen = {0, 255, 0, 255};
        EXPECT_TRUE(CheckPixels(x + 50, y + 53, 1, 1, 0, kGreen));
        EXPECT_TRUE(CheckPixels(x + 26, y + 76, 1, 1, 0, kGreen));

        // Outside the path we should have black.
        static const angle::GLColor black = {0, 0, 0, 0};
        EXPECT_TRUE(CheckPixels(x + 10, y + 10, 1, 1, 0, black));
        EXPECT_TRUE(CheckPixels(x + 80, y + 80, 1, 1, 0, black));
    }

    GLuint mColorLoc;
};

// Tests that basic path rendering functions work.
TEST_P(CHROMIUMPathRenderingDrawTest, TestPathRendering)
{
    if (!isApplicable())
        return;

    static const float kBlue[]  = {0.0f, 0.0f, 1.0f, 1.0f};
    static const float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};

    setupStateForTestPattern();

    GLuint path = glGenPathsCHROMIUM(1);
    setupPathStateForTestPattern(path);

    // Do the stencil fill, cover fill, stencil stroke, cover stroke
    // in unconventional order:
    // 1) stencil the stroke in stencil high bit
    // 2) stencil the fill in low bits
    // 3) cover the fill
    // 4) cover the stroke
    // This is done to check that glPathStencilFunc works, eg the mask
    // goes through. Stencil func is not tested ATM, for simplicity.

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0xFF);
    glStencilStrokePathCHROMIUM(path, 0x80, 0x80);

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0x7F);
    glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F);

    glStencilFunc(GL_LESS, 0, 0x7F);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kBlue);
    glCoverFillPathCHROMIUM(path, GL_BOUNDING_BOX_CHROMIUM);

    glStencilFunc(GL_EQUAL, 0x80, 0x80);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kGreen);
    glCoverStrokePathCHROMIUM(path, GL_CONVEX_HULL_CHROMIUM);

    glDeletePathsCHROMIUM(path, 1);

    ASSERT_GL_NO_ERROR();

    // Verify the image.
    verifyTestPatternFill(0, 0);
    verifyTestPatternBg(0, 0);
    verifyTestPatternStroke(0, 0);
}

// Test that StencilThen{Stroke,Fill} path rendering functions work
TEST_P(CHROMIUMPathRenderingDrawTest, TestPathRenderingThenFunctions)
{
    if (!isApplicable())
        return;

    static float kBlue[]  = {0.0f, 0.0f, 1.0f, 1.0f};
    static float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};

    setupStateForTestPattern();

    GLuint path = glGenPathsCHROMIUM(1);
    setupPathStateForTestPattern(path);

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0xFF);
    glStencilFunc(GL_EQUAL, 0x80, 0x80);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kGreen);
    glStencilThenCoverStrokePathCHROMIUM(path, 0x80, 0x80, GL_BOUNDING_BOX_CHROMIUM);

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0x7F);
    glStencilFunc(GL_LESS, 0, 0x7F);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kBlue);
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F, GL_CONVEX_HULL_CHROMIUM);

    glDeletePathsCHROMIUM(path, 1);

    // Verify the image.
    verifyTestPatternFill(0, 0);
    verifyTestPatternBg(0, 0);
    verifyTestPatternStroke(0, 0);
}

// Tests that drawing with *Instanced functions work.
TEST_P(CHROMIUMPathRenderingDrawTest, TestPathRenderingInstanced)
{
    if (!isApplicable())
        return;

    static const float kBlue[]  = {0.0f, 0.0f, 1.0f, 1.0f};
    static const float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};

    setupStateForTestPattern();

    GLuint path = glGenPathsCHROMIUM(1);
    setupPathStateForTestPattern(path);

    const GLuint kPaths[]                             = {1, 1, 1, 1, 1};
    const GLsizei kPathCount                          = 5;
    const GLfloat kShapeSize                          = 80.0f;
    static const GLfloat kTransforms[kPathCount * 12] = {
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,           0.0f,       0.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, kShapeSize,     0.0f,       0.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, kShapeSize * 2, 0.0f,       0.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,           kShapeSize, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, kShapeSize,     kShapeSize, 0.0f};

    // The test pattern is the same as in the simple draw case above,
    // except that the path is drawn kPathCount times with different offsets.
    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0xFF);
    glStencilStrokePathInstancedCHROMIUM(kPathCount, GL_UNSIGNED_INT, kPaths, path - 1, 0x80, 0x80,
                                         GL_AFFINE_3D_CHROMIUM, kTransforms);

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0x7F);
    glUniform4fv(mColorLoc, 1, kBlue);
    glStencilFillPathInstancedCHROMIUM(kPathCount, GL_UNSIGNED_INT, kPaths, path - 1,
                                       GL_COUNT_UP_CHROMIUM, 0x7F, GL_AFFINE_3D_CHROMIUM,
                                       kTransforms);

    ASSERT_GL_NO_ERROR();

    glStencilFunc(GL_LESS, 0, 0x7F);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glCoverFillPathInstancedCHROMIUM(kPathCount, GL_UNSIGNED_INT, kPaths, path - 1,
                                     GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM,
                                     GL_AFFINE_3D_CHROMIUM, kTransforms);

    ASSERT_GL_NO_ERROR();

    glStencilFunc(GL_EQUAL, 0x80, 0x80);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kGreen);
    glCoverStrokePathInstancedCHROMIUM(kPathCount, GL_UNSIGNED_INT, kPaths, path - 1,
                                       GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM,
                                       GL_AFFINE_3D_CHROMIUM, kTransforms);

    ASSERT_GL_NO_ERROR();

    glDeletePathsCHROMIUM(path, 1);

    // Verify the image.
    verifyTestPatternFill(0.0f, 0.0f);
    verifyTestPatternBg(0.0f, 0.0f);
    verifyTestPatternStroke(0.0f, 0.0f);

    verifyTestPatternFill(kShapeSize, 0.0f);
    verifyTestPatternBg(kShapeSize, 0.0f);
    verifyTestPatternStroke(kShapeSize, 0.0f);

    verifyTestPatternFill(kShapeSize * 2, 0.0f);
    verifyTestPatternBg(kShapeSize * 2, 0.0f);
    verifyTestPatternStroke(kShapeSize * 2, 0.0f);

    verifyTestPatternFill(0.0f, kShapeSize);
    verifyTestPatternBg(0.0f, kShapeSize);
    verifyTestPatternStroke(0.0f, kShapeSize);

    verifyTestPatternFill(kShapeSize, kShapeSize);
    verifyTestPatternBg(kShapeSize, kShapeSize);
    verifyTestPatternStroke(kShapeSize, kShapeSize);
}

// Test that instanced fill/stroke then cover functions work.
TEST_P(CHROMIUMPathRenderingDrawTest, TestPathRenderingThenFunctionsInstanced)
{
    if (!isApplicable())
        return;

    static const float kBlue[]  = {0.0f, 0.0f, 1.0f, 1.0f};
    static const float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};

    setupStateForTestPattern();

    GLuint path = glGenPathsCHROMIUM(1);
    setupPathStateForTestPattern(path);

    const GLuint kPaths[]              = {1, 1, 1, 1, 1};
    const GLsizei kPathCount           = 5;
    const GLfloat kShapeSize           = 80.0f;
    static const GLfloat kTransforms[] = {
        0.0f, 0.0f, kShapeSize, 0.0f,       kShapeSize * 2,
        0.0f, 0.0f, kShapeSize, kShapeSize, kShapeSize,
    };

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0xFF);
    glStencilFunc(GL_EQUAL, 0x80, 0x80);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kGreen);
    glStencilThenCoverStrokePathInstancedCHROMIUM(
        kPathCount, GL_UNSIGNED_INT, kPaths, path - 1, 0x80, 0x80,
        GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM, GL_TRANSLATE_2D_CHROMIUM, kTransforms);

    ASSERT_GL_NO_ERROR();

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0x7F);
    glStencilFunc(GL_LESS, 0, 0x7F);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kBlue);
    glStencilThenCoverFillPathInstancedCHROMIUM(
        kPathCount, GL_UNSIGNED_INT, kPaths, path - 1, GL_COUNT_UP_CHROMIUM, 0x7F,
        GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM, GL_TRANSLATE_2D_CHROMIUM, kTransforms);

    ASSERT_GL_NO_ERROR();

    glDeletePathsCHROMIUM(path, 1);

    // Verify the image.
    verifyTestPatternFill(0.0f, 0.0f);
    verifyTestPatternBg(0.0f, 0.0f);
    verifyTestPatternStroke(0.0f, 0.0f);

    verifyTestPatternFill(kShapeSize, 0.0f);
    verifyTestPatternBg(kShapeSize, 0.0f);
    verifyTestPatternStroke(kShapeSize, 0.0f);

    verifyTestPatternFill(kShapeSize * 2, 0.0f);
    verifyTestPatternBg(kShapeSize * 2, 0.0f);
    verifyTestPatternStroke(kShapeSize * 2, 0.0f);

    verifyTestPatternFill(0.0f, kShapeSize);
    verifyTestPatternBg(0.0f, kShapeSize);
    verifyTestPatternStroke(0.0f, kShapeSize);

    verifyTestPatternFill(kShapeSize, kShapeSize);
    verifyTestPatternBg(kShapeSize, kShapeSize);
    verifyTestPatternStroke(kShapeSize, kShapeSize);
}

// This class implements a test that draws a grid of v-shapes. The grid is
// drawn so that even rows (from the bottom) are drawn with DrawArrays and odd
// rows are drawn with path rendering.  It can be used to test various texturing
// modes, comparing how the fill would work in normal GL rendering and how to
// setup same sort of fill with path rendering.
// The texturing test is parametrized to run the test with and without
// ANGLE name hashing.
class CHROMIUMPathRenderingWithTexturingTest : public ANGLETest
{
  protected:
    CHROMIUMPathRenderingWithTexturingTest() : mProgram(0)
    {
        setWindowWidth(kResolution);
        setWindowHeight(kResolution);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);
    }

    bool isApplicable() const { return IsGLExtensionEnabled("GL_CHROMIUM_path_rendering"); }

    void testTearDown() override
    {
        if (mProgram)
        {
            glDeleteProgram(mProgram);
            ASSERT_GL_NO_ERROR();
        }
    }

    // Sets up the GL program state for the test.
    // Vertex shader needs at least following variables:
    //  uniform mat4 view_matrix;
    //  uniform mat? color_matrix; (accessible with kColorMatrixLocation)
    //  uniform vec2 model_translate;
    //  attribute vec2 position;
    //  varying vec4 color;
    //
    // Fragment shader needs at least following variables:
    //  varying vec4 color;
    //
    //  (? can be anything)
    void CompileProgram(const char *vertexShaderSource, const char *fragmentShaderSource)
    {
        glViewport(0, 0, kResolution, kResolution);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glStencilMask(0xffffffff);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        ASSERT_GL_NO_ERROR();

        GLuint vShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
        GLuint fShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
        ASSERT_NE(0u, vShader);
        ASSERT_NE(0u, fShader);

        mProgram = glCreateProgram();

        glAttachShader(mProgram, vShader);
        glAttachShader(mProgram, fShader);
        glDeleteShader(vShader);
        glDeleteShader(fShader);

        ASSERT_GL_NO_ERROR();
    }

    void bindProgram()
    {
        glBindAttribLocation(mProgram, kPositionLocation, "position");
        glBindUniformLocationCHROMIUM(mProgram, kViewMatrixLocation, "view_matrix");
        glBindUniformLocationCHROMIUM(mProgram, kColorMatrixLocation, "color_matrix");
        glBindUniformLocationCHROMIUM(mProgram, kModelTranslateLocation, "model_translate");
        glBindFragmentInputLocationCHROMIUM(mProgram, kColorFragmentInputLocation, "color");

        ASSERT_GL_NO_ERROR();
    }

    bool linkProgram()
    {
        glLinkProgram(mProgram);

        GLint linked = 0;
        glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);
        if (linked)
        {
            glUseProgram(mProgram);
        }

        return (linked == 1);
    }

    void drawTestPattern()
    {
        // This v-shape is used both for DrawArrays and path rendering.
        static const GLfloat kVertices[] = {75.0f, 75.0f, 50.0f, 25.5f, 50.0f, 50.0f, 25.0f, 75.0f};

        GLuint vbo = 0;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(kPositionLocation);
        glVertexAttribPointer(kPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);

        // Setup state for drawing the shape with path rendering.
        glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0x7F);
        glStencilFunc(GL_LESS, 0, 0x7F);
        glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
        glMatrixLoadfCHROMIUM(GL_PATH_PROJECTION_CHROMIUM, kProjectionMatrix);
        glMatrixLoadIdentityCHROMIUM(GL_PATH_MODELVIEW_CHROMIUM);

        static const GLubyte kCommands[] = {GL_MOVE_TO_CHROMIUM, GL_LINE_TO_CHROMIUM,
                                            GL_LINE_TO_CHROMIUM, GL_LINE_TO_CHROMIUM,
                                            GL_CLOSE_PATH_CHROMIUM};

        static const GLfloat kCoords[] = {
            kVertices[0], kVertices[1], kVertices[2], kVertices[3],
            kVertices[6], kVertices[7], kVertices[4], kVertices[5],
        };

        GLuint path = glGenPathsCHROMIUM(1);
        glPathCommandsCHROMIUM(path, static_cast<GLsizei>(ArraySize(kCommands)), kCommands,
                               static_cast<GLsizei>(ArraySize(kCoords)), GL_FLOAT, kCoords);
        ASSERT_GL_NO_ERROR();

        GLfloat path_model_translate[16] = {
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        };

        // Draws the shapes. Every even row from the bottom is drawn with
        // DrawArrays, odd row with path rendering. The shader program is
        // the same for the both draws.
        for (int j = 0; j < kTestRows; ++j)
        {
            for (int i = 0; i < kTestColumns; ++i)
            {
                if (j % 2 == 0)
                {
                    glDisable(GL_STENCIL_TEST);
                    glUniform2f(kModelTranslateLocation, static_cast<GLfloat>(i * kShapeWidth),
                                static_cast<GLfloat>(j * kShapeHeight));
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
                else
                {
                    glEnable(GL_STENCIL_TEST);
                    path_model_translate[12] = static_cast<GLfloat>(i * kShapeWidth);
                    path_model_translate[13] = static_cast<GLfloat>(j * kShapeHeight);
                    glMatrixLoadfCHROMIUM(GL_PATH_MODELVIEW_CHROMIUM, path_model_translate);
                    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F,
                                                       GL_BOUNDING_BOX_CHROMIUM);
                }
            }
        }
        ASSERT_GL_NO_ERROR();

        glDisableVertexAttribArray(kPositionLocation);
        glDeleteBuffers(1, &vbo);
        glDeletePathsCHROMIUM(path, 1);
        ASSERT_GL_NO_ERROR();
    }

    enum
    {
        kShapeWidth  = 75,
        kShapeHeight = 75,
        kTestRows    = kResolution / kShapeHeight,
        kTestColumns = kResolution / kShapeWidth,
    };

    GLuint mProgram;

    // This uniform be can set by the test. It should be used to set the color for
    // drawing with DrawArrays.
    static const GLint kColorMatrixLocation = 4;

    // This fragment input can be set by the test. It should be used to set the
    // color for drawing with path rendering.
    static const GLint kColorFragmentInputLocation = 7;

    static const GLint kModelTranslateLocation = 3;
    static const GLint kPositionLocation       = 0;
    static const GLint kViewMatrixLocation     = 7;
};

// Test success and error cases for binding fragment input location.
TEST_P(CHROMIUMPathRenderingWithTexturingTest, TestBindFragmentInputLocation)
{
    if (!isApplicable())
        return;

    // original NV_path_rendering specification doesn't define whether the
    // fragment shader input variables should be defined in the vertex shader or
    // not. In fact it doesn't even require a vertex shader.
    // However the GLES3.1 spec basically says that fragment inputs are
    // either built-ins or come from the previous shader stage.
    // (§ 14.1, Fragment Shader Variables).
    // Additionally there are many places that are based on the assumption of having
    // a vertex shader (command buffer, angle) so we're going to stick to the same
    // semantics and require a vertex shader and to have the vertex shader define the
    // varying fragment shader input.

    // clang-format off
    const char* kVertexShaderSource =
       "varying vec4 color;\n"
       "void main() {}\n";

    const char* kFragmentShaderSource =
        "precision mediump float;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(1.0);\n"
        "}\n";

    // clang-format on
    CompileProgram(kVertexShaderSource, kFragmentShaderSource);

    enum kBindLocations
    {
        kColorLocation     = 5,
        kFragColorLocation = 6
    };

    // successful bind.
    glBindFragmentInputLocationCHROMIUM(mProgram, kColorLocation, "color");
    ASSERT_GL_NO_ERROR();

    // any name can be bound and names that do not actually exist in the program after
    // linking are ignored.
    glBindFragmentInputLocationCHROMIUM(mProgram, kColorLocation, "doesnt_exist");
    ASSERT_GL_NO_ERROR();

    // illegal program
    glBindFragmentInputLocationCHROMIUM(mProgram + 1, kColorLocation, "color");
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // illegal bind (built-in)
    glBindFragmentInputLocationCHROMIUM(mProgram, kFragColorLocation, "gl_FragColor");
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBindFragmentInputLocationCHROMIUM(mProgram, kFragColorLocation, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glBindFragmentInputLocationCHROMIUM(mProgram, 0xffffff, "color");
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    ASSERT_TRUE(linkProgram() == true);

    const GLfloat kCoefficients16[] = {1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
                                       9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};

    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorLocation, GL_EYE_LINEAR_CHROMIUM, 4,
                                          kCoefficients16);
    ASSERT_GL_NO_ERROR();

    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, GL_EYE_LINEAR_CHROMIUM, 4, kCoefficients16);
    ASSERT_GL_NO_ERROR();
}

// Test fragment input interpolation in CHROMIUM_EYE coordinates.
TEST_P(CHROMIUMPathRenderingWithTexturingTest, TestProgramPathFragmentInputGenCHROMIUM_EYE)
{
    if (!isApplicable())
        return;

    // clang-format off
    const char *kVertexShaderSource =
        "uniform mat4 view_matrix;\n"
        "uniform mat4 color_matrix;\n"
        "uniform vec2 model_translate;\n"
        "attribute vec2 position;\n"
        "varying vec3 color;\n"
        "void main() {\n"
        "  vec4 p = vec4(model_translate + position, 1.0, 1.0);\n"
        "  color = (color_matrix * p).rgb;\n"
        "  gl_Position = view_matrix * p;\n"
        "}\n";

    const char *kFragmentShaderSource =
        "precision mediump float;\n"
        "varying vec3 color;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(color, 1.0);\n"
        "}\n";
    // clang-format on

    CompileProgram(kVertexShaderSource, kFragmentShaderSource);
    bindProgram();
    ASSERT_TRUE(linkProgram() == true);

    glUniformMatrix4fv(kViewMatrixLocation, 1, GL_FALSE, kProjectionMatrix);

    static const GLfloat kColorMatrix[16] = {1.0f / kResolution,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             1.0f / kResolution,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f};

    glUniformMatrix4fv(kColorMatrixLocation, 1, GL_FALSE, kColorMatrix);

    // This is the functionality we are testing: ProgramPathFragmentInputGen
    // does the same work as the color transform in vertex shader.
    static const GLfloat kColorCoefficients[12] = {1.0f / kResolution,
                                                   0.0f,
                                                   0.0f,
                                                   0.0f,
                                                   0.0f,
                                                   1.0f / kResolution,
                                                   0.0f,
                                                   0.0f,
                                                   0.0f,
                                                   0.0f,
                                                   0.0f,
                                                   0.0f};
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorFragmentInputLocation,
                                          GL_EYE_LINEAR_CHROMIUM, 3, kColorCoefficients);
    ASSERT_GL_NO_ERROR();

    drawTestPattern();

    const GLfloat kFillCoords[6] = {59.0f, 50.0f, 50.0f, 28.0f, 66.0f, 63.0f};

    for (int j = 0; j < kTestRows; ++j)
    {
        for (int i = 0; i < kTestColumns; ++i)
        {
            for (size_t k = 0; k < ArraySize(kFillCoords); k += 2)
            {
                const float fx = kFillCoords[k];
                const float fy = kFillCoords[k + 1];
                const float px = static_cast<float>(i * kShapeWidth);
                const float py = static_cast<float>(j * kShapeHeight);

                angle::GLColor color;
                color.R = static_cast<GLubyte>(std::roundf((px + fx) / kResolution * 255.0f));
                color.G = static_cast<GLubyte>(std::roundf((py + fy) / kResolution * 255.0f));
                color.B = 0;
                color.A = 255;
                CheckPixels(static_cast<GLint>(px + fx), static_cast<GLint>(py + fy), 1, 1, 2,
                            color);
            }
        }
    }
}

// Test fragment input interpolation in CHROMIUM_OBJECT coordinates.
TEST_P(CHROMIUMPathRenderingWithTexturingTest, TestProgramPathFragmentInputGenCHROMIUM_OBJECT)
{
    if (!isApplicable())
        return;

    // clang-format off
    const char *kVertexShaderSource =
        "uniform mat4 view_matrix;\n"
        "uniform mat4 color_matrix;\n"
        "uniform vec2 model_translate;\n"
        "attribute vec2 position;\n"
        "varying vec3 color;\n"
        "void main() {\n"
        "  color = (color_matrix * vec4(position, 1.0, 1.0)).rgb;\n"
        "  vec4 p = vec4(model_translate + position, 1.0, 1.0);\n"
        "  gl_Position = view_matrix * p;\n"
        "}";

    const char *kFragmentShaderSource =
        "precision mediump float;\n"
        "varying vec3 color;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(color.rgb, 1.0);\n"
        "}";
    // clang-format on

    CompileProgram(kVertexShaderSource, kFragmentShaderSource);
    bindProgram();
    ASSERT_TRUE(linkProgram() == true);

    glUniformMatrix4fv(kViewMatrixLocation, 1, GL_FALSE, kProjectionMatrix);

    static const GLfloat kColorMatrix[16] = {1.0f / kShapeWidth,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             1.0f / kShapeHeight,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f,
                                             0.0f};
    glUniformMatrix4fv(kColorMatrixLocation, 1, GL_FALSE, kColorMatrix);

    // This is the functionality we are testing: ProgramPathFragmentInputGen
    // does the same work as the color transform in vertex shader.
    static const GLfloat kColorCoefficients[9] = {
        1.0f / kShapeWidth, 0.0f, 0.0f, 0.0f, 1.0f / kShapeHeight, 0.0f, 0.0f, 0.0f, 0.0f};
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorFragmentInputLocation,
                                          GL_OBJECT_LINEAR_CHROMIUM, 3, kColorCoefficients);

    ASSERT_GL_NO_ERROR();

    drawTestPattern();

    const GLfloat kFillCoords[6] = {59.0f, 50.0f, 50.0f, 28.0f, 66.0f, 63.0f};

    for (int j = 0; j < kTestRows; ++j)
    {
        for (int i = 0; i < kTestColumns; ++i)
        {
            for (size_t k = 0; k < ArraySize(kFillCoords); k += 2)
            {
                const float fx = kFillCoords[k];
                const float fy = kFillCoords[k + 1];
                const float px = static_cast<float>(i * kShapeWidth);
                const float py = static_cast<float>(j * kShapeHeight);

                angle::GLColor color;
                color.R = static_cast<GLubyte>(std::roundf(fx / kShapeWidth * 255.0f));
                color.G = static_cast<GLubyte>(std::roundf(fy / kShapeHeight * 255.0f));
                color.B = 0;
                color.A = 255;
                CheckPixels(static_cast<GLint>(px + fx), static_cast<GLint>(py + fy), 1, 1, 2,
                            color);
            }
        }
    }
}

// Test success and error cases for setting interpolation parameters.
TEST_P(CHROMIUMPathRenderingWithTexturingTest, TestProgramPathFragmentInputGenArgs)
{
    if (!isApplicable())
        return;

    // clang-format off
    const char *kVertexShaderSource =
        "varying vec2 vec2_var;\n"
        "varying vec3 vec3_var;\n"
        "varying vec4 vec4_var;\n"
        "varying float float_var;\n"
        "varying mat2 mat2_var;\n"
        "varying mat3 mat3_var;\n"
        "varying mat4 mat4_var;\n"
        "attribute float avoid_opt;\n"
        "void main() {\n"
        "  vec2_var = vec2(1.0, 2.0 + avoid_opt);\n"
        "  vec3_var = vec3(1.0, 2.0, 3.0 + avoid_opt);\n"
        "  vec4_var = vec4(1.0, 2.0, 3.0, 4.0 + avoid_opt);\n"
        "  float_var = 5.0 + avoid_opt;\n"
        "  mat2_var = mat2(2.0 + avoid_opt);\n"
        "  mat3_var = mat3(3.0 + avoid_opt);\n"
        "  mat4_var = mat4(4.0 + avoid_opt);\n"
        "  gl_Position = vec4(1.0);\n"
        "}";

    const char* kFragmentShaderSource =
        "precision mediump float;\n"
        "varying vec2 vec2_var;\n"
        "varying vec3 vec3_var;\n"
        "varying vec4 vec4_var;\n"
        "varying float float_var;\n"
        "varying mat2 mat2_var;\n"
        "varying mat3 mat3_var;\n"
        "varying mat4 mat4_var;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(vec2_var, 0, 0) + vec4(vec3_var, 0) + vec4_var + "
        "               vec4(float_var) + "
        "               vec4(mat2_var[0][0], mat3_var[1][1], mat4_var[2][2], 1);\n"
        "}";
    // clang-format on

    enum
    {
        kVec2Location = 0,
        kVec3Location,
        kVec4Location,
        kFloatLocation,
        kMat2Location,
        kMat3Location,
        kMat4Location,
    };
    struct
    {
        GLint location;
        const char *name;
        GLint components;
    } variables[] = {
        {kVec2Location, "vec2_var", 2},
        {kVec3Location, "vec3_var", 3},
        {kVec4Location, "vec4_var", 4},
        {kFloatLocation, "float_var", 1},
        // If a varying is not single-precision floating-point scalar or
        // vector, it always causes an invalid operation.
        {kMat2Location, "mat2_var", -1},
        {kMat3Location, "mat3_var", -1},
        {kMat4Location, "mat4_var", -1},
    };

    CompileProgram(kVertexShaderSource, kFragmentShaderSource);

    for (size_t i = 0; i < ArraySize(variables); ++i)
    {
        glBindFragmentInputLocationCHROMIUM(mProgram, variables[i].location, variables[i].name);
    }

    // test that using invalid (not linked) program is an invalid operation.
    // See similar calls at the end of the test for discussion about the arguments.
    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, GL_NONE, 0, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    ASSERT_TRUE(linkProgram() == true);

    const GLfloat kCoefficients16[] = {1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
                                       9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    const GLenum kGenModes[]        = {GL_NONE, GL_EYE_LINEAR_CHROMIUM, GL_OBJECT_LINEAR_CHROMIUM,
                                GL_CONSTANT_CHROMIUM};

    for (size_t variable = 0; variable < ArraySize(variables); ++variable)
    {
        for (GLint components = 0; components <= 4; ++components)
        {
            for (size_t genmode = 0; genmode < ArraySize(kGenModes); ++genmode)
            {
                glProgramPathFragmentInputGenCHROMIUM(mProgram, variables[variable].location,
                                                      kGenModes[genmode], components,
                                                      kCoefficients16);

                if (components == 0 && kGenModes[genmode] == GL_NONE)
                {
                    if (variables[variable].components == -1)
                    {
                        // Clearing a fragment input that is not single-precision floating
                        // point scalar or vector is an invalid operation.
                        ASSERT_GL_ERROR(GL_INVALID_OPERATION);
                    }
                    else
                    {
                        // Clearing a valid fragment input is ok.
                        ASSERT_GL_NO_ERROR();
                    }
                }
                else if (components == 0 || kGenModes[genmode] == GL_NONE)
                {
                    ASSERT_GL_ERROR(GL_INVALID_VALUE);
                }
                else
                {
                    if (components == variables[variable].components)
                    {
                        // Setting a generator for a single-precision floating point
                        // scalar or vector fragment input is ok.
                        ASSERT_GL_NO_ERROR();
                    }
                    else
                    {
                        // Setting a generator when components do not match is an invalid operation.
                        ASSERT_GL_ERROR(GL_INVALID_OPERATION);
                    }
                }
            }
        }
    }

    enum
    {
        kValidGenMode      = GL_CONSTANT_CHROMIUM,
        kValidComponents   = 3,
        kInvalidGenMode    = 0xAB,
        kInvalidComponents = 5,
    };

    // The location == -1 would mean fragment input was optimized away. At the
    // time of writing, -1 can not happen because the only way to obtain the
    // location numbers is through bind. Test just to be consistent.
    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, kValidGenMode, kValidComponents,
                                          kCoefficients16);
    ASSERT_GL_NO_ERROR();

    // Test that even though the spec says location == -1 causes the operation to
    // be skipped, the verification of other parameters is still done. This is a
    // GL policy.
    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, kInvalidGenMode, kValidComponents,
                                          kCoefficients16);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);

    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, kInvalidGenMode, kInvalidComponents,
                                          kCoefficients16);
    ASSERT_GL_ERROR(GL_INVALID_ENUM);

    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, kValidGenMode, kInvalidComponents,
                                          kCoefficients16);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    glDeleteProgram(mProgram);

    // Test that using invalid (deleted) program is an invalid operation.
    EXPECT_GL_TRUE(glIsProgram(mProgram));

    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, kValidGenMode, kValidComponents,
                                          kCoefficients16);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, kInvalidGenMode, kValidComponents,
                                          kCoefficients16);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, kInvalidGenMode, kInvalidComponents,
                                          kCoefficients16);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, kValidGenMode, kInvalidComponents,
                                          kCoefficients16);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    mProgram = 0u;
}

// Test that having input statically aliased fragment inputs the linking fails
// and then succeeds when the conflict is resolved.
TEST_P(CHROMIUMPathRenderingWithTexturingTest, TestConflictingBind)
{
    if (!isApplicable())
        return;

    // clang-format off
    const char* kVertexShaderSource =
        "attribute vec4 position;\n"
        "varying vec4 colorA;\n"
        "varying vec4 colorB;\n"
        "void main() {\n"
        "  gl_Position = position;\n"
        "  colorA = position + vec4(1);\n"
        "  colorB = position + vec4(2);\n"
        "}";

    const char* kFragmentShaderSource =
        "precision mediump float;\n"
        "varying vec4 colorA;\n"
        "varying vec4 colorB;\n"
        "void main() {\n"
        "  gl_FragColor = colorA + colorB;\n"
        "}";
    // clang-format on

    const GLint kColorALocation = 3;
    const GLint kColorBLocation = 4;

    CompileProgram(kVertexShaderSource, kFragmentShaderSource);

    glBindFragmentInputLocationCHROMIUM(mProgram, kColorALocation, "colorA");
    // Bind colorB to location a, causing conflicts. Linking should fail.
    glBindFragmentInputLocationCHROMIUM(mProgram, kColorALocation, "colorB");

    // Should fail now.
    ASSERT_TRUE(linkProgram() == false);
    ASSERT_GL_NO_ERROR();

    // Resolve the bind conflict.
    glBindFragmentInputLocationCHROMIUM(mProgram, kColorBLocation, "colorB");

    ASSERT_TRUE(linkProgram() == true);
    ASSERT_GL_NO_ERROR();
}

// Test binding with array variables, using zero indices. Tests that
// binding colorA[0] with explicit "colorA[0]" as well as "colorA" produces
// a correct location that can be used with PathProgramFragmentInputGen.
// For path rendering, colorA[0] is bound to a location. The input generator for
// the location is set to produce vec4(0, 0.1, 0, 0.1).
// The default varying, color, is bound to a location and its generator
// will produce vec4(10.0).  The shader program produces green pixels.
// For vertex-based rendering, the vertex shader produces the same effect as
// the input generator for path rendering.
TEST_P(CHROMIUMPathRenderingWithTexturingTest, BindFragmentInputArray)
{
    if (!isApplicable())
        return;

    //clang-format off
    const char *kVertexShaderSource =
        "uniform mat4 view_matrix;\n"
        "uniform mat4 color_matrix;\n"
        "uniform vec2 model_translate;\n"
        "attribute vec2 position;\n"
        "varying vec4 color;\n"
        "varying vec4 colorA[4];\n"
        "void main() {\n"
        "  vec4 p = vec4(model_translate + position, 1, 1);\n"
        "  gl_Position = view_matrix * p;\n"
        "  colorA[0] = vec4(0.0, 0.1, 0, 0.1);\n"
        "  colorA[1] = vec4(0.2);\n"
        "  colorA[2] = vec4(0.3);\n"
        "  colorA[3] = vec4(0.4);\n"
        "  color = vec4(10.0);\n"
        "}";

    const char *kFragmentShaderSource =
        "precision mediump float;\n"
        "varying vec4 color;\n"
        "varying vec4 colorA[4];\n"
        "void main() {\n"
        "  gl_FragColor = colorA[0] * color;\n"
        "}";
    // clang-format on

    const GLint kColorA0Location = 4;
    const GLint kUnusedLocation  = 5;
    const GLfloat kColorA0[]     = {0.0f, 0.1f, 0.0f, 0.1f};
    const GLfloat kColor[]       = {10.0f, 10.0f, 10.0f, 10.0f};
    const GLfloat kFillCoords[6] = {59.0f, 50.0f, 50.0f, 28.0f, 66.0f, 63.0f};

    for (int pass = 0; pass < 2; ++pass)
    {
        CompileProgram(kVertexShaderSource, kFragmentShaderSource);
        if (pass == 0)
        {
            glBindFragmentInputLocationCHROMIUM(mProgram, kUnusedLocation, "colorA[0]");
            glBindFragmentInputLocationCHROMIUM(mProgram, kColorA0Location, "colorA");
        }
        else
        {
            glBindFragmentInputLocationCHROMIUM(mProgram, kUnusedLocation, "colorA");
            glBindFragmentInputLocationCHROMIUM(mProgram, kColorA0Location, "colorA[0]");
        }

        bindProgram();

        ASSERT_TRUE(linkProgram() == true);

        glUniformMatrix4fv(kViewMatrixLocation, 1, GL_FALSE, kProjectionMatrix);
        glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorA0Location, GL_CONSTANT_CHROMIUM, 4,
                                              kColorA0);
        glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorFragmentInputLocation,
                                              GL_CONSTANT_CHROMIUM, 4, kColor);
        ASSERT_GL_NO_ERROR();

        drawTestPattern();

        for (int j = 0; j < kTestRows; ++j)
        {
            for (int i = 0; i < kTestColumns; ++i)
            {
                for (size_t k = 0; k < ArraySize(kFillCoords); k += 2)
                {
                    const float fx = kFillCoords[k];
                    const float fy = kFillCoords[k + 1];
                    const float px = static_cast<float>(i * kShapeWidth);
                    const float py = static_cast<float>(j * kShapeHeight);

                    angle::GLColor color;
                    color.R = 0;
                    color.G = 255;
                    color.B = 0;
                    color.A = 255;
                    CheckPixels(static_cast<GLint>(px + fx), static_cast<GLint>(py + fy), 1, 1, 2,
                                color);
                }
            }
        }
    }
}

// Test binding array variables. This is like BindFragmentInputArray.
// Currently disabled since it seems there's a driver bug with the
// older drivers. This should work with driver >= 364.12
TEST_P(CHROMIUMPathRenderingWithTexturingTest, DISABLED_BindFragmentInputArrayNonZeroIndex)
{
    if (!isApplicable())
        return;

    // clang-format off
    const char* kVertexShaderSource =
        "uniform mat4 view_matrix;\n"
        "uniform mat4 color_matrix;\n"
        "uniform vec2 model_translate;\n"
        "attribute vec2 position;\n"
        "varying vec4 color;\n"
        "varying vec4 colorA[4];\n"
        "void main() {\n"
        "  vec4 p = vec4(model_translate + position, 1, 1);\n"
        "  gl_Position = view_matrix * p;\n"
        "  colorA[0] = vec4(0, 0.1, 0, 0.1);\n"
        "  colorA[1] = vec4(0, 1, 0, 1);\n"
        "  colorA[2] = vec4(0, 0.8, 0, 0.8);\n"
        "  colorA[3] = vec4(0, 0.5, 0, 0.5);\n"
        "  color = vec4(0.2);\n"
        "}\n";

    const char* kFragmentShaderSource =
        "precision mediump float;\n"
        "varying vec4 colorA[4];\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  gl_FragColor = (colorA[0] * colorA[1]) +\n"
        "      colorA[2] + (colorA[3] * color);\n"
        "}\n";
    // clang-format on

    const GLint kColorA0Location = 4;
    const GLint kColorA1Location = 1;
    const GLint kColorA2Location = 2;
    const GLint kColorA3Location = 3;
    const GLint kUnusedLocation  = 5;
    const GLfloat kColorA0[]     = {0.0f, 0.1f, 0.0f, 0.1f};
    const GLfloat kColorA1[]     = {0.0f, 1.0f, 0.0f, 1.0f};
    const GLfloat kColorA2[]     = {0.0f, 0.8f, 0.0f, 0.8f};
    const GLfloat kColorA3[]     = {0.0f, 0.5f, 0.0f, 0.5f};
    const GLfloat kColor[]       = {0.2f, 0.2f, 0.2f, 0.2f};
    const GLfloat kFillCoords[6] = {59.0f, 50.0f, 50.0f, 28.0f, 66.0f, 63.0f};

    CompileProgram(kVertexShaderSource, kFragmentShaderSource);

    glBindFragmentInputLocationCHROMIUM(mProgram, kUnusedLocation, "colorA[0]");
    glBindFragmentInputLocationCHROMIUM(mProgram, kColorA1Location, "colorA[1]");
    glBindFragmentInputLocationCHROMIUM(mProgram, kColorA2Location, "colorA[2]");
    glBindFragmentInputLocationCHROMIUM(mProgram, kColorA3Location, "colorA[3]");
    glBindFragmentInputLocationCHROMIUM(mProgram, kColorA0Location, "colorA");
    ASSERT_GL_NO_ERROR();

    bindProgram();
    ASSERT_TRUE(linkProgram() == true);

    glUniformMatrix4fv(kViewMatrixLocation, 1, GL_FALSE, kProjectionMatrix);

    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorA0Location, GL_CONSTANT_CHROMIUM, 4,
                                          kColorA0);
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorA1Location, GL_CONSTANT_CHROMIUM, 4,
                                          kColorA1);
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorA2Location, GL_CONSTANT_CHROMIUM, 4,
                                          kColorA2);
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorA3Location, GL_CONSTANT_CHROMIUM, 4,
                                          kColorA3);
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorFragmentInputLocation,
                                          GL_CONSTANT_CHROMIUM, 4, kColor);
    ASSERT_GL_NO_ERROR();

    drawTestPattern();

    for (int j = 0; j < kTestRows; ++j)
    {
        for (int i = 0; i < kTestColumns; ++i)
        {
            for (size_t k = 0; k < ArraySize(kFillCoords); k += 2)
            {
                const float fx = kFillCoords[k];
                const float fy = kFillCoords[k + 1];
                const float px = static_cast<float>(i * kShapeWidth);
                const float py = static_cast<float>(j * kShapeHeight);

                angle::GLColor color;
                color.R = 0;
                color.G = 255;
                color.B = 0;
                color.A = 255;
                CheckPixels(static_cast<GLint>(px + fx), static_cast<GLint>(py + fy), 1, 1, 2,
                            color);
            }
        }
    }
}

TEST_P(CHROMIUMPathRenderingWithTexturingTest, UnusedFragmentInputUpdate)
{
    if (!isApplicable())
        return;

    // clang-format off
    const char* kVertexShaderString =
        "attribute vec4 a_position;\n"
        "void main() {\n"
        "  gl_Position = a_position;\n"
        "}";

    const char* kFragmentShaderString =
        "precision mediump float;\n"
        "uniform vec4 u_colorA;\n"
        "uniform float u_colorU;\n"
        "uniform vec4 u_colorC;\n"
        "void main() {\n"
        "  gl_FragColor = u_colorA + u_colorC;\n"
        "}";
    // clang-format on

    const GLint kColorULocation      = 1;
    const GLint kNonexistingLocation = 5;
    const GLint kUnboundLocation     = 6;

    CompileProgram(kVertexShaderString, kFragmentShaderString);

    glBindFragmentInputLocationCHROMIUM(mProgram, kColorULocation, "u_colorU");

    // The non-existing input should behave like existing but optimized away input.
    glBindFragmentInputLocationCHROMIUM(mProgram, kNonexistingLocation, "nonexisting");

    // Let A and C be assigned automatic locations.
    ASSERT_TRUE(linkProgram() == true);

    const GLfloat kColor[16] = {};

    // No errors on bound locations, since caller does not know
    // if the driver optimizes them away or not.
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorULocation, GL_CONSTANT_CHROMIUM, 1,
                                          kColor);
    ASSERT_GL_NO_ERROR();

    // No errors on bound locations of names that do not exist
    // in the shader. Otherwise it would be inconsistent wrt the
    // optimization case.
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kNonexistingLocation, GL_CONSTANT_CHROMIUM, 1,
                                          kColor);
    ASSERT_GL_NO_ERROR();

    // The above are equal to updating -1.
    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, GL_CONSTANT_CHROMIUM, 1, kColor);
    ASSERT_GL_NO_ERROR();

    // No errors when updating with other type either.
    // The type can not be known with the non-existing case.
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kColorULocation, GL_CONSTANT_CHROMIUM, 4,
                                          kColor);
    ASSERT_GL_NO_ERROR();

    glProgramPathFragmentInputGenCHROMIUM(mProgram, kNonexistingLocation, GL_CONSTANT_CHROMIUM, 4,
                                          kColor);
    ASSERT_GL_NO_ERROR();

    glProgramPathFragmentInputGenCHROMIUM(mProgram, -1, GL_CONSTANT_CHROMIUM, 4, kColor);
    ASSERT_GL_NO_ERROR();

    // Updating an unbound, non-existing location still causes an error.
    glProgramPathFragmentInputGenCHROMIUM(mProgram, kUnboundLocation, GL_CONSTANT_CHROMIUM, 4,
                                          kColor);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

}  // namespace

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(CHROMIUMPathRenderingTest);
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(CHROMIUMPathRenderingDrawTest);
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(CHROMIUMPathRenderingWithTexturingTest);

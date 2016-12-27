//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RobustClientMemoryTest.cpp : Tests of the GL_ANGLE_robust_client_memory extension.

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

namespace angle
{
class RobustClientMemoryTest : public ANGLETest
{
  protected:
    RobustClientMemoryTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGetBooleanvRobustANGLE = reinterpret_cast<PFNGLGETBOOLEANVROBUSTANGLE>(
            eglGetProcAddress("glGetBooleanvRobustANGLE"));
        glGetBufferParameterivRobustANGLE = reinterpret_cast<PFNGLGETBUFFERPARAMETERIVROBUSTANGLE>(
            eglGetProcAddress("glGetBufferParameterivRobustANGLE"));
        glGetFloatvRobustANGLE = reinterpret_cast<PFNGLGETFLOATVROBUSTANGLE>(
            eglGetProcAddress("glGetFloatvRobustANGLE"));
        glGetFramebufferAttachmentParameterivRobustANGLE =
            reinterpret_cast<PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVROBUSTANGLE>(
                eglGetProcAddress("glGetFramebufferAttachmentParameterivRobustANGLE"));
        glGetIntegervRobustANGLE = reinterpret_cast<PFNGLGETINTEGERVROBUSTANGLE>(
            eglGetProcAddress("glGetIntegervRobustANGLE"));
        glGetProgramivRobustANGLE = reinterpret_cast<PFNGLGETPROGRAMIVROBUSTANGLE>(
            eglGetProcAddress("glGetProgramivRobustANGLE"));
        glGetRenderbufferParameterivRobustANGLE =
            reinterpret_cast<PFNGLGETRENDERBUFFERPARAMETERIVROBUSTANGLE>(
                eglGetProcAddress("glGetRenderbufferParameterivRobustANGLE"));
        glGetShaderivRobustANGLE = reinterpret_cast<PFNGLGETSHADERIVROBUSTANGLE>(
            eglGetProcAddress("glGetShaderivRobustANGLE"));
        glGetTexParameterfvRobustANGLE = reinterpret_cast<PFNGLGETTEXPARAMETERFVROBUSTANGLE>(
            eglGetProcAddress("glGetTexParameterfvRobustANGLE"));
        glGetTexParameterivRobustANGLE = reinterpret_cast<PFNGLGETTEXPARAMETERIVROBUSTANGLE>(
            eglGetProcAddress("glGetTexParameterivRobustANGLE"));
        glGetUniformfvRobustANGLE = reinterpret_cast<PFNGLGETUNIFORMFVROBUSTANGLE>(
            eglGetProcAddress("glGetUniformfvRobustANGLE"));
        glGetUniformivRobustANGLE = reinterpret_cast<PFNGLGETUNIFORMIVROBUSTANGLE>(
            eglGetProcAddress("glGetUniformivRobustANGLE"));
        glGetVertexAttribfvRobustANGLE = reinterpret_cast<PFNGLGETVERTEXATTRIBFVROBUSTANGLE>(
            eglGetProcAddress("glGetVertexAttribfvRobustANGLE"));
        glGetVertexAttribivRobustANGLE = reinterpret_cast<PFNGLGETVERTEXATTRIBIVROBUSTANGLE>(
            eglGetProcAddress("glGetVertexAttribivRobustANGLE"));
        glGetVertexAttribPointervRobustANGLE =
            reinterpret_cast<PFNGLGETVERTEXATTRIBPOINTERVROBUSTANGLE>(
                eglGetProcAddress("glGetVertexAttribPointervRobustANGLE"));
        glReadPixelsRobustANGLE = reinterpret_cast<PFNGLREADPIXELSROBUSTANGLE>(
            eglGetProcAddress("glReadPixelsRobustANGLE"));
        glTexImage2DRobustANGLE = reinterpret_cast<PFNGLTEXIMAGE2DROBUSTANGLE>(
            eglGetProcAddress("glTexImage2DRobustANGLE"));
        glTexParameterfvRobustANGLE = reinterpret_cast<PFNGLTEXPARAMETERFVROBUSTANGLE>(
            eglGetProcAddress("glTexParameterfvRobustANGLE"));
        glTexParameterivRobustANGLE = reinterpret_cast<PFNGLTEXPARAMETERIVROBUSTANGLE>(
            eglGetProcAddress("glTexParameterivRobustANGLE"));
        glTexSubImage2DRobustANGLE = reinterpret_cast<PFNGLTEXSUBIMAGE2DROBUSTANGLE>(
            eglGetProcAddress("glTexSubImage2DRobustANGLE"));
        glTexImage3DRobustANGLE = reinterpret_cast<PFNGLTEXIMAGE3DROBUSTANGLE>(
            eglGetProcAddress("glTexImage3DRobustANGLE"));
        glTexSubImage3DRobustANGLE = reinterpret_cast<PFNGLTEXSUBIMAGE3DROBUSTANGLE>(
            eglGetProcAddress("glTexSubImage3DRobustANGLE"));
        glGetQueryivRobustANGLE = reinterpret_cast<PFNGLGETQUERYIVROBUSTANGLE>(
            eglGetProcAddress("glGetQueryivRobustANGLE"));
        glGetQueryObjectuivRobustANGLE = reinterpret_cast<PFNGLGETQUERYOBJECTUIVROBUSTANGLE>(
            eglGetProcAddress("glGetQueryObjectuivRobustANGLE"));
        glGetBufferPointervRobustANGLE = reinterpret_cast<PFNGLGETBUFFERPOINTERVROBUSTANGLE>(
            eglGetProcAddress("glGetBufferPointervRobustANGLE"));
        glGetIntegeri_vRobustANGLE = reinterpret_cast<PFNGLGETINTEGERI_VROBUSTANGLE>(
            eglGetProcAddress("glGetIntegeri_vRobustANGLE"));
        glGetInternalformativRobustANGLE = reinterpret_cast<PFNGETINTERNALFORMATIVROBUSTANGLE>(
            eglGetProcAddress("glGetInternalformativRobustANGLE"));
        glGetVertexAttribIivRobustANGLE = reinterpret_cast<PFNGLGETVERTEXATTRIBIIVROBUSTANGLE>(
            eglGetProcAddress("glGetVertexAttribIivRobustANGLE"));
        glGetVertexAttribIuivRobustANGLE = reinterpret_cast<PFNGLGETVERTEXATTRIBIUIVROBUSTANGLE>(
            eglGetProcAddress("glGetVertexAttribIuivRobustANGLE"));
        glGetUniformuivRobustANGLE = reinterpret_cast<PFNGLGETUNIFORMUIVROBUSTANGLE>(
            eglGetProcAddress("glGetUniformuivRobustANGLE"));
        glGetActiveUniformBlockivRobustANGLE =
            reinterpret_cast<PFNGLGETACTIVEUNIFORMBLOCKIVROBUSTANGLE>(
                eglGetProcAddress("glGetActiveUniformBlockivRobustANGLE"));
        glGetInteger64vRobustANGLE = reinterpret_cast<PFNGLGETINTEGER64VROBUSTANGLE>(
            eglGetProcAddress("glGetInteger64vRobustANGLE"));
        glGetInteger64i_vRobustANGLE = reinterpret_cast<PFNGLGETINTEGER64I_VROBUSTANGLE>(
            eglGetProcAddress("glGetInteger64i_vRobustANGLE"));
        glGetBufferParameteri64vRobustANGLE =
            reinterpret_cast<PFNGLGETBUFFERPARAMETERI64VROBUSTANGLE>(
                eglGetProcAddress("glGetBufferParameteri64vRobustANGLE"));
        glSamplerParameterivRobustANGLE = reinterpret_cast<PFNGLSAMPLERPARAMETERIVROBUSTANGLE>(
            eglGetProcAddress("glSamplerParameterivRobustANGLE"));
        glSamplerParameterfvRobustANGLE = reinterpret_cast<PFNGLSAMPLERPARAMETERFVROBUSTANGLE>(
            eglGetProcAddress("glSamplerParameterfvRobustANGLE"));
        glGetSamplerParameterivRobustANGLE =
            reinterpret_cast<PFNGLGETSAMPLERPARAMETERIVROBUSTANGLE>(
                eglGetProcAddress("glGetSamplerParameterivRobustANGLE"));
        glGetSamplerParameterfvRobustANGLE =
            reinterpret_cast<PFNGLGETSAMPLERPARAMETERFVROBUSTANGLE>(
                eglGetProcAddress("glGetSamplerParameterfvRobustANGLE"));
        glGetFramebufferParameterivRobustANGLE =
            reinterpret_cast<PFNGLGETFRAMEBUFFERPARAMETERIVROBUSTANGLE>(
                eglGetProcAddress("glGetFramebufferParameterivRobustANGLE"));
        glGetProgramInterfaceivRobustANGLE =
            reinterpret_cast<PFNGLGETPROGRAMINTERFACEIVROBUSTANGLE>(
                eglGetProcAddress("glGetProgramInterfaceivRobustANGLE"));
        glGetBooleani_vRobustANGLE = reinterpret_cast<PFNGLGETBOOLEANI_VROBUSTANGLE>(
            eglGetProcAddress("glGetBooleani_vRobustANGLE"));
        glGetMultisamplefvRobustANGLE = reinterpret_cast<PFNGLGETMULTISAMPLEFVROBUSTANGLE>(
            eglGetProcAddress("glGetMultisamplefvRobustANGLE"));
        glGetTexLevelParameterivRobustANGLE =
            reinterpret_cast<PFNGLGETTEXLEVELPARAMETERIVROBUSTANGLE>(
                eglGetProcAddress("glGetTexLevelParameterivRobustANGLE"));
        glGetTexLevelParameterfvRobustANGLE =
            reinterpret_cast<PFNGLGETTEXLEVELPARAMETERFVROBUSTANGLE>(
                eglGetProcAddress("glGetTexLevelParameterfvRobustANGLE"));
        glGetPointervRobustANGLERobustANGLE =
            reinterpret_cast<PFNGLGETPOINTERVROBUSTANGLEROBUSTANGLE>(
                eglGetProcAddress("glGetPointervRobustANGLERobustANGLE"));
        glReadnPixelsRobustANGLE = reinterpret_cast<PFNGLREADNPIXELSROBUSTANGLE>(
            eglGetProcAddress("glReadnPixelsRobustANGLE"));
        glGetnUniformfvRobustANGLE = reinterpret_cast<PFNGLGETNUNIFORMFVROBUSTANGLE>(
            eglGetProcAddress("glGetnUniformfvRobustANGLE"));
        glGetnUniformivRobustANGLE = reinterpret_cast<PFNGLGETNUNIFORMIVROBUSTANGLE>(
            eglGetProcAddress("glGetnUniformivRobustANGLE"));
        glGetnUniformuivRobustANGLE = reinterpret_cast<PFNGLGETNUNIFORMUIVROBUSTANGLE>(
            eglGetProcAddress("glGetnUniformuivRobustANGLE"));
        glTexParameterIivRobustANGLE = reinterpret_cast<PFNGLTEXPARAMETERIIVROBUSTANGLE>(
            eglGetProcAddress("glTexParameterIivRobustANGLE"));
        glTexParameterIuivRobustANGLE = reinterpret_cast<PFNGLTEXPARAMETERIUIVROBUSTANGLE>(
            eglGetProcAddress("glTexParameterIuivRobustANGLE"));
        glGetTexParameterIivRobustANGLE = reinterpret_cast<PFNGLGETTEXPARAMETERIIVROBUSTANGLE>(
            eglGetProcAddress("glGetTexParameterIivRobustANGLE"));
        glGetTexParameterIuivRobustANGLE = reinterpret_cast<PFNGLGETTEXPARAMETERIUIVROBUSTANGLE>(
            eglGetProcAddress("glGetTexParameterIuivRobustANGLE"));
        glSamplerParameterIivRobustANGLE = reinterpret_cast<PFNGLSAMPLERPARAMETERIIVROBUSTANGLE>(
            eglGetProcAddress("glSamplerParameterIivRobustANGLE"));
        glSamplerParameterIuivRobustANGLE = reinterpret_cast<PFNGLSAMPLERPARAMETERIUIVROBUSTANGLE>(
            eglGetProcAddress("glSamplerParameterIuivRobustANGLE"));
        glGetSamplerParameterIivRobustANGLE =
            reinterpret_cast<PFNGLGETSAMPLERPARAMETERIIVROBUSTANGLE>(
                eglGetProcAddress("glGetSamplerParameterIivRobustANGLE"));
        glGetSamplerParameterIuivRobustANGLE =
            reinterpret_cast<PFNGLGETSAMPLERPARAMETERIUIVROBUSTANGLE>(
                eglGetProcAddress("glGetSamplerParameterIuivRobustANGLE"));
        glGetQueryObjectivRobustANGLE = reinterpret_cast<PFNGLGETQUERYOBJECTIVROBUSTANGLE>(
            eglGetProcAddress("glGetQueryObjectivRobustANGLE"));
        glGetQueryObjecti64vRobustANGLE = reinterpret_cast<PFNGLGETQUERYOBJECTI64VROBUSTANGLE>(
            eglGetProcAddress("glGetQueryObjecti64vRobustANGLE"));
        glGetQueryObjectui64vRobustANGLE = reinterpret_cast<PFNGLGETQUERYOBJECTUI64VROBUSTANGLE>(
            eglGetProcAddress("glGetQueryObjectui64vRobustANGLE"));
    }

    void TearDown() override { ANGLETest::TearDown(); }

    bool extensionsPresent() const
    {
        if (!extensionEnabled("GL_ANGLE_robust_client_memory"))
        {
            std::cout << "Test skipped because GL_ANGLE_robust_client_memory is not available.";
            return false;
        }

        return true;
    }

    PFNGLGETBOOLEANVROBUSTANGLE glGetBooleanvRobustANGLE                   = nullptr;
    PFNGLGETBUFFERPARAMETERIVROBUSTANGLE glGetBufferParameterivRobustANGLE = nullptr;
    PFNGLGETFLOATVROBUSTANGLE glGetFloatvRobustANGLE                       = nullptr;
    PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVROBUSTANGLE
    glGetFramebufferAttachmentParameterivRobustANGLE                                   = nullptr;
    PFNGLGETINTEGERVROBUSTANGLE glGetIntegervRobustANGLE                               = nullptr;
    PFNGLGETPROGRAMIVROBUSTANGLE glGetProgramivRobustANGLE                             = nullptr;
    PFNGLGETRENDERBUFFERPARAMETERIVROBUSTANGLE glGetRenderbufferParameterivRobustANGLE = nullptr;
    PFNGLGETSHADERIVROBUSTANGLE glGetShaderivRobustANGLE                               = nullptr;
    PFNGLGETTEXPARAMETERFVROBUSTANGLE glGetTexParameterfvRobustANGLE                   = nullptr;
    PFNGLGETTEXPARAMETERIVROBUSTANGLE glGetTexParameterivRobustANGLE                   = nullptr;
    PFNGLGETUNIFORMFVROBUSTANGLE glGetUniformfvRobustANGLE                             = nullptr;
    PFNGLGETUNIFORMIVROBUSTANGLE glGetUniformivRobustANGLE                             = nullptr;
    PFNGLGETVERTEXATTRIBFVROBUSTANGLE glGetVertexAttribfvRobustANGLE                   = nullptr;
    PFNGLGETVERTEXATTRIBIVROBUSTANGLE glGetVertexAttribivRobustANGLE                   = nullptr;
    PFNGLGETVERTEXATTRIBPOINTERVROBUSTANGLE glGetVertexAttribPointervRobustANGLE       = nullptr;
    PFNGLREADPIXELSROBUSTANGLE glReadPixelsRobustANGLE                                 = nullptr;
    PFNGLTEXIMAGE2DROBUSTANGLE glTexImage2DRobustANGLE                                 = nullptr;
    PFNGLTEXPARAMETERFVROBUSTANGLE glTexParameterfvRobustANGLE                         = nullptr;
    PFNGLTEXPARAMETERIVROBUSTANGLE glTexParameterivRobustANGLE                         = nullptr;
    PFNGLTEXSUBIMAGE2DROBUSTANGLE glTexSubImage2DRobustANGLE                           = nullptr;
    PFNGLTEXIMAGE3DROBUSTANGLE glTexImage3DRobustANGLE                                 = nullptr;
    PFNGLTEXSUBIMAGE3DROBUSTANGLE glTexSubImage3DRobustANGLE                           = nullptr;
    PFNGLGETQUERYIVROBUSTANGLE glGetQueryivRobustANGLE                                 = nullptr;
    PFNGLGETQUERYOBJECTUIVROBUSTANGLE glGetQueryObjectuivRobustANGLE                   = nullptr;
    PFNGLGETBUFFERPOINTERVROBUSTANGLE glGetBufferPointervRobustANGLE                   = nullptr;
    PFNGLGETINTEGERI_VROBUSTANGLE glGetIntegeri_vRobustANGLE                           = nullptr;
    PFNGETINTERNALFORMATIVROBUSTANGLE glGetInternalformativRobustANGLE                 = nullptr;
    PFNGLGETVERTEXATTRIBIIVROBUSTANGLE glGetVertexAttribIivRobustANGLE                 = nullptr;
    PFNGLGETVERTEXATTRIBIUIVROBUSTANGLE glGetVertexAttribIuivRobustANGLE               = nullptr;
    PFNGLGETUNIFORMUIVROBUSTANGLE glGetUniformuivRobustANGLE                           = nullptr;
    PFNGLGETACTIVEUNIFORMBLOCKIVROBUSTANGLE glGetActiveUniformBlockivRobustANGLE       = nullptr;
    PFNGLGETINTEGER64VROBUSTANGLE glGetInteger64vRobustANGLE                           = nullptr;
    PFNGLGETINTEGER64I_VROBUSTANGLE glGetInteger64i_vRobustANGLE                       = nullptr;
    PFNGLGETBUFFERPARAMETERI64VROBUSTANGLE glGetBufferParameteri64vRobustANGLE         = nullptr;
    PFNGLSAMPLERPARAMETERIVROBUSTANGLE glSamplerParameterivRobustANGLE                 = nullptr;
    PFNGLSAMPLERPARAMETERFVROBUSTANGLE glSamplerParameterfvRobustANGLE                 = nullptr;
    PFNGLGETSAMPLERPARAMETERIVROBUSTANGLE glGetSamplerParameterivRobustANGLE           = nullptr;
    PFNGLGETSAMPLERPARAMETERFVROBUSTANGLE glGetSamplerParameterfvRobustANGLE           = nullptr;
    PFNGLGETFRAMEBUFFERPARAMETERIVROBUSTANGLE glGetFramebufferParameterivRobustANGLE   = nullptr;
    PFNGLGETPROGRAMINTERFACEIVROBUSTANGLE glGetProgramInterfaceivRobustANGLE           = nullptr;
    PFNGLGETBOOLEANI_VROBUSTANGLE glGetBooleani_vRobustANGLE                           = nullptr;
    PFNGLGETMULTISAMPLEFVROBUSTANGLE glGetMultisamplefvRobustANGLE                     = nullptr;
    PFNGLGETTEXLEVELPARAMETERIVROBUSTANGLE glGetTexLevelParameterivRobustANGLE         = nullptr;
    PFNGLGETTEXLEVELPARAMETERFVROBUSTANGLE glGetTexLevelParameterfvRobustANGLE         = nullptr;
    PFNGLGETPOINTERVROBUSTANGLEROBUSTANGLE glGetPointervRobustANGLERobustANGLE         = nullptr;
    PFNGLREADNPIXELSROBUSTANGLE glReadnPixelsRobustANGLE                               = nullptr;
    PFNGLGETNUNIFORMFVROBUSTANGLE glGetnUniformfvRobustANGLE                           = nullptr;
    PFNGLGETNUNIFORMIVROBUSTANGLE glGetnUniformivRobustANGLE                           = nullptr;
    PFNGLGETNUNIFORMUIVROBUSTANGLE glGetnUniformuivRobustANGLE                         = nullptr;
    PFNGLTEXPARAMETERIIVROBUSTANGLE glTexParameterIivRobustANGLE                       = nullptr;
    PFNGLTEXPARAMETERIUIVROBUSTANGLE glTexParameterIuivRobustANGLE                     = nullptr;
    PFNGLGETTEXPARAMETERIIVROBUSTANGLE glGetTexParameterIivRobustANGLE                 = nullptr;
    PFNGLGETTEXPARAMETERIUIVROBUSTANGLE glGetTexParameterIuivRobustANGLE               = nullptr;
    PFNGLSAMPLERPARAMETERIIVROBUSTANGLE glSamplerParameterIivRobustANGLE               = nullptr;
    PFNGLSAMPLERPARAMETERIUIVROBUSTANGLE glSamplerParameterIuivRobustANGLE             = nullptr;
    PFNGLGETSAMPLERPARAMETERIIVROBUSTANGLE glGetSamplerParameterIivRobustANGLE         = nullptr;
    PFNGLGETSAMPLERPARAMETERIUIVROBUSTANGLE glGetSamplerParameterIuivRobustANGLE       = nullptr;
    PFNGLGETQUERYOBJECTIVROBUSTANGLE glGetQueryObjectivRobustANGLE                     = nullptr;
    PFNGLGETQUERYOBJECTI64VROBUSTANGLE glGetQueryObjecti64vRobustANGLE                 = nullptr;
    PFNGLGETQUERYOBJECTUI64VROBUSTANGLE glGetQueryObjectui64vRobustANGLE               = nullptr;
};

// Test basic usage and validation of glGetIntegervRobustANGLE
TEST_P(RobustClientMemoryTest, GetInteger)
{
    if (!extensionsPresent())
    {
        return;
    }

    // Verify that the robust and regular entry points return the same values
    GLint resultRobust;
    GLsizei length;
    glGetIntegervRobustANGLE(GL_MAX_VERTEX_ATTRIBS, 1, &length, &resultRobust);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, length);

    GLint resultRegular;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &resultRegular);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(resultRegular, resultRobust);

    // Query a dynamic value
    GLint numCompressedFormats;
    glGetIntegervRobustANGLE(GL_NUM_COMPRESSED_TEXTURE_FORMATS, 1, &length, &numCompressedFormats);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, length);

    if (numCompressedFormats > 0)
    {
        std::vector<GLint> resultBuf(numCompressedFormats * 2, 0);

        // Test when the bufSize is too low
        glGetIntegervRobustANGLE(GL_COMPRESSED_TEXTURE_FORMATS, numCompressedFormats - 1, &length,
                                 resultBuf.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        EXPECT_TRUE(std::all_of(resultBuf.begin(), resultBuf.end(),
                                [](GLint value) { return value == 0; }));

        // Make sure the GL doesn't touch the end of the buffer
        glGetIntegervRobustANGLE(GL_COMPRESSED_TEXTURE_FORMATS,
                                 static_cast<GLsizei>(resultBuf.size()), &length, resultBuf.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(numCompressedFormats, length);
        EXPECT_TRUE(std::none_of(resultBuf.begin(), resultBuf.begin() + length,
                                 [](GLint value) { return value == 0; }));
        EXPECT_TRUE(std::all_of(resultBuf.begin() + length, resultBuf.end(),
                                [](GLint value) { return value == 0; }));
    }

    // Test with null length
    glGetIntegervRobustANGLE(GL_MAX_VARYING_VECTORS, 1, nullptr, &resultRobust);
    EXPECT_GL_NO_ERROR();

    glGetIntegervRobustANGLE(GL_MAX_VIEWPORT_DIMS, 1, nullptr, &resultRobust);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    GLint maxViewportDims[2];
    glGetIntegervRobustANGLE(GL_MAX_VIEWPORT_DIMS, 2, nullptr, maxViewportDims);
    EXPECT_GL_NO_ERROR();
}

// Test basic usage and validation of glTexImage2DRobustANGLE
TEST_P(RobustClientMemoryTest, TexImage2D)
{
    if (!extensionsPresent())
    {
        return;
    }
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex.get());

    GLsizei dataDimension = 1024;
    std::vector<GLubyte> rgbaData(dataDimension * dataDimension * 4);

    // Test the regular case
    glTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, GL_RGBA, dataDimension, dataDimension, 0, GL_RGBA,
                            GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()),
                            rgbaData.data());
    EXPECT_GL_NO_ERROR();

    // Test with a data size that is too small
    glTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, GL_RGBA, dataDimension, dataDimension, 0, GL_RGBA,
                            GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()) / 2,
                            rgbaData.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (getClientMajorVersion() >= 3)
    {
        // Set an unpack parameter that would cause the driver to read past the end of the buffer
        glPixelStorei(GL_UNPACK_ROW_LENGTH, dataDimension + 1);
        glTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, GL_RGBA, dataDimension, dataDimension, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()),
                                rgbaData.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

// Test basic usage and validation of glReadPixelsRobustANGLE
TEST_P(RobustClientMemoryTest, ReadPixels)
{
    if (!extensionsPresent())
    {
        return;
    }

    GLsizei dataDimension = 16;
    std::vector<GLubyte> rgbaData(dataDimension * dataDimension * 4);

    // Test the regular case
    GLsizei length = 0;
    glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                            static_cast<GLsizei>(rgbaData.size()), &length, rgbaData.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(static_cast<GLsizei>(rgbaData.size()), length);

    // Test with a data size that is too small
    glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                            static_cast<GLsizei>(rgbaData.size()) - 1, &length, rgbaData.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (getClientMajorVersion() >= 3)
    {
        // Set a pack parameter that would cause the driver to write past the end of the buffer
        glPixelStorei(GL_PACK_ROW_LENGTH, dataDimension + 1);
        glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                                static_cast<GLsizei>(rgbaData.size()), &length, rgbaData.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(RobustClientMemoryTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());

}  // namespace

//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FormatPrintTest:
//   Prints all format support info
//

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_instantiate.h"
// 'None' is defined as 'struct None {};' in
// third_party/googletest/src/googletest/include/gtest/internal/gtest-type-util.h.
// But 'None' is also defined as a numeric constant 0L in <X11/X.h>.
// So we need to include ANGLETest.h first to avoid this conflict.

#include "libANGLE/Context.h"
#include "libANGLE/capture/gl_enum_utils.h"
#include "libANGLE/formatutils.h"
#include "util/EGLWindow.h"

using namespace angle;

namespace
{

class FormatPrintTest : public ANGLETest<>
{};

// This test enumerates all sized and unsized GL formats and prints out support information
// This test omits unsupported formats
// The output is csv parseable and has a header and a new line.
// Each row consists of:
// (InternalFormat,Type,texturable,filterable,textureAttachmentSupported,renderBufferSupported)
TEST_P(FormatPrintTest, PrintAllSupportedFormats)
{
    // Hack the angle!
    gl::Context *context = static_cast<gl::Context *>(getEGLWindow()->getContext());
    const gl::InternalFormatInfoMap &allSupportedFormats = gl::GetInternalFormatMap();

    std::cout << std::endl
              << "InternalFormat,Type,Texturable,Filterable,Texture attachment,Renderbuffer"
              << std::endl
              << std::endl;

    for (const auto &internalFormat : allSupportedFormats)
    {
        for (const auto &typeFormatPair : internalFormat.second)
        {
            bool textureSupport = typeFormatPair.second.textureSupport(context->getClientVersion(),
                                                                       context->getExtensions());
            bool filterSupport  = typeFormatPair.second.filterSupport(context->getClientVersion(),
                                                                      context->getExtensions());
            bool textureAttachmentSupport = typeFormatPair.second.textureAttachmentSupport(
                context->getClientVersion(), context->getExtensions());
            bool renderbufferSupport = typeFormatPair.second.renderbufferSupport(
                context->getClientVersion(), context->getExtensions());

            // Skip if not supported
            // A format is not supported if the only feature bit enabled is "filterSupport"
            if (!(textureSupport || textureAttachmentSupport || renderbufferSupport))
            {
                continue;
            }

            // Lookup enum strings from enum
            std::stringstream resultStringStream;
            gl::OutputGLenumString(resultStringStream, gl::GLESEnum::InternalFormat,
                                   internalFormat.first);
            resultStringStream << ",";
            gl::OutputGLenumString(resultStringStream, gl::GLESEnum::PixelType,
                                   typeFormatPair.first);
            resultStringStream << ",";

            // able to be sampled from, see GLSL sampler variables
            if (textureSupport)
            {
                resultStringStream << "texturable";
            }
            resultStringStream << ",";

            // able to be linearly filtered (GL_LINEAR)
            if (filterSupport)
            {
                resultStringStream << "filterable";
            }
            resultStringStream << ",";

            // a texture with this can be used for glFramebufferTexture2D
            if (textureAttachmentSupport)
            {
                resultStringStream << "textureAttachmentSupported";
            }
            resultStringStream << ",";

            // usable with glFramebufferRenderbuffer, glRenderbufferStorage,
            // glNamedRenderbufferStorage
            if (renderbufferSupport)
            {
                resultStringStream << "renderbufferSupported";
            }

            std::cout << resultStringStream.str() << std::endl;
        }
    }
}

ANGLE_INSTANTIATE_TEST(FormatPrintTest, ES2_VULKAN(), ES3_VULKAN());
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(FormatPrintTest);

}  // anonymous namespace

// Included here to fix a compile error due to white box tests using angle_end2end_tests_main.
void RegisterContextCompatibilityTests() {}

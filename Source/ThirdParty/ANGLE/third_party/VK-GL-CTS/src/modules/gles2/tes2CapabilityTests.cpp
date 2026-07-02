/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Capability Tests.
 *//*--------------------------------------------------------------------*/

#include "tes2CapabilityTests.hpp"
#include "gluStrUtil.hpp"
#include "tcuTestLog.hpp"
#include "deStringUtil.hpp"
#include "gluContextInfo.hpp"

#include <algorithm>
#include <iterator>

#include "glw.h"

using std::string;
using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gles2
{

class GetIntCase : public tcu::TestCase
{
public:
    GetIntCase(Context &context, const char *name, const char *description, GLenum param)
        : tcu::TestCase(context.getTestContext(), tcu::NODETYPE_CAPABILITY, name, description)
        , m_param(param)
    {
    }

    IterateResult iterate(void)
    {
        GLint value = 0;
        GLU_CHECK_CALL(glGetIntegerv(m_param, &value));

        m_testCtx.getLog() << TestLog::Message << glu::getParamQueryStr(m_param) << " = " << value
                           << TestLog::EndMessage;

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::toString(value).c_str());
        return STOP;
    }

private:
    GLenum m_param;
};

class LimitTests : public TestCaseGroup
{
public:
    LimitTests(Context &context) : TestCaseGroup(context, "limits", "Implementation-defined limits")
    {
    }

    void init(void)
    {
        static const struct
        {
            const char *name;
            const char *description;
            GLenum param;
        } getIntCases[] = {
            {"vertex_attribs", "Number of vertex attributes supported", GL_MAX_VERTEX_ATTRIBS},
            {"varying_vectors", "Number of varying vectors supported", GL_MAX_VARYING_VECTORS},
            {"vertex_uniform_vectors", "Number of vertex uniform vectors supported", GL_MAX_VERTEX_UNIFORM_VECTORS},
            {"fragment_uniform_vectors", "Number of fragment uniform vectors supported",
             GL_MAX_FRAGMENT_UNIFORM_VECTORS},
            {"texture_image_units", "Number of fragment texture units supported", GL_MAX_TEXTURE_IMAGE_UNITS},
            {"vertex_texture_image_units", "Number of vertex texture units supported",
             GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS},
            {"combined_texture_image_units", "Number of vertex and fragment combined texture units supported",
             GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS},
            {"texture_2d_size", "Maximum 2D texture size", GL_MAX_TEXTURE_SIZE},
            {"texture_cube_size", "Maximum cubemap texture size", GL_MAX_CUBE_MAP_TEXTURE_SIZE},
            {"renderbuffer_size", "Maximum renderbuffer size", GL_MAX_RENDERBUFFER_SIZE},
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(getIntCases); ndx++)
            addChild(
                new GetIntCase(m_context, getIntCases[ndx].name, getIntCases[ndx].description, getIntCases[ndx].param));
    }
};

class ExtensionCase : public tcu::TestCase
{
public:
    ExtensionCase(tcu::TestContext &testCtx, const glu::ContextInfo &ctxInfo, const char *name, const char *desc,
                  const char *extName);

    IterateResult iterate(void);

private:
    const glu::ContextInfo &m_ctxInfo;
    std::string m_extName;
};

ExtensionCase::ExtensionCase(tcu::TestContext &testCtx, const glu::ContextInfo &ctxInfo, const char *name,
                             const char *desc, const char *extName)
    : tcu::TestCase(testCtx, tcu::NODETYPE_CAPABILITY, name, desc)
    , m_ctxInfo(ctxInfo)
    , m_extName(extName)
{
}

ExtensionCase::IterateResult ExtensionCase::iterate(void)
{
    bool isSupported = std::find(m_ctxInfo.getExtensions().begin(), m_ctxInfo.getExtensions().end(), m_extName) !=
                       m_ctxInfo.getExtensions().end();
    m_testCtx.setTestResult(isSupported ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_NOT_SUPPORTED,
                            isSupported ? "Supported" : "Not supported");
    return STOP;
}

class ExtensionTests : public TestCaseGroup
{
public:
    ExtensionTests(Context &context) : TestCaseGroup(context, "extensions", "Supported extensions")
    {
    }

    void init(void)
    {
        struct ExtGroup
        {
            tcu::TestCaseGroup *group;
            const glu::ContextInfo &ctxInfo;

            ExtGroup(TestCaseGroup *parent, const char *name, const char *desc)
                : group(nullptr)
                , ctxInfo(parent->getContext().getContextInfo())
            {
                group = new tcu::TestCaseGroup(parent->getTestContext(), name, desc);
                parent->addChild(group);
            }

            ExtGroup &operator<<(const char *extName)
            {
                group->addChild(new ExtensionCase(group->getTestContext(), ctxInfo, extName, "", extName));
                return *this;
            }
        };

        // Uncompressed formats.
        ExtGroup(this, "uncompressed_texture_formats", "Uncompressed texture formats")
            << "GL_OES_texture_float_linear"
            << "GL_OES_texture_half_float_linear"
            << "GL_OES_texture_float"
            << "GL_OES_texture_half_float"
            << "GL_OES_texture_npot"
            << "GL_EXT_texture_format_BGRA8888"
            << "GL_EXT_texture_rg"
            << "GL_EXT_texture_type_2_10_10_10_REV"
            << "GL_EXT_sRGB"
            << "GL_APPLE_rgb_422"
            << "GL_APPLE_texture_format_BGRA8888";

        // Compressed formats.
        ExtGroup(this, "compressed_texture_formats", "Compressed texture formats")
            << "GL_OES_compressed_ETC1_RGB8_texture"
            << "GL_OES_compressed_paletted_texture"
            << "GL_EXT_texture_compression_dxt1"
            << "GL_AMD_compressed_3DC_texture"
            << "GL_AMD_compressed_ATC_texture"
            << "GL_IMG_texture_compression_pvrtc"
            << "GL_NV_texture_compression_s3tc_update";

        // Texture features.
        ExtGroup(this, "texture", "Texturing features") << "GL_OES_texture_3D"
                                                        << "GL_OES_depth_texture"
                                                        << "GL_EXT_texture_filter_anisotropic"
                                                        << "GL_EXT_texture_lod_bias"
                                                        << "GL_EXT_shadow_samplers"
                                                        << "GL_EXT_texture_storage"
                                                        << "GL_NV_texture_npot_2D_mipmap"
                                                        << "GL_APPLE_texture_max_level";

        // FBO features
        ExtGroup(this, "fbo", "FBO features") << "GL_OES_depth24"
                                              << "GL_OES_depth32"
                                              << "GL_OES_packed_depth_stencil"
                                              << "GL_OES_fbo_render_mipmap"
                                              << "GL_OES_rgb8_rgba8"
                                              << "GL_OES_stencil1"
                                              << "GL_OES_stencil4"
                                              << "GL_OES_stencil8"
                                              << "GL_EXT_color_buffer_half_float"
                                              << "GL_EXT_multisampled_render_to_texture"
                                              << "GL_IMG_multisampled_render_to_texture"
                                              << "GL_ARM_rgba8"
                                              << "GL_NV_depth_nonlinear"
                                              << "GL_NV_draw_buffers"
                                              << "GL_NV_fbo_color_attachments"
                                              << "GL_NV_read_buffer"
                                              << "GL_APPLE_framebuffer_multisample";

        // Vertex data formats.
        ExtGroup(this, "vertex_data_formats", "Vertex data formats") << "GL_OES_element_index_uint"
                                                                     << "GL_OES_vertex_half_float"
                                                                     << "GL_OES_vertex_type_10_10_10_2";

        // Shader functionality.
        ExtGroup(this, "shaders", "Shader features") << "GL_OES_fragment_precision_high"
                                                     << "GL_OES_standard_derivatives"
                                                     << "GL_EXT_shader_texture_lod"
                                                     << "GL_EXT_frag_depth"
                                                     << "GL_EXT_separate_shader_objects";

        // Shader binary formats.
        ExtGroup(this, "shader_binary_formats", "Shader binary formats") << "GL_OES_get_program_binary"
                                                                         << "GL_AMD_program_binary_Z400"
                                                                         << "GL_IMG_shader_binary"
                                                                         << "GL_IMG_program_binary"
                                                                         << "GL_ARM_mali_shader_binary"
                                                                         << "GL_VIV_shader_binary"
                                                                         << "GL_DMP_shader_binary";

        // Development features.
        ExtGroup(this, "development", "Development aids") << "GL_EXT_debug_label"
                                                          << "GL_EXT_debug_marker"
                                                          << "GL_AMD_performance_monitor"
                                                          << "GL_QCOM_performance_monitor_global_mode"
                                                          << "GL_QCOM_extended_get"
                                                          << "GL_QCOM_extended_get2";

        // Other extensions.
        ExtGroup(this, "other", "Other extensions") << "GL_OES_draw_texture"
                                                    << "GL_OES_mapbuffer"
                                                    << "GL_OES_vertex_array_object"
                                                    << "GL_EXT_occlusion_query_boolean"
                                                    << "GL_EXT_robustness"
                                                    << "GL_EXT_discard_framebuffer"
                                                    << "GL_EXT_read_format_bgra"
                                                    << "GL_EXT_multi_draw_arrays"
                                                    << "GL_EXT_unpack_subimage"
                                                    << "GL_EXT_blend_minmax"
                                                    << "GL_IMG_read_format"
                                                    << "GL_NV_coverage_sample"
                                                    << "GL_NV_read_depth_stencil"
                                                    << "GL_SUN_multi_draw_arrays";
    }
};

CapabilityTests::CapabilityTests(Context &context) : TestCaseGroup(context, "capability", "Capability Tests")
{
}

CapabilityTests::~CapabilityTests(void)
{
}

void CapabilityTests::init(void)
{
    addChild(new LimitTests(m_context));
    addChild(new ExtensionTests(m_context));
}

} // namespace gles2
} // namespace deqp

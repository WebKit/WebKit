/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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
 * \brief Implementation-defined limit tests.
 *//*--------------------------------------------------------------------*/

#include "es3fImplementationLimitTests.hpp"
#include "tcuTestLog.hpp"
#include "gluDefs.hpp"
#include "gluStrUtil.hpp"
#include "gluRenderContext.hpp"

#include <vector>
#include <set>
#include <algorithm>
#include <iterator>
#include <limits>

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::set;
using std::string;
using std::vector;
using namespace glw; // GL types

namespace LimitQuery
{

// Query function template.
template <typename T>
T query(const glw::Functions &gl, uint32_t param);

// Compare template.
template <typename T>
inline bool compare(const T &min, const T &reported)
{
    return min <= reported;
}

// Types for queries

struct NegInt
{
    GLint value;
    NegInt(GLint value_) : value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const NegInt &v)
{
    return str << v.value;
}

struct FloatRange
{
    float min;
    float max;
    FloatRange(float min_, float max_) : min(min_), max(max_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const FloatRange &range)
{
    return str << range.min << ", " << range.max;
}

struct AlignmentInt
{
    GLint value;
    AlignmentInt(GLint value_) : value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const AlignmentInt &v)
{
    return str << v.value;
}

// For custom formatting
struct Boolean
{
    GLboolean value;
    Boolean(GLboolean value_) : value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const Boolean &boolean)
{
    return str << (boolean.value ? "GL_TRUE" : "GL_FALSE");
}

// Query function implementations.
template <>
GLint query<GLint>(const glw::Functions &gl, uint32_t param)
{
    GLint val = -1;
    gl.getIntegerv(param, &val);
    return val;
}

template <>
GLint64 query<GLint64>(const glw::Functions &gl, uint32_t param)
{
    GLint64 val = -1;
    gl.getInteger64v(param, &val);
    return val;
}

template <>
GLuint64 query<GLuint64>(const glw::Functions &gl, uint32_t param)
{
    GLint64 val = 0;
    gl.getInteger64v(param, &val);
    return (GLuint64)val;
}

template <>
GLfloat query<GLfloat>(const glw::Functions &gl, uint32_t param)
{
    GLfloat val = -1000.f;
    gl.getFloatv(param, &val);
    return val;
}

template <>
NegInt query<NegInt>(const glw::Functions &gl, uint32_t param)
{
    return NegInt(query<GLint>(gl, param));
}

template <>
Boolean query<Boolean>(const glw::Functions &gl, uint32_t param)
{
    GLboolean val = GL_FALSE;
    gl.getBooleanv(param, &val);
    return Boolean(val);
}

template <>
FloatRange query<FloatRange>(const glw::Functions &gl, uint32_t param)
{
    float v[2] = {-1.0f, -1.0f};
    gl.getFloatv(param, &v[0]);
    return FloatRange(v[0], v[1]);
}

template <>
AlignmentInt query<AlignmentInt>(const glw::Functions &gl, uint32_t param)
{
    return AlignmentInt(query<GLint>(gl, param));
}

// Special comparison operators
template <>
bool compare<Boolean>(const Boolean &min, const Boolean &reported)
{
    return !min.value || (min.value && reported.value);
}

template <>
bool compare<NegInt>(const NegInt &min, const NegInt &reported)
{
    // Reverse comparison.
    return reported.value <= min.value;
}

template <>
bool compare<FloatRange>(const FloatRange &min, const FloatRange &reported)
{
    return reported.min <= min.min && min.max <= reported.max;
}

template <>
bool compare<AlignmentInt>(const AlignmentInt &min, const AlignmentInt &reported)
{
    // Reverse comparison.
    return reported.value <= min.value;
}

// Special error descriptions

enum QueryClass
{
    CLASS_VALUE = 0,
    CLASS_RANGE,
    CLASS_ALIGNMENT,
};

template <QueryClass Class>
struct QueryClassTraits
{
    static const char *const s_errorDescription;
};

template <>
const char *const QueryClassTraits<CLASS_VALUE>::s_errorDescription =
    "reported value is less than minimum required value!";

template <>
const char *const QueryClassTraits<CLASS_RANGE>::s_errorDescription =
    "reported range does not contain the minimum required range!";

template <>
const char *const QueryClassTraits<CLASS_ALIGNMENT>::s_errorDescription =
    "reported alignment is larger than minimum required aligmnent!";

template <typename T>
struct QueryTypeTraits
{
};

template <>
struct QueryTypeTraits<GLint>
{
    enum
    {
        CLASS = CLASS_VALUE
    };
};
template <>
struct QueryTypeTraits<GLint64>
{
    enum
    {
        CLASS = CLASS_VALUE
    };
};
template <>
struct QueryTypeTraits<GLuint64>
{
    enum
    {
        CLASS = CLASS_VALUE
    };
};
template <>
struct QueryTypeTraits<GLfloat>
{
    enum
    {
        CLASS = CLASS_VALUE
    };
};
template <>
struct QueryTypeTraits<Boolean>
{
    enum
    {
        CLASS = CLASS_VALUE
    };
};
template <>
struct QueryTypeTraits<NegInt>
{
    enum
    {
        CLASS = CLASS_VALUE
    };
};
template <>
struct QueryTypeTraits<FloatRange>
{
    enum
    {
        CLASS = CLASS_RANGE
    };
};
template <>
struct QueryTypeTraits<AlignmentInt>
{
    enum
    {
        CLASS = CLASS_ALIGNMENT
    };
};

} // namespace LimitQuery

using namespace LimitQuery;
using tcu::TestLog;

template <typename T>
class LimitQueryCase : public TestCase
{
public:
    LimitQueryCase(Context &context, const char *name, const char *description, uint32_t limit,
                   const T &minRequiredValue)
        : TestCase(context, name, description)
        , m_limit(limit)
        , m_minRequiredValue(minRequiredValue)
    {
    }

    IterateResult iterate(void)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const T value            = query<T>(m_context.getRenderContext().getFunctions(), m_limit);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Query failed");

        const bool isOk = compare<T>(m_minRequiredValue, value);

        m_testCtx.getLog() << TestLog::Message << "Reported: " << value << TestLog::EndMessage;
        m_testCtx.getLog() << TestLog::Message << "Minimum required: " << m_minRequiredValue << TestLog::EndMessage;

        if (!isOk)
            m_testCtx.getLog() << TestLog::Message << "FAIL: "
                               << QueryClassTraits<(QueryClass)QueryTypeTraits<T>::CLASS>::s_errorDescription
                               << TestLog::EndMessage;

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Requirement not satisfied");
        return STOP;
    }

private:
    uint32_t m_limit;
    T m_minRequiredValue;
};

static const uint32_t s_requiredCompressedTexFormats[] = {GL_COMPRESSED_R11_EAC,
                                                          GL_COMPRESSED_SIGNED_R11_EAC,
                                                          GL_COMPRESSED_RG11_EAC,
                                                          GL_COMPRESSED_SIGNED_RG11_EAC,
                                                          GL_COMPRESSED_RGB8_ETC2,
                                                          GL_COMPRESSED_SRGB8_ETC2,
                                                          GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
                                                          GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
                                                          GL_COMPRESSED_RGBA8_ETC2_EAC,
                                                          GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC};

class CompressedTextureFormatsQueryCase : public TestCase
{
public:
    CompressedTextureFormatsQueryCase(Context &context)
        : TestCase(context, "compressed_texture_formats", "GL_COMPRESSED_TEXTURE_FORMATS")
    {
    }

    IterateResult iterate(void)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const GLint numFormats   = query<GLint>(gl, GL_NUM_COMPRESSED_TEXTURE_FORMATS);
        vector<GLint> formats(numFormats);
        bool allFormatsOk = true;

        if (numFormats > 0)
            gl.getIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, &formats[0]);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Query failed");

        // Log formats.
        m_testCtx.getLog() << TestLog::Message << "Reported:" << TestLog::EndMessage;
        for (vector<GLint>::const_iterator fmt = formats.begin(); fmt != formats.end(); fmt++)
            m_testCtx.getLog() << TestLog::Message << glu::getCompressedTextureFormatStr(*fmt) << TestLog::EndMessage;

        // Check that all required formats are in list.
        {
            set<GLint> formatSet(formats.begin(), formats.end());

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_requiredCompressedTexFormats); ndx++)
            {
                const uint32_t fmt = s_requiredCompressedTexFormats[ndx];
                const bool found   = formatSet.find(fmt) != formatSet.end();

                if (!found)
                {
                    m_testCtx.getLog() << TestLog::Message << "ERROR: " << glu::getCompressedTextureFormatStr(fmt)
                                       << " is missing!" << TestLog::EndMessage;
                    allFormatsOk = false;
                }
            }
        }

        m_testCtx.setTestResult(allFormatsOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                allFormatsOk ? "Pass" : "Requirement not satisfied");
        return STOP;
    }
};

static vector<string> queryExtensionsNonIndexed(const glw::Functions &gl)
{
    const string extensionStr = (const char *)gl.getString(GL_EXTENSIONS);
    vector<string> extensionList;
    size_t pos = 0;

    for (;;)
    {
        const size_t nextPos = extensionStr.find(' ', pos);
        const size_t len     = nextPos == string::npos ? extensionStr.length() - pos : nextPos - pos;

        if (len > 0)
            extensionList.push_back(extensionStr.substr(pos, len));

        if (nextPos == string::npos)
            break;
        else
            pos = nextPos + 1;
    }

    return extensionList;
}

static vector<string> queryExtensionsIndexed(const glw::Functions &gl)
{
    const int numExtensions = query<GLint>(gl, GL_NUM_EXTENSIONS);
    vector<string> extensions(numExtensions);

    GLU_EXPECT_NO_ERROR(gl.getError(), "GL_NUM_EXTENSIONS query failed");

    for (int ndx = 0; ndx < numExtensions; ndx++)
        extensions[ndx] = (const char *)gl.getStringi(GL_EXTENSIONS, (GLuint)ndx);

    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetStringi(GL_EXTENSIONS) failed");

    return extensions;
}

static bool compareExtensionLists(const vector<string> &a, const vector<string> &b)
{
    if (a.size() != b.size())
        return false;

    set<string> extsInB(b.begin(), b.end());

    for (vector<string>::const_iterator i = a.begin(); i != a.end(); ++i)
    {
        if (extsInB.find(*i) == extsInB.end())
            return false;
    }

    return true;
}

class ExtensionQueryCase : public TestCase
{
public:
    ExtensionQueryCase(Context &context) : TestCase(context, "extensions", "GL_EXTENSIONS")
    {
    }

    IterateResult iterate(void)
    {
        const glw::Functions &gl            = m_context.getRenderContext().getFunctions();
        const vector<string> nonIndexedExts = queryExtensionsNonIndexed(gl);
        const vector<string> indexedExts    = queryExtensionsIndexed(gl);
        const bool isOk                     = compareExtensionLists(nonIndexedExts, indexedExts);

        m_testCtx.getLog() << TestLog::Message
                           << "Extensions as reported by glGetStringi(GL_EXTENSIONS):" << TestLog::EndMessage;
        for (vector<string>::const_iterator ext = indexedExts.begin(); ext != indexedExts.end(); ++ext)
            m_testCtx.getLog() << TestLog::Message << *ext << TestLog::EndMessage;

        if (!isOk)
        {
            m_testCtx.getLog() << TestLog::Message
                               << "Extensions as reported by glGetString(GL_EXTENSIONS):" << TestLog::EndMessage;
            for (vector<string>::const_iterator ext = nonIndexedExts.begin(); ext != nonIndexedExts.end(); ++ext)
                m_testCtx.getLog() << TestLog::Message << *ext << TestLog::EndMessage;

            m_testCtx.getLog() << TestLog::Message << "ERROR: Extension lists do not match!" << TestLog::EndMessage;
        }

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Invalid extension list");
        return STOP;
    }
};

ImplementationLimitTests::ImplementationLimitTests(Context &context)
    : TestCaseGroup(context, "implementation_limits", "Implementation-defined limits")
{
}

ImplementationLimitTests::~ImplementationLimitTests(void)
{
}

void ImplementationLimitTests::init(void)
{
    const int minVertexUniformBlocks     = 12;
    const int minVertexUniformComponents = 1024;

    const int minFragmentUniformBlocks     = 12;
    const int minFragmentUniformComponents = 896;

    const int minUniformBlockSize = 16384;
    const int minCombinedVertexUniformComponents =
        (minVertexUniformBlocks * minUniformBlockSize) / 4 + minVertexUniformComponents;
    const int minCombinedFragmentUniformComponents =
        (minFragmentUniformBlocks * minUniformBlockSize) / 4 + minFragmentUniformComponents;

#define LIMIT_CASE(NAME, PARAM, TYPE, MIN_VAL) \
    addChild(new LimitQueryCase<TYPE>(m_context, #NAME, #PARAM, PARAM, MIN_VAL))

    LIMIT_CASE(max_element_index, GL_MAX_ELEMENT_INDEX, GLint64, (1 << 24) - 1);
    LIMIT_CASE(subpixel_bits, GL_SUBPIXEL_BITS, GLint, 4);
    LIMIT_CASE(max_3d_texture_size, GL_MAX_3D_TEXTURE_SIZE, GLint, 256);
    LIMIT_CASE(max_texture_size, GL_MAX_TEXTURE_SIZE, GLint, 2048);
    LIMIT_CASE(max_array_texture_layers, GL_MAX_ARRAY_TEXTURE_LAYERS, GLint, 256);
    LIMIT_CASE(max_texture_lod_bias, GL_MAX_TEXTURE_LOD_BIAS, GLfloat, 2.0f);
    LIMIT_CASE(max_cube_map_texture_size, GL_MAX_CUBE_MAP_TEXTURE_SIZE, GLint, 2048);
    LIMIT_CASE(max_renderbuffer_size, GL_MAX_RENDERBUFFER_SIZE, GLint, 2048);
    LIMIT_CASE(max_draw_buffers, GL_MAX_DRAW_BUFFERS, GLint, 4);
    LIMIT_CASE(max_color_attachments, GL_MAX_COLOR_ATTACHMENTS, GLint, 4);
    // GL_MAX_VIEWPORT_DIMS
    LIMIT_CASE(aliased_point_size_range, GL_ALIASED_POINT_SIZE_RANGE, FloatRange, FloatRange(1, 1));
    LIMIT_CASE(aliased_line_width_range, GL_ALIASED_LINE_WIDTH_RANGE, FloatRange, FloatRange(1, 1));
    LIMIT_CASE(max_elements_indices, GL_MAX_ELEMENTS_INDICES, GLint, 0);
    LIMIT_CASE(max_elements_vertices, GL_MAX_ELEMENTS_VERTICES, GLint, 0);
    LIMIT_CASE(num_compressed_texture_formats, GL_NUM_COMPRESSED_TEXTURE_FORMATS, GLint,
               DE_LENGTH_OF_ARRAY(s_requiredCompressedTexFormats));
    addChild(new CompressedTextureFormatsQueryCase(m_context)); // GL_COMPRESSED_TEXTURE_FORMATS
    // GL_PROGRAM_BINARY_FORMATS
    LIMIT_CASE(num_program_binary_formats, GL_NUM_PROGRAM_BINARY_FORMATS, GLint, 0);
    // GL_SHADER_BINARY_FORMATS
    LIMIT_CASE(num_shader_binary_formats, GL_NUM_SHADER_BINARY_FORMATS, GLint, 0);
    LIMIT_CASE(shader_compiler, GL_SHADER_COMPILER, Boolean, GL_TRUE);
    // Shader data type ranges & precisions
    LIMIT_CASE(max_server_wait_timeout, GL_MAX_SERVER_WAIT_TIMEOUT, GLuint64, 0);

    // Version and extension support
    addChild(new ExtensionQueryCase(m_context)); // GL_EXTENSIONS + consistency validation
    LIMIT_CASE(num_extensions, GL_NUM_EXTENSIONS, GLint, 0);
    LIMIT_CASE(major_version, GL_MAJOR_VERSION, GLint, 3);
    LIMIT_CASE(minor_version, GL_MINOR_VERSION, GLint, 0);
    // GL_RENDERER
    // GL_SHADING_LANGUAGE_VERSION
    // GL_VENDOR
    // GL_VERSION

    // Vertex shader limits
    LIMIT_CASE(max_vertex_attribs, GL_MAX_VERTEX_ATTRIBS, GLint, 16);
    LIMIT_CASE(max_vertex_uniform_components, GL_MAX_VERTEX_UNIFORM_COMPONENTS, GLint, minVertexUniformComponents);
    LIMIT_CASE(max_vertex_uniform_vectors, GL_MAX_VERTEX_UNIFORM_VECTORS, GLint, minVertexUniformComponents / 4);
    LIMIT_CASE(max_vertex_uniform_blocks, GL_MAX_VERTEX_UNIFORM_BLOCKS, GLint, minVertexUniformBlocks);
    LIMIT_CASE(max_vertex_output_components, GL_MAX_VERTEX_OUTPUT_COMPONENTS, GLint, 64);
    LIMIT_CASE(max_vertex_texture_image_units, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, GLint, 16);

    // Fragment shader limits
    LIMIT_CASE(max_fragment_uniform_components, GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, GLint,
               minFragmentUniformComponents);
    LIMIT_CASE(max_fragment_uniform_vectors, GL_MAX_FRAGMENT_UNIFORM_VECTORS, GLint, minFragmentUniformComponents / 4);
    LIMIT_CASE(max_fragment_uniform_blocks, GL_MAX_FRAGMENT_UNIFORM_BLOCKS, GLint, minFragmentUniformBlocks);
    LIMIT_CASE(max_fragment_input_components, GL_MAX_FRAGMENT_INPUT_COMPONENTS, GLint, 60);
    LIMIT_CASE(max_texture_image_units, GL_MAX_TEXTURE_IMAGE_UNITS, GLint, 16);
    LIMIT_CASE(min_program_texel_offset, GL_MIN_PROGRAM_TEXEL_OFFSET, NegInt, -8);
    LIMIT_CASE(max_program_texel_offset, GL_MAX_PROGRAM_TEXEL_OFFSET, GLint, 7);

    // Aggregate shader limits
    LIMIT_CASE(max_uniform_buffer_bindings, GL_MAX_UNIFORM_BUFFER_BINDINGS, GLint, 24);
    LIMIT_CASE(max_uniform_block_size, GL_MAX_UNIFORM_BLOCK_SIZE, GLint64, minUniformBlockSize);
    LIMIT_CASE(uniform_buffer_offset_alignment, GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, AlignmentInt, 256);
    LIMIT_CASE(max_combined_uniform_blocks, GL_MAX_COMBINED_UNIFORM_BLOCKS, GLint, 24);
    LIMIT_CASE(max_combined_vertex_uniform_components, GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS, GLint64,
               minCombinedVertexUniformComponents);
    LIMIT_CASE(max_combined_fragment_uniform_components, GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS, GLint64,
               minCombinedFragmentUniformComponents);
    LIMIT_CASE(max_varying_components, GL_MAX_VARYING_COMPONENTS, GLint, 60);
    LIMIT_CASE(max_varying_vectors, GL_MAX_VARYING_VECTORS, GLint, 15);
    LIMIT_CASE(max_combined_texture_image_units, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, GLint, 32);

    // Transform feedback limits
    LIMIT_CASE(max_transform_feedback_interleaved_components, GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, GLint,
               64);
    LIMIT_CASE(max_transform_feedback_separate_attribs, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, GLint, 4);
    LIMIT_CASE(max_transform_feedback_separate_components, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, GLint, 4);
}

} // namespace Functional
} // namespace gles3
} // namespace deqp

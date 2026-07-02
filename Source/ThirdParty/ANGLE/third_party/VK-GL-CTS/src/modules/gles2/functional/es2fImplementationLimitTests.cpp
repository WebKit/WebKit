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
 * \brief Implementation-defined limit tests.
 *//*--------------------------------------------------------------------*/

#include "es2fImplementationLimitTests.hpp"
#include "tcuTestLog.hpp"
#include "gluDefs.hpp"
#include "gluStrUtil.hpp"
#include "gluRenderContext.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

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
            m_testCtx.getLog() << TestLog::Message << "FAIL: reported value is less than minimum required value!"
                               << TestLog::EndMessage;

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Requirement not satisfied");
        return STOP;
    }

private:
    uint32_t m_limit;
    T m_minRequiredValue;
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
#define LIMIT_CASE(NAME, PARAM, TYPE, MIN_VAL) \
    addChild(new LimitQueryCase<TYPE>(m_context, #NAME, #PARAM, PARAM, MIN_VAL))

    LIMIT_CASE(subpixel_bits, GL_SUBPIXEL_BITS, GLint, 4);
    LIMIT_CASE(max_texture_size, GL_MAX_TEXTURE_SIZE, GLint, 64);
    LIMIT_CASE(max_cube_map_texture_size, GL_MAX_CUBE_MAP_TEXTURE_SIZE, GLint, 16);
    // GL_MAX_VIEWPORT_DIMS
    LIMIT_CASE(aliased_point_size_range, GL_ALIASED_POINT_SIZE_RANGE, FloatRange, FloatRange(1, 1));
    LIMIT_CASE(aliased_line_width_range, GL_ALIASED_LINE_WIDTH_RANGE, FloatRange, FloatRange(1, 1));
    // LIMIT_CASE(sample_buffers, GL_SAMPLE_BUFFERS, GLint, 0);
    // LIMIT_CASE(samples, GL_SAMPLES, GLint, 0);
    LIMIT_CASE(num_compressed_texture_formats, GL_NUM_COMPRESSED_TEXTURE_FORMATS, GLint, 0);
    LIMIT_CASE(num_shader_binary_formats, GL_NUM_SHADER_BINARY_FORMATS, GLint, 0);
    LIMIT_CASE(shader_compiler, GL_SHADER_COMPILER, Boolean, GL_FALSE);
    // Shader precision format
    LIMIT_CASE(max_vertex_attribs, GL_MAX_VERTEX_ATTRIBS, GLint, 8);
    LIMIT_CASE(max_vertex_uniform_vectors, GL_MAX_VERTEX_UNIFORM_VECTORS, GLint, 128);
    LIMIT_CASE(max_varying_vectors, GL_MAX_VARYING_VECTORS, GLint, 8);
    LIMIT_CASE(max_combined_texture_image_units, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, GLint, 8);
    LIMIT_CASE(max_vertex_texture_image_units, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, GLint, 0);
    LIMIT_CASE(max_texture_image_units, GL_MAX_TEXTURE_IMAGE_UNITS, GLint, 8);
    LIMIT_CASE(max_fragment_uniform_vectors, GL_MAX_FRAGMENT_UNIFORM_VECTORS, GLint, 16);
    LIMIT_CASE(max_renderbuffer_size, GL_MAX_RENDERBUFFER_SIZE, GLint, 1);
}

} // namespace Functional
} // namespace gles2
} // namespace deqp

/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Context Info Class.
 *//*--------------------------------------------------------------------*/

#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <iterator>
#include <algorithm>

using std::set;
using std::string;
using std::vector;

namespace glu
{

class TryCompileProgram
{
public:
    // \note Assumes that shader pointer can be stored as is (eg. it is static data)
    TryCompileProgram(const char *vertexSource, const char *fragmentSource)
        : m_vertexSource(vertexSource)
        , m_fragmentSource(fragmentSource)
    {
    }

    bool operator()(const RenderContext &context) const
    {
        ShaderProgram program(context, ProgramSources()
                                           << VertexSource(m_vertexSource) << FragmentSource(m_fragmentSource));
        return program.isOk();
    }

private:
    const char *m_vertexSource;
    const char *m_fragmentSource;
};

typedef CachedValue<bool, TryCompileProgram> IsProgramSupported;

bool IsES3Compatible(const glw::Functions &gl)
{
    // Detect compatible GLES context by querying GL_MAJOR_VERSION.
    // This query does not exist on GLES2 so succeeding query implies GLES3+ context.
    glw::GLint majorVersion = 0;
    gl.getError();
    gl.getIntegerv(GL_MAJOR_VERSION, &majorVersion);

    return (gl.getError() == GL_NO_ERROR);
}

// ES2-specific context info
class ES2ContextInfo : public ContextInfo
{
public:
    ES2ContextInfo(const RenderContext &context);
    ~ES2ContextInfo(void)
    {
    }

    bool isVertexUniformLoopSupported(void) const
    {
        return m_vertexUniformLoopsSupported.getValue(m_context);
    }
    bool isVertexDynamicLoopSupported(void) const
    {
        return m_vertexDynamicLoopsSupported.getValue(m_context);
    }
    bool isFragmentHighPrecisionSupported(void) const
    {
        return m_fragmentHighPrecisionSupported.getValue(m_context);
    }
    bool isFragmentUniformLoopSupported(void) const
    {
        return m_fragmentUniformLoopsSupported.getValue(m_context);
    }
    bool isFragmentDynamicLoopSupported(void) const
    {
        return m_fragmentDynamicLoopsSupported.getValue(m_context);
    }

private:
    IsProgramSupported m_vertexUniformLoopsSupported;
    IsProgramSupported m_vertexDynamicLoopsSupported;

    IsProgramSupported m_fragmentHighPrecisionSupported;
    IsProgramSupported m_fragmentUniformLoopsSupported;
    IsProgramSupported m_fragmentDynamicLoopsSupported;
};

static const char *s_defaultVertexShader   = "attribute highp vec4 a_position;\n"
                                             "void main (void) {\n"
                                             "    gl_Position = a_position;\n"
                                             "}\n";
static const char *s_defaultFragmentShader = "void main (void) {\n"
                                             "    gl_FragColor = vec4(1.0);\n"
                                             "}\n";

static const char *s_vertexUniformLoopsSupported = "attribute highp vec4    a_position;\n"
                                                   "uniform int            u_numIters;\n"
                                                   "void main (void) {\n"
                                                   "    gl_Position = a_position;\n"
                                                   "    for (int i = 0; i < u_numIters; i++)\n"
                                                   "        gl_Position += vec4(0.1);\n"
                                                   "}\n";
static const char *s_vertexDynamicLoopsSupported = "attribute highp vec4    a_position;\n"
                                                   "uniform mediump float    a, b;\n"
                                                   "void main (void) {\n"
                                                   "    gl_Position = a_position;\n"
                                                   "    int numIters = a < b ? int(3.0*b) : int(a_position.x);\n"
                                                   "    for (int i = 0; i < numIters; i++)\n"
                                                   "        gl_Position += vec4(0.1);\n"
                                                   "}\n";

static const char *s_fragmentHighPrecisionSupported = "varying highp vec4        v_color;\n"
                                                      "void main (void) {\n"
                                                      "    highp float tmp = v_color.r;\n"
                                                      "    gl_FragColor = v_color;\n"
                                                      "}\n";
static const char *s_fragmentUniformLoopsSupported  = "varying mediump vec4    v_color;\n"
                                                      "uniform int            u_numIters;\n"
                                                      "void main (void) {\n"
                                                      "    gl_FragColor = v_color;\n"
                                                      "    for (int i = 0; i < u_numIters; i++)\n"
                                                      "        gl_FragColor += vec4(0.1);\n"
                                                      "}\n";
static const char *s_fragmentDynamicLoopsSupported  = "varying mediump vec4    v_color;\n"
                                                      "uniform mediump float    a, b;\n"
                                                      "void main (void) {\n"
                                                      "    gl_FragColor = v_color;\n"
                                                      "    int numIters = a < b ? int(3.0*b) : int(v_color.x);\n"
                                                      "    for (int i = 0; i < numIters; i++)\n"
                                                      "        gl_FragColor += vec4(0.1);\n"
                                                      "}\n";

ES2ContextInfo::ES2ContextInfo(const RenderContext &context)
    : glu::ContextInfo(context)
    , m_vertexUniformLoopsSupported(TryCompileProgram(s_vertexUniformLoopsSupported, s_defaultFragmentShader))
    , m_vertexDynamicLoopsSupported(TryCompileProgram(s_vertexDynamicLoopsSupported, s_defaultFragmentShader))
    , m_fragmentHighPrecisionSupported(TryCompileProgram(s_defaultVertexShader, s_fragmentHighPrecisionSupported))
    , m_fragmentUniformLoopsSupported(TryCompileProgram(s_defaultVertexShader, s_fragmentUniformLoopsSupported))
    , m_fragmentDynamicLoopsSupported(TryCompileProgram(s_defaultVertexShader, s_fragmentDynamicLoopsSupported))
{
}

static void split(vector<string> &dst, const string &src)
{
    size_t start = 0;
    size_t end   = string::npos;

    while ((end = src.find(' ', start)) != string::npos)
    {
        dst.push_back(src.substr(start, end - start));
        start = end + 1;
    }

    if (start < end)
        dst.push_back(src.substr(start, end - start));
}

set<int> GetCompressedTextureFormats::operator()(const RenderContext &context) const
{
    const glw::Functions &gl = context.getFunctions();

    int numFormats = 0;
    gl.getIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &numFormats);

    vector<int> formats(numFormats);
    if (numFormats > 0)
        gl.getIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, &formats[0]);

    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS) failed");

    set<int> formatSet;
    std::copy(formats.begin(), formats.end(), std::inserter(formatSet, formatSet.begin()));

    return formatSet;
}

// ContextInfo

ContextInfo::ContextInfo(const RenderContext &context) : m_context(context)
{
    const glw::Functions &gl = context.getFunctions();

    if (context.getType().getAPI() == ApiType::es(2, 0))
    {
        const char *result = (const char *)gl.getString(GL_EXTENSIONS);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetString(GL_EXTENSIONS) failed");

        split(m_extensions, string(result));
    }
    else
    {
        int numExtensions = 0;

        gl.getIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv(GL_NUM_EXTENSIONS) failed");

        m_extensions.resize(numExtensions);
        for (int ndx = 0; ndx < numExtensions; ndx++)
            m_extensions[ndx] = (const char *)gl.getStringi(GL_EXTENSIONS, ndx);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetStringi(GL_EXTENSIONS, ndx) failed");
    }
}

ContextInfo::~ContextInfo(void)
{
}

int ContextInfo::getInt(int param) const
{
    int val = -1;
    m_context.getFunctions().getIntegerv(param, &val);
    GLU_EXPECT_NO_ERROR(m_context.getFunctions().getError(), "glGetIntegerv() failed");
    return val;
}

bool ContextInfo::getBool(int param) const
{
    glw::GLboolean val = GL_FALSE;
    m_context.getFunctions().getBooleanv(param, &val);
    GLU_EXPECT_NO_ERROR(m_context.getFunctions().getError(), "glGetBooleanv() failed");
    return val != GL_FALSE;
}

const char *ContextInfo::getString(int param) const
{
    const char *str = (const char *)m_context.getFunctions().getString(param);
    GLU_EXPECT_NO_ERROR(m_context.getFunctions().getError(), "glGetString() failed");
    return str;
}

bool ContextInfo::isCompressedTextureFormatSupported(int format) const
{
    const set<int> &formats = m_compressedTextureFormats.getValue(m_context);
    return formats.find(format) != formats.end();
}

bool ContextInfo::isExtensionSupported(const char *name) const
{
    const std::vector<std::string> &extensions = getExtensions();
    return std::find(extensions.begin(), extensions.end(), name) != extensions.end();
}

bool ContextInfo::isES3Compatible() const
{
    return IsES3Compatible(m_context.getFunctions());
}

ContextInfo *ContextInfo::create(const RenderContext &context)
{
    // ES2 uses special variant that checks support for various shader features
    // by trying to compile shader programs.
    if (context.getType().getAPI() == ApiType::es(2, 0))
        return new ES2ContextInfo(context);

    return new ContextInfo(context);
}

} // namespace glu

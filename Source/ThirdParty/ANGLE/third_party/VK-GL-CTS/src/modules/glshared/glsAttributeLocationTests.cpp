/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Attribute location tests
 *//*--------------------------------------------------------------------*/

#include "glsAttributeLocationTests.hpp"

#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

#include "gluDefs.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "gluStrUtil.hpp"

#include "glwFunctions.hpp"

#include "deStringUtil.hpp"

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <cstring>

#include "glw.h"

using tcu::TestLog;

using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

using namespace deqp::gls::AttributeLocationTestUtil;

namespace deqp
{
namespace gls
{
namespace
{

int32_t getBoundLocation(const map<string, uint32_t> &bindings, const string &attrib)
{
    std::map<string, uint32_t>::const_iterator iter = bindings.find(attrib);

    return (iter == bindings.end() ? (int32_t)Attribute::LOC_UNDEF : iter->second);
}

bool hasAttributeAliasing(const vector<Attribute> &attributes, const map<string, uint32_t> &bindings)
{
    vector<bool> reservedSpaces;

    for (int attribNdx = 0; attribNdx < (int)attributes.size(); attribNdx++)
    {
        const int32_t location = getBoundLocation(bindings, attributes[attribNdx].getName());
        const uint32_t size    = attributes[attribNdx].getType().getLocationSize();

        if (location != Attribute::LOC_UNDEF)
        {
            if (reservedSpaces.size() < location + size)
                reservedSpaces.resize(location + size, false);

            for (int i = 0; i < (int)size; i++)
            {
                if (reservedSpaces[location + i])
                    return true;

                reservedSpaces[location + i] = true;
            }
        }
    }

    return false;
}

int32_t getMaxAttributeLocations(glu::RenderContext &renderCtx)
{
    const glw::Functions &gl = renderCtx.getFunctions();
    int32_t maxAttribs;

    gl.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

    return maxAttribs;
}

string generateAttributeDefinitions(const vector<Attribute> &attributes)
{
    std::ostringstream src;

    for (vector<Attribute>::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
    {
        if (iter->getLayoutLocation() != Attribute::LOC_UNDEF)
            src << "layout(location = " << iter->getLayoutLocation() << ") ";

        src << "${VTX_INPUT} mediump " << iter->getType().getName() << " " << iter->getName()
            << (iter->getArraySize() != Attribute::NOT_ARRAY ? "[" + de::toString(iter->getArraySize()) + "]" : "")
            << ";\n";
    }

    return src.str();
}

string generateConditionUniformDefinitions(const vector<Attribute> &attributes)
{
    std::ostringstream src;
    set<string> conditions;

    for (vector<Attribute>::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
    {
        if (iter->getCondition() != Cond::COND_NEVER && iter->getCondition() != Cond::COND_ALWAYS)
            conditions.insert(iter->getCondition().getName());
    }

    for (set<string>::const_iterator iter = conditions.begin(); iter != conditions.end(); ++iter)
        src << "uniform mediump float u_" << (*iter) << ";\n";

    return src.str();
}

string generateToVec4Expression(const Attribute &attrib, int id = -1)
{
    const string variableName(attrib.getName() +
                              (attrib.getArraySize() != Attribute::NOT_ARRAY ? "[" + de::toString(id) + "]" : ""));
    std::ostringstream src;

    switch (attrib.getType().getGLTypeEnum())
    {
    case GL_INT_VEC2:
    case GL_UNSIGNED_INT_VEC2:
    case GL_FLOAT_VEC2:
        src << "vec4(" << variableName << ".xy, " << variableName << ".yx)";
        break;

    case GL_INT_VEC3:
    case GL_UNSIGNED_INT_VEC3:
    case GL_FLOAT_VEC3:
        src << "vec4(" << variableName << ".xyz, " << variableName << ".x)";
        break;

    default:
        src << "vec4(" << variableName << ")";
        break;
    }

    return src.str();
}

string generateOutputCode(const vector<Attribute> &attributes)
{
    std::ostringstream src;

    for (vector<Attribute>::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
    {
        if (iter->getCondition() == Cond::COND_NEVER)
        {
            src << "\tif (0 != 0)\n"
                   "\t{\n";

            if (iter->getArraySize() == Attribute::NOT_ARRAY)
                src << "\t\tcolor += " << generateToVec4Expression(*iter) << ";\n";
            else
            {
                for (int i = 0; i < iter->getArraySize(); i++)
                    src << "\t\tcolor += " << generateToVec4Expression(*iter, i) << ";\n";
            }

            src << "\t}\n";
        }
        else if (iter->getCondition() == Cond::COND_ALWAYS)
        {
            if (iter->getArraySize() == Attribute::NOT_ARRAY)
                src << "\tcolor += " << generateToVec4Expression(*iter) << ";\n";
            else
            {
                for (int i = 0; i < iter->getArraySize(); i++)
                    src << "\tcolor += " << generateToVec4Expression(*iter, i) << ";\n";
            }
        }
        else
        {
            src << "\tif (u_" << iter->getCondition().getName() << (iter->getCondition().getNegate() ? " != " : " == ")
                << "0.0)\n"
                   "\t{\n";

            if (iter->getArraySize() == Attribute::NOT_ARRAY)
                src << "\t\tcolor += " << generateToVec4Expression(*iter) << ";\n";
            else
            {
                for (int i = 0; i < iter->getArraySize(); i++)
                    src << "\t\tcolor += " << generateToVec4Expression(*iter, i) << ";\n";
            }

            src << "\t}\n";
        }
    }

    return src.str();
}

string generateVertexShaderTemplate(const vector<Attribute> &attributes)
{
    std::ostringstream src;

    src << "${VERSION}\n"
           "${VTX_OUTPUT} mediump vec4 v_color;\n";

    src << generateAttributeDefinitions(attributes) << "\n" << generateConditionUniformDefinitions(attributes) << "\n";

    src << "void main (void)\n"
           "{\n"
           "\tmediump vec4 color = vec4(0.0);\n"
           "\n";

    src << generateOutputCode(attributes);

    src << "\n"
           "\tv_color = color;\n"
           "\tgl_Position = color;\n"
           "}\n";

    return src.str();
}

string createVertexShaderSource(glu::RenderContext &renderCtx, const vector<Attribute> &attributes,
                                bool attributeAliasing)
{
    // \note On GLES only GLSL #version 100 supports aliasing
    const glu::GLSLVersion contextGLSLVersion = glu::getContextTypeGLSLVersion(renderCtx.getType());
    const glu::GLSLVersion glslVersion =
        (attributeAliasing && glu::glslVersionIsES(contextGLSLVersion) ? glu::GLSL_VERSION_100_ES : contextGLSLVersion);
    const bool usesInOutQualifiers = glu::glslVersionUsesInOutQualifiers(glslVersion);
    const tcu::StringTemplate vertexShaderTemplate(generateVertexShaderTemplate(attributes));

    map<string, string> parameters;

    parameters["VERSION"]         = glu::getGLSLVersionDeclaration(glslVersion);
    parameters["VTX_OUTPUT"]      = (usesInOutQualifiers ? "out" : "varying");
    parameters["VTX_INPUT"]       = (usesInOutQualifiers ? "in" : "attribute");
    parameters["FRAG_INPUT"]      = (usesInOutQualifiers ? "in" : "varying");
    parameters["FRAG_OUTPUT_VAR"] = (usesInOutQualifiers ? "dEQP_FragColor" : "gl_FragColor");
    parameters["FRAG_OUTPUT_DECLARATION"] =
        (usesInOutQualifiers ? "layout(location=0) out mediump vec4 dEQP_FragColor;" : "");

    return vertexShaderTemplate.specialize(parameters);
}

string createFragmentShaderSource(glu::RenderContext &renderCtx, bool attributeAliasing)
{
    const char *const fragmentShaderSource = "${VERSION}\n"
                                             "${FRAG_OUTPUT_DECLARATION}\n"
                                             "${FRAG_INPUT} mediump vec4 v_color;\n"
                                             "void main (void)\n"
                                             "{\n"
                                             "\t${FRAG_OUTPUT_VAR} = v_color;\n"
                                             "}\n";

    // \note On GLES only GLSL #version 100 supports aliasing
    const glu::GLSLVersion contextGLSLVersion = glu::getContextTypeGLSLVersion(renderCtx.getType());
    const glu::GLSLVersion glslVersion =
        (attributeAliasing && glu::glslVersionIsES(contextGLSLVersion) ? glu::GLSL_VERSION_100_ES : contextGLSLVersion);
    const tcu::StringTemplate fragmentShaderTemplate(fragmentShaderSource);
    const bool usesInOutQualifiers = glu::glslVersionUsesInOutQualifiers(glslVersion);

    map<string, string> parameters;

    parameters["VERSION"]         = glu::getGLSLVersionDeclaration(glslVersion);
    parameters["VTX_OUTPUT"]      = (usesInOutQualifiers ? "out" : "varying");
    parameters["VTX_INPUT"]       = (usesInOutQualifiers ? "in" : "attribute");
    parameters["FRAG_INPUT"]      = (usesInOutQualifiers ? "in" : "varying");
    parameters["FRAG_OUTPUT_VAR"] = (usesInOutQualifiers ? "dEQP_FragColor" : "gl_FragColor");
    parameters["FRAG_OUTPUT_DECLARATION"] =
        (usesInOutQualifiers ? "layout(location=0) out mediump vec4 dEQP_FragColor;" : "");

    return fragmentShaderTemplate.specialize(parameters);
}

string getShaderInfoLog(const glw::Functions &gl, uint32_t shader)
{
    int32_t length = 0;
    string infoLog;

    gl.getShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetShaderiv()");

    infoLog.resize(length, '\0');

    gl.getShaderInfoLog(shader, (glw::GLsizei)infoLog.length(), nullptr, &(infoLog[0]));
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetShaderInfoLog()");

    return infoLog;
}

bool getShaderCompileStatus(const glw::Functions &gl, uint32_t shader)
{
    int32_t status;

    gl.getShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetShaderiv()");

    return status == GL_TRUE;
}

string getProgramInfoLog(const glw::Functions &gl, uint32_t program)
{
    int32_t length = 0;
    string infoLog;

    gl.getProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv()");

    infoLog.resize(length, '\0');

    gl.getProgramInfoLog(program, (glw::GLsizei)infoLog.length(), nullptr, &(infoLog[0]));
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramInfoLog()");

    return infoLog;
}

bool getProgramLinkStatus(const glw::Functions &gl, uint32_t program)
{
    int32_t status;

    gl.getProgramiv(program, GL_LINK_STATUS, &status);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv()");

    return status == GL_TRUE;
}

void logProgram(TestLog &log, const glw::Functions &gl, uint32_t program)
{
    const bool programLinkOk    = getProgramLinkStatus(gl, program);
    const string programInfoLog = getProgramInfoLog(gl, program);
    tcu::ScopedLogSection linkInfo(log, "Program Link Info", "Program Link Info");

    {
        tcu::ScopedLogSection infoLogSection(log, "Info Log", "Info Log");

        log << TestLog::Message << programInfoLog << TestLog::EndMessage;
    }

    log << TestLog::Message << "Link result: " << (programLinkOk ? "Ok" : "Fail") << TestLog::EndMessage;
}

void logShaders(TestLog &log, const string &vertexShaderSource, const string &vertexShaderInfoLog, bool vertexCompileOk,
                const string &fragmentShaderSource, const string &fragmentShaderInfoLog, bool fragmentCompileOk)
{
    // \todo [mika] Log as real shader elements. Currently not supported by TestLog.
    {
        tcu::ScopedLogSection shaderSection(log, "Vertex Shader Info", "Vertex Shader Info");

        log << TestLog::KernelSource(vertexShaderSource);

        {
            tcu::ScopedLogSection infoLogSection(log, "Info Log", "Info Log");

            log << TestLog::Message << vertexShaderInfoLog << TestLog::EndMessage;
        }

        log << TestLog::Message << "Compilation result: " << (vertexCompileOk ? "Ok" : "Failed") << TestLog::EndMessage;
    }

    {
        tcu::ScopedLogSection shaderSection(log, "Fragment Shader Info", "Fragment Shader Info");

        log << TestLog::KernelSource(fragmentShaderSource);

        {
            tcu::ScopedLogSection infoLogSection(log, "Info Log", "Info Log");

            log << TestLog::Message << fragmentShaderInfoLog << TestLog::EndMessage;
        }

        log << TestLog::Message << "Compilation result: " << (fragmentCompileOk ? "Ok" : "Failed")
            << TestLog::EndMessage;
    }
}

pair<uint32_t, uint32_t> createAndAttachShaders(TestLog &log, glu::RenderContext &renderCtx, uint32_t program,
                                                const vector<Attribute> &attributes, bool attributeAliasing)
{
    const glw::Functions &gl          = renderCtx.getFunctions();
    const string vertexShaderSource   = createVertexShaderSource(renderCtx, attributes, attributeAliasing);
    const string fragmentShaderSource = createFragmentShaderSource(renderCtx, attributeAliasing);

    const uint32_t vertexShader   = gl.createShader(GL_VERTEX_SHADER);
    const uint32_t fragmentShader = gl.createShader(GL_FRAGMENT_SHADER);

    try
    {
        GLU_EXPECT_NO_ERROR(gl.getError(), "glCreateShader()");

        {
            const char *const vertexShaderString   = vertexShaderSource.c_str();
            const char *const fragmentShaderString = fragmentShaderSource.c_str();

            gl.shaderSource(vertexShader, 1, &vertexShaderString, nullptr);
            gl.shaderSource(fragmentShader, 1, &fragmentShaderString, nullptr);

            GLU_EXPECT_NO_ERROR(gl.getError(), "glShaderSource()");
        }

        gl.compileShader(vertexShader);
        gl.compileShader(fragmentShader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glCompileShader()");

        gl.attachShader(program, vertexShader);
        gl.attachShader(program, fragmentShader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glAttachShader()");

        {
            const bool vertexCompileOk   = getShaderCompileStatus(gl, vertexShader);
            const bool fragmentCompileOk = getShaderCompileStatus(gl, fragmentShader);

            const string vertexShaderInfoLog   = getShaderInfoLog(gl, vertexShader);
            const string fragmentShaderInfoLog = getShaderInfoLog(gl, fragmentShader);

            logShaders(log, vertexShaderSource, vertexShaderInfoLog, vertexCompileOk, fragmentShaderSource,
                       fragmentShaderInfoLog, fragmentCompileOk);

            TCU_CHECK_MSG(vertexCompileOk, "Vertex shader compilation failed");
            TCU_CHECK_MSG(fragmentCompileOk, "Fragment shader compilation failed");
        }

        gl.deleteShader(vertexShader);
        gl.deleteShader(fragmentShader);

        return pair<uint32_t, uint32_t>(vertexShader, fragmentShader);
    }
    catch (...)
    {
        if (vertexShader != 0)
            gl.deleteShader(vertexShader);

        if (fragmentShader != 0)
            gl.deleteShader(fragmentShader);

        throw;
    }
}

void bindAttributes(TestLog &log, const glw::Functions &gl, uint32_t program, const vector<Bind> &binds)
{
    for (vector<Bind>::const_iterator iter = binds.begin(); iter != binds.end(); ++iter)
    {
        log << TestLog::Message << "Bind attribute: '" << iter->getAttributeName() << "' to " << iter->getLocation()
            << TestLog::EndMessage;
        gl.bindAttribLocation(program, iter->getLocation(), iter->getAttributeName().c_str());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindAttribLocation()");
    }
}

void logAttributes(TestLog &log, const vector<Attribute> &attributes)
{
    for (int attribNdx = 0; attribNdx < (int)attributes.size(); attribNdx++)
    {
        const Attribute &attrib = attributes[attribNdx];

        log << TestLog::Message << "Type: " << attrib.getType().getName() << ", Name: " << attrib.getName()
            << (attrib.getLayoutLocation() != Attribute::LOC_UNDEF ?
                    ", Layout location " + de::toString(attrib.getLayoutLocation()) :
                    "")
            << TestLog::EndMessage;
    }
}

bool checkActiveAttribQuery(TestLog &log, const glw::Functions &gl, uint32_t program,
                            const vector<Attribute> &attributes)
{
    int32_t activeAttribCount = 0;
    set<string> activeAttributes;
    bool isOk = true;

    gl.getProgramiv(program, GL_ACTIVE_ATTRIBUTES, &activeAttribCount);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &activeAttribCount)");

    for (int activeAttribNdx = 0; activeAttribNdx < activeAttribCount; activeAttribNdx++)
    {
        char name[128];
        const size_t maxNameSize = DE_LENGTH_OF_ARRAY(name) - 1;
        int32_t length           = 0;
        int32_t size             = 0;
        uint32_t type            = 0;

        std::memset(name, 0, sizeof(name));

        gl.getActiveAttrib(program, activeAttribNdx, maxNameSize, &length, &size, &type, name);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetActiveAttrib()");

        log << TestLog::Message << "glGetActiveAttrib(program"
            << ", index=" << activeAttribNdx << ", bufSize=" << maxNameSize << ", length=" << length
            << ", size=" << size << ", type=" << glu::getShaderVarTypeStr(type) << ", name='" << name << "')"
            << TestLog::EndMessage;

        {
            bool found = false;

            for (int attribNdx = 0; attribNdx < (int)attributes.size(); attribNdx++)
            {
                const Attribute &attrib = attributes[attribNdx];

                if (attrib.getName() == name)
                {
                    if (type != attrib.getType().getGLTypeEnum())
                    {
                        log << TestLog::Message << "Error: Wrong type " << glu::getShaderVarTypeStr(type)
                            << " expected " << glu::getShaderVarTypeStr(attrib.getType().getGLTypeEnum())
                            << TestLog::EndMessage;

                        isOk = false;
                    }

                    if (attrib.getArraySize() == Attribute::NOT_ARRAY)
                    {
                        if (size != 1)
                        {
                            log << TestLog::Message << "Error: Wrong size " << size << " expected " << 1
                                << TestLog::EndMessage;
                            isOk = false;
                        }
                    }
                    else
                    {
                        if (size != attrib.getArraySize())
                        {
                            log << TestLog::Message << "Error: Wrong size " << size << " expected "
                                << attrib.getArraySize() << TestLog::EndMessage;
                            isOk = false;
                        }
                    }

                    found = true;
                    break;
                }
            }

            if (!found)
            {
                log << TestLog::Message << "Error: Unknown attribute '" << name << "' returned by glGetActiveAttrib()."
                    << TestLog::EndMessage;
                isOk = false;
            }
        }

        activeAttributes.insert(name);
    }

    for (int attribNdx = 0; attribNdx < (int)attributes.size(); attribNdx++)
    {
        const Attribute &attrib = attributes[attribNdx];
        const bool isActive     = attrib.getCondition() != Cond::COND_NEVER;

        if (isActive)
        {
            if (activeAttributes.find(attrib.getName()) == activeAttributes.end())
            {
                log << TestLog::Message << "Error: Active attribute " << attrib.getName()
                    << " wasn't returned by glGetActiveAttrib()." << TestLog::EndMessage;
                isOk = false;
            }
        }
        else
        {
            if (activeAttributes.find(attrib.getName()) != activeAttributes.end())
                log << TestLog::Message << "Note: Inactive attribute " << attrib.getName()
                    << " was returned by glGetActiveAttrib()." << TestLog::EndMessage;
        }
    }

    return isOk;
}

bool checkAttribLocationQuery(TestLog &log, const glw::Functions &gl, uint32_t program,
                              const vector<Attribute> &attributes, const map<string, uint32_t> &bindings)
{
    bool isOk = true;

    for (int attribNdx = 0; attribNdx < (int)attributes.size(); attribNdx++)
    {
        const Attribute &attrib = attributes[attribNdx];
        const int32_t expectedLocation =
            (attrib.getLayoutLocation() != Attribute::LOC_UNDEF ? attrib.getLayoutLocation() :
                                                                  getBoundLocation(bindings, attrib.getName()));
        const int32_t location = gl.getAttribLocation(program, attrib.getName().c_str());

        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

        log << TestLog::Message << location << " = glGetAttribLocation(program, \"" << attrib.getName() << "\")"
            << (attrib.getCondition() != Cond::COND_NEVER && expectedLocation != Attribute::LOC_UNDEF ?
                    ", expected " + de::toString(expectedLocation) :
                    "")
            << "." << TestLog::EndMessage;

        if (attrib.getCondition() == Cond::COND_NEVER && location != -1)
            log << TestLog::Message << "\tNote: Inactive attribute with location." << TestLog::EndMessage;

        if (attrib.getCondition() != Cond::COND_NEVER && expectedLocation != Attribute::LOC_UNDEF &&
            expectedLocation != location)
            log << TestLog::Message << "\tError: Invalid attribute location." << TestLog::EndMessage;

        isOk &= (attrib.getCondition() == Cond::COND_NEVER || expectedLocation == Attribute::LOC_UNDEF ||
                 expectedLocation == location);
    }

    return isOk;
}

bool checkQuery(TestLog &log, const glw::Functions &gl, uint32_t program, const vector<Attribute> &attributes,
                const map<string, uint32_t> &bindings)
{
    bool isOk = checkActiveAttribQuery(log, gl, program, attributes);

    if (!checkAttribLocationQuery(log, gl, program, attributes, bindings))
        isOk = false;

    return isOk;
}

string generateTestName(const AttribType &type, int arraySize)
{
    return type.getName() + (arraySize != Attribute::NOT_ARRAY ? "_array_" + de::toString(arraySize) : "");
}

} // namespace

namespace AttributeLocationTestUtil
{

AttribType::AttribType(const string &name, uint32_t localSize, uint32_t typeEnum)
    : m_name(name)
    , m_locationSize(localSize)
    , m_glTypeEnum(typeEnum)
{
}

Cond::Cond(const string &name, bool negate) : m_negate(negate), m_name(name)
{
}

Cond::Cond(ConstCond cond) : m_negate(cond != COND_NEVER), m_name("__always__")
{
    DE_ASSERT(cond == COND_ALWAYS || cond == COND_NEVER);
}

Attribute::Attribute(const AttribType &type, const string &name, int32_t layoutLocation, const Cond &cond,
                     int arraySize)
    : m_type(type)
    , m_name(name)
    , m_layoutLocation(layoutLocation)
    , m_cond(cond)
    , m_arraySize(arraySize)
{
}

Bind::Bind(const std::string &attribute, uint32_t location) : m_attribute(attribute), m_location(location)
{
}

void runTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const vector<Attribute> &attributes,
             const vector<Bind> &preAttachBind, const vector<Bind> &preLinkBind, const vector<Bind> &postLinkBind,
             bool relink, bool reattach = false, const vector<Attribute> &reattachAttributes = vector<Attribute>())
{
    TestLog &log             = testCtx.getLog();
    const glw::Functions &gl = renderCtx.getFunctions();
    uint32_t program         = 0;
    pair<uint32_t, uint32_t> shaders;

    try
    {
        bool isOk = true;
        map<string, uint32_t> activeBindings;

        for (int bindNdx = 0; bindNdx < (int)preAttachBind.size(); bindNdx++)
            activeBindings[preAttachBind[bindNdx].getAttributeName()] = preAttachBind[bindNdx].getLocation();

        for (int bindNdx = 0; bindNdx < (int)preLinkBind.size(); bindNdx++)
            activeBindings[preLinkBind[bindNdx].getAttributeName()] = preLinkBind[bindNdx].getLocation();

        {
            tcu::ScopedLogSection section(log, "Attributes", "Attribute information");
            logAttributes(testCtx.getLog(), attributes);
        }

        log << TestLog::Message << "Create program." << TestLog::EndMessage;
        program = gl.createProgram();
        GLU_EXPECT_NO_ERROR(gl.getError(), "glCreateProgram()");

        if (!preAttachBind.empty())
            bindAttributes(log, gl, program, preAttachBind);

        log << TestLog::Message << "Create and attach shaders to program." << TestLog::EndMessage;
        shaders = createAndAttachShaders(log, renderCtx, program, attributes,
                                         hasAttributeAliasing(attributes, activeBindings));

        if (!preLinkBind.empty())
            bindAttributes(log, gl, program, preLinkBind);

        log << TestLog::Message << "Link program." << TestLog::EndMessage;

        gl.linkProgram(program);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glLinkProgram()");

        logProgram(log, gl, program);
        TCU_CHECK_MSG(getProgramLinkStatus(gl, program), "Program link failed");

        if (!checkQuery(log, gl, program, attributes, activeBindings))
            isOk = false;

        if (!postLinkBind.empty())
        {
            bindAttributes(log, gl, program, postLinkBind);

            if (!checkQuery(log, gl, program, attributes, activeBindings))
                isOk = false;
        }

        if (relink)
        {
            log << TestLog::Message << "Relink program." << TestLog::EndMessage;
            gl.linkProgram(program);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glLinkProgram()");

            logProgram(log, gl, program);
            TCU_CHECK_MSG(getProgramLinkStatus(gl, program), "Program link failed");

            for (int bindNdx = 0; bindNdx < (int)postLinkBind.size(); bindNdx++)
                activeBindings[postLinkBind[bindNdx].getAttributeName()] = postLinkBind[bindNdx].getLocation();

            if (!checkQuery(log, gl, program, attributes, activeBindings))
                isOk = false;
        }

        if (reattach)
        {
            gl.detachShader(program, shaders.first);
            gl.detachShader(program, shaders.second);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glDetachShader()");

            log << TestLog::Message << "Create and attach shaders to program." << TestLog::EndMessage;
            createAndAttachShaders(log, renderCtx, program, reattachAttributes,
                                   hasAttributeAliasing(reattachAttributes, activeBindings));

            log << TestLog::Message << "Relink program." << TestLog::EndMessage;
            gl.linkProgram(program);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glLinkProgram()");

            logProgram(log, gl, program);
            TCU_CHECK_MSG(getProgramLinkStatus(gl, program), "Program link failed");

            if (!checkQuery(log, gl, program, reattachAttributes, activeBindings))
                isOk = false;
        }

        gl.deleteProgram(program);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteProgram()");

        if (isOk)
            testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }
    catch (...)
    {
        if (program)
            gl.deleteProgram(program);

        throw;
    }
}

} // namespace AttributeLocationTestUtil

BindAttributeTest::BindAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                                     int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult BindAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;
    vector<Bind> bindings;

    attributes.push_back(Attribute(m_type, "a_0", Attribute::LOC_UNDEF, Cond::COND_ALWAYS, m_arraySize));
    bindings.push_back(Bind("a_0", 3));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

BindMaxAttributesTest::BindMaxAttributesTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                             const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult BindMaxAttributesTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Attribute> attributes;
    vector<Bind> bindings;
    int ndx = 0;

    m_testCtx.getLog() << TestLog::Message << "GL_MAX_VERTEX_ATTRIBS: " << maxAttributes << TestLog::EndMessage;

    for (int loc = maxAttributes - (arrayElementCount * m_type.getLocationSize()); loc >= 0;
         loc -= (arrayElementCount * m_type.getLocationSize()))
    {
        attributes.push_back(
            Attribute(m_type, "a_" + de::toString(ndx), Attribute::LOC_UNDEF, Cond::COND_ALWAYS, m_arraySize));
        bindings.push_back(Bind("a_" + de::toString(ndx), loc));
        ndx++;
    }

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

BindAliasingAttributeTest::BindAliasingAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                     const AttribType &type, int offset, int arraySize)
    : TestCase(testCtx,
               ("cond_" + generateTestName(type, arraySize) + (offset != 0 ? "_offset_" + de::toString(offset) : ""))
                   .c_str(),
               ("cond_" + generateTestName(type, arraySize) + (offset != 0 ? "_offset_" + de::toString(offset) : ""))
                   .c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_offset(offset)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult BindAliasingAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;
    vector<Bind> bindings;

    attributes.push_back(Attribute(m_type, "a_0", Attribute::LOC_UNDEF, Cond("A", true), m_arraySize));
    attributes.push_back(
        Attribute(AttribType("vec4", 1, GL_FLOAT_VEC4), "a_1", Attribute::LOC_UNDEF, Cond("A", false)));
    bindings.push_back(Bind("a_0", 1));
    bindings.push_back(Bind("a_1", 1 + m_offset));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

BindMaxAliasingAttributeTest::BindMaxAliasingAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                           const AttribType &type, int arraySize)
    : TestCase(testCtx, ("max_cond_" + generateTestName(type, arraySize)).c_str(),
               ("max_cond_" + generateTestName(type, arraySize)).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult BindMaxAliasingAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Attribute> attributes;
    vector<Bind> bindings;
    int ndx = 0;

    m_testCtx.getLog() << TestLog::Message << "GL_MAX_VERTEX_ATTRIBS: " << maxAttributes << TestLog::EndMessage;

    for (int loc = maxAttributes - arrayElementCount * m_type.getLocationSize(); loc >= 0;
         loc -= m_type.getLocationSize() * arrayElementCount)
    {
        attributes.push_back(Attribute(m_type, "a_" + de::toString(ndx), Attribute::LOC_UNDEF, Cond("A", true)));
        bindings.push_back(Bind("a_" + de::toString(ndx), loc));

        attributes.push_back(
            Attribute(m_type, "a_" + de::toString(ndx + maxAttributes), Attribute::LOC_UNDEF, Cond("A", false)));
        bindings.push_back(Bind("a_" + de::toString(ndx + maxAttributes), loc));
        ndx++;
    }

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

BindHoleAttributeTest::BindHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                             const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult BindHoleAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Attribute> attributes;
    vector<Bind> bindings;
    int ndx;

    attributes.push_back(Attribute(vec4, "a_0"));
    bindings.push_back(Bind("a_0", 0));

    attributes.push_back(Attribute(m_type, "a_1", Attribute::LOC_UNDEF, Cond::COND_ALWAYS, m_arraySize));

    ndx = 2;
    for (int loc = 1 + m_type.getLocationSize() * arrayElementCount; loc < maxAttributes; loc++)
    {
        attributes.push_back(Attribute(vec4, "a_" + de::toString(ndx)));
        bindings.push_back(Bind("a_" + de::toString(ndx), loc));

        ndx++;
    }

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

BindInactiveAliasingAttributeTest::BindInactiveAliasingAttributeTest(tcu::TestContext &testCtx,
                                                                     glu::RenderContext &renderCtx,
                                                                     const AttribType &type, int arraySize)
    : TestCase(testCtx, ("max_inactive_" + generateTestName(type, arraySize)).c_str(),
               ("max_inactive_" + generateTestName(type, arraySize)).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult BindInactiveAliasingAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Attribute> attributes;
    vector<Bind> bindings;
    int ndx = 0;

    m_testCtx.getLog() << TestLog::Message << "GL_MAX_VERTEX_ATTRIBS: " << maxAttributes << TestLog::EndMessage;

    for (int loc = maxAttributes - arrayElementCount * m_type.getLocationSize(); loc >= 0;
         loc -= m_type.getLocationSize() * arrayElementCount)
    {
        attributes.push_back(Attribute(m_type, "a_" + de::toString(ndx), Attribute::LOC_UNDEF, Cond("A")));
        bindings.push_back(Bind("a_" + de::toString(ndx), loc));

        attributes.push_back(
            Attribute(m_type, "a_" + de::toString(ndx + maxAttributes), Attribute::LOC_UNDEF, Cond::COND_NEVER));
        bindings.push_back(Bind("a_" + de::toString(ndx + maxAttributes), loc));
        ndx++;
    }

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

PreAttachBindAttributeTest::PreAttachBindAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "pre_attach", "pre_attach")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult PreAttachBindAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;
    vector<Bind> bindings;

    attributes.push_back(Attribute(AttribType("vec4", 1, GL_FLOAT_VEC4), "a_0"));
    bindings.push_back(Bind("a_0", 3));

    runTest(m_testCtx, m_renderCtx, attributes, bindings, noBindings, noBindings, false);
    return STOP;
}

PreLinkBindAttributeTest::PreLinkBindAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "pre_link", "pre_link")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult PreLinkBindAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;
    vector<Bind> bindings;

    attributes.push_back(Attribute(AttribType("vec4", 1, GL_FLOAT_VEC4), "a_0"));
    bindings.push_back(Bind("a_0", 3));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

PostLinkBindAttributeTest::PostLinkBindAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "post_link", "post_link")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult PostLinkBindAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;
    vector<Bind> bindings;

    attributes.push_back(Attribute(AttribType("vec4", 1, GL_FLOAT_VEC4), "a_0"));
    bindings.push_back(Bind("a_0", 3));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, noBindings, bindings, false);
    return STOP;
}

LocationAttributeTest::LocationAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                             const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult LocationAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;

    attributes.push_back(Attribute(m_type, "a_0", 3, Cond::COND_ALWAYS, m_arraySize));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, noBindings, noBindings, false);
    return STOP;
}

LocationMaxAttributesTest::LocationMaxAttributesTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                     const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult LocationMaxAttributesTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Attribute> attributes;
    int ndx = 0;

    m_testCtx.getLog() << TestLog::Message << "GL_MAX_VERTEX_ATTRIBS: " << maxAttributes << TestLog::EndMessage;

    for (int loc = maxAttributes - (arrayElementCount * m_type.getLocationSize()); loc >= 0;
         loc -= (arrayElementCount * m_type.getLocationSize()))
    {
        attributes.push_back(Attribute(m_type, "a_" + de::toString(ndx), loc, Cond::COND_ALWAYS, m_arraySize));
        ndx++;
    }

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, noBindings, noBindings, false);
    return STOP;
}

LocationHoleAttributeTest::LocationHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                     const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult LocationHoleAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Attribute> attributes;
    int ndx;

    attributes.push_back(Attribute(vec4, "a_0", 0));

    attributes.push_back(Attribute(m_type, "a_1", Attribute::LOC_UNDEF, Cond::COND_ALWAYS, m_arraySize));

    ndx = 2;
    for (int loc = 1 + m_type.getLocationSize() * arrayElementCount; loc < maxAttributes; loc++)
    {
        attributes.push_back(Attribute(vec4, "a_" + de::toString(ndx), loc));
        ndx++;
    }

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, noBindings, noBindings, false);
    return STOP;
}

MixedAttributeTest::MixedAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                                       int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult MixedAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Bind> bindings;
    vector<Attribute> attributes;

    attributes.push_back(Attribute(m_type, "a_0", 3, Cond::COND_ALWAYS, m_arraySize));
    bindings.push_back(Bind("a_0", 4));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

MixedMaxAttributesTest::MixedMaxAttributesTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult MixedMaxAttributesTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Bind> bindings;
    vector<Attribute> attributes;
    int ndx = 0;

    m_testCtx.getLog() << TestLog::Message << "GL_MAX_VERTEX_ATTRIBS: " << maxAttributes << TestLog::EndMessage;

    for (int loc = maxAttributes - (arrayElementCount * m_type.getLocationSize()); loc >= 0;
         loc -= (arrayElementCount * m_type.getLocationSize()))
    {
        if ((ndx % 2) != 0)
            attributes.push_back(Attribute(m_type, "a_" + de::toString(ndx), loc, Cond::COND_ALWAYS, m_arraySize));
        else
        {
            attributes.push_back(
                Attribute(m_type, "a_" + de::toString(ndx), Attribute::LOC_UNDEF, Cond::COND_ALWAYS, m_arraySize));
            bindings.push_back(Bind("a_" + de::toString(ndx), loc));
        }
        ndx++;
    }

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

MixedHoleAttributeTest::MixedHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult MixedHoleAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Bind> bindings;
    vector<Attribute> attributes;
    int ndx;

    attributes.push_back(Attribute(vec4, "a_0"));
    bindings.push_back(Bind("a_0", 0));

    attributes.push_back(Attribute(m_type, "a_1", Attribute::LOC_UNDEF, Cond::COND_ALWAYS, m_arraySize));

    ndx = 2;
    for (int loc = 1 + m_type.getLocationSize() * arrayElementCount; loc < maxAttributes; loc++)
    {
        if ((ndx % 2) != 0)
            attributes.push_back(Attribute(vec4, "a_" + de::toString(ndx), loc));
        else
        {
            attributes.push_back(Attribute(vec4, "a_" + de::toString(ndx), loc));
            bindings.push_back(Bind("a_" + de::toString(ndx), loc));
        }
        ndx++;
    }

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

BindRelinkAttributeTest::BindRelinkAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "relink", "relink")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult BindRelinkAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);

    vector<Attribute> attributes;
    vector<Bind> preLinkBindings;
    vector<Bind> postLinkBindings;

    attributes.push_back(Attribute(vec4, "a_0"));
    attributes.push_back(Attribute(vec4, "a_1"));

    preLinkBindings.push_back(Bind("a_0", 3));
    preLinkBindings.push_back(Bind("a_0", 5));

    postLinkBindings.push_back(Bind("a_0", 6));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, preLinkBindings, postLinkBindings, true);
    return STOP;
}

BindRelinkHoleAttributeTest::BindRelinkHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                         const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult BindRelinkHoleAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Attribute> attributes;
    vector<Bind> preLinkBindings;
    vector<Bind> postLinkBindings;
    int ndx;

    attributes.push_back(Attribute(vec4, "a_0"));
    preLinkBindings.push_back(Bind("a_0", 0));

    attributes.push_back(Attribute(m_type, "a_1", Attribute::LOC_UNDEF, Cond::COND_ALWAYS, m_arraySize));

    ndx = 2;
    for (int loc = 1 + m_type.getLocationSize() * arrayElementCount; loc < maxAttributes; loc++)
    {
        attributes.push_back(Attribute(vec4, "a_" + de::toString(ndx)));
        preLinkBindings.push_back(Bind("a_" + de::toString(ndx), loc));

        ndx++;
    }

    postLinkBindings.push_back(Bind("a_2", 1));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, preLinkBindings, postLinkBindings, true);
    return STOP;
}

MixedRelinkHoleAttributeTest::MixedRelinkHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                           const AttribType &type, int arraySize)
    : TestCase(testCtx, generateTestName(type, arraySize).c_str(), generateTestName(type, arraySize).c_str())
    , m_renderCtx(renderCtx)
    , m_type(type)
    , m_arraySize(arraySize)
{
}

tcu::TestCase::IterateResult MixedRelinkHoleAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const int32_t maxAttributes = getMaxAttributeLocations(m_renderCtx);
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);
    const int arrayElementCount = (m_arraySize != Attribute::NOT_ARRAY ? m_arraySize : 1);

    vector<Bind> preLinkBindings;
    vector<Bind> postLinkBindings;
    vector<Attribute> attributes;
    int ndx;

    attributes.push_back(Attribute(vec4, "a_0"));
    preLinkBindings.push_back(Bind("a_0", 0));

    attributes.push_back(Attribute(m_type, "a_1", Attribute::LOC_UNDEF, Cond::COND_ALWAYS, m_arraySize));

    ndx = 2;
    for (int loc = 1 + m_type.getLocationSize() * arrayElementCount; loc < maxAttributes; loc++)
    {
        if ((ndx % 2) != 0)
            attributes.push_back(Attribute(vec4, "a_" + de::toString(ndx), loc));
        else
        {
            attributes.push_back(Attribute(vec4, "a_" + de::toString(ndx)));
            preLinkBindings.push_back(Bind("a_" + de::toString(ndx), loc));
        }
        ndx++;
    }

    postLinkBindings.push_back(Bind("a_2", 1));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, preLinkBindings, postLinkBindings, true);
    return STOP;
}

BindReattachAttributeTest::BindReattachAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "reattach", "reattach")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult BindReattachAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);
    const AttribType vec2("vec2", 1, GL_FLOAT_VEC2);

    vector<Bind> bindings;
    vector<Attribute> attributes;
    vector<Attribute> reattachAttributes;

    attributes.push_back(Attribute(vec4, "a_0"));
    bindings.push_back(Bind("a_0", 1));
    bindings.push_back(Bind("a_1", 1));

    reattachAttributes.push_back(Attribute(vec2, "a_1"));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false, true, reattachAttributes);
    return STOP;
}

PreAttachMixedAttributeTest::PreAttachMixedAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "pre_attach", "pre_attach")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult PreAttachMixedAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;
    vector<Bind> bindings;

    attributes.push_back(Attribute(AttribType("vec4", 1, GL_FLOAT_VEC4), "a_0", 1));
    bindings.push_back(Bind("a_0", 3));

    runTest(m_testCtx, m_renderCtx, attributes, bindings, noBindings, noBindings, false);
    return STOP;
}

PreLinkMixedAttributeTest::PreLinkMixedAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "pre_link", "pre_link")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult PreLinkMixedAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;
    vector<Bind> bindings;

    attributes.push_back(Attribute(AttribType("vec4", 1, GL_FLOAT_VEC4), "a_0", 1));
    bindings.push_back(Bind("a_0", 3));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false);
    return STOP;
}

PostLinkMixedAttributeTest::PostLinkMixedAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "post_link", "post_link")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult PostLinkMixedAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;

    vector<Attribute> attributes;
    vector<Bind> bindings;

    attributes.push_back(Attribute(AttribType("vec4", 1, GL_FLOAT_VEC4), "a_0", 1));
    bindings.push_back(Bind("a_0", 3));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, noBindings, bindings, false);
    return STOP;
}

MixedReattachAttributeTest::MixedReattachAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "reattach", "reattach")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult MixedReattachAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);
    const AttribType vec2("vec2", 1, GL_FLOAT_VEC2);

    vector<Bind> bindings;
    vector<Attribute> attributes;
    vector<Attribute> reattachAttributes;

    attributes.push_back(Attribute(vec4, "a_0", 2));
    bindings.push_back(Bind("a_0", 1));
    bindings.push_back(Bind("a_1", 1));

    reattachAttributes.push_back(Attribute(vec2, "a_1"));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, bindings, noBindings, false, true, reattachAttributes);
    return STOP;
}

MixedRelinkAttributeTest::MixedRelinkAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx)
    : TestCase(testCtx, "relink", "relink")
    , m_renderCtx(renderCtx)
{
}

tcu::TestCase::IterateResult MixedRelinkAttributeTest::iterate(void)
{
    const vector<Bind> noBindings;
    const AttribType vec4("vec4", 1, GL_FLOAT_VEC4);

    vector<Attribute> attributes;
    vector<Bind> preLinkBindings;
    vector<Bind> postLinkBindings;

    attributes.push_back(Attribute(vec4, "a_0", 1));
    attributes.push_back(Attribute(vec4, "a_1"));

    preLinkBindings.push_back(Bind("a_0", 3));
    preLinkBindings.push_back(Bind("a_0", 5));

    postLinkBindings.push_back(Bind("a_0", 6));

    runTest(m_testCtx, m_renderCtx, attributes, noBindings, preLinkBindings, postLinkBindings, true);
    return STOP;
}

} // namespace gls
} // namespace deqp

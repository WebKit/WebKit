/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief State Query test utils.
 *//*--------------------------------------------------------------------*/
#include "glsStateQueryUtil.hpp"
#include "tcuTestContext.hpp"
#include "tcuFormatUtil.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "deStringUtil.hpp"

#include <cmath>

namespace deqp
{
namespace gls
{
namespace StateQueryUtil
{

static glw::GLboolean mapBoolToGLBoolean(bool b)
{
    return (b ? GL_TRUE : GL_FALSE);
}

static bool checkError(tcu::ResultCollector &result, glu::CallLogWrapper &gl, const char *msg)
{
    const glw::GLenum errorCode = gl.glGetError();

    if (errorCode == GL_NO_ERROR)
        return true;

    result.fail(std::string(msg) + ": glGetError() returned " + glu::getErrorStr(errorCode).toString());
    return false;
}

QueriedState::QueriedState(void) : m_type(DATATYPE_LAST)
{
}

QueriedState::QueriedState(glw::GLint v) : m_type(DATATYPE_INTEGER)
{
    m_v.vInt = v;
}

QueriedState::QueriedState(glw::GLint64 v) : m_type(DATATYPE_INTEGER64)
{
    m_v.vInt64 = v;
}

QueriedState::QueriedState(bool v) : m_type(DATATYPE_BOOLEAN)
{
    m_v.vBool = v;
}

QueriedState::QueriedState(glw::GLfloat v) : m_type(DATATYPE_FLOAT)
{
    m_v.vFloat = v;
}

QueriedState::QueriedState(glw::GLuint v) : m_type(DATATYPE_UNSIGNED_INTEGER)
{
    m_v.vUint = v;
}

QueriedState::QueriedState(const GLIntVec3 &v) : m_type(DATATYPE_INTEGER_VEC3)
{
    m_v.vIntVec3[0] = v[0];
    m_v.vIntVec3[1] = v[1];
    m_v.vIntVec3[2] = v[2];
}

QueriedState::QueriedState(void *v) : m_type(DATATYPE_POINTER)
{
    m_v.vPtr = v;
}

QueriedState::QueriedState(const GLIntVec4 &v) : m_type(DATATYPE_INTEGER_VEC4)
{
    m_v.vIntVec4[0] = v[0];
    m_v.vIntVec4[1] = v[1];
    m_v.vIntVec4[2] = v[2];
    m_v.vIntVec4[3] = v[3];
}

QueriedState::QueriedState(const GLUintVec4 &v) : m_type(DATATYPE_UNSIGNED_INTEGER_VEC4)
{
    m_v.vUintVec4[0] = v[0];
    m_v.vUintVec4[1] = v[1];
    m_v.vUintVec4[2] = v[2];
    m_v.vUintVec4[3] = v[3];
}

QueriedState::QueriedState(const GLFloatVec4 &v) : m_type(DATATYPE_FLOAT_VEC4)
{
    m_v.vFloatVec4[0] = v[0];
    m_v.vFloatVec4[1] = v[1];
    m_v.vFloatVec4[2] = v[2];
    m_v.vFloatVec4[3] = v[3];
}

QueriedState::QueriedState(const BooleanVec4 &v) : m_type(DATATYPE_BOOLEAN_VEC4)
{
    m_v.vBooleanVec4[0] = v[0];
    m_v.vBooleanVec4[1] = v[1];
    m_v.vBooleanVec4[2] = v[2];
    m_v.vBooleanVec4[3] = v[3];
}

QueriedState::QueriedState(const GLInt64Vec4 &v) : m_type(DATATYPE_INTEGER64_VEC4)
{
    m_v.vInt64Vec4[0] = v[0];
    m_v.vInt64Vec4[1] = v[1];
    m_v.vInt64Vec4[2] = v[2];
    m_v.vInt64Vec4[3] = v[3];
}

bool QueriedState::isUndefined(void) const
{
    return m_type == DATATYPE_LAST;
}

DataType QueriedState::getType(void) const
{
    return m_type;
}

glw::GLint &QueriedState::getIntAccess(void)
{
    DE_ASSERT(m_type == DATATYPE_INTEGER);
    return m_v.vInt;
}

glw::GLint64 &QueriedState::getInt64Access(void)
{
    DE_ASSERT(m_type == DATATYPE_INTEGER64);
    return m_v.vInt64;
}

bool &QueriedState::getBoolAccess(void)
{
    DE_ASSERT(m_type == DATATYPE_BOOLEAN);
    return m_v.vBool;
}

glw::GLfloat &QueriedState::getFloatAccess(void)
{
    DE_ASSERT(m_type == DATATYPE_FLOAT);
    return m_v.vFloat;
}

glw::GLuint &QueriedState::getUintAccess(void)
{
    DE_ASSERT(m_type == DATATYPE_UNSIGNED_INTEGER);
    return m_v.vUint;
}

QueriedState::GLIntVec3 &QueriedState::getIntVec3Access(void)
{
    DE_ASSERT(m_type == DATATYPE_INTEGER_VEC3);
    return m_v.vIntVec3;
}

void *&QueriedState::getPtrAccess(void)
{
    DE_ASSERT(m_type == DATATYPE_POINTER);
    return m_v.vPtr;
}

QueriedState::GLIntVec4 &QueriedState::getIntVec4Access(void)
{
    DE_ASSERT(m_type == DATATYPE_INTEGER_VEC4);
    return m_v.vIntVec4;
}

QueriedState::GLUintVec4 &QueriedState::getUintVec4Access(void)
{
    DE_ASSERT(m_type == DATATYPE_UNSIGNED_INTEGER_VEC4);
    return m_v.vUintVec4;
}

QueriedState::GLFloatVec4 &QueriedState::getFloatVec4Access(void)
{
    DE_ASSERT(m_type == DATATYPE_FLOAT_VEC4);
    return m_v.vFloatVec4;
}

QueriedState::BooleanVec4 &QueriedState::getBooleanVec4Access(void)
{
    DE_ASSERT(m_type == DATATYPE_BOOLEAN_VEC4);
    return m_v.vBooleanVec4;
}

QueriedState::GLInt64Vec4 &QueriedState::getInt64Vec4Access(void)
{
    DE_ASSERT(m_type == DATATYPE_INTEGER64_VEC4);
    return m_v.vInt64Vec4;
}

// query

static bool verifyBooleanValidity(tcu::ResultCollector &result, glw::GLboolean v)
{
    if (v == GL_TRUE || v == GL_FALSE)
        return true;
    else
    {
        std::ostringstream buf;
        buf << "Boolean value was not neither GL_TRUE nor GL_FALSE, got " << de::toString(tcu::Format::Hex<2>(v));
        result.fail(buf.str());
        return false;
    }
}

static bool verifyBooleanVec4Validity(tcu::ResultCollector &result, const glw::GLboolean v[4])
{
    bool valid = true;

    for (int i = 0; i < 4; i++)
    {
        if (v[i] != GL_TRUE && v[i] != GL_FALSE)
            valid = false;
    }

    if (!valid)
    {
        std::ostringstream buf;
        buf << "Boolean vec4 value was not neither GL_TRUE nor GL_FALSE, got (";

        for (int i = 0; i < 4; i++)
            buf << (i > 0 ? ", " : "") << de::toString(tcu::Format::Hex<2>(v[i]));

        buf << ")";

        result.fail(buf.str());
    }

    return valid;
}

void queryState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                QueriedState &state)
{
    switch (type)
    {
    case QUERY_ISENABLED:
    {
        const glw::GLboolean value = gl.glIsEnabled(target);

        if (!checkError(result, gl, "glIsEnabled"))
            return;

        if (!verifyBooleanValidity(result, value))
            return;

        state = QueriedState(value == GL_TRUE);
        break;
    }

    case QUERY_BOOLEAN:
    {
        StateQueryMemoryWriteGuard<glw::GLboolean> value;
        gl.glGetBooleanv(target, &value);

        if (!checkError(result, gl, "glGetBooleanv"))
            return;

        if (!value.verifyValidity(result))
            return;
        if (!verifyBooleanValidity(result, value))
            return;

        state = QueriedState(value == GL_TRUE);
        break;
    }

    case QUERY_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetIntegerv(target, &value);

        if (!checkError(result, gl, "glGetIntegerv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }

    case QUERY_INTEGER64:
    {
        StateQueryMemoryWriteGuard<glw::GLint64> value;
        gl.glGetInteger64v(target, &value);

        if (!checkError(result, gl, "glGetInteger64v"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }

    case QUERY_FLOAT:
    {
        StateQueryMemoryWriteGuard<glw::GLfloat> value;
        gl.glGetFloatv(target, &value);

        if (!checkError(result, gl, "glGetFloatv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void queryIndexedState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                       int index, QueriedState &state)
{
    switch (type)
    {
    case QUERY_INDEXED_BOOLEAN_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLboolean[4]> value;
        gl.glGetBooleani_v(target, index, value);

        if (!checkError(result, gl, "glGetBooleani_v"))
            return;

        if (!value.verifyValidity(result))
            return;

        if (!verifyBooleanVec4Validity(result, value))
            return;

        {
            bool res[4];

            for (int i = 0; i < 4; i++)
                res[i] = value[i] == GL_TRUE;

            state = QueriedState(res);
        }

        break;
    }

    case QUERY_INDEXED_INTEGER_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLint[4]> value;
        gl.glGetIntegeri_v(target, index, value);

        if (!checkError(result, gl, "glGetIntegeri_v"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }

    case QUERY_INDEXED_INTEGER64_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLint64[4]> value;
        gl.glGetInteger64i_v(target, index, value);

        if (!checkError(result, gl, "glGetInteger64i_v"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }

    case QUERY_INDEXED_ISENABLED:
    {
        const glw::GLboolean value = gl.glIsEnabledi(target, index);

        if (!checkError(result, gl, "glIsEnabledi"))
            return;

        if (!verifyBooleanValidity(result, value))
            return;

        state = QueriedState(value == GL_TRUE);
        break;
    }

    case QUERY_INDEXED_BOOLEAN:
    {
        StateQueryMemoryWriteGuard<glw::GLboolean> value;
        gl.glGetBooleani_v(target, index, &value);

        if (!checkError(result, gl, "glGetBooleani_v"))
            return;

        if (!value.verifyValidity(result))
            return;
        if (!verifyBooleanValidity(result, value))
            return;

        state = QueriedState(value == GL_TRUE);
        break;
    }

    case QUERY_INDEXED_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetIntegeri_v(target, index, &value);

        if (!checkError(result, gl, "glGetIntegeri_v"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }

    case QUERY_INDEXED_INTEGER64:
    {
        StateQueryMemoryWriteGuard<glw::GLint64> value;
        gl.glGetInteger64i_v(target, index, &value);

        if (!checkError(result, gl, "glGetInteger64i_v"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void queryAttributeState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                         int index, QueriedState &state)
{
    switch (type)
    {
    case QUERY_ATTRIBUTE_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetVertexAttribiv(index, target, &value);

        if (!checkError(result, gl, "glGetVertexAttribiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_ATTRIBUTE_FLOAT:
    {
        StateQueryMemoryWriteGuard<glw::GLfloat> value;
        gl.glGetVertexAttribfv(index, target, &value);

        if (!checkError(result, gl, "glGetVertexAttribfv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_ATTRIBUTE_PURE_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetVertexAttribIiv(index, target, &value);

        if (!checkError(result, gl, "glGetVertexAttribIiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_ATTRIBUTE_PURE_UNSIGNED_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLuint> value;
        gl.glGetVertexAttribIuiv(index, target, &value);

        if (!checkError(result, gl, "glGetVertexAttribIuiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void queryFramebufferState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                           glw::GLenum pname, QueriedState &state)
{
    switch (type)
    {
    case QUERY_FRAMEBUFFER_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetFramebufferParameteriv(target, pname, &value);

        if (!checkError(result, gl, "glGetVertexAttribiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void queryProgramState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLuint program,
                       glw::GLenum pname, QueriedState &state)
{
    switch (type)
    {
    case QUERY_PROGRAM_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetProgramiv(program, pname, &value);

        if (!checkError(result, gl, "glGetProgramiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_PROGRAM_INTEGER_VEC3:
    {
        StateQueryMemoryWriteGuard<glw::GLint[3]> value;
        gl.glGetProgramiv(program, pname, value);

        if (!checkError(result, gl, "glGetProgramiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void queryPipelineState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLuint pipeline,
                        glw::GLenum pname, QueriedState &state)
{
    switch (type)
    {
    case QUERY_PIPELINE_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetProgramPipelineiv(pipeline, pname, &value);

        if (!checkError(result, gl, "glGetProgramiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void queryTextureParamState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                            glw::GLenum pname, QueriedState &state)
{
    switch (type)
    {
    case QUERY_TEXTURE_PARAM_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetTexParameteriv(target, pname, &value);

        if (!checkError(result, gl, "glGetTexParameteriv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_TEXTURE_PARAM_FLOAT:
    {
        StateQueryMemoryWriteGuard<glw::GLfloat> value;
        gl.glGetTexParameterfv(target, pname, &value);

        if (!checkError(result, gl, "glGetTexParameterfv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_TEXTURE_PARAM_PURE_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetTexParameterIiv(target, pname, &value);

        if (!checkError(result, gl, "GetTexParameterIiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLuint> value;
        gl.glGetTexParameterIuiv(target, pname, &value);

        if (!checkError(result, gl, "GetTexParameterIuiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_TEXTURE_PARAM_INTEGER_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLint[4]> value;
        gl.glGetTexParameteriv(target, pname, value);

        if (!checkError(result, gl, "glGetTexParameteriv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_TEXTURE_PARAM_FLOAT_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLfloat[4]> value;
        gl.glGetTexParameterfv(target, pname, value);

        if (!checkError(result, gl, "glGetTexParameterfv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_TEXTURE_PARAM_PURE_INTEGER_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLint[4]> value;
        gl.glGetTexParameterIiv(target, pname, value);

        if (!checkError(result, gl, "GetTexParameterIiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLuint[4]> value;
        gl.glGetTexParameterIuiv(target, pname, value);

        if (!checkError(result, gl, "GetTexParameterIuiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void queryTextureLevelState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                            int level, glw::GLenum pname, QueriedState &state)
{
    switch (type)
    {
    case QUERY_TEXTURE_LEVEL_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetTexLevelParameteriv(target, level, pname, &value);

        if (!checkError(result, gl, "glGetTexLevelParameteriv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_TEXTURE_LEVEL_FLOAT:
    {
        StateQueryMemoryWriteGuard<glw::GLfloat> value;
        gl.glGetTexLevelParameterfv(target, level, pname, &value);

        if (!checkError(result, gl, "glGetTexLevelParameterfv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void queryPointerState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum pname,
                       QueriedState &state)
{
    switch (type)
    {
    case QUERY_POINTER:
    {
        StateQueryMemoryWriteGuard<void *> value;
        gl.glGetPointerv(pname, &value);

        if (!checkError(result, gl, "glGetPointerv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void queryObjectState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLuint handle,
                      QueriedState &state)
{
    switch (type)
    {
    case QUERY_ISTEXTURE:
    {
        const glw::GLboolean value = gl.glIsTexture(handle);

        if (!checkError(result, gl, "glIsTexture"))
            return;

        if (!verifyBooleanValidity(result, value))
            return;

        state = QueriedState(value == GL_TRUE);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void queryQueryState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                     glw::GLenum pname, QueriedState &state)
{
    switch (type)
    {
    case QUERY_QUERY:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetQueryiv(target, pname, &value);

        if (!checkError(result, gl, "glGetQueryiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

void querySamplerState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLuint sampler,
                       glw::GLenum pname, QueriedState &state)
{
    switch (type)
    {
    case QUERY_SAMPLER_PARAM_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetSamplerParameteriv(sampler, pname, &value);

        if (!checkError(result, gl, "glGetSamplerParameteriv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_SAMPLER_PARAM_FLOAT:
    {
        StateQueryMemoryWriteGuard<glw::GLfloat> value;
        gl.glGetSamplerParameterfv(sampler, pname, &value);

        if (!checkError(result, gl, "glGetSamplerParameteriv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_SAMPLER_PARAM_PURE_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLint> value;
        gl.glGetSamplerParameterIiv(sampler, pname, &value);

        if (!checkError(result, gl, "glGetSamplerParameterIiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER:
    {
        StateQueryMemoryWriteGuard<glw::GLuint> value;
        gl.glGetSamplerParameterIuiv(sampler, pname, &value);

        if (!checkError(result, gl, "glGetSamplerParameterIuiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_SAMPLER_PARAM_INTEGER_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLint[4]> value;
        gl.glGetSamplerParameteriv(sampler, pname, value);

        if (!checkError(result, gl, "glGetSamplerParameteriv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_SAMPLER_PARAM_FLOAT_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLfloat[4]> value;
        gl.glGetSamplerParameterfv(sampler, pname, value);

        if (!checkError(result, gl, "glGetSamplerParameteriv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_SAMPLER_PARAM_PURE_INTEGER_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLint[4]> value;
        gl.glGetSamplerParameterIiv(sampler, pname, value);

        if (!checkError(result, gl, "glGetSamplerParameterIiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    case QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER_VEC4:
    {
        StateQueryMemoryWriteGuard<glw::GLuint[4]> value;
        gl.glGetSamplerParameterIuiv(sampler, pname, value);

        if (!checkError(result, gl, "glGetSamplerParameterIuiv"))
            return;

        if (!value.verifyValidity(result))
            return;

        state = QueriedState(value);
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

// verify

void verifyBoolean(tcu::ResultCollector &result, QueriedState &state, bool expected)
{
    switch (state.getType())
    {
    case DATATYPE_BOOLEAN:
    {
        if (state.getBoolAccess() != expected)
        {
            std::ostringstream buf;
            buf << "Expected " << glu::getBooleanStr(mapBoolToGLBoolean(expected)) << ", got "
                << glu::getBooleanStr(mapBoolToGLBoolean(state.getBoolAccess()));
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER:
    {
        const glw::GLint reference = expected ? 1 : 0;
        if (state.getIntAccess() != reference)
        {
            std::ostringstream buf;
            buf << "Expected " << reference << ", got " << state.getIntAccess();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER64:
    {
        const glw::GLint64 reference = expected ? 1 : 0;
        if (state.getInt64Access() != reference)
        {
            std::ostringstream buf;
            buf << "Expected " << reference << ", got " << state.getInt64Access();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_FLOAT:
    {
        const glw::GLfloat reference = expected ? 1.0f : 0.0f;
        if (state.getFloatAccess() != reference)
        {
            std::ostringstream buf;
            buf << "Expected " << reference << ", got " << state.getFloatAccess();
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyInteger(tcu::ResultCollector &result, QueriedState &state, int expected)
{
    switch (state.getType())
    {
    case DATATYPE_BOOLEAN:
    {
        const bool reference = (expected != 0);
        if (state.getBoolAccess() != reference)
        {
            std::ostringstream buf;
            buf << "Expected " << glu::getBooleanStr(mapBoolToGLBoolean(reference)) << ", got "
                << glu::getBooleanStr(mapBoolToGLBoolean(state.getBoolAccess()));
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER:
    {
        const glw::GLint reference = expected;
        if (state.getIntAccess() != reference)
        {
            std::ostringstream buf;
            buf << "Expected " << reference << "(" << de::toString(tcu::Format::Hex<8>(reference)) << ") , got "
                << state.getIntAccess() << "(" << de::toString(tcu::Format::Hex<8>(state.getIntAccess())) << ")";
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER64:
    {
        const glw::GLint64 reference = (glw::GLint64)expected;
        if (state.getInt64Access() != reference)
        {
            std::ostringstream buf;
            buf << "Expected " << reference << "(" << de::toString(tcu::Format::Hex<8>(reference)) << "), got "
                << state.getInt64Access() << "(" << de::toString(tcu::Format::Hex<8>(state.getInt64Access())) << ")";
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_FLOAT:
    {
        const glw::GLfloat refValueMin = deInt32ToFloatRoundToNegInf(expected);
        const glw::GLfloat refValueMax = deInt32ToFloatRoundToPosInf(expected);

        if (state.getFloatAccess() < refValueMin || state.getFloatAccess() > refValueMax ||
            std::isnan(state.getFloatAccess()))
        {
            std::ostringstream buf;

            if (refValueMin == refValueMax)
                buf << "Expected " << refValueMin << ", got " << state.getFloatAccess();
            else
                buf << "Expected in range [" << refValueMin << ", " << refValueMax << "], got "
                    << state.getFloatAccess();

            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_UNSIGNED_INTEGER:
    {
        const glw::GLuint reference = (glw::GLuint)expected;
        if (state.getUintAccess() != reference)
        {
            std::ostringstream buf;
            buf << "Expected " << reference << "(" << de::toString(tcu::Format::Hex<8>(reference)) << "), got "
                << state.getUintAccess() << "(" << de::toString(tcu::Format::Hex<8>(state.getUintAccess())) << ")";
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyIntegerMin(tcu::ResultCollector &result, QueriedState &state, int minValue)
{
    switch (state.getType())
    {
    case DATATYPE_BOOLEAN:
    {
        if (minValue > 0 && state.getBoolAccess() != true)
        {
            std::ostringstream buf;
            buf << "Expected GL_TRUE, got GL_FALSE";
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER:
    {
        if (state.getIntAccess() < minValue)
        {
            std::ostringstream buf;
            buf << "Expected greater or equal to " << minValue << ", got " << state.getIntAccess();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER64:
    {
        if (state.getInt64Access() < minValue)
        {
            std::ostringstream buf;
            buf << "Expected greater or equal to " << minValue << ", got " << state.getInt64Access();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_FLOAT:
    {
        if (state.getFloatAccess() < deInt32ToFloatRoundToNegInf(minValue) || std::isnan(state.getFloatAccess()))
        {
            std::ostringstream buf;
            buf << "Expected greater or equal to " << minValue << ", got " << state.getFloatAccess();
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyIntegerMax(tcu::ResultCollector &result, QueriedState &state, int maxValue)
{
    switch (state.getType())
    {
    case DATATYPE_BOOLEAN:
    {
        if (maxValue < 0 && state.getBoolAccess() != true)
        {
            std::ostringstream buf;
            buf << "Expected GL_TRUE, got GL_FALSE";
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER:
    {
        if (state.getIntAccess() > maxValue)
        {
            std::ostringstream buf;
            buf << "Expected less or equal to " << maxValue << ", got " << state.getIntAccess();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER64:
    {
        if (state.getInt64Access() > maxValue)
        {
            std::ostringstream buf;
            buf << "Expected less or equal to " << maxValue << ", got " << state.getInt64Access();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_FLOAT:
    {
        if (state.getFloatAccess() > deInt32ToFloatRoundToPosInf(maxValue) || std::isnan(state.getFloatAccess()))
        {
            std::ostringstream buf;
            buf << "Expected less or equal to " << maxValue << ", got " << state.getFloatAccess();
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyFloat(tcu::ResultCollector &result, QueriedState &state, float expected)
{
    switch (state.getType())
    {
    case DATATYPE_BOOLEAN:
    {
        const bool reference = (expected != 0.0f);

        if (state.getBoolAccess() != reference)
        {
            std::ostringstream buf;
            buf << "Expected " << glu::getBooleanStr(mapBoolToGLBoolean(reference)) << ", got "
                << glu::getBooleanStr(mapBoolToGLBoolean(state.getBoolAccess()));
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER:
    {
        const glw::GLint refValueMin = roundGLfloatToNearestIntegerHalfDown<glw::GLint>(expected);
        const glw::GLint refValueMax = roundGLfloatToNearestIntegerHalfUp<glw::GLint>(expected);

        if (state.getIntAccess() < refValueMin || state.getIntAccess() > refValueMax)
        {
            std::ostringstream buf;

            if (refValueMin == refValueMax)
                buf << "Expected " << refValueMin << ", got " << state.getIntAccess();
            else
                buf << "Expected in range [" << refValueMin << ", " << refValueMax << "], got " << state.getIntAccess();

            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_FLOAT:
    {
        if (state.getFloatAccess() != expected)
        {
            std::ostringstream buf;
            buf << "Expected " << expected << ", got " << state.getFloatAccess();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER64:
    {
        const glw::GLint64 refValueMin = roundGLfloatToNearestIntegerHalfDown<glw::GLint64>(expected);
        const glw::GLint64 refValueMax = roundGLfloatToNearestIntegerHalfUp<glw::GLint64>(expected);

        if (state.getInt64Access() < refValueMin || state.getInt64Access() > refValueMax)
        {
            std::ostringstream buf;

            if (refValueMin == refValueMax)
                buf << "Expected " << refValueMin << ", got " << state.getInt64Access();
            else
                buf << "Expected in range [" << refValueMin << ", " << refValueMax << "], got "
                    << state.getInt64Access();

            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_UNSIGNED_INTEGER:
    {
        const glw::GLuint refValueMin = roundGLfloatToNearestIntegerHalfDown<glw::GLuint>(expected);
        const glw::GLuint refValueMax = roundGLfloatToNearestIntegerHalfUp<glw::GLuint>(expected);

        if (state.getUintAccess() < refValueMin || state.getUintAccess() > refValueMax)
        {
            std::ostringstream buf;

            if (refValueMin == refValueMax)
                buf << "Expected " << refValueMin << ", got " << state.getUintAccess();
            else
                buf << "Expected in range [" << refValueMin << ", " << refValueMax << "], got "
                    << state.getUintAccess();

            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyFloatMin(tcu::ResultCollector &result, QueriedState &state, float minValue)
{
    switch (state.getType())
    {
    case DATATYPE_BOOLEAN:
    {
        if (minValue > 0.0f && state.getBoolAccess() != true)
            result.fail("expected GL_TRUE, got GL_FALSE");
        break;
    }

    case DATATYPE_INTEGER:
    {
        const glw::GLint refValue = roundGLfloatToNearestIntegerHalfDown<glw::GLint>(minValue);

        if (state.getIntAccess() < refValue)
        {
            std::ostringstream buf;
            buf << "Expected greater or equal to " << refValue << ", got " << state.getIntAccess();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_FLOAT:
    {
        if (state.getFloatAccess() < minValue || std::isnan(state.getFloatAccess()))
        {
            std::ostringstream buf;
            buf << "Expected greater or equal to " << minValue << ", got " << state.getFloatAccess();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER64:
    {
        const glw::GLint64 refValue = roundGLfloatToNearestIntegerHalfDown<glw::GLint64>(minValue);

        if (state.getInt64Access() < refValue)
        {
            std::ostringstream buf;
            buf << "Expected greater or equal to " << refValue << ", got " << state.getInt64Access();
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyFloatMax(tcu::ResultCollector &result, QueriedState &state, float maxValue)
{
    switch (state.getType())
    {
    case DATATYPE_BOOLEAN:
    {
        if (maxValue < 0.0f && state.getBoolAccess() != true)
            result.fail("expected GL_TRUE, got GL_FALSE");
        break;
    }

    case DATATYPE_INTEGER:
    {
        const glw::GLint refValue = roundGLfloatToNearestIntegerHalfUp<glw::GLint>(maxValue);

        if (state.getIntAccess() > refValue)
        {
            std::ostringstream buf;
            buf << "Expected less or equal to " << refValue << ", got " << state.getIntAccess();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_FLOAT:
    {
        if (state.getFloatAccess() > maxValue || std::isnan(state.getFloatAccess()))
        {
            std::ostringstream buf;
            buf << "Expected less or equal to " << maxValue << ", got " << state.getFloatAccess();
            result.fail(buf.str());
        }
        break;
    }

    case DATATYPE_INTEGER64:
    {
        const glw::GLint64 refValue = roundGLfloatToNearestIntegerHalfUp<glw::GLint64>(maxValue);

        if (state.getInt64Access() > refValue)
        {
            std::ostringstream buf;
            buf << "Expected less or equal to " << refValue << ", got " << state.getInt64Access();
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyIntegerVec3(tcu::ResultCollector &result, QueriedState &state, const tcu::IVec3 &expected)
{
    switch (state.getType())
    {
    case DATATYPE_INTEGER_VEC3:
    {
        if (state.getIntVec3Access()[0] != expected[0] || state.getIntVec3Access()[1] != expected[1] ||
            state.getIntVec3Access()[2] != expected[2])
        {
            std::ostringstream buf;
            buf << "Expected " << expected << ", got " << tcu::formatArray(state.getIntVec3Access());
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyIntegerVec4(tcu::ResultCollector &result, QueriedState &state, const tcu::IVec4 &expected)
{
    switch (state.getType())
    {
    case DATATYPE_INTEGER_VEC4:
    {
        if (state.getIntVec4Access()[0] != expected[0] || state.getIntVec4Access()[1] != expected[1] ||
            state.getIntVec4Access()[2] != expected[2] || state.getIntVec4Access()[3] != expected[3])
        {
            std::ostringstream buf;
            buf << "Expected " << expected << ", got " << tcu::formatArray(state.getIntVec4Access());
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyUnsignedIntegerVec4(tcu::ResultCollector &result, QueriedState &state, const tcu::UVec4 &expected)
{
    switch (state.getType())
    {
    case DATATYPE_UNSIGNED_INTEGER_VEC4:
    {
        if (state.getUintVec4Access()[0] != expected[0] || state.getUintVec4Access()[1] != expected[1] ||
            state.getUintVec4Access()[2] != expected[2] || state.getUintVec4Access()[3] != expected[3])
        {
            std::ostringstream buf;
            buf << "Expected " << expected << ", got " << tcu::formatArray(state.getUintVec4Access());
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyBooleanVec4(tcu::ResultCollector &result, QueriedState &state, const tcu::BVec4 &expected)
{
    switch (state.getType())
    {
    case DATATYPE_BOOLEAN_VEC4:
    {
        const glw::GLboolean referenceVec4[4] = {mapBoolToGLBoolean(expected[0]), mapBoolToGLBoolean(expected[1]),
                                                 mapBoolToGLBoolean(expected[2]), mapBoolToGLBoolean(expected[3])};

        const glw::GLboolean resultVec4[4] = {
            mapBoolToGLBoolean(state.getBooleanVec4Access()[0]), mapBoolToGLBoolean(state.getBooleanVec4Access()[1]),
            mapBoolToGLBoolean(state.getBooleanVec4Access()[2]), mapBoolToGLBoolean(state.getBooleanVec4Access()[3])};

        if (resultVec4[0] != referenceVec4[0] || resultVec4[1] != referenceVec4[1] ||
            resultVec4[2] != referenceVec4[2] || resultVec4[3] != referenceVec4[3])
        {
            std::ostringstream buf;
            buf << "Expected " << glu::getBooleanPointerStr(referenceVec4, 4) << ", got "
                << glu::getBooleanPointerStr(resultVec4, 4);
            result.fail(buf.str());
        }

        break;
    }
    case DATATYPE_FLOAT_VEC4:
    {
        const glw::GLfloat reference[4] = {(expected[0] ? 1.0f : 0.0f), (expected[1] ? 1.0f : 0.0f),
                                           (expected[2] ? 1.0f : 0.0f), (expected[3] ? 1.0f : 0.0f)};

        if (state.getFloatVec4Access()[0] != reference[0] || state.getFloatVec4Access()[1] != reference[1] ||
            state.getFloatVec4Access()[2] != reference[2] || state.getFloatVec4Access()[3] != reference[3])
        {
            std::ostringstream buf;
            buf << "Expected " << reference << ", got " << tcu::formatArray(state.getFloatVec4Access());
            result.fail(buf.str());
        }
        break;
    }
    case DATATYPE_INTEGER_VEC4:
    {
        const glw::GLint reference[4] = {(expected[0] ? 1 : 0), (expected[1] ? 1 : 0), (expected[2] ? 1 : 0),
                                         (expected[3] ? 1 : 0)};

        if (state.getIntVec4Access()[0] != reference[0] || state.getIntVec4Access()[1] != reference[1] ||
            state.getIntVec4Access()[2] != reference[2] || state.getIntVec4Access()[3] != reference[3])
        {
            std::ostringstream buf;
            buf << "Expected " << reference << ", got " << tcu::formatArray(state.getIntVec4Access());
            result.fail(buf.str());
        }
        break;
    }
    case DATATYPE_INTEGER64_VEC4:
    {
        const glw::GLint64 reference[4] = {(expected[0] ? 1 : 0), (expected[1] ? 1 : 0), (expected[2] ? 1 : 0),
                                           (expected[3] ? 1 : 0)};

        if (state.getInt64Vec4Access()[0] != reference[0] || state.getInt64Vec4Access()[1] != reference[1] ||
            state.getInt64Vec4Access()[2] != reference[2] || state.getInt64Vec4Access()[3] != reference[3])
        {
            std::ostringstream buf;
            buf << "Expected " << reference << ", got " << tcu::formatArray(state.getInt64Vec4Access());
            result.fail(buf.str());
        }
        break;
    }
    case DATATYPE_UNSIGNED_INTEGER_VEC4:
    {
        const glw::GLuint reference[4] = {(expected[0] ? 1u : 0u), (expected[1] ? 1u : 0u), (expected[2] ? 1u : 0u),
                                          (expected[3] ? 1u : 0u)};

        if (state.getUintVec4Access()[0] != reference[0] || state.getUintVec4Access()[1] != reference[1] ||
            state.getUintVec4Access()[2] != reference[2] || state.getUintVec4Access()[3] != reference[3])
        {
            std::ostringstream buf;
            buf << "Expected " << reference << ", got " << tcu::formatArray(state.getUintVec4Access());
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyFloatVec4(tcu::ResultCollector &result, QueriedState &state, const tcu::Vec4 &expected)
{
    switch (state.getType())
    {
    case DATATYPE_FLOAT_VEC4:
    {
        if (state.getFloatVec4Access()[0] != expected[0] || state.getFloatVec4Access()[1] != expected[1] ||
            state.getFloatVec4Access()[2] != expected[2] || state.getFloatVec4Access()[3] != expected[3])
        {
            std::ostringstream buf;
            buf << "Expected " << expected << ", got " << tcu::formatArray(state.getFloatVec4Access());
            result.fail(buf.str());
        }
        break;
    }
    case DATATYPE_INTEGER_VEC4:
    {
        bool anyError = false;
        std::ostringstream expectation;

        for (int ndx = 0; ndx < 4; ++ndx)
        {
            const glw::GLint refValueMin = roundGLfloatToNearestIntegerHalfDown<glw::GLint>(expected[ndx]);
            const glw::GLint refValueMax = roundGLfloatToNearestIntegerHalfUp<glw::GLint>(expected[ndx]);

            if (state.getIntVec4Access()[ndx] < refValueMin || state.getIntVec4Access()[ndx] > refValueMax)
            {
                std::ostringstream buf;

                if (ndx > 0)
                    expectation << " ,";

                if (refValueMin == refValueMax)
                    buf << refValueMin;
                else
                    buf << "[" << refValueMin << ", " << refValueMax << "]";
            }
        }

        if (anyError)
        {
            std::ostringstream buf;
            buf << "Expected {" << expectation.str() << "}, got " << tcu::formatArray(state.getIntVec4Access());
            result.fail(buf.str());
        }
        break;
    }
    case DATATYPE_UNSIGNED_INTEGER_VEC4:
    {
        bool anyError = false;
        std::ostringstream expectation;

        for (int ndx = 0; ndx < 4; ++ndx)
        {
            const glw::GLuint refValueMin = roundGLfloatToNearestIntegerHalfDown<glw::GLuint>(expected[ndx]);
            const glw::GLuint refValueMax = roundGLfloatToNearestIntegerHalfUp<glw::GLuint>(expected[ndx]);

            if (state.getUintVec4Access()[ndx] < refValueMin || state.getUintVec4Access()[ndx] > refValueMax)
            {
                std::ostringstream buf;

                if (ndx > 0)
                    expectation << " ,";

                if (refValueMin == refValueMax)
                    buf << refValueMin;
                else
                    buf << "[" << refValueMin << ", " << refValueMax << "]";
            }
        }

        if (anyError)
        {
            std::ostringstream buf;
            buf << "Expected {" << expectation.str() << "}, got " << tcu::formatArray(state.getUintVec4Access());
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyPointer(tcu::ResultCollector &result, QueriedState &state, const void *expected)
{
    switch (state.getType())
    {
    case DATATYPE_POINTER:
    {
        if (state.getPtrAccess() != expected)
        {
            std::ostringstream buf;
            buf << "Expected " << expected << ", got " << state.getPtrAccess();
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

static float normalizeI32Float(int32_t c)
{
    return de::max((float)c / float((1ul << 31) - 1u), -1.0f);
}

void verifyNormalizedI32Vec4(tcu::ResultCollector &result, QueriedState &state, const tcu::IVec4 &expected)
{
    // \note: normalization precision is irrelevant for these tests, we can use very large thresholds
    const float normalizationError = 0.1f;
    const tcu::Vec4 reference(normalizeI32Float(expected[0]), normalizeI32Float(expected[1]),
                              normalizeI32Float(expected[2]), normalizeI32Float(expected[3]));
    const tcu::Vec4 validHigh(
        de::min(1.0f, reference[0] + normalizationError), de::min(1.0f, reference[1] + normalizationError),
        de::min(1.0f, reference[2] + normalizationError), de::min(1.0f, reference[3] + normalizationError));
    const tcu::Vec4 validLow(
        de::max(-1.0f, reference[0] - normalizationError), de::max(-1.0f, reference[1] - normalizationError),
        de::max(-1.0f, reference[2] - normalizationError), de::max(-1.0f, reference[3] - normalizationError));

    switch (state.getType())
    {
    case DATATYPE_FLOAT_VEC4:
    {
        bool anyError = false;
        std::ostringstream expectation;

        for (int ndx = 0; ndx < 4; ++ndx)
        {
            if (state.getFloatVec4Access()[ndx] < validLow[ndx] || state.getFloatVec4Access()[ndx] > validHigh[ndx])
            {
                std::ostringstream buf;

                if (ndx > 0)
                    expectation << " ,";
                buf << "[" << validLow[ndx] << ", " << validHigh[ndx] << "]";
            }
        }

        if (anyError)
        {
            std::ostringstream buf;
            buf << "Expected {" << expectation.str() << "}, got " << tcu::formatArray(state.getFloatVec4Access());
            result.fail(buf.str());
        }
        break;
    }
    case DATATYPE_INTEGER_VEC4:
    {
        bool anyError = false;
        std::ostringstream expectation;

        for (int ndx = 0; ndx < 4; ++ndx)
        {
            const glw::GLint refValueMin = roundGLfloatToNearestIntegerHalfDown<glw::GLint>(validHigh[ndx]);
            const glw::GLint refValueMax = roundGLfloatToNearestIntegerHalfUp<glw::GLint>(validLow[ndx]);

            if (state.getIntVec4Access()[ndx] < refValueMin || state.getIntVec4Access()[ndx] > refValueMax)
            {
                std::ostringstream buf;

                if (ndx > 0)
                    expectation << " ,";

                if (refValueMin == refValueMax)
                    buf << refValueMin;
                else
                    buf << "[" << refValueMin << ", " << refValueMax << "]";
            }
        }

        if (anyError)
        {
            std::ostringstream buf;
            buf << "Expected {" << expectation.str() << "}, got " << tcu::formatArray(state.getIntVec4Access());
            result.fail(buf.str());
        }
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

// helpers

void verifyStateBoolean(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, bool refValue,
                        QueryType type)
{
    QueriedState state;

    queryState(result, gl, type, target, state);

    if (!state.isUndefined())
        verifyBoolean(result, state, refValue);
}

void verifyStateInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int refValue,
                        QueryType type)
{
    QueriedState state;

    queryState(result, gl, type, target, state);

    if (!state.isUndefined())
        verifyInteger(result, state, refValue);
}

void verifyStateIntegerMin(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int minValue,
                           QueryType type)
{
    QueriedState state;

    queryState(result, gl, type, target, state);

    if (!state.isUndefined())
        verifyIntegerMin(result, state, minValue);
}

void verifyStateIntegerMax(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int maxValue,
                           QueryType type)
{
    QueriedState state;

    queryState(result, gl, type, target, state);

    if (!state.isUndefined())
        verifyIntegerMax(result, state, maxValue);
}

void verifyStateIntegerEqualToOther(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                    glw::GLenum other, QueryType type)
{
    QueriedState stateA;
    QueriedState stateB;

    queryState(result, gl, type, target, stateA);
    queryState(result, gl, type, other, stateB);

    if (stateA.isUndefined() || stateB.isUndefined())
        return;

    switch (type)
    {
    case QUERY_BOOLEAN:
    {
        if (stateA.getBoolAccess() != stateB.getBoolAccess())
            result.fail("expected equal results");
        break;
    }

    case QUERY_INTEGER:
    {
        if (stateA.getIntAccess() != stateB.getIntAccess())
            result.fail("expected equal results");
        break;
    }

    case QUERY_INTEGER64:
    {
        if (stateA.getInt64Access() != stateB.getInt64Access())
            result.fail("expected equal results");
        break;
    }

    case QUERY_FLOAT:
    {
        if (stateA.getFloatAccess() != stateB.getFloatAccess())
            result.fail("expected equal results");
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

void verifyStateFloat(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, float reference,
                      QueryType type)
{
    QueriedState state;

    queryState(result, gl, type, target, state);

    if (!state.isUndefined())
        verifyFloat(result, state, reference);
}

void verifyStateFloatMin(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, float minValue,
                         QueryType type)
{
    QueriedState state;

    queryState(result, gl, type, target, state);

    if (!state.isUndefined())
        verifyFloatMin(result, state, minValue);
}

void verifyStateFloatMax(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, float maxValue,
                         QueryType type)
{
    QueriedState state;

    queryState(result, gl, type, target, state);

    if (!state.isUndefined())
        verifyFloatMax(result, state, maxValue);
}

void verifyStatePointer(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, const void *expected,
                        QueryType type)
{
    QueriedState state;

    queryPointerState(result, gl, type, target, state);

    if (!state.isUndefined())
        verifyPointer(result, state, expected);
}

void verifyStateIndexedBoolean(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                               bool expected, QueryType type)
{
    QueriedState state;

    queryIndexedState(result, gl, type, target, index, state);

    if (!state.isUndefined())
        verifyBoolean(result, state, expected);
}

void verifyStateIndexedBooleanVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                                   const tcu::BVec4 &expected, QueryType type)
{
    QueriedState state;

    queryIndexedState(result, gl, type, target, index, state);

    if (!state.isUndefined())
        verifyBooleanVec4(result, state, expected);
}

void verifyStateIndexedInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                               int expected, QueryType type)
{
    QueriedState state;

    queryIndexedState(result, gl, type, target, index, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateIndexedIntegerMin(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                                  int minValue, QueryType type)
{
    QueriedState state;

    queryIndexedState(result, gl, type, target, index, state);

    if (!state.isUndefined())
        verifyIntegerMin(result, state, minValue);
}

void verifyStateAttributeInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                                 int expected, QueryType type)
{
    QueriedState state;

    queryAttributeState(result, gl, type, target, index, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateFramebufferInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                   glw::GLenum pname, int expected, QueryType type)
{
    QueriedState state;

    queryFramebufferState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateFramebufferIntegerMin(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                      glw::GLenum pname, int minValue, QueryType type)
{
    QueriedState state;

    queryFramebufferState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyIntegerMin(result, state, minValue);
}

void verifyStateProgramInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint program,
                               glw::GLenum pname, int expected, QueryType type)
{
    QueriedState state;

    queryProgramState(result, gl, type, program, pname, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateProgramIntegerVec3(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint program,
                                   glw::GLenum pname, const tcu::IVec3 &expected, QueryType type)
{
    QueriedState state;

    queryProgramState(result, gl, type, program, pname, state);

    if (!state.isUndefined())
        verifyIntegerVec3(result, state, expected);
}

void verifyStatePipelineInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint pipeline,
                                glw::GLenum pname, int expected, QueryType type)
{
    QueriedState state;

    queryPipelineState(result, gl, type, pipeline, pname, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateTextureParamInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                    glw::GLenum pname, int expected, QueryType type)
{
    QueriedState state;

    queryTextureParamState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateTextureParamFloat(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                  glw::GLenum pname, float expected, QueryType type)
{
    QueriedState state;

    queryTextureParamState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyFloat(result, state, expected);
}

void verifyStateTextureParamFloatVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                      glw::GLenum pname, const tcu::Vec4 &expected, QueryType type)
{
    QueriedState state;

    queryTextureParamState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyFloatVec4(result, state, expected);
}

void verifyStateTextureParamNormalizedI32Vec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                              glw::GLenum pname, const tcu::IVec4 &expected, QueryType type)
{
    QueriedState state;

    queryTextureParamState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyNormalizedI32Vec4(result, state, expected);
}

void verifyStateTextureParamIntegerVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                        glw::GLenum pname, const tcu::IVec4 &expected, QueryType type)
{
    QueriedState state;

    queryTextureParamState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyIntegerVec4(result, state, expected);
}

void verifyStateTextureParamUnsignedIntegerVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl,
                                                glw::GLenum target, glw::GLenum pname, const tcu::UVec4 &expected,
                                                QueryType type)
{
    QueriedState state;

    queryTextureParamState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyUnsignedIntegerVec4(result, state, expected);
}

void verifyStateTextureLevelInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                    int level, glw::GLenum pname, int expected, QueryType type)
{
    QueriedState state;

    queryTextureLevelState(result, gl, type, target, level, pname, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateObjectBoolean(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint handle, bool expected,
                              QueryType type)
{
    QueriedState state;

    queryObjectState(result, gl, type, handle, state);

    if (!state.isUndefined())
        verifyBoolean(result, state, expected);
}

void verifyStateQueryInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                             glw::GLenum pname, int expected, QueryType type)
{
    QueriedState state;

    queryQueryState(result, gl, type, target, pname, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateSamplerParamInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint sampler,
                                    glw::GLenum pname, int expected, QueryType type)
{
    QueriedState state;

    querySamplerState(result, gl, type, sampler, pname, state);

    if (!state.isUndefined())
        verifyInteger(result, state, expected);
}

void verifyStateSamplerParamFloat(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint sampler,
                                  glw::GLenum pname, float expected, QueryType type)
{
    QueriedState state;

    querySamplerState(result, gl, type, sampler, pname, state);

    if (!state.isUndefined())
        verifyFloat(result, state, expected);
}

void verifyStateSamplerParamFloatVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint sampler,
                                      glw::GLenum pname, const tcu::Vec4 &expected, QueryType type)
{
    QueriedState state;

    querySamplerState(result, gl, type, sampler, pname, state);

    if (!state.isUndefined())
        verifyFloatVec4(result, state, expected);
}

void verifyStateSamplerParamNormalizedI32Vec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl,
                                              glw::GLuint sampler, glw::GLenum pname, const tcu::IVec4 &expected,
                                              QueryType type)
{
    QueriedState state;

    querySamplerState(result, gl, type, sampler, pname, state);

    if (!state.isUndefined())
        verifyNormalizedI32Vec4(result, state, expected);
}

void verifyStateSamplerParamIntegerVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint sampler,
                                        glw::GLenum pname, const tcu::IVec4 &expected, QueryType type)
{
    QueriedState state;

    querySamplerState(result, gl, type, sampler, pname, state);

    if (!state.isUndefined())
        verifyIntegerVec4(result, state, expected);
}

void verifyStateSamplerParamUnsignedIntegerVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl,
                                                glw::GLuint sampler, glw::GLenum pname, const tcu::UVec4 &expected,
                                                QueryType type)
{
    QueriedState state;

    querySamplerState(result, gl, type, sampler, pname, state);

    if (!state.isUndefined())
        verifyUnsignedIntegerVec4(result, state, expected);
}

} // namespace StateQueryUtil
} // namespace gls
} // namespace deqp

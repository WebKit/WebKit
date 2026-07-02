#ifndef _GLSSTATEQUERYUTIL_HPP
#define _GLSSTATEQUERYUTIL_HPP
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
 * \brief State Query test utils.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestContext.hpp"
#include "tcuResultCollector.hpp"
#include "glwDefs.hpp"
#include "deMath.h"

namespace glu
{
class CallLogWrapper;
} // namespace glu

namespace deqp
{
namespace gls
{
namespace StateQueryUtil
{

#define GLS_COLLECT_GL_ERROR(RES, ERR, MSG)                                                          \
    do                                                                                               \
    {                                                                                                \
        const uint32_t err = (ERR);                                                                  \
        if (err != GL_NO_ERROR)                                                                      \
            (RES).fail(std::string("Got Error ") + glu::getErrorStr(err).toString() + ": " + (MSG)); \
    } while (false)

/*--------------------------------------------------------------------*//*!
 * \brief Rounds given float to the nearest integer (half up).
 *
 * Returns the nearest integer for a float argument. In the case that there
 * are two nearest integers at the equal distance (aka. the argument is of
 * form x.5), the integer with the higher value is chosen. (x.5 rounds to x+1)
 *//*--------------------------------------------------------------------*/
template <typename T>
T roundGLfloatToNearestIntegerHalfUp(float val)
{
    return (T)(deFloatFloor(val + 0.5f));
}

/*--------------------------------------------------------------------*//*!
 * \brief Rounds given float to the nearest integer (half down).
 *
 * Returns the nearest integer for a float argument. In the case that there
 * are two nearest integers at the equal distance (aka. the argument is of
 * form x.5), the integer with the higher value is chosen. (x.5 rounds to x)
 *//*--------------------------------------------------------------------*/
template <typename T>
T roundGLfloatToNearestIntegerHalfDown(float val)
{
    return (T)(deFloatCeil(val - 0.5f));
}

template <typename T>
class StateQueryMemoryWriteGuard
{
public:
    StateQueryMemoryWriteGuard(void);

    operator T &(void);
    T *operator&(void);

    bool isUndefined(void) const;
    bool isMemoryContaminated(void) const;
    bool isPreguardContaminated(void) const;
    bool isPostguardContaminated(void) const;
    bool verifyValidity(tcu::TestContext &testCtx) const;
    bool verifyValidity(tcu::ResultCollector &result) const;

    const T &get(void) const
    {
        return m_value;
    }

private:
    enum
    {
        WRITE_GUARD_VALUE = 0xDE
    };

    T m_preguard;
    T m_value;
    T m_postguard; // \note guards are not const qualified since the GL implementation might modify them
};

template <typename T>
StateQueryMemoryWriteGuard<T>::StateQueryMemoryWriteGuard(void)
{
    DE_STATIC_ASSERT(sizeof(T) * 3 == sizeof(StateQueryMemoryWriteGuard<T>)); // tightly packed

    deMemset(&m_preguard, WRITE_GUARD_VALUE, sizeof(m_preguard));
    deMemset(&m_value, WRITE_GUARD_VALUE, sizeof(m_value));
    deMemset(&m_postguard, WRITE_GUARD_VALUE, sizeof(m_postguard));
}

template <typename T>
StateQueryMemoryWriteGuard<T>::operator T &(void)
{
    return m_value;
}

template <typename T>
T *StateQueryMemoryWriteGuard<T>::operator&(void)
{
    return &m_value;
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::isUndefined() const
{
    for (size_t i = 0; i < sizeof(T); ++i)
        if (((uint8_t *)&m_value)[i] != (uint8_t)WRITE_GUARD_VALUE)
            return false;
    return true;
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::isMemoryContaminated() const
{
    return isPreguardContaminated() || isPostguardContaminated();
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::isPreguardContaminated(void) const
{
    for (size_t i = 0; i < sizeof(T); ++i)
        if (((uint8_t *)&m_preguard)[i] != (uint8_t)WRITE_GUARD_VALUE)
            return true;
    return false;
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::isPostguardContaminated(void) const
{
    for (size_t i = 0; i < sizeof(T); ++i)
        if (((uint8_t *)&m_postguard)[i] != (uint8_t)WRITE_GUARD_VALUE)
            return true;
    return false;
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::verifyValidity(tcu::TestContext &testCtx) const
{
    using tcu::TestLog;

    if (isPreguardContaminated())
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Pre-guard value was modified " << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS || testCtx.getTestResult() == QP_TEST_RESULT_LAST)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Get* did an illegal memory write");

        return false;
    }
    else if (isPostguardContaminated())
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Post-guard value was modified " << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS || testCtx.getTestResult() == QP_TEST_RESULT_LAST)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Get* did an illegal memory write");

        return false;
    }
    else if (isUndefined())
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Get* did not return a value" << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS || testCtx.getTestResult() == QP_TEST_RESULT_LAST)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Get* did not return a value");

        return false;
    }

    return true;
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::verifyValidity(tcu::ResultCollector &result) const
{
    using tcu::TestLog;

    if (isPreguardContaminated())
    {
        result.fail("pre-guard value was modified");
        return false;
    }
    else if (isPostguardContaminated())
    {
        result.fail("post-guard value was modified");
        return false;
    }
    else if (isUndefined())
    {
        result.fail("Get* did not return a value");
        return false;
    }

    return true;
}

template <typename T>
std::ostream &operator<<(std::ostream &str, const StateQueryMemoryWriteGuard<T> &guard)
{
    return str << guard.get();
}

// Verifiers

enum QueryType
{
    QUERY_BOOLEAN = 0,
    QUERY_BOOLEAN_VEC4,
    QUERY_ISENABLED,
    QUERY_INTEGER,
    QUERY_INTEGER64,
    QUERY_FLOAT,

    // indexed
    QUERY_INDEXED_BOOLEAN,
    QUERY_INDEXED_BOOLEAN_VEC4,
    QUERY_INDEXED_ISENABLED,
    QUERY_INDEXED_INTEGER,
    QUERY_INDEXED_INTEGER_VEC4,
    QUERY_INDEXED_INTEGER64,
    QUERY_INDEXED_INTEGER64_VEC4,

    // attributes
    QUERY_ATTRIBUTE_INTEGER,
    QUERY_ATTRIBUTE_FLOAT,
    QUERY_ATTRIBUTE_PURE_INTEGER,
    QUERY_ATTRIBUTE_PURE_UNSIGNED_INTEGER,

    // fb
    QUERY_FRAMEBUFFER_INTEGER,

    // program
    QUERY_PROGRAM_INTEGER,
    QUERY_PROGRAM_INTEGER_VEC3,

    // program pipeline
    QUERY_PIPELINE_INTEGER,

    // texture param
    QUERY_TEXTURE_PARAM_INTEGER,
    QUERY_TEXTURE_PARAM_FLOAT,
    QUERY_TEXTURE_PARAM_PURE_INTEGER,
    QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER,
    QUERY_TEXTURE_PARAM_INTEGER_VEC4,
    QUERY_TEXTURE_PARAM_FLOAT_VEC4,
    QUERY_TEXTURE_PARAM_PURE_INTEGER_VEC4,
    QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER_VEC4,

    // texture level
    QUERY_TEXTURE_LEVEL_INTEGER,
    QUERY_TEXTURE_LEVEL_FLOAT,

    // pointer
    QUERY_POINTER,

    // object states
    QUERY_ISTEXTURE,

    // query queries
    QUERY_QUERY,

    // sampler state
    QUERY_SAMPLER_PARAM_INTEGER,
    QUERY_SAMPLER_PARAM_FLOAT,
    QUERY_SAMPLER_PARAM_PURE_INTEGER,
    QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER,
    QUERY_SAMPLER_PARAM_INTEGER_VEC4,
    QUERY_SAMPLER_PARAM_FLOAT_VEC4,
    QUERY_SAMPLER_PARAM_PURE_INTEGER_VEC4,
    QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER_VEC4,

    QUERY_LAST
};

enum DataType
{
    DATATYPE_BOOLEAN = 0,
    DATATYPE_INTEGER,
    DATATYPE_INTEGER64,
    DATATYPE_FLOAT16,
    DATATYPE_FLOAT,
    DATATYPE_UNSIGNED_INTEGER,
    DATATYPE_INTEGER_VEC3,
    DATATYPE_FLOAT_VEC4,
    DATATYPE_INTEGER_VEC4,
    DATATYPE_INTEGER64_VEC4,
    DATATYPE_UNSIGNED_INTEGER_VEC4,
    DATATYPE_BOOLEAN_VEC4,
    DATATYPE_POINTER,

    DATATYPE_LAST
};

class QueriedState
{
public:
    typedef glw::GLint GLIntVec3[3];
    typedef glw::GLint GLIntVec4[4];
    typedef glw::GLuint GLUintVec4[4];
    typedef glw::GLfloat GLFloatVec4[4];
    typedef bool BooleanVec4[4];
    typedef glw::GLint64 GLInt64Vec4[4];

    QueriedState(void);
    explicit QueriedState(glw::GLint);
    explicit QueriedState(glw::GLint64);
    explicit QueriedState(bool);
    explicit QueriedState(glw::GLfloat);
    explicit QueriedState(glw::GLuint);
    explicit QueriedState(const GLIntVec3 &);
    explicit QueriedState(void *);
    explicit QueriedState(const GLIntVec4 &);
    explicit QueriedState(const GLUintVec4 &);
    explicit QueriedState(const GLFloatVec4 &);
    explicit QueriedState(const BooleanVec4 &);
    explicit QueriedState(const GLInt64Vec4 &);

    bool isUndefined(void) const;
    DataType getType(void) const;

    glw::GLint &getIntAccess(void);
    glw::GLint64 &getInt64Access(void);
    bool &getBoolAccess(void);
    glw::GLfloat &getFloatAccess(void);
    glw::GLuint &getUintAccess(void);
    GLIntVec3 &getIntVec3Access(void);
    void *&getPtrAccess(void);
    GLIntVec4 &getIntVec4Access(void);
    GLUintVec4 &getUintVec4Access(void);
    GLFloatVec4 &getFloatVec4Access(void);
    BooleanVec4 &getBooleanVec4Access(void);
    GLInt64Vec4 &getInt64Vec4Access(void);

private:
    DataType m_type;
    union
    {
        glw::GLint vInt;
        glw::GLint64 vInt64;
        bool vBool;
        glw::GLfloat vFloat;
        glw::GLuint vUint;
        GLIntVec3 vIntVec3;
        void *vPtr;
        GLIntVec4 vIntVec4;
        GLUintVec4 vUintVec4;
        GLFloatVec4 vFloatVec4;
        BooleanVec4 vBooleanVec4;
        GLInt64Vec4 vInt64Vec4;
    } m_v;
};

// query functions

void queryState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum pname,
                QueriedState &state);
void queryIndexedState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                       int index, QueriedState &state);
void queryAttributeState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                         int index, QueriedState &state);
void queryFramebufferState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                           glw::GLenum pname, QueriedState &state);
void queryProgramState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLuint program,
                       glw::GLenum pname, QueriedState &state);
void queryPipelineState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLuint pipeline,
                        glw::GLenum pname, QueriedState &state);
void queryTextureParamState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                            glw::GLenum pname, QueriedState &state);
void queryTextureLevelState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                            int level, glw::GLenum pname, QueriedState &state);
void queryPointerState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum pname,
                       QueriedState &state);
void queryObjectState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLuint handle,
                      QueriedState &state);
void queryQueryState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLenum target,
                     glw::GLenum pname, QueriedState &state);
void querySamplerState(tcu::ResultCollector &result, glu::CallLogWrapper &gl, QueryType type, glw::GLuint sampler,
                       glw::GLenum pname, QueriedState &state);

// verification functions

void verifyBoolean(tcu::ResultCollector &result, QueriedState &state, bool expected);
void verifyInteger(tcu::ResultCollector &result, QueriedState &state, int expected);
void verifyIntegerMin(tcu::ResultCollector &result, QueriedState &state, int minValue);
void verifyIntegerMax(tcu::ResultCollector &result, QueriedState &state, int maxValue);
void verifyIntegersEqual(tcu::ResultCollector &result, QueriedState &stateA, QueriedState &stateB);
void verifyFloat(tcu::ResultCollector &result, QueriedState &state, float expected);
void verifyFloatMin(tcu::ResultCollector &result, QueriedState &state, float minValue);
void verifyFloatMax(tcu::ResultCollector &result, QueriedState &state, float maxValue);
void verifyIntegerVec3(tcu::ResultCollector &result, QueriedState &state, const tcu::IVec3 &expected);
void verifyIntegerVec4(tcu::ResultCollector &result, QueriedState &state, const tcu::IVec4 &expected);
void verifyUnsignedIntegerVec4(tcu::ResultCollector &result, QueriedState &state, const tcu::UVec4 &expected);
void verifyFloatVec4(tcu::ResultCollector &result, QueriedState &state, const tcu::Vec4 &expected);
void verifyBooleanVec4(tcu::ResultCollector &result, QueriedState &state, const tcu::BVec4 &expected);
void verifyPointer(tcu::ResultCollector &result, QueriedState &state, const void *expected);
void verifyNormalizedI32Vec4(tcu::ResultCollector &result, QueriedState &state, const tcu::IVec4 &expected);

// Helper functions that both query and verify

void verifyStateBoolean(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, bool expected,
                        QueryType type);
void verifyStateInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int expected,
                        QueryType type);
void verifyStateIntegerMin(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int minValue,
                           QueryType type);
void verifyStateIntegerMax(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int maxValue,
                           QueryType type);
void verifyStateIntegerEqualToOther(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                    glw::GLenum other, QueryType type);
void verifyStateFloat(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, float reference,
                      QueryType type);
void verifyStateFloatMin(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, float minValue,
                         QueryType type);
void verifyStateFloatMax(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, float maxValue,
                         QueryType type);
void verifyStatePointer(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, const void *expected,
                        QueryType type);
void verifyStateIndexedBoolean(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                               bool expected, QueryType type);
void verifyStateIndexedBooleanVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                                   const tcu::BVec4 &expected, QueryType type);
void verifyStateIndexedInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                               int expected, QueryType type);
void verifyStateIndexedIntegerMin(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                                  int minValue, QueryType type);
void verifyStateAttributeInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target, int index,
                                 int expected, QueryType type);
void verifyStateFramebufferInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                   glw::GLenum pname, int expected, QueryType type);
void verifyStateFramebufferIntegerMin(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                      glw::GLenum pname, int minValue, QueryType type);
void verifyStateProgramInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint program,
                               glw::GLenum pname, int expected, QueryType type);
void verifyStateProgramIntegerVec3(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint program,
                                   glw::GLenum pname, const tcu::IVec3 &expected, QueryType type);
void verifyStatePipelineInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint pipeline,
                                glw::GLenum pname, int expected, QueryType type);
void verifyStateTextureParamInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                    glw::GLenum pname, int expected, QueryType type);
void verifyStateTextureParamFloat(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                  glw::GLenum pname, float expected, QueryType type);
void verifyStateTextureParamFloatVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                      glw::GLenum pname, const tcu::Vec4 &expected, QueryType type);
void verifyStateTextureParamNormalizedI32Vec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                              glw::GLenum pname, const tcu::IVec4 &expected, QueryType type);
void verifyStateTextureParamIntegerVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                        glw::GLenum pname, const tcu::IVec4 &expected, QueryType type);
void verifyStateTextureParamUnsignedIntegerVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl,
                                                glw::GLenum target, glw::GLenum pname, const tcu::UVec4 &expected,
                                                QueryType type);
void verifyStateTextureLevelInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                                    int level, glw::GLenum pname, int expected, QueryType type);
void verifyStateObjectBoolean(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint handle, bool expected,
                              QueryType type);
void verifyStateQueryInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLenum target,
                             glw::GLenum pname, int expected, QueryType type);
void verifyStateSamplerParamInteger(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint sampler,
                                    glw::GLenum pname, int expected, QueryType type);
void verifyStateSamplerParamFloat(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint sampler,
                                  glw::GLenum pname, float expected, QueryType type);
void verifyStateSamplerParamFloatVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint sampler,
                                      glw::GLenum pname, const tcu::Vec4 &expected, QueryType type);
void verifyStateSamplerParamNormalizedI32Vec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl,
                                              glw::GLuint sampler, glw::GLenum pname, const tcu::IVec4 &expected,
                                              QueryType type);
void verifyStateSamplerParamIntegerVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl, glw::GLuint sampler,
                                        glw::GLenum pname, const tcu::IVec4 &expected, QueryType type);
void verifyStateSamplerParamUnsignedIntegerVec4(tcu::ResultCollector &result, glu::CallLogWrapper &gl,
                                                glw::GLuint sampler, glw::GLenum pname, const tcu::UVec4 &expected,
                                                QueryType type);

} // namespace StateQueryUtil
} // namespace gls
} // namespace deqp

#endif // _GLSSTATEQUERYUTIL_HPP

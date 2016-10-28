//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// queryconversions.cpp: Implementation of state query cast conversions

#include "libANGLE/queryconversions.h"

#include <vector>

#include "libANGLE/Context.h"
#include "common/utilities.h"

namespace gl
{

namespace
{

GLint64 ExpandFloatToInteger(GLfloat value)
{
    return static_cast<GLint64>((static_cast<double>(0xFFFFFFFFULL) * value - 1.0) / 2.0);
}

template <typename QueryT>
QueryT ClampToQueryRange(GLint64 value)
{
    const GLint64 min = static_cast<GLint64>(std::numeric_limits<QueryT>::min());
    const GLint64 max = static_cast<GLint64>(std::numeric_limits<QueryT>::max());
    return static_cast<QueryT>(clamp(value, min, max));
}

template <typename QueryT, typename NativeT>
QueryT CastStateValueToInt(GLenum pname, NativeT value)
{
    GLenum queryType  = GLTypeToGLenum<QueryT>::value;
    GLenum nativeType = GLTypeToGLenum<NativeT>::value;

    if (nativeType == GL_FLOAT)
    {
        // RGBA color values and DepthRangeF values are converted to integer using Equation 2.4 from Table 4.5
        if (pname == GL_DEPTH_RANGE || pname == GL_COLOR_CLEAR_VALUE || pname == GL_DEPTH_CLEAR_VALUE || pname == GL_BLEND_COLOR)
        {
            return ClampToQueryRange<QueryT>(ExpandFloatToInteger(static_cast<GLfloat>(value)));
        }
        else
        {
            return gl::iround<QueryT>(static_cast<GLfloat>(value));
        }
    }

    // Clamp 64-bit int values when casting to int
    if (nativeType == GL_INT_64_ANGLEX && queryType == GL_INT)
    {
        GLint64 minIntValue = static_cast<GLint64>(std::numeric_limits<GLint>::min());
        GLint64 maxIntValue = static_cast<GLint64>(std::numeric_limits<GLint>::max());
        GLint64 clampedValue = std::max(std::min(static_cast<GLint64>(value), maxIntValue), minIntValue);
        return static_cast<QueryT>(clampedValue);
    }

    return static_cast<QueryT>(value);
}

template <typename QueryT, typename NativeT>
QueryT CastStateValue(GLenum pname, NativeT value)
{
    GLenum queryType = GLTypeToGLenum<QueryT>::value;

    switch (queryType)
    {
        case GL_INT:
            return CastStateValueToInt<QueryT, NativeT>(pname, value);
        case GL_INT_64_ANGLEX:
            return CastStateValueToInt<QueryT, NativeT>(pname, value);
        case GL_FLOAT:
            return static_cast<QueryT>(value);
        case GL_BOOL:
            return static_cast<QueryT>(value == static_cast<NativeT>(0) ? GL_FALSE : GL_TRUE);
        default:
            UNREACHABLE();
            return 0;
    }
}

}  // anonymous namespace

template <typename QueryT>
void CastStateValues(Context *context, GLenum nativeType, GLenum pname,
                     unsigned int numParams, QueryT *outParams)
{
    if (nativeType == GL_INT)
    {
        std::vector<GLint> intParams(numParams, 0);
        context->getIntegerv(pname, intParams.data());

        for (unsigned int i = 0; i < numParams; ++i)
        {
            outParams[i] = CastStateValue<QueryT>(pname, intParams[i]);
        }
    }
    else if (nativeType == GL_BOOL)
    {
        std::vector<GLboolean> boolParams(numParams, GL_FALSE);
        context->getBooleanv(pname, boolParams.data());

        for (unsigned int i = 0; i < numParams; ++i)
        {
            outParams[i] = (boolParams[i] == GL_FALSE ? static_cast<QueryT>(0) : static_cast<QueryT>(1));
        }
    }
    else if (nativeType == GL_FLOAT)
    {
        std::vector<GLfloat> floatParams(numParams, 0.0f);
        context->getFloatv(pname, floatParams.data());

        for (unsigned int i = 0; i < numParams; ++i)
        {
            outParams[i] = CastStateValue<QueryT>(pname, floatParams[i]);
        }
    }
    else if (nativeType == GL_INT_64_ANGLEX)
    {
        std::vector<GLint64> int64Params(numParams, 0);
        context->getInteger64v(pname, int64Params.data());

        for (unsigned int i = 0; i < numParams; ++i)
        {
            outParams[i] = CastStateValue<QueryT>(pname, int64Params[i]);
        }
    }
    else UNREACHABLE();
}

// Explicit template instantiation (how we export template functions in different files)
// The calls below will make CastStateValues successfully link with the GL state query types
// The GL state query API types are: bool, int, uint, float, int64

template void CastStateValues<GLboolean>(Context *, GLenum, GLenum, unsigned int, GLboolean *);
template void CastStateValues<GLint>(Context *, GLenum, GLenum, unsigned int, GLint *);
template void CastStateValues<GLuint>(Context *, GLenum, GLenum, unsigned int, GLuint *);
template void CastStateValues<GLfloat>(Context *, GLenum, GLenum, unsigned int, GLfloat *);
template void CastStateValues<GLint64>(Context *, GLenum, GLenum, unsigned int, GLint64 *);

template <typename QueryT>
void CastIndexedStateValues(Context *context,
                            GLenum nativeType,
                            GLenum pname,
                            GLuint index,
                            unsigned int numParams,
                            QueryT *outParams)
{
    if (nativeType == GL_INT)
    {
        std::vector<GLint> intParams(numParams, 0);
        context->getIntegeri_v(pname, index, intParams.data());

        for (unsigned int i = 0; i < numParams; ++i)
        {
            outParams[i] = CastStateValue<QueryT>(pname, intParams[i]);
        }
    }
    else if (nativeType == GL_BOOL)
    {
        std::vector<GLboolean> boolParams(numParams, GL_FALSE);
        context->getBooleani_v(pname, index, boolParams.data());

        for (unsigned int i = 0; i < numParams; ++i)
        {
            outParams[i] =
                (boolParams[i] == GL_FALSE ? static_cast<QueryT>(0) : static_cast<QueryT>(1));
        }
    }
    else if (nativeType == GL_INT_64_ANGLEX)
    {
        std::vector<GLint64> int64Params(numParams, 0);
        context->getInteger64i_v(pname, index, int64Params.data());

        for (unsigned int i = 0; i < numParams; ++i)
        {
            outParams[i] = CastStateValue<QueryT>(pname, int64Params[i]);
        }
    }
    else
        UNREACHABLE();
}

template void CastIndexedStateValues<GLboolean>(Context *,
                                                GLenum,
                                                GLenum,
                                                GLuint index,
                                                unsigned int,
                                                GLboolean *);
template void CastIndexedStateValues<GLint>(Context *,
                                            GLenum,
                                            GLenum,
                                            GLuint index,
                                            unsigned int,
                                            GLint *);
template void CastIndexedStateValues<GLuint>(Context *,
                                             GLenum,
                                             GLenum,
                                             GLuint index,
                                             unsigned int,
                                             GLuint *);
template void CastIndexedStateValues<GLfloat>(Context *,
                                              GLenum,
                                              GLenum,
                                              GLuint index,
                                              unsigned int,
                                              GLfloat *);
template void CastIndexedStateValues<GLint64>(Context *,
                                              GLenum,
                                              GLenum,
                                              GLuint index,
                                              unsigned int,
                                              GLint64 *);
}

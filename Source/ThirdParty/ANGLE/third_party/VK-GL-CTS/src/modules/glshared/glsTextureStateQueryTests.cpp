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
 * \brief Texture State Query tests.
 *//*--------------------------------------------------------------------*/

#include "glsTextureStateQueryTests.hpp"
#include "gluStrUtil.hpp"
#include "gluObjectWrapper.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluContextInfo.hpp"
#include "gluTextureUtil.hpp"
#include "glwEnums.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"

namespace deqp
{
namespace gls
{
namespace TextureStateQueryTests
{
namespace
{

using namespace glw;
using namespace gls::StateQueryUtil;

static glw::GLenum mapTesterToPname(TesterType tester)
{

#define CASE_ALL_SETTERS(X) \
    case X:                 \
    case X##_SET_PURE_INT:  \
    case X##_SET_PURE_UINT

    switch (tester)
    {
        CASE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_R) : return GL_TEXTURE_SWIZZLE_R;
        CASE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_G) : return GL_TEXTURE_SWIZZLE_G;
        CASE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_B) : return GL_TEXTURE_SWIZZLE_B;
        CASE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_A) : return GL_TEXTURE_SWIZZLE_A;

    CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_S) : case TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER:
        return GL_TEXTURE_WRAP_S;

    CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_T) : case TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER:
        return GL_TEXTURE_WRAP_T;

    CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_R) : case TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER:
        return GL_TEXTURE_WRAP_R;

        CASE_ALL_SETTERS(TESTER_TEXTURE_MAG_FILTER) : return GL_TEXTURE_MAG_FILTER;
        CASE_ALL_SETTERS(TESTER_TEXTURE_MIN_FILTER) : return GL_TEXTURE_MIN_FILTER;
        CASE_ALL_SETTERS(TESTER_TEXTURE_MIN_LOD) : return GL_TEXTURE_MIN_LOD;
        CASE_ALL_SETTERS(TESTER_TEXTURE_MAX_LOD) : return GL_TEXTURE_MAX_LOD;
        CASE_ALL_SETTERS(TESTER_TEXTURE_BASE_LEVEL) : return GL_TEXTURE_BASE_LEVEL;
        CASE_ALL_SETTERS(TESTER_TEXTURE_MAX_LEVEL) : return GL_TEXTURE_MAX_LEVEL;
        CASE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_MODE) : return GL_TEXTURE_COMPARE_MODE;
        CASE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_FUNC) : return GL_TEXTURE_COMPARE_FUNC;
    case TESTER_TEXTURE_IMMUTABLE_LEVELS:
        return GL_TEXTURE_IMMUTABLE_LEVELS;
    case TESTER_TEXTURE_IMMUTABLE_FORMAT:
        return GL_TEXTURE_IMMUTABLE_FORMAT;
        CASE_ALL_SETTERS(TESTER_DEPTH_STENCIL_TEXTURE_MODE) : return GL_DEPTH_STENCIL_TEXTURE_MODE;
        CASE_ALL_SETTERS(TESTER_TEXTURE_SRGB_DECODE_EXT) : return GL_TEXTURE_SRGB_DECODE_EXT;
    case TESTER_TEXTURE_BORDER_COLOR:
        return GL_TEXTURE_BORDER_COLOR;

    default:
        DE_ASSERT(false);
        return -1;
    }

#undef CASE_PURE_SETTERS
}

static bool querySupportsSigned(QueryType type)
{
    return type != QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER && type != QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER;
}

static bool isPureIntTester(TesterType tester)
{
#define HANDLE_ALL_SETTERS(X) \
    case X:                   \
    case X##_SET_PURE_UINT:   \
        return false;         \
    case X##_SET_PURE_INT:    \
        return true;

    switch (tester)
    {
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_R)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_G)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_B)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_A)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_WRAP_S)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_WRAP_T)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_WRAP_R)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MAG_FILTER)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MIN_FILTER)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MIN_LOD)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MAX_LOD)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_BASE_LEVEL)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MAX_LEVEL)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_MODE)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_FUNC)
        HANDLE_ALL_SETTERS(TESTER_DEPTH_STENCIL_TEXTURE_MODE)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SRGB_DECODE_EXT)

    case TESTER_TEXTURE_IMMUTABLE_LEVELS:
    case TESTER_TEXTURE_IMMUTABLE_FORMAT:
    case TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_BORDER_COLOR:
        return false;

    default:
        DE_ASSERT(false);
        return false;
    }

#undef HANDLE_ALL_SETTERS
}

static bool isPureUintTester(TesterType tester)
{
#define HANDLE_ALL_SETTERS(X) \
    case X:                   \
    case X##_SET_PURE_INT:    \
        return false;         \
    case X##_SET_PURE_UINT:   \
        return true;

    switch (tester)
    {
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_R)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_G)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_B)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_A)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_WRAP_S)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_WRAP_T)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_WRAP_R)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MAG_FILTER)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MIN_FILTER)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MIN_LOD)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MAX_LOD)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_BASE_LEVEL)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_MAX_LEVEL)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_MODE)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_FUNC)
        HANDLE_ALL_SETTERS(TESTER_DEPTH_STENCIL_TEXTURE_MODE)
        HANDLE_ALL_SETTERS(TESTER_TEXTURE_SRGB_DECODE_EXT)

    case TESTER_TEXTURE_IMMUTABLE_LEVELS:
    case TESTER_TEXTURE_IMMUTABLE_FORMAT:
    case TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_BORDER_COLOR:
        return false;

    default:
        DE_ASSERT(false);
        return false;
    }

#undef HANDLE_ALL_SETTERS
}

class RequiredExtensions
{
public:
    RequiredExtensions(void)
    {
    }
    explicit RequiredExtensions(const char *ext)
    {
        add(ext);
    }
    RequiredExtensions(const char *extA, const char *extB)
    {
        add(extA);
        add(extB);
    }

    void add(const char *ext);
    void add(const RequiredExtensions &other);
    void check(const glu::ContextInfo &) const;

private:
    std::vector<const char *> m_extensions;
};

void RequiredExtensions::add(const char *ext)
{
    for (int ndx = 0; ndx < (int)m_extensions.size(); ++ndx)
        if (strcmp(m_extensions[ndx], ext) == 0)
            return;
    m_extensions.push_back(ext);
}

void RequiredExtensions::add(const RequiredExtensions &other)
{
    for (int ndx = 0; ndx < (int)other.m_extensions.size(); ++ndx)
        add(other.m_extensions[ndx]);
}

void RequiredExtensions::check(const glu::ContextInfo &ctxInfo) const
{
    std::vector<const char *> failedExtensions;

    for (int ndx = 0; ndx < (int)m_extensions.size(); ++ndx)
        if (!ctxInfo.isExtensionSupported(m_extensions[ndx]))
            failedExtensions.push_back(m_extensions[ndx]);

    if (!failedExtensions.empty())
    {
        std::ostringstream buf;
        buf << "Test requires extension: ";

        for (int ndx = 0; ndx < (int)failedExtensions.size(); ++ndx)
        {
            if (ndx)
                buf << ", ";
            buf << failedExtensions[ndx];
        }

        throw tcu::NotSupportedError(buf.str());
    }
}

namespace es30
{

static bool isCoreTextureTarget(glw::GLenum target)
{
    return target == GL_TEXTURE_2D || target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
           target == GL_TEXTURE_CUBE_MAP;
}

static RequiredExtensions getTextureTargetExtension(glw::GLenum target)
{
    DE_UNREF(target);
    DE_ASSERT(false);
    return RequiredExtensions();
}

static bool isCoreTextureParam(glw::GLenum pname)
{
    return pname == GL_TEXTURE_BASE_LEVEL || pname == GL_TEXTURE_COMPARE_MODE || pname == GL_TEXTURE_COMPARE_FUNC ||
           pname == GL_TEXTURE_MAG_FILTER || pname == GL_TEXTURE_MAX_LEVEL || pname == GL_TEXTURE_MAX_LOD ||
           pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_SWIZZLE_R ||
           pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A ||
           pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R ||
           pname == GL_TEXTURE_IMMUTABLE_FORMAT || pname == GL_TEXTURE_IMMUTABLE_LEVELS;
}

static RequiredExtensions getTextureParamExtension(glw::GLenum pname)
{
    DE_UNREF(pname);
    DE_ASSERT(false);
    return RequiredExtensions();
}

static bool isCoreQuery(QueryType query)
{
    return query == QUERY_TEXTURE_PARAM_INTEGER || query == QUERY_TEXTURE_PARAM_FLOAT ||
           query == QUERY_TEXTURE_PARAM_INTEGER_VEC4 || query == QUERY_TEXTURE_PARAM_FLOAT_VEC4 ||
           query == QUERY_SAMPLER_PARAM_INTEGER || query == QUERY_SAMPLER_PARAM_FLOAT ||
           query == QUERY_SAMPLER_PARAM_INTEGER_VEC4 || query == QUERY_SAMPLER_PARAM_FLOAT_VEC4;
}

static RequiredExtensions getQueryExtension(QueryType query)
{
    DE_UNREF(query);
    DE_ASSERT(false);
    return RequiredExtensions();
}

static bool isCoreTester(TesterType tester)
{
    return tester == TESTER_TEXTURE_SWIZZLE_R || tester == TESTER_TEXTURE_SWIZZLE_G ||
           tester == TESTER_TEXTURE_SWIZZLE_B || tester == TESTER_TEXTURE_SWIZZLE_A ||
           tester == TESTER_TEXTURE_WRAP_S || tester == TESTER_TEXTURE_WRAP_T || tester == TESTER_TEXTURE_WRAP_R ||
           tester == TESTER_TEXTURE_MAG_FILTER || tester == TESTER_TEXTURE_MIN_FILTER ||
           tester == TESTER_TEXTURE_MIN_LOD || tester == TESTER_TEXTURE_MAX_LOD ||
           tester == TESTER_TEXTURE_BASE_LEVEL || tester == TESTER_TEXTURE_MAX_LEVEL ||
           tester == TESTER_TEXTURE_COMPARE_MODE || tester == TESTER_TEXTURE_COMPARE_FUNC ||
           tester == TESTER_TEXTURE_IMMUTABLE_LEVELS || tester == TESTER_TEXTURE_IMMUTABLE_FORMAT;
}

static RequiredExtensions getTesterExtension(TesterType tester)
{
    DE_UNREF(tester);
    DE_ASSERT(false);
    return RequiredExtensions();
}

} // namespace es30

namespace es31
{

static bool isCoreTextureTarget(glw::GLenum target)
{
    return es30::isCoreTextureTarget(target) || target == GL_TEXTURE_2D_MULTISAMPLE;
}

static RequiredExtensions getTextureTargetExtension(glw::GLenum target)
{
    switch (target)
    {
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        return RequiredExtensions("GL_OES_texture_storage_multisample_2d_array");
    case GL_TEXTURE_BUFFER:
        return RequiredExtensions("GL_EXT_texture_buffer");
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        return RequiredExtensions("GL_EXT_texture_cube_map_array");
    default:
        DE_ASSERT(false);
        return RequiredExtensions();
    }
}

static bool isCoreTextureParam(glw::GLenum pname)
{
    return es30::isCoreTextureParam(pname) || pname == GL_DEPTH_STENCIL_TEXTURE_MODE;
}

static RequiredExtensions getTextureParamExtension(glw::GLenum pname)
{
    switch (pname)
    {
    case GL_TEXTURE_SRGB_DECODE_EXT:
        return RequiredExtensions("GL_EXT_texture_sRGB_decode");
    case GL_TEXTURE_BORDER_COLOR:
        return RequiredExtensions("GL_EXT_texture_border_clamp");
    default:
        DE_ASSERT(false);
        return RequiredExtensions();
    }
}

static bool isCoreQuery(QueryType query)
{
    return es30::isCoreQuery(query);
}

static RequiredExtensions getQueryExtension(QueryType query)
{
    switch (query)
    {
    case QUERY_TEXTURE_PARAM_PURE_INTEGER:
    case QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER:
    case QUERY_TEXTURE_PARAM_PURE_INTEGER_VEC4:
    case QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER_VEC4:
    case QUERY_SAMPLER_PARAM_PURE_INTEGER:
    case QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER:
    case QUERY_SAMPLER_PARAM_PURE_INTEGER_VEC4:
    case QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER_VEC4:
        return RequiredExtensions("GL_EXT_texture_border_clamp");

    default:
        DE_ASSERT(false);
        return RequiredExtensions();
    }
}

static bool isCoreTester(TesterType tester)
{
    return es30::isCoreTester(tester) || tester == TESTER_DEPTH_STENCIL_TEXTURE_MODE;
}

static RequiredExtensions getTesterExtension(TesterType tester)
{
#define CASE_PURE_SETTERS(X) \
    case X##_SET_PURE_INT:   \
    case X##_SET_PURE_UINT

    switch (tester)
    {
    CASE_PURE_SETTERS(TESTER_TEXTURE_SWIZZLE_R)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_SWIZZLE_G)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_SWIZZLE_B)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_SWIZZLE_A)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_WRAP_S)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_WRAP_T)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_WRAP_R)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_MAG_FILTER)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_MIN_FILTER)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_MIN_LOD)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_MAX_LOD)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_BASE_LEVEL)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_MAX_LEVEL)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_COMPARE_MODE)
        : CASE_PURE_SETTERS(TESTER_TEXTURE_COMPARE_FUNC)
        : CASE_PURE_SETTERS(TESTER_DEPTH_STENCIL_TEXTURE_MODE)
        : case TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_BORDER_COLOR:
        return RequiredExtensions("GL_EXT_texture_border_clamp");

    case TESTER_TEXTURE_SRGB_DECODE_EXT:
        return RequiredExtensions("GL_EXT_texture_sRGB_decode");

        CASE_PURE_SETTERS(TESTER_TEXTURE_SRGB_DECODE_EXT)
            : return RequiredExtensions("GL_EXT_texture_sRGB_decode", "GL_EXT_texture_border_clamp");

    default:
        DE_ASSERT(false);
        return RequiredExtensions();
    }

#undef CASE_PURE_SETTERS
}

} // namespace es31

namespace es32
{

static bool isCoreTextureTarget(glw::GLenum target)
{
    return es31::isCoreTextureTarget(target) || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
           target == GL_TEXTURE_BUFFER || target == GL_TEXTURE_CUBE_MAP_ARRAY;
}

static RequiredExtensions getTextureTargetExtension(glw::GLenum target)
{
    DE_UNREF(target);
    DE_ASSERT(false);
    return RequiredExtensions();
}

static bool isCoreTextureParam(glw::GLenum pname)
{
    return es31::isCoreTextureParam(pname) || pname == GL_TEXTURE_BORDER_COLOR;
}

static RequiredExtensions getTextureParamExtension(glw::GLenum pname)
{
    switch (pname)
    {
    case GL_TEXTURE_SRGB_DECODE_EXT:
        return RequiredExtensions("GL_EXT_texture_sRGB_decode");
    default:
        DE_ASSERT(false);
        return RequiredExtensions();
    }
}

static bool isCoreQuery(QueryType query)
{
    return es31::isCoreQuery(query) || query == QUERY_TEXTURE_PARAM_PURE_INTEGER ||
           query == QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER || query == QUERY_TEXTURE_PARAM_PURE_INTEGER_VEC4 ||
           query == QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER_VEC4 || query == QUERY_SAMPLER_PARAM_PURE_INTEGER ||
           query == QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER || query == QUERY_SAMPLER_PARAM_PURE_INTEGER_VEC4 ||
           query == QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER_VEC4;
}

static RequiredExtensions getQueryExtension(QueryType query)
{
    DE_UNREF(query);
    DE_ASSERT(false);
    return RequiredExtensions();
}

static bool isCoreTester(TesterType tester)
{
#define COMPARE_PURE_SETTERS(TESTER, X) ((TESTER) == X##_SET_PURE_INT) || ((TESTER) == X##_SET_PURE_UINT)

    return es31::isCoreTester(tester) || COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_SWIZZLE_R) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_SWIZZLE_G) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_SWIZZLE_B) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_SWIZZLE_A) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_WRAP_S) || COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_WRAP_T) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_WRAP_R) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_MAG_FILTER) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_MIN_FILTER) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_MIN_LOD) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_MAX_LOD) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_BASE_LEVEL) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_MAX_LEVEL) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_COMPARE_MODE) ||
           COMPARE_PURE_SETTERS(tester, TESTER_TEXTURE_COMPARE_FUNC) ||
           COMPARE_PURE_SETTERS(tester, TESTER_DEPTH_STENCIL_TEXTURE_MODE) ||
           tester == TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER || tester == TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER ||
           tester == TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER || tester == TESTER_TEXTURE_BORDER_COLOR;

#undef COMPARE_PURE_SETTERS
}

static RequiredExtensions getTesterExtension(TesterType tester)
{
#define CASE_PURE_SETTERS(X) \
    case X##_SET_PURE_INT:   \
    case X##_SET_PURE_UINT

    switch (tester)
    {
    CASE_PURE_SETTERS(TESTER_TEXTURE_SRGB_DECODE_EXT) : case TESTER_TEXTURE_SRGB_DECODE_EXT:
        return RequiredExtensions("GL_EXT_texture_sRGB_decode");

        default: DE_ASSERT(false);
        return RequiredExtensions();
    }

#undef CASE_PURE_SETTERS
}

} // namespace es32

namespace gl45
{

static bool isCoreTextureTarget(glw::GLenum target)
{
    return es31::isCoreTextureTarget(target);
}

static RequiredExtensions getTextureTargetExtension(glw::GLenum target)
{
    DE_UNREF(target);
    return RequiredExtensions();
}

static bool isCoreTextureParam(glw::GLenum pname)
{
    return es31::isCoreTextureParam(pname);
}

static RequiredExtensions getTextureParamExtension(glw::GLenum pname)
{
    DE_UNREF(pname);
    return RequiredExtensions();
}

static bool isCoreQuery(QueryType query)
{
    return es31::isCoreQuery(query);
}

static RequiredExtensions getQueryExtension(QueryType query)
{
    DE_UNREF(query);
    return RequiredExtensions();
}

static bool isCoreTester(TesterType tester)
{
    return es31::isCoreTester(tester);
}

static RequiredExtensions getTesterExtension(TesterType tester)
{
    DE_UNREF(tester);
    return RequiredExtensions();
}

} // namespace gl45

static bool isCoreTextureTarget(const glu::ContextType &contextType, glw::GLenum target)
{
    if (contextSupports(contextType, glu::ApiType::core(4, 5)))
        return gl45::isCoreTextureTarget(target);
    else if (contextSupports(contextType, glu::ApiType::es(3, 2)))
        return es32::isCoreTextureTarget(target);
    else if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        return es31::isCoreTextureTarget(target);
    else if (contextSupports(contextType, glu::ApiType::es(3, 0)))
        return es30::isCoreTextureTarget(target);
    else
    {
        DE_ASSERT(false);
        return false;
    }
}

static bool isCoreTextureParam(const glu::ContextType &contextType, glw::GLenum pname)
{
    if (contextSupports(contextType, glu::ApiType::core(4, 5)))
        return gl45::isCoreTextureParam(pname);
    else if (contextSupports(contextType, glu::ApiType::es(3, 2)))
        return es32::isCoreTextureParam(pname);
    else if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        return es31::isCoreTextureParam(pname);
    else if (contextSupports(contextType, glu::ApiType::es(3, 0)))
        return es30::isCoreTextureParam(pname);
    else
    {
        DE_ASSERT(false);
        return false;
    }
}

static bool isCoreQuery(const glu::ContextType &contextType, QueryType query)
{
    if (contextSupports(contextType, glu::ApiType::core(4, 5)))
        return gl45::isCoreQuery(query);
    else if (contextSupports(contextType, glu::ApiType::es(3, 2)))
        return es32::isCoreQuery(query);
    else if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        return es31::isCoreQuery(query);
    else if (contextSupports(contextType, glu::ApiType::es(3, 0)))
        return es30::isCoreQuery(query);
    else
    {
        DE_ASSERT(false);
        return false;
    }
}

static bool isCoreTester(const glu::ContextType &contextType, TesterType tester)
{
    if (contextSupports(contextType, glu::ApiType::core(4, 5)))
        return gl45::isCoreTester(tester);
    else if (contextSupports(contextType, glu::ApiType::es(3, 2)))
        return es32::isCoreTester(tester);
    else if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        return es31::isCoreTester(tester);
    else if (contextSupports(contextType, glu::ApiType::es(3, 0)))
        return es30::isCoreTester(tester);
    else
    {
        DE_ASSERT(false);
        return false;
    }
}

static RequiredExtensions getTextureTargetExtension(const glu::ContextType &contextType, glw::GLenum target)
{
    DE_ASSERT(!isCoreTextureTarget(contextType, target));

    if (contextSupports(contextType, glu::ApiType::core(4, 5)))
        return gl45::getTextureTargetExtension(target);
    else if (contextSupports(contextType, glu::ApiType::es(3, 2)))
        return es32::getTextureTargetExtension(target);
    else if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        return es31::getTextureTargetExtension(target);
    else if (contextSupports(contextType, glu::ApiType::es(3, 0)))
        return es30::getTextureTargetExtension(target);
    else
    {
        DE_ASSERT(false);
        return RequiredExtensions();
    }
}

static RequiredExtensions getTextureParamExtension(const glu::ContextType &contextType, glw::GLenum pname)
{
    DE_ASSERT(!isCoreTextureParam(contextType, pname));

    if (contextSupports(contextType, glu::ApiType::core(4, 5)))
        return gl45::getTextureParamExtension(pname);
    else if (contextSupports(contextType, glu::ApiType::es(3, 2)))
        return es32::getTextureParamExtension(pname);
    else if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        return es31::getTextureParamExtension(pname);
    else if (contextSupports(contextType, glu::ApiType::es(3, 0)))
        return es30::getTextureParamExtension(pname);
    else
    {
        DE_ASSERT(false);
        return RequiredExtensions();
    }
}

static RequiredExtensions getQueryExtension(const glu::ContextType &contextType, QueryType query)
{
    DE_ASSERT(!isCoreQuery(contextType, query));

    if (contextSupports(contextType, glu::ApiType::core(4, 5)))
        return gl45::getQueryExtension(query);
    else if (contextSupports(contextType, glu::ApiType::es(3, 2)))
        return es32::getQueryExtension(query);
    else if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        return es31::getQueryExtension(query);
    else if (contextSupports(contextType, glu::ApiType::es(3, 0)))
        return es30::getQueryExtension(query);
    else
    {
        DE_ASSERT(false);
        return RequiredExtensions();
    }
}

static RequiredExtensions getTesterExtension(const glu::ContextType &contextType, TesterType tester)
{
    DE_ASSERT(!isCoreTester(contextType, tester));

    if (contextSupports(contextType, glu::ApiType::core(4, 5)))
        return gl45::getTesterExtension(tester);
    else if (contextSupports(contextType, glu::ApiType::es(3, 2)))
        return es32::getTesterExtension(tester);
    else if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        return es31::getTesterExtension(tester);
    else if (contextSupports(contextType, glu::ApiType::es(3, 0)))
        return es30::getTesterExtension(tester);
    else
    {
        DE_ASSERT(false);
        return RequiredExtensions();
    }
}

class TextureTest : public tcu::TestCase
{
public:
    TextureTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name, const char *desc,
                glw::GLenum target, TesterType tester, QueryType type);

    void init(void);
    IterateResult iterate(void);

    virtual void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const = 0;

protected:
    const glu::RenderContext &m_renderCtx;
    const glw::GLenum m_target;
    const glw::GLenum m_pname;
    const TesterType m_tester;
    const QueryType m_type;
};

TextureTest::TextureTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                         const char *desc, glw::GLenum target, TesterType tester, QueryType type)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_target(target)
    , m_pname(mapTesterToPname(tester))
    , m_tester(tester)
    , m_type(type)
{
}

void TextureTest::init(void)
{
    const de::UniquePtr<glu::ContextInfo> ctxInfo(glu::ContextInfo::create(m_renderCtx));
    RequiredExtensions extensions;

    // target
    if (!isCoreTextureTarget(m_renderCtx.getType(), m_target))
        extensions.add(getTextureTargetExtension(m_renderCtx.getType(), m_target));

    // param
    if (!isCoreTextureParam(m_renderCtx.getType(), m_pname))
        extensions.add(getTextureParamExtension(m_renderCtx.getType(), m_pname));

    // query
    if (!isCoreQuery(m_renderCtx.getType(), m_type))
        extensions.add(getQueryExtension(m_renderCtx.getType(), m_type));

    // test type
    if (!isCoreTester(m_renderCtx.getType(), m_tester))
        extensions.add(getTesterExtension(m_renderCtx.getType(), m_tester));

    extensions.check(*ctxInfo);
}

TextureTest::IterateResult TextureTest::iterate(void)
{
    glu::CallLogWrapper gl(m_renderCtx.getFunctions(), m_testCtx.getLog());
    tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");

    gl.enableLogging(true);
    test(gl, result);

    result.setTestContextResult(m_testCtx);
    return STOP;
}

class IsTextureCase : public tcu::TestCase
{
public:
    IsTextureCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name, const char *desc,
                  glw::GLenum target);

    void init(void);
    IterateResult iterate(void);

protected:
    const glu::RenderContext &m_renderCtx;
    const glw::GLenum m_target;
};

IsTextureCase::IsTextureCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                             const char *desc, glw::GLenum target)
    : tcu::TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_target(target)
{
}

void IsTextureCase::init(void)
{
    const de::UniquePtr<glu::ContextInfo> ctxInfo(glu::ContextInfo::create(m_renderCtx));
    RequiredExtensions extensions;

    // target
    if (!isCoreTextureTarget(m_renderCtx.getType(), m_target))
        extensions.add(getTextureTargetExtension(m_renderCtx.getType(), m_target));

    extensions.check(*ctxInfo);
}

IsTextureCase::IterateResult IsTextureCase::iterate(void)
{
    glu::CallLogWrapper gl(m_renderCtx.getFunctions(), m_testCtx.getLog());
    tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");
    glw::GLuint textureId = 0;

    gl.enableLogging(true);

    gl.glGenTextures(1, &textureId);
    gl.glBindTexture(m_target, textureId);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindTexture");

    verifyStateObjectBoolean(result, gl, textureId, true, QUERY_ISTEXTURE);

    gl.glDeleteTextures(1, &textureId);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteTextures");

    verifyStateObjectBoolean(result, gl, textureId, false, QUERY_ISTEXTURE);

    result.setTestContextResult(m_testCtx);
    return STOP;
}

class DepthStencilModeCase : public TextureTest
{
public:
    DepthStencilModeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                         const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

DepthStencilModeCase::DepthStencilModeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                           const char *name, const char *desc, glw::GLenum target, TesterType tester,
                                           QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void DepthStencilModeCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);
    glu::Texture texture(m_renderCtx);

    gl.glBindTexture(m_target, *texture);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "bind");

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DEPTH_COMPONENT, m_type);
    }

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Toggle", "Toggle");
        const glw::GLint depthComponentInt     = GL_DEPTH_COMPONENT;
        const glw::GLfloat depthComponentFloat = (glw::GLfloat)GL_DEPTH_COMPONENT;

        gl.glTexParameteri(m_target, m_pname, GL_STENCIL_INDEX);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_STENCIL_INDEX, m_type);

        gl.glTexParameteriv(m_target, m_pname, &depthComponentInt);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DEPTH_COMPONENT, m_type);

        gl.glTexParameterf(m_target, m_pname, GL_STENCIL_INDEX);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_STENCIL_INDEX, m_type);

        gl.glTexParameterfv(m_target, m_pname, &depthComponentFloat);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DEPTH_COMPONENT, m_type);
    }

    if (isPureIntTester(m_tester))
    {
        const glw::GLint depthComponent = GL_DEPTH_COMPONENT;
        const glw::GLint stencilIndex   = GL_STENCIL_INDEX;

        gl.glTexParameterIiv(m_target, m_pname, &stencilIndex);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_STENCIL_INDEX, m_type);

        gl.glTexParameterIiv(m_target, m_pname, &depthComponent);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DEPTH_COMPONENT, m_type);
    }

    if (isPureUintTester(m_tester))
    {
        const glw::GLuint depthComponent = GL_DEPTH_COMPONENT;
        const glw::GLuint stencilIndex   = GL_STENCIL_INDEX;

        gl.glTexParameterIuiv(m_target, m_pname, &stencilIndex);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_STENCIL_INDEX, m_type);

        gl.glTexParameterIuiv(m_target, m_pname, &depthComponent);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DEPTH_COMPONENT, m_type);
    }
}

class TextureSRGBDecodeCase : public TextureTest
{
public:
    TextureSRGBDecodeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                          const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureSRGBDecodeCase::TextureSRGBDecodeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                             const char *name, const char *desc, glw::GLenum target, TesterType tester,
                                             QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureSRGBDecodeCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);
    glu::Texture texture(m_renderCtx);

    gl.glBindTexture(m_target, *texture);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "bind");

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);
    }

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Toggle", "Toggle");
        const glw::GLint decodeInt     = GL_DECODE_EXT;
        const glw::GLfloat decodeFloat = (glw::GLfloat)GL_DECODE_EXT;

        gl.glTexParameteri(m_target, m_pname, GL_SKIP_DECODE_EXT);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_SKIP_DECODE_EXT, m_type);

        gl.glTexParameteriv(m_target, m_pname, &decodeInt);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);

        gl.glTexParameterf(m_target, m_pname, GL_SKIP_DECODE_EXT);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_SKIP_DECODE_EXT, m_type);

        gl.glTexParameterfv(m_target, m_pname, &decodeFloat);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);
    }

    if (isPureIntTester(m_tester))
    {
        const glw::GLint skipDecode = GL_SKIP_DECODE_EXT;
        const glw::GLint decode     = GL_DECODE_EXT;

        gl.glTexParameterIiv(m_target, m_pname, &skipDecode);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_SKIP_DECODE_EXT, m_type);

        gl.glTexParameterIiv(m_target, m_pname, &decode);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);
    }

    if (isPureUintTester(m_tester))
    {
        const glw::GLuint skipDecode = GL_SKIP_DECODE_EXT;
        const glw::GLuint decode     = GL_DECODE_EXT;

        gl.glTexParameterIuiv(m_target, m_pname, &skipDecode);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_SKIP_DECODE_EXT, m_type);

        gl.glTexParameterIuiv(m_target, m_pname, &decode);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);
    }
}

class TextureSwizzleCase : public TextureTest
{
public:
    TextureSwizzleCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                       const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureSwizzleCase::TextureSwizzleCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                                       const char *desc, glw::GLenum target, TesterType tester, QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureSwizzleCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase  = isPureIntTester(m_tester) || isPureUintTester(m_tester);
    const int initialValue = (m_pname == GL_TEXTURE_SWIZZLE_R) ? (GL_RED) :
                             (m_pname == GL_TEXTURE_SWIZZLE_G) ? (GL_GREEN) :
                             (m_pname == GL_TEXTURE_SWIZZLE_B) ? (GL_BLUE) :
                             (m_pname == GL_TEXTURE_SWIZZLE_A) ? (GL_ALPHA) :
                                                                 (-1);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, initialValue, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const GLenum swizzleValues[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO, GL_ONE};

        if (isPureCase)
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(swizzleValues); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)swizzleValues[ndx];
                    gl.glTexParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = swizzleValues[ndx];
                    gl.glTexParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
                }

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, swizzleValues[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(swizzleValues); ++ndx)
            {
                gl.glTexParameteri(m_target, m_pname, swizzleValues[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, swizzleValues[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(swizzleValues); ++ndx)
            {
                gl.glTexParameterf(m_target, m_pname, (GLfloat)swizzleValues[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, swizzleValues[ndx], m_type);
            }
        }
    }
}

class TextureWrapCase : public TextureTest
{
public:
    TextureWrapCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                    const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureWrapCase::TextureWrapCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                                 const char *desc, glw::GLenum target, TesterType tester, QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureWrapCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_REPEAT, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const GLenum wrapValues[] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT};

        if (isPureCase)
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)wrapValues[ndx];
                    gl.glTexParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = wrapValues[ndx];
                    gl.glTexParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
                }

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, wrapValues[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
            {
                gl.glTexParameteri(m_target, m_pname, wrapValues[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, wrapValues[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
            {
                gl.glTexParameterf(m_target, m_pname, (GLfloat)wrapValues[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, wrapValues[ndx], m_type);
            }
        }
    }
}

class TextureFilterCase : public TextureTest
{
public:
    TextureFilterCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                      const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureFilterCase::TextureFilterCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                                     const char *desc, glw::GLenum target, TesterType tester, QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureFilterCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);
    glw::GLenum initial   = (m_pname == GL_TEXTURE_MAG_FILTER) ? (GL_LINEAR) :
                            (m_pname == GL_TEXTURE_MIN_FILTER) ? (GL_NEAREST_MIPMAP_LINEAR) :
                                                                 (0);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, initial, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        std::vector<GLenum> values;

        values.push_back(GL_NEAREST);
        values.push_back(GL_LINEAR);
        if (m_pname == GL_TEXTURE_MIN_FILTER)
        {
            values.push_back(GL_NEAREST_MIPMAP_NEAREST);
            values.push_back(GL_NEAREST_MIPMAP_LINEAR);
            values.push_back(GL_LINEAR_MIPMAP_NEAREST);
            values.push_back(GL_LINEAR_MIPMAP_LINEAR);
        }

        if (isPureCase)
        {
            for (int ndx = 0; ndx < (int)values.size(); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)values[ndx];
                    gl.glTexParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = values[ndx];
                    gl.glTexParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
                }

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, values[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < (int)values.size(); ++ndx)
            {
                gl.glTexParameteri(m_target, m_pname, values[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, values[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < (int)values.size(); ++ndx)
            {
                gl.glTexParameterf(m_target, m_pname, (GLfloat)values[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, values[ndx], m_type);
            }
        }
    }
}

class TextureLODCase : public TextureTest
{
public:
    TextureLODCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                   const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureLODCase::TextureLODCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                               const char *desc, glw::GLenum target, TesterType tester, QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureLODCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase  = isPureIntTester(m_tester) || isPureUintTester(m_tester);
    const int initialValue = (m_pname == GL_TEXTURE_MIN_LOD) ? (-1000) :
                             (m_pname == GL_TEXTURE_MAX_LOD) ? (1000) :
                                                               (-1);

    if ((querySupportsSigned(m_type) || initialValue >= 0) && !isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, initialValue, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const int numIterations = 20;
        de::Random rnd(0xabcdef);

        if (isPureCase)
        {
            if (isPureIntTester(m_tester))
            {
                for (int ndx = 0; ndx < numIterations; ++ndx)
                {
                    const GLint ref = rnd.getInt(-1000, 1000);

                    gl.glTexParameterIiv(m_target, m_pname, &ref);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");

                    verifyStateTextureParamFloat(result, gl, m_target, m_pname, (float)ref, m_type);
                }
            }
            else
            {
                DE_ASSERT(isPureUintTester(m_tester));

                for (int ndx = 0; ndx < numIterations; ++ndx)
                {
                    const GLuint ref = (glw::GLuint)rnd.getInt(0, 1000);

                    gl.glTexParameterIuiv(m_target, m_pname, &ref);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");

                    verifyStateTextureParamFloat(result, gl, m_target, m_pname, (float)ref, m_type);
                }
            }
        }
        else
        {
            const int minLimit = (querySupportsSigned(m_type)) ? (-1000) : (0);

            for (int ndx = 0; ndx < numIterations; ++ndx)
            {
                const GLfloat ref = rnd.getFloat((float)minLimit, 1000.f);

                gl.glTexParameterf(m_target, m_pname, ref);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");

                verifyStateTextureParamFloat(result, gl, m_target, m_pname, ref, m_type);
            }

            // check unit conversions with int

            for (int ndx = 0; ndx < numIterations; ++ndx)
            {
                const GLint ref = rnd.getInt(minLimit, 1000);

                gl.glTexParameteri(m_target, m_pname, ref);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");

                verifyStateTextureParamFloat(result, gl, m_target, m_pname, (float)ref, m_type);
            }
        }
    }
}

class TextureLevelCase : public TextureTest
{
public:
    TextureLevelCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                     const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureLevelCase::TextureLevelCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                                   const char *desc, glw::GLenum target, TesterType tester, QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureLevelCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase  = isPureIntTester(m_tester) || isPureUintTester(m_tester);
    const int initialValue = (m_pname == GL_TEXTURE_BASE_LEVEL) ? (0) :
                             (m_pname == GL_TEXTURE_MAX_LEVEL)  ? (1000) :
                                                                  (-1);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, initialValue, m_type);
    }

    if (m_target == GL_TEXTURE_2D_MULTISAMPLE || m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
        // only 0 allowed
        {
            const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");

            gl.glTexParameteri(m_target, m_pname, 0);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");
            verifyStateTextureParamInteger(result, gl, m_target, m_pname, 0, m_type);

            gl.glTexParameterf(m_target, m_pname, 0.0f);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");
            verifyStateTextureParamInteger(result, gl, m_target, m_pname, 0, m_type);
        }
    }
    else
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const int numIterations = 20;
        de::Random rnd(0xabcdef);

        if (isPureCase)
        {
            for (int ndx = 0; ndx < numIterations; ++ndx)
            {
                const GLint ref   = rnd.getInt(0, 64000);
                const GLuint uRef = (glw::GLuint)ref;

                if (isPureIntTester(m_tester))
                {
                    gl.glTexParameterIiv(m_target, m_pname, &ref);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));
                    gl.glTexParameterIuiv(m_target, m_pname, &uRef);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
                }

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, ref, m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < numIterations; ++ndx)
            {
                const GLint ref = rnd.getInt(0, 64000);

                gl.glTexParameteri(m_target, m_pname, ref);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, ref, m_type);
            }

            // check unit conversions with float

            const float nonSignificantOffsets[] = {
                -0.45f, -0.25f, 0,
                0.45f}; // offsets O so that for any integers z in Z, o in O roundToClosestInt(z+o)==z

            const int numConversionIterations = 30;
            for (int ndx = 0; ndx < numConversionIterations; ++ndx)
            {
                const GLint ref = rnd.getInt(1, 64000);

                for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(nonSignificantOffsets); ++offsetNdx)
                {
                    gl.glTexParameterf(m_target, m_pname, ((GLfloat)ref) + nonSignificantOffsets[offsetNdx]);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");

                    verifyStateTextureParamInteger(result, gl, m_target, m_pname, ref, m_type);
                }
            }
        }
    }
}

class TextureCompareModeCase : public TextureTest
{
public:
    TextureCompareModeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                           const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureCompareModeCase::TextureCompareModeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                               const char *name, const char *desc, glw::GLenum target,
                                               TesterType tester, QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureCompareModeCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_NONE, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const GLenum modes[] = {GL_COMPARE_REF_TO_TEXTURE, GL_NONE};

        if (isPureCase)
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)modes[ndx];
                    gl.glTexParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = modes[ndx];
                    gl.glTexParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
                }

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, modes[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
            {
                gl.glTexParameteri(m_target, m_pname, modes[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, modes[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
            {
                gl.glTexParameterf(m_target, m_pname, (GLfloat)modes[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, modes[ndx], m_type);
            }
        }
    }
}

class TextureCompareFuncCase : public TextureTest
{
public:
    TextureCompareFuncCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                           const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureCompareFuncCase::TextureCompareFuncCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                               const char *name, const char *desc, glw::GLenum target,
                                               TesterType tester, QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureCompareFuncCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_LEQUAL, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const GLenum compareFuncs[] = {GL_LEQUAL, GL_GEQUAL,   GL_LESS,   GL_GREATER,
                                       GL_EQUAL,  GL_NOTEQUAL, GL_ALWAYS, GL_NEVER};

        if (isPureCase)
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)compareFuncs[ndx];
                    gl.glTexParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = compareFuncs[ndx];
                    gl.glTexParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");
                }

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, compareFuncs[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
            {
                gl.glTexParameteri(m_target, m_pname, compareFuncs[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, compareFuncs[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
            {
                gl.glTexParameterf(m_target, m_pname, (GLfloat)compareFuncs[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");

                verifyStateTextureParamInteger(result, gl, m_target, m_pname, compareFuncs[ndx], m_type);
            }
        }
    }
}

class TextureImmutableLevelsCase : public TextureTest
{
public:
    TextureImmutableLevelsCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                               const char *desc, glw::GLenum target, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureImmutableLevelsCase::TextureImmutableLevelsCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                                       const char *name, const char *desc, glw::GLenum target,
                                                       QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, TESTER_TEXTURE_IMMUTABLE_LEVELS, type)
{
}

void TextureImmutableLevelsCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, 0, m_type);
    }

    if (m_target == GL_TEXTURE_2D_MULTISAMPLE || m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
        // no levels
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Level", "Level");
        GLuint textureID = 0;

        gl.glGenTextures(1, &textureID);
        gl.glBindTexture(m_target, textureID);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindTexture");

        if (m_target == GL_TEXTURE_2D_MULTISAMPLE)
            gl.glTexStorage2DMultisample(m_target, 2, GL_RGB8, 64, 64, GL_FALSE);
        else if (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
            gl.glTexStorage3DMultisample(m_target, 2, GL_RGB8, 64, 64, 2, GL_FALSE);
        else
            DE_ASSERT(false);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexStorage");

        verifyStateTextureParamInteger(result, gl, m_target, m_pname, 1, m_type);

        gl.glDeleteTextures(1, &textureID);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteTextures");
    }
    else
    {
        for (int level = 1; level <= 7; ++level)
        {
            const tcu::ScopedLogSection section(m_testCtx.getLog(), "Levels", "Levels = " + de::toString(level));
            GLuint textureID = 0;

            gl.glGenTextures(1, &textureID);
            gl.glBindTexture(m_target, textureID);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindTexture");

            if (m_target == GL_TEXTURE_2D || m_target == GL_TEXTURE_CUBE_MAP)
                gl.glTexStorage2D(m_target, level, GL_RGB8, 64, 64);
            else if (m_target == GL_TEXTURE_2D_ARRAY || m_target == GL_TEXTURE_3D)
                gl.glTexStorage3D(m_target, level, GL_RGB8, 64, 64, 64);
            else if (m_target == GL_TEXTURE_CUBE_MAP_ARRAY)
                gl.glTexStorage3D(m_target, level, GL_RGB8, 64, 64, 6 * 2);
            else
                DE_ASSERT(false);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexStorage");

            verifyStateTextureParamInteger(result, gl, m_target, m_pname, level, m_type);

            gl.glDeleteTextures(1, &textureID);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteTextures");
        }
    }
}

class TextureImmutableFormatCase : public TextureTest
{
public:
    TextureImmutableFormatCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                               const char *desc, glw::GLenum target, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureImmutableFormatCase::TextureImmutableFormatCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                                       const char *name, const char *desc, glw::GLenum target,
                                                       QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, TESTER_TEXTURE_IMMUTABLE_FORMAT, type)
{
}

void TextureImmutableFormatCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamInteger(result, gl, m_target, m_pname, 0, m_type);
    }

    {
        const tcu::ScopedLogSection subsection(m_testCtx.getLog(), "Immutable", "Immutable");
        GLuint textureID = 0;

        gl.glGenTextures(1, &textureID);
        gl.glBindTexture(m_target, textureID);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindTexture");

        switch (m_target)
        {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
        {
            gl.glTexStorage2D(m_target, 1, GL_RGBA8, 32, 32);
            break;
        }
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_3D:
        {
            gl.glTexStorage3D(m_target, 1, GL_RGBA8, 32, 32, 8);
            break;
        }
        case GL_TEXTURE_2D_MULTISAMPLE:
        {
            gl.glTexStorage2DMultisample(m_target, 2, GL_RGB8, 64, 64, GL_FALSE);
            break;
        }
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        {
            gl.glTexStorage3DMultisample(m_target, 2, GL_RGB8, 64, 64, 2, GL_FALSE);
            break;
        }
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        {
            gl.glTexStorage3D(m_target, 1, GL_RGBA8, 32, 32, 6 * 2);
            break;
        }
        default:
            DE_ASSERT(false);
        }
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "setup texture");

        verifyStateTextureParamInteger(result, gl, m_target, m_pname, 1, m_type);

        gl.glDeleteTextures(1, &textureID);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteTextures");
    }

    // no mutable
    if (m_target == GL_TEXTURE_2D_MULTISAMPLE || m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        return;

    // test mutable
    {
        const tcu::ScopedLogSection subsection(m_testCtx.getLog(), "Mutable", "Mutable");
        GLuint textureID = 0;

        gl.glGenTextures(1, &textureID);
        gl.glBindTexture(m_target, textureID);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindTexture");

        switch (m_target)
        {
        case GL_TEXTURE_2D:
        {
            gl.glTexImage2D(m_target, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            break;
        }
        case GL_TEXTURE_CUBE_MAP:
        {
            gl.glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            break;
        }
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_3D:
        {
            gl.glTexImage3D(m_target, 0, GL_RGBA8, 32, 32, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            break;
        }
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        {
            gl.glTexImage3D(m_target, 0, GL_RGBA8, 32, 32, 6 * 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            break;
        }
        default:
            DE_ASSERT(false);
        }
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "setup texture");

        verifyStateTextureParamInteger(result, gl, m_target, m_pname, 0, m_type);

        gl.glDeleteTextures(1, &textureID);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteTextures");
    }
}

class TextureWrapClampToBorderCase : public TextureTest
{
public:
    TextureWrapClampToBorderCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                                 const char *desc, glw::GLenum target, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureWrapClampToBorderCase::TextureWrapClampToBorderCase(tcu::TestContext &testCtx,
                                                           const glu::RenderContext &renderCtx, const char *name,
                                                           const char *desc, glw::GLenum target, TesterType tester,
                                                           QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, tester, type)
{
}

void TextureWrapClampToBorderCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    gl.glTexParameteri(m_target, m_pname, GL_CLAMP_TO_BORDER_EXT);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");
    verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_CLAMP_TO_BORDER_EXT, m_type);

    gl.glTexParameteri(m_target, m_pname, GL_REPEAT);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteri");

    gl.glTexParameterf(m_target, m_pname, (GLfloat)GL_CLAMP_TO_BORDER_EXT);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterf");

    verifyStateTextureParamInteger(result, gl, m_target, m_pname, GL_CLAMP_TO_BORDER_EXT, m_type);
}

class TextureBorderColorCase : public TextureTest
{
public:
    TextureBorderColorCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                           const char *desc, glw::GLenum target, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

TextureBorderColorCase::TextureBorderColorCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                               const char *name, const char *desc, glw::GLenum target, QueryType type)
    : TextureTest(testCtx, renderCtx, name, desc, target, TESTER_TEXTURE_BORDER_COLOR, type)
{
}

void TextureBorderColorCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    // border color is undefined if queried with pure type and was not set to pure value
    if (m_type == QUERY_TEXTURE_PARAM_INTEGER_VEC4 || m_type == QUERY_TEXTURE_PARAM_FLOAT_VEC4)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateTextureParamFloatVec4(result, gl, m_target, m_pname, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_type);
    }

    if (m_type == QUERY_TEXTURE_PARAM_PURE_INTEGER_VEC4)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const tcu::IVec4 color(0x7FFFFFFF, -2, 3, -128);

        gl.glTexParameterIiv(m_target, m_pname, color.getPtr());
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIiv");

        verifyStateTextureParamIntegerVec4(result, gl, m_target, m_pname, color, m_type);
    }
    else if (m_type == QUERY_TEXTURE_PARAM_PURE_UNSIGNED_INTEGER_VEC4)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const tcu::UVec4 color(0x8000000ul, 2, 3, 128);

        gl.glTexParameterIuiv(m_target, m_pname, color.getPtr());
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterIuiv");

        verifyStateTextureParamUnsignedIntegerVec4(result, gl, m_target, m_pname, color, m_type);
    }
    else
    {
        DE_ASSERT(m_type == QUERY_TEXTURE_PARAM_INTEGER_VEC4 || m_type == QUERY_TEXTURE_PARAM_FLOAT_VEC4);

        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const tcu::Vec4 color(0.25f, 1.0f, 0.0f, 0.77f);
        const tcu::IVec4 icolor(0x8000000ul, 0x7FFFFFFF, 0, 0x0FFFFFFF);

        gl.glTexParameterfv(m_target, m_pname, color.getPtr());
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameterfv");

        verifyStateTextureParamFloatVec4(result, gl, m_target, m_pname, color, m_type);

        gl.glTexParameteriv(m_target, m_pname, icolor.getPtr());
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glTexParameteriv");

        verifyStateTextureParamNormalizedI32Vec4(result, gl, m_target, m_pname, icolor, m_type);
    }
}

class SamplerTest : public tcu::TestCase
{
public:
    SamplerTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name, const char *desc,
                TesterType tester, QueryType type);

    void init(void);
    IterateResult iterate(void);

    virtual void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const = 0;

protected:
    const glu::RenderContext &m_renderCtx;
    const glw::GLenum m_pname;
    const TesterType m_tester;
    const QueryType m_type;
    glw::GLuint m_target;
};

SamplerTest::SamplerTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                         const char *desc, TesterType tester, QueryType type)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_pname(mapTesterToPname(tester))
    , m_tester(tester)
    , m_type(type)
    , m_target(0)
{
}

void SamplerTest::init(void)
{
    const de::UniquePtr<glu::ContextInfo> ctxInfo(glu::ContextInfo::create(m_renderCtx));
    RequiredExtensions extensions;

    // param
    if (!isCoreTextureParam(m_renderCtx.getType(), m_pname))
        extensions.add(getTextureParamExtension(m_renderCtx.getType(), m_pname));

    // query
    if (!isCoreQuery(m_renderCtx.getType(), m_type))
        extensions.add(getQueryExtension(m_renderCtx.getType(), m_type));

    // test type
    if (!isCoreTester(m_renderCtx.getType(), m_tester))
        extensions.add(getTesterExtension(m_renderCtx.getType(), m_tester));

    extensions.check(*ctxInfo);
}

SamplerTest::IterateResult SamplerTest::iterate(void)
{
    glu::CallLogWrapper gl(m_renderCtx.getFunctions(), m_testCtx.getLog());
    tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");
    glu::Sampler sampler(m_renderCtx);

    gl.enableLogging(true);

    m_target = *sampler;
    test(gl, result);
    m_target = 0;

    result.setTestContextResult(m_testCtx);
    return STOP;
}

class SamplerWrapCase : public SamplerTest
{
public:
    SamplerWrapCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                    const char *desc, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

SamplerWrapCase::SamplerWrapCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                                 const char *desc, TesterType tester, QueryType type)
    : SamplerTest(testCtx, renderCtx, name, desc, tester, type)
{
}

void SamplerWrapCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_REPEAT, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const GLenum wrapValues[] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT};

        if (isPureCase)
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)wrapValues[ndx];
                    gl.glSamplerParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = wrapValues[ndx];
                    gl.glSamplerParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIuiv");
                }

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, wrapValues[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
            {
                gl.glSamplerParameteri(m_target, m_pname, wrapValues[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameteri");

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, wrapValues[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
            {
                gl.glSamplerParameterf(m_target, m_pname, (GLfloat)wrapValues[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterf");

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, wrapValues[ndx], m_type);
            }
        }
    }
}

class SamplerFilterCase : public SamplerTest
{
public:
    SamplerFilterCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                      const char *desc, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

SamplerFilterCase::SamplerFilterCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                                     const char *desc, TesterType tester, QueryType type)
    : SamplerTest(testCtx, renderCtx, name, desc, tester, type)
{
}

void SamplerFilterCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase     = isPureIntTester(m_tester) || isPureUintTester(m_tester);
    const glw::GLenum initial = (m_pname == GL_TEXTURE_MAG_FILTER) ? (GL_LINEAR) :
                                (m_pname == GL_TEXTURE_MIN_FILTER) ? (GL_NEAREST_MIPMAP_LINEAR) :
                                                                     (0);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, initial, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        std::vector<GLenum> values;

        values.push_back(GL_NEAREST);
        values.push_back(GL_LINEAR);
        if (m_pname == GL_TEXTURE_MIN_FILTER)
        {
            values.push_back(GL_NEAREST_MIPMAP_NEAREST);
            values.push_back(GL_NEAREST_MIPMAP_LINEAR);
            values.push_back(GL_LINEAR_MIPMAP_NEAREST);
            values.push_back(GL_LINEAR_MIPMAP_LINEAR);
        }

        if (isPureCase)
        {
            for (int ndx = 0; ndx < (int)values.size(); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)values[ndx];
                    gl.glSamplerParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = values[ndx];
                    gl.glSamplerParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIuiv");
                }

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, values[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < (int)values.size(); ++ndx)
            {
                gl.glSamplerParameteri(m_target, m_pname, values[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameteri");

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, values[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < (int)values.size(); ++ndx)
            {
                gl.glSamplerParameterf(m_target, m_pname, (GLfloat)values[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterf");

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, values[ndx], m_type);
            }
        }
    }
}

class SamplerLODCase : public SamplerTest
{
public:
    SamplerLODCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                   const char *desc, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

SamplerLODCase::SamplerLODCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name,
                               const char *desc, TesterType tester, QueryType type)
    : SamplerTest(testCtx, renderCtx, name, desc, tester, type)
{
}

void SamplerLODCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase  = isPureIntTester(m_tester) || isPureUintTester(m_tester);
    const int initialValue = (m_pname == GL_TEXTURE_MIN_LOD) ? (-1000) :
                             (m_pname == GL_TEXTURE_MAX_LOD) ? (1000) :
                                                               (-1);

    if ((querySupportsSigned(m_type) || initialValue >= 0) && !isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, initialValue, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const int numIterations = 20;
        de::Random rnd(0xabcdef);

        if (isPureCase)
        {
            if (isPureIntTester(m_tester))
            {
                for (int ndx = 0; ndx < numIterations; ++ndx)
                {
                    const GLint ref = rnd.getInt(-1000, 1000);

                    gl.glSamplerParameterIiv(m_target, m_pname, &ref);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIiv");

                    verifyStateSamplerParamFloat(result, gl, m_target, m_pname, (float)ref, m_type);
                }
            }
            else
            {
                DE_ASSERT(isPureUintTester(m_tester));

                for (int ndx = 0; ndx < numIterations; ++ndx)
                {
                    const GLuint ref = (glw::GLuint)rnd.getInt(0, 1000);

                    gl.glSamplerParameterIuiv(m_target, m_pname, &ref);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIuiv");

                    verifyStateSamplerParamFloat(result, gl, m_target, m_pname, (float)ref, m_type);
                }
            }
        }
        else
        {
            const int minLimit = (querySupportsSigned(m_type)) ? (-1000) : (0);

            for (int ndx = 0; ndx < numIterations; ++ndx)
            {
                const GLfloat ref = rnd.getFloat((float)minLimit, 1000.f);

                gl.glSamplerParameterf(m_target, m_pname, ref);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterf");

                verifyStateSamplerParamFloat(result, gl, m_target, m_pname, ref, m_type);
            }

            // check unit conversions with int

            for (int ndx = 0; ndx < numIterations; ++ndx)
            {
                const GLint ref = rnd.getInt(minLimit, 1000);

                gl.glSamplerParameteri(m_target, m_pname, ref);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameteri");

                verifyStateSamplerParamFloat(result, gl, m_target, m_pname, (float)ref, m_type);
            }
        }
    }
}

class SamplerCompareModeCase : public SamplerTest
{
public:
    SamplerCompareModeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                           const char *desc, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

SamplerCompareModeCase::SamplerCompareModeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                               const char *name, const char *desc, TesterType tester, QueryType type)
    : SamplerTest(testCtx, renderCtx, name, desc, tester, type)
{
}

void SamplerCompareModeCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_NONE, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const GLenum modes[] = {GL_COMPARE_REF_TO_TEXTURE, GL_NONE};

        if (isPureCase)
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)modes[ndx];
                    gl.glSamplerParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = modes[ndx];
                    gl.glSamplerParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIuiv");
                }

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, modes[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
            {
                gl.glSamplerParameteri(m_target, m_pname, modes[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameteri");

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, modes[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
            {
                gl.glSamplerParameterf(m_target, m_pname, (GLfloat)modes[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterf");

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, modes[ndx], m_type);
            }
        }
    }
}

class SamplerCompareFuncCase : public SamplerTest
{
public:
    SamplerCompareFuncCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                           const char *desc, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

SamplerCompareFuncCase::SamplerCompareFuncCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                               const char *name, const char *desc, TesterType tester, QueryType type)
    : SamplerTest(testCtx, renderCtx, name, desc, tester, type)
{
}

void SamplerCompareFuncCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_LEQUAL, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const GLenum compareFuncs[] = {GL_LEQUAL, GL_GEQUAL,   GL_LESS,   GL_GREATER,
                                       GL_EQUAL,  GL_NOTEQUAL, GL_ALWAYS, GL_NEVER};

        if (isPureCase)
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
            {
                if (isPureIntTester(m_tester))
                {
                    const glw::GLint value = (glw::GLint)compareFuncs[ndx];
                    gl.glSamplerParameterIiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIiv");
                }
                else
                {
                    DE_ASSERT(isPureUintTester(m_tester));

                    const glw::GLuint value = compareFuncs[ndx];
                    gl.glSamplerParameterIuiv(m_target, m_pname, &value);
                    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIuiv");
                }

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, compareFuncs[ndx], m_type);
            }
        }
        else
        {
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
            {
                gl.glSamplerParameteri(m_target, m_pname, compareFuncs[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameteri");

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, compareFuncs[ndx], m_type);
            }

            //check unit conversions with float

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
            {
                gl.glSamplerParameterf(m_target, m_pname, (GLfloat)compareFuncs[ndx]);
                GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterf");

                verifyStateSamplerParamInteger(result, gl, m_target, m_pname, compareFuncs[ndx], m_type);
            }
        }
    }
}

class SamplerWrapClampToBorderCase : public SamplerTest
{
public:
    SamplerWrapClampToBorderCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                                 const char *desc, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

SamplerWrapClampToBorderCase::SamplerWrapClampToBorderCase(tcu::TestContext &testCtx,
                                                           const glu::RenderContext &renderCtx, const char *name,
                                                           const char *desc, TesterType tester, QueryType type)
    : SamplerTest(testCtx, renderCtx, name, desc, tester, type)
{
}

void SamplerWrapClampToBorderCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    gl.glSamplerParameteri(m_target, m_pname, GL_CLAMP_TO_BORDER_EXT);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameteri");
    verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_CLAMP_TO_BORDER_EXT, m_type);

    gl.glSamplerParameteri(m_target, m_pname, GL_REPEAT);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameteri");

    gl.glSamplerParameterf(m_target, m_pname, (GLfloat)GL_CLAMP_TO_BORDER_EXT);
    GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterf");

    verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_CLAMP_TO_BORDER_EXT, m_type);
}

class SamplerSRGBDecodeCase : public SamplerTest
{
public:
    SamplerSRGBDecodeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                          const char *desc, TesterType tester, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

SamplerSRGBDecodeCase::SamplerSRGBDecodeCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                             const char *name, const char *desc, TesterType tester, QueryType type)
    : SamplerTest(testCtx, renderCtx, name, desc, tester, type)
{
}

void SamplerSRGBDecodeCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    const bool isPureCase = isPureIntTester(m_tester) || isPureUintTester(m_tester);

    if (!isPureCase)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Toggle", "Toggle");
        const glw::GLint decodeInt     = GL_DECODE_EXT;
        const glw::GLfloat decodeFloat = (glw::GLfloat)GL_DECODE_EXT;

        gl.glSamplerParameteri(m_target, m_pname, GL_SKIP_DECODE_EXT);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_SKIP_DECODE_EXT, m_type);

        gl.glSamplerParameteriv(m_target, m_pname, &decodeInt);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);

        gl.glSamplerParameterf(m_target, m_pname, GL_SKIP_DECODE_EXT);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_SKIP_DECODE_EXT, m_type);

        gl.glSamplerParameterfv(m_target, m_pname, &decodeFloat);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "set state");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);
    }

    if (isPureIntTester(m_tester))
    {
        const glw::GLint skipDecode = GL_SKIP_DECODE_EXT;
        const glw::GLint decode     = GL_DECODE_EXT;

        gl.glSamplerParameterIiv(m_target, m_pname, &skipDecode);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIiv");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_SKIP_DECODE_EXT, m_type);

        gl.glSamplerParameterIiv(m_target, m_pname, &decode);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIiv");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);
    }

    if (isPureUintTester(m_tester))
    {
        const glw::GLuint skipDecode = GL_SKIP_DECODE_EXT;
        const glw::GLuint decode     = GL_DECODE_EXT;

        gl.glSamplerParameterIuiv(m_target, m_pname, &skipDecode);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIuiv");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_SKIP_DECODE_EXT, m_type);

        gl.glSamplerParameterIuiv(m_target, m_pname, &decode);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIuiv");
        verifyStateSamplerParamInteger(result, gl, m_target, m_pname, GL_DECODE_EXT, m_type);
    }
}

class SamplerBorderColorCase : public SamplerTest
{
public:
    SamplerBorderColorCase(tcu::TestContext &testCtx, const glu::RenderContext &renderContext, const char *name,
                           const char *desc, QueryType type);
    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const;
};

SamplerBorderColorCase::SamplerBorderColorCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                               const char *name, const char *desc, QueryType type)
    : SamplerTest(testCtx, renderCtx, name, desc, TESTER_TEXTURE_BORDER_COLOR, type)
{
    DE_ASSERT(m_type == QUERY_SAMPLER_PARAM_INTEGER_VEC4 || m_type == QUERY_SAMPLER_PARAM_FLOAT_VEC4 ||
              m_type == QUERY_SAMPLER_PARAM_PURE_INTEGER_VEC4 ||
              m_type == QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER_VEC4);
}

void SamplerBorderColorCase::test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
{
    // border color is undefined if queried with pure type and was not set to pure value
    if (m_type == QUERY_SAMPLER_PARAM_INTEGER_VEC4 || m_type == QUERY_SAMPLER_PARAM_FLOAT_VEC4)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
        verifyStateSamplerParamFloatVec4(result, gl, m_target, m_pname, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_type);
    }

    if (m_type == QUERY_SAMPLER_PARAM_PURE_INTEGER_VEC4)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const tcu::IVec4 color(0x7FFFFFFF, -2, 3, -128);

        gl.glSamplerParameterIiv(m_target, m_pname, color.getPtr());
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIiv");

        verifyStateSamplerParamIntegerVec4(result, gl, m_target, m_pname, color, m_type);
    }
    else if (m_type == QUERY_SAMPLER_PARAM_PURE_UNSIGNED_INTEGER_VEC4)
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const tcu::UVec4 color(0x8000000ul, 2, 3, 128);

        gl.glSamplerParameterIuiv(m_target, m_pname, color.getPtr());
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterIuiv");

        verifyStateSamplerParamUnsignedIntegerVec4(result, gl, m_target, m_pname, color, m_type);
    }
    else
    {
        DE_ASSERT(m_type == QUERY_SAMPLER_PARAM_INTEGER_VEC4 || m_type == QUERY_SAMPLER_PARAM_FLOAT_VEC4);

        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Set", "Set");
        const tcu::Vec4 color(0.25f, 1.0f, 0.0f, 0.77f);
        const tcu::IVec4 icolor(0x8000000ul, 0x7FFFFFFF, 0, 0x0FFFFFFF);

        gl.glSamplerParameterfv(m_target, m_pname, color.getPtr());
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameterfv");

        verifyStateSamplerParamFloatVec4(result, gl, m_target, m_pname, color, m_type);

        gl.glSamplerParameteriv(m_target, m_pname, icolor.getPtr());
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glSamplerParameteriv");

        verifyStateSamplerParamNormalizedI32Vec4(result, gl, m_target, m_pname, icolor, m_type);
    }
}

} // namespace

bool isLegalTesterForTarget(glw::GLenum target, TesterType tester)
{
    // no 3d filtering on 2d targets
    if ((tester == TESTER_TEXTURE_WRAP_R || tester == TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER) && target != GL_TEXTURE_3D)
        return false;

    // no sampling on multisample
    if (isMultisampleTarget(target) && isSamplerStateTester(tester))
        return false;

    // no states in buffer
    if (target == GL_TEXTURE_BUFFER)
        return false;

    return true;
}

bool isMultisampleTarget(glw::GLenum target)
{
    return target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
}

bool isSamplerStateTester(TesterType tester)
{
    return tester == TESTER_TEXTURE_WRAP_S || tester == TESTER_TEXTURE_WRAP_T || tester == TESTER_TEXTURE_WRAP_R ||
           tester == TESTER_TEXTURE_MAG_FILTER || tester == TESTER_TEXTURE_MIN_FILTER ||
           tester == TESTER_TEXTURE_MIN_LOD || tester == TESTER_TEXTURE_MAX_LOD ||
           tester == TESTER_TEXTURE_COMPARE_MODE || tester == TESTER_TEXTURE_COMPARE_FUNC ||
           tester == TESTER_TEXTURE_SRGB_DECODE_EXT || tester == TESTER_TEXTURE_BORDER_COLOR ||
           tester == TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER || tester == TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER ||
           tester == TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER;
}

tcu::TestCase *createIsTextureTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                   const std::string &name, const std::string &description, glw::GLenum target)
{
    return new IsTextureCase(testCtx, renderCtx, name.c_str(), description.c_str(), target);
}

tcu::TestCase *createTexParamTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                  const std::string &name, const std::string &description, QueryType queryType,
                                  glw::GLenum target, TesterType tester)
{
    if (isMultisampleTarget(target) && isSamplerStateTester(tester))
    {
        DE_FATAL("Multisample textures have no sampler state");
        return nullptr;
    }
    if (target == GL_TEXTURE_BUFFER)
    {
        DE_FATAL("Buffer textures have no texture state");
        return nullptr;
    }
    if (target != GL_TEXTURE_3D && mapTesterToPname(tester) == GL_TEXTURE_WRAP_R)
    {
        DE_FATAL("Only 3D textures have wrap r filter");
        return nullptr;
    }

#define CASE_ALL_SETTERS(X) \
    case X:                 \
    case X##_SET_PURE_INT:  \
    case X##_SET_PURE_UINT

    switch (tester)
    {
        CASE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_R)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_G)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_B)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_SWIZZLE_A)
            : return new TextureSwizzleCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                            queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_S)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_T)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_R)
            : return new TextureWrapCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                         queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_MAG_FILTER)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_MIN_FILTER)
            : return new TextureFilterCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                           queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_MIN_LOD)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_MAX_LOD)
            : return new TextureLODCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                        queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_BASE_LEVEL)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_MAX_LEVEL)
            : return new TextureLevelCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                          queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_MODE)
            : return new TextureCompareModeCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                                queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_FUNC)
            : return new TextureCompareFuncCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                                queryType);

    case TESTER_TEXTURE_IMMUTABLE_LEVELS:
        return new TextureImmutableLevelsCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, queryType);

    case TESTER_TEXTURE_IMMUTABLE_FORMAT:
        return new TextureImmutableFormatCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, queryType);

    case TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER:
        return new TextureWrapClampToBorderCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                                queryType);

        CASE_ALL_SETTERS(TESTER_DEPTH_STENCIL_TEXTURE_MODE)
            : return new DepthStencilModeCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                              queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_SRGB_DECODE_EXT)
            : return new TextureSRGBDecodeCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, tester,
                                               queryType);

    case TESTER_TEXTURE_BORDER_COLOR:
        return new TextureBorderColorCase(testCtx, renderCtx, name.c_str(), description.c_str(), target, queryType);

    default:
        break;
    }

#undef CASE_ALL_SETTERS

    DE_ASSERT(false);
    return nullptr;
}

tcu::TestCase *createSamplerParamTest(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx,
                                      const std::string &name, const std::string &description,
                                      StateQueryUtil::QueryType queryType, TesterType tester)
{
#define CASE_ALL_SETTERS(X) \
    case X:                 \
    case X##_SET_PURE_INT:  \
    case X##_SET_PURE_UINT

    switch (tester)
    {
        CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_S)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_T)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_WRAP_R)
            : return new SamplerWrapCase(testCtx, renderCtx, name.c_str(), description.c_str(), tester, queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_MAG_FILTER)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_MIN_FILTER)
            : return new SamplerFilterCase(testCtx, renderCtx, name.c_str(), description.c_str(), tester, queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_MIN_LOD)
            : CASE_ALL_SETTERS(TESTER_TEXTURE_MAX_LOD)
            : return new SamplerLODCase(testCtx, renderCtx, name.c_str(), description.c_str(), tester, queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_MODE)
            : return new SamplerCompareModeCase(testCtx, renderCtx, name.c_str(), description.c_str(), tester,
                                                queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_COMPARE_FUNC)
            : return new SamplerCompareFuncCase(testCtx, renderCtx, name.c_str(), description.c_str(), tester,
                                                queryType);

    case TESTER_TEXTURE_WRAP_S_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_T_CLAMP_TO_BORDER:
    case TESTER_TEXTURE_WRAP_R_CLAMP_TO_BORDER:
        return new SamplerWrapClampToBorderCase(testCtx, renderCtx, name.c_str(), description.c_str(), tester,
                                                queryType);

        CASE_ALL_SETTERS(TESTER_TEXTURE_SRGB_DECODE_EXT)
            : return new SamplerSRGBDecodeCase(testCtx, renderCtx, name.c_str(), description.c_str(), tester,
                                               queryType);

    case TESTER_TEXTURE_BORDER_COLOR:
        return new SamplerBorderColorCase(testCtx, renderCtx, name.c_str(), description.c_str(), queryType);

    default:
        break;
    }

#undef CASE_ALL_SETTERS

    DE_ASSERT(false);
    return nullptr;
}

} // namespace TextureStateQueryTests
} // namespace gls
} // namespace deqp

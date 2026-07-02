#ifndef _GLUCONTEXTINFO_HPP
#define _GLUCONTEXTINFO_HPP
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

#include "glwFunctions.hpp"
#include "gluDefs.hpp"

#include <vector>
#include <string>
#include <set>

namespace glu
{

class RenderContext;

template <typename T, class ComputeValue>
class CachedValue
{
public:
    CachedValue(ComputeValue compute = ComputeValue(), const T &defaultValue = T())
        : m_compute(compute)
        , m_value(defaultValue)
        , m_isComputed(false)
    {
    }

    const T &getValue(const RenderContext &context) const
    {
        if (!m_isComputed)
        {
            m_value      = m_compute(context);
            m_isComputed = true;
        }
        return m_value;
    }

private:
    ComputeValue m_compute;
    mutable T m_value;
    mutable bool m_isComputed;
};

class GetCompressedTextureFormats
{
public:
    std::set<int> operator()(const RenderContext &context) const;
};

typedef CachedValue<std::set<int>, GetCompressedTextureFormats> CompressedTextureFormats;

bool IsES3Compatible(const glw::Functions &gl);

/*--------------------------------------------------------------------*//*!
 * \brief Context information & limit query.
 *//*--------------------------------------------------------------------*/
class ContextInfo
{
public:
    virtual ~ContextInfo(void);

    virtual int getInt(int param) const;
    virtual bool getBool(int param) const;
    virtual const char *getString(int param) const;

    virtual bool isVertexUniformLoopSupported(void) const
    {
        return true;
    }
    virtual bool isVertexDynamicLoopSupported(void) const
    {
        return true;
    }
    virtual bool isFragmentHighPrecisionSupported(void) const
    {
        return true;
    }
    virtual bool isFragmentUniformLoopSupported(void) const
    {
        return true;
    }
    virtual bool isFragmentDynamicLoopSupported(void) const
    {
        return true;
    }

    virtual bool isCompressedTextureFormatSupported(int format) const;

    const std::vector<std::string> &getExtensions(void) const
    {
        return m_extensions;
    }
    bool isExtensionSupported(const char *extName) const;

    bool isES3Compatible() const;

    static ContextInfo *create(const RenderContext &context);

protected:
    ContextInfo(const RenderContext &context);

    const RenderContext &m_context;

private:
    ContextInfo(const ContextInfo &other);
    ContextInfo &operator=(const ContextInfo &other);

    std::vector<std::string> m_extensions;
    CompressedTextureFormats m_compressedTextureFormats;
};

} // namespace glu

#endif // _GLUCONTEXTINFO_HPP

/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief EGL Config selection helper.
 *//*--------------------------------------------------------------------*/

#include "egluConfigFilter.hpp"
#include "egluUtil.hpp"
#include "egluConfigInfo.hpp"
#include "eglwEnums.hpp"
#include "deSTLUtil.hpp"

#include <algorithm>

using std::vector;

namespace eglu
{

using namespace eglw;

CandidateConfig::CandidateConfig(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLConfig config)
    : m_type(TYPE_EGL_OBJECT)
{
    m_cfg.object.egl     = &egl;
    m_cfg.object.display = display;
    m_cfg.object.config  = config;
}

CandidateConfig::CandidateConfig(const ConfigInfo &configInfo) : m_type(TYPE_CONFIG_INFO)
{
    m_cfg.configInfo = &configInfo;
}

int CandidateConfig::get(uint32_t attrib) const
{
    if (m_type == TYPE_CONFIG_INFO)
        return m_cfg.configInfo->getAttribute(attrib);
    else
    {
        if (attrib == EGL_COLOR_COMPONENT_TYPE_EXT)
        {
            const std::vector<std::string> extensions = getDisplayExtensions(*m_cfg.object.egl, m_cfg.object.display);

            if (de::contains(extensions.begin(), extensions.end(), "EGL_EXT_pixel_format_float"))
                return getConfigAttribInt(*m_cfg.object.egl, m_cfg.object.display, m_cfg.object.config, attrib);
            else
                return EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;
        }
        else
            return getConfigAttribInt(*m_cfg.object.egl, m_cfg.object.display, m_cfg.object.config, attrib);
    }
}

int CandidateConfig::id(void) const
{
    return get(EGL_CONFIG_ID);
}
int CandidateConfig::redSize(void) const
{
    return get(EGL_RED_SIZE);
}
int CandidateConfig::greenSize(void) const
{
    return get(EGL_GREEN_SIZE);
}
int CandidateConfig::blueSize(void) const
{
    return get(EGL_BLUE_SIZE);
}
int CandidateConfig::alphaSize(void) const
{
    return get(EGL_ALPHA_SIZE);
}
int CandidateConfig::depthSize(void) const
{
    return get(EGL_DEPTH_SIZE);
}
int CandidateConfig::stencilSize(void) const
{
    return get(EGL_STENCIL_SIZE);
}
int CandidateConfig::samples(void) const
{
    return get(EGL_SAMPLES);
}
uint32_t CandidateConfig::renderableType(void) const
{
    return (uint32_t)get(EGL_RENDERABLE_TYPE);
}
uint32_t CandidateConfig::surfaceType(void) const
{
    return (uint32_t)get(EGL_SURFACE_TYPE);
}
uint32_t CandidateConfig::colorComponentType(void) const
{
    return (uint32_t)get(EGL_COLOR_COMPONENT_TYPE_EXT);
}
uint32_t CandidateConfig::colorBufferType(void) const
{
    return (uint32_t)get(EGL_COLOR_BUFFER_TYPE);
}

FilterList &FilterList::operator<<(ConfigFilter filter)
{
    m_rules.push_back(filter);
    return *this;
}

FilterList &FilterList::operator<<(const FilterList &other)
{
    size_t oldEnd = m_rules.size();
    m_rules.resize(m_rules.size() + other.m_rules.size());
    std::copy(other.m_rules.begin(), other.m_rules.end(), m_rules.begin() + oldEnd);
    return *this;
}

bool FilterList::match(const Library &egl, EGLDisplay display, EGLConfig config) const
{
    return match(CandidateConfig(egl, display, config));
}

bool FilterList::match(const ConfigInfo &configInfo) const
{
    return match(CandidateConfig(configInfo));
}

bool FilterList::match(const CandidateConfig &candidate) const
{
    for (vector<ConfigFilter>::const_iterator filterIter = m_rules.begin(); filterIter != m_rules.end(); filterIter++)
    {
        ConfigFilter filter = *filterIter;

        if (!filter(candidate))
            return false;
    }

    return true;
}

} // namespace eglu

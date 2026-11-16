#ifndef _RSGSAMPLERS_HPP
#define _RSGSAMPLERS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
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
 * \brief Samplers.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "tcuTexture.hpp"

#include <map>

namespace rsg
{

class Sampler2D
{
public:
    Sampler2D(void) : m_texture(nullptr), m_sampler()
    {
    }

    Sampler2D(const tcu::Texture2D *texture, const tcu::Sampler &sampler) : m_texture(texture), m_sampler(sampler)
    {
    }

    inline tcu::Vec4 sample(float s, float t, float lod) const
    {
        return m_texture->sample(m_sampler, s, t, lod);
    }

private:
    const tcu::Texture2D *m_texture;
    tcu::Sampler m_sampler;
};

class SamplerCube
{
public:
    SamplerCube(void) : m_texture(nullptr), m_sampler()
    {
    }

    SamplerCube(const tcu::TextureCube *texture, const tcu::Sampler &sampler) : m_texture(texture), m_sampler(sampler)
    {
    }

    inline tcu::Vec4 sample(float s, float t, float r, float lod) const
    {
        return m_texture->sample(m_sampler, s, t, r, lod);
    }

private:
    const tcu::TextureCube *m_texture;
    tcu::Sampler m_sampler;
};

typedef std::map<int, Sampler2D> Sampler2DMap;
typedef std::map<int, SamplerCube> SamplerCubeMap;

} // namespace rsg

#endif // _RSGSAMPLERS_HPP

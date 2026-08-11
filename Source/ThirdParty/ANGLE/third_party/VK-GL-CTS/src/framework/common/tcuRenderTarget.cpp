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
 * \brief Render target info.
 *//*--------------------------------------------------------------------*/

#include "tcuRenderTarget.hpp"

namespace tcu
{

RenderTarget::RenderTarget(void)
    : m_width(0)
    , m_height(0)
    , m_pixelFormat(PixelFormat(0, 0, 0, 0))
    , m_depthBits(0)
    , m_stencilBits(0)
    , m_numSamples(0)
{
}

RenderTarget::RenderTarget(int width, int height, const PixelFormat &format, int depthBits, int stencilBits,
                           int numSamples)
    : m_width(width)
    , m_height(height)
    , m_pixelFormat(format)
    , m_depthBits(depthBits)
    , m_stencilBits(stencilBits)
    , m_numSamples(numSamples)
{
}

} // namespace tcu

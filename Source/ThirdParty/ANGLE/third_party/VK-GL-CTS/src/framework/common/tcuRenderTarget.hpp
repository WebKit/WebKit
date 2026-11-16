#ifndef _TCURENDERTARGET_HPP
#define _TCURENDERTARGET_HPP
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

#include "tcuDefs.hpp"
#include "tcuPixelFormat.hpp"

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Render target info
 *//*--------------------------------------------------------------------*/
class RenderTarget
{
public:
    RenderTarget(void);
    RenderTarget(int width, int height, const PixelFormat &pixelFormat, int depthBits, int stencilBits, int numSamples);
    ~RenderTarget(void)
    {
    }

    const PixelFormat &getPixelFormat(void) const
    {
        return m_pixelFormat;
    }
    int getDepthBits(void) const
    {
        return m_depthBits;
    }
    int getStencilBits(void) const
    {
        return m_stencilBits;
    }
    int getNumSamples(void) const
    {
        return m_numSamples;
    }
    int getWidth(void) const
    {
        return m_width;
    }
    int getHeight(void) const
    {
        return m_height;
    }

private:
    // \note Copy constructor and assignment operators are public and auto-generated

    int m_width;
    int m_height;
    PixelFormat m_pixelFormat;
    int m_depthBits;
    int m_stencilBits;
    int m_numSamples; //!< MSAA sample count.
};

} // namespace tcu

#endif // _TCURENDERTARGET_HPP

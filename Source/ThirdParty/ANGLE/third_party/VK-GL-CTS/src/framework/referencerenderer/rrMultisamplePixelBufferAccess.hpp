#ifndef _RRMULTISAMPLEPIXELBUFFERACCESS_HPP
#define _RRMULTISAMPLEPIXELBUFFERACCESS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Reference Renderer
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
 * \brief Multisampled pixel buffer access
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "tcuTexture.hpp"

namespace rr
{

/*--------------------------------------------------------------------*//*!
 * \brief Read-write pixel data access to multisampled buffers.
 *
 * Multisampled data access follows the multisampled indexing convention.
 *
 * Prevents accidental usage of non-multisampled buffer as multisampled
 * with PixelBufferAccess.
 *//*--------------------------------------------------------------------*/
class MultisamplePixelBufferAccess
{
    MultisamplePixelBufferAccess(const tcu::PixelBufferAccess &rawAccess);

public:
    MultisamplePixelBufferAccess(void);

    inline const tcu::PixelBufferAccess &raw(void) const
    {
        return m_access;
    }
    inline int getNumSamples(void) const
    {
        return raw().getWidth();
    }

    const tcu::PixelBufferAccess toSinglesampleAccess(void) const;

    static MultisamplePixelBufferAccess fromSinglesampleAccess(const tcu::PixelBufferAccess &singlesampledAccess);
    static MultisamplePixelBufferAccess fromMultisampleAccess(const tcu::PixelBufferAccess &multisampledAccess);

private:
    tcu::PixelBufferAccess m_access;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Read-only pixel data access to multisampled buffers.
 *
 * Multisampled data access follows the multisampled indexing convention.
 *
 * Prevents accidental usage of non-multisampled buffer as multisampled
 * with PixelBufferAccess.
 *//*--------------------------------------------------------------------*/
class MultisampleConstPixelBufferAccess
{
    MultisampleConstPixelBufferAccess(const tcu::ConstPixelBufferAccess &rawAccess);

public:
    MultisampleConstPixelBufferAccess(const rr::MultisamplePixelBufferAccess &msAccess);
    MultisampleConstPixelBufferAccess(void);

    inline const tcu::ConstPixelBufferAccess &raw(void) const
    {
        return m_access;
    }
    inline int getNumSamples(void) const
    {
        return raw().getWidth();
    }

    const tcu::ConstPixelBufferAccess toSinglesampleAccess(void) const;

    static MultisampleConstPixelBufferAccess fromSinglesampleAccess(
        const tcu::ConstPixelBufferAccess &singlesampledAccess);
    static MultisampleConstPixelBufferAccess fromMultisampleAccess(
        const tcu::ConstPixelBufferAccess &multisampledAccess);

private:
    tcu::ConstPixelBufferAccess m_access;
} DE_WARN_UNUSED_TYPE;

// Multisampled versions of tcu-utils

MultisamplePixelBufferAccess getSubregion(const MultisamplePixelBufferAccess &access, int x, int y, int width,
                                          int height);
MultisampleConstPixelBufferAccess getSubregion(const MultisampleConstPixelBufferAccess &access, int x, int y, int width,
                                               int height);

void resolveMultisampleColorBuffer(const tcu::PixelBufferAccess &dst, const MultisampleConstPixelBufferAccess &src);
void resolveMultisampleDepthBuffer(const tcu::PixelBufferAccess &dst, const MultisampleConstPixelBufferAccess &src);
void resolveMultisampleStencilBuffer(const tcu::PixelBufferAccess &dst, const MultisampleConstPixelBufferAccess &src);
void resolveMultisampleBuffer(const tcu::PixelBufferAccess &dst, const MultisampleConstPixelBufferAccess &src);
tcu::Vec4 resolveMultisamplePixel(const MultisampleConstPixelBufferAccess &access, int x, int y);

void clear(const MultisamplePixelBufferAccess &access, const tcu::Vec4 &color);
void clear(const MultisamplePixelBufferAccess &access, const tcu::IVec4 &color);
void clearDepth(const MultisamplePixelBufferAccess &access, float depth);
void clearStencil(const MultisamplePixelBufferAccess &access, int stencil);

} // namespace rr

#endif // _RRMULTISAMPLEPIXELBUFFERACCESS_HPP

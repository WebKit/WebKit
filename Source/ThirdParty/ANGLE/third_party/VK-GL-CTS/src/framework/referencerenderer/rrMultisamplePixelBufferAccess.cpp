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

#include "rrMultisamplePixelBufferAccess.hpp"
#include "tcuTextureUtil.hpp"

namespace rr
{

MultisamplePixelBufferAccess::MultisamplePixelBufferAccess(const tcu::PixelBufferAccess &rawAccess)
    : m_access(rawAccess)
{
}

MultisamplePixelBufferAccess::MultisamplePixelBufferAccess(void) : m_access(tcu::PixelBufferAccess())
{
}

const tcu::PixelBufferAccess MultisamplePixelBufferAccess::toSinglesampleAccess(void) const
{
    DE_ASSERT(getNumSamples() == 1);

    return tcu::PixelBufferAccess(
        m_access.getFormat(), tcu::IVec3(m_access.getHeight(), m_access.getDepth(), 1),
        tcu::IVec3(m_access.getRowPitch(), m_access.getSlicePitch(), m_access.getSlicePitch() * m_access.getDepth()),
        m_access.getDataPtr());
}

MultisamplePixelBufferAccess MultisamplePixelBufferAccess::fromSinglesampleAccess(
    const tcu::PixelBufferAccess &original)
{
    return MultisamplePixelBufferAccess(tcu::PixelBufferAccess(
        original.getFormat(), tcu::IVec3(1, original.getWidth(), original.getHeight()),
        tcu::IVec3(original.getPixelPitch(), original.getPixelPitch(), original.getRowPitch()), original.getDataPtr()));
}

MultisamplePixelBufferAccess MultisamplePixelBufferAccess::fromMultisampleAccess(
    const tcu::PixelBufferAccess &multisampledAccess)
{
    return MultisamplePixelBufferAccess(multisampledAccess);
}

MultisampleConstPixelBufferAccess::MultisampleConstPixelBufferAccess(void) : m_access(tcu::ConstPixelBufferAccess())
{
}

MultisampleConstPixelBufferAccess::MultisampleConstPixelBufferAccess(const tcu::ConstPixelBufferAccess &rawAccess)
    : m_access(rawAccess)
{
}

MultisampleConstPixelBufferAccess::MultisampleConstPixelBufferAccess(const rr::MultisamplePixelBufferAccess &msAccess)
    : m_access(msAccess.raw())
{
}

const tcu::ConstPixelBufferAccess MultisampleConstPixelBufferAccess::toSinglesampleAccess(void) const
{
    DE_ASSERT(getNumSamples() == 1);

    return tcu::ConstPixelBufferAccess(
        m_access.getFormat(), tcu::IVec3(m_access.getHeight(), m_access.getDepth(), 1),
        tcu::IVec3(m_access.getRowPitch(), m_access.getSlicePitch(), m_access.getSlicePitch() * m_access.getDepth()),
        m_access.getDataPtr());
}

MultisampleConstPixelBufferAccess MultisampleConstPixelBufferAccess::fromSinglesampleAccess(
    const tcu::ConstPixelBufferAccess &original)
{
    return MultisampleConstPixelBufferAccess(tcu::ConstPixelBufferAccess(
        original.getFormat(), tcu::IVec3(1, original.getWidth(), original.getHeight()),
        tcu::IVec3(original.getPixelPitch(), original.getPixelPitch(), original.getRowPitch()), original.getDataPtr()));
}

MultisampleConstPixelBufferAccess MultisampleConstPixelBufferAccess::fromMultisampleAccess(
    const tcu::ConstPixelBufferAccess &multisampledAccess)
{
    return MultisampleConstPixelBufferAccess(multisampledAccess);
}

MultisamplePixelBufferAccess getSubregion(const MultisamplePixelBufferAccess &access, int x, int y, int width,
                                          int height)
{
    return MultisamplePixelBufferAccess::fromMultisampleAccess(
        tcu::getSubregion(access.raw(), 0, x, y, access.getNumSamples(), width, height));
}

MultisampleConstPixelBufferAccess getSubregion(const MultisampleConstPixelBufferAccess &access, int x, int y, int width,
                                               int height)
{
    return MultisampleConstPixelBufferAccess::fromMultisampleAccess(
        tcu::getSubregion(access.raw(), 0, x, y, access.getNumSamples(), width, height));
}

void resolveMultisampleColorBuffer(const tcu::PixelBufferAccess &dst, const MultisampleConstPixelBufferAccess &src)
{
    DE_ASSERT(dst.getWidth() == src.raw().getHeight());
    DE_ASSERT(dst.getHeight() == src.raw().getDepth());

    if (src.getNumSamples() == 1)
    {
        // fast-path for non-multisampled cases
        tcu::copy(dst, src.toSinglesampleAccess());
    }
    else
    {
        const float numSamplesInv = 1.0f / (float)src.getNumSamples();

        for (int y = 0; y < dst.getHeight(); y++)
            for (int x = 0; x < dst.getWidth(); x++)
            {
                tcu::Vec4 sum;
                for (int s = 0; s < src.raw().getWidth(); s++)
                    sum += src.raw().getPixel(s, x, y);

                dst.setPixel(sum * numSamplesInv, x, y);
            }
    }
}

void resolveMultisampleDepthBuffer(const tcu::PixelBufferAccess &dst, const MultisampleConstPixelBufferAccess &src)
{
    DE_ASSERT(dst.getWidth() == src.raw().getHeight());
    DE_ASSERT(dst.getHeight() == src.raw().getDepth());

    const tcu::ConstPixelBufferAccess effectiveSrc =
        tcu::getEffectiveDepthStencilAccess(src.raw(), tcu::Sampler::MODE_DEPTH);
    const tcu::PixelBufferAccess effectiveDst = tcu::getEffectiveDepthStencilAccess(dst, tcu::Sampler::MODE_DEPTH);

    if (src.getNumSamples() == 1)
    {
        // fast-path for non-multisampled cases
        tcu::copy(effectiveDst,
                  MultisampleConstPixelBufferAccess::fromMultisampleAccess(effectiveSrc).toSinglesampleAccess());
    }
    else
    {
        const float numSamplesInv = 1.0f / (float)src.getNumSamples();

        for (int y = 0; y < dst.getHeight(); y++)
            for (int x = 0; x < dst.getWidth(); x++)
            {
                float sum = 0.0f;
                for (int s = 0; s < src.getNumSamples(); s++)
                    sum += effectiveSrc.getPixDepth(s, x, y);

                effectiveDst.setPixDepth(sum * numSamplesInv, x, y);
            }
    }
}

void resolveMultisampleStencilBuffer(const tcu::PixelBufferAccess &dst, const MultisampleConstPixelBufferAccess &src)
{
    DE_ASSERT(dst.getWidth() == src.raw().getHeight());
    DE_ASSERT(dst.getHeight() == src.raw().getDepth());

    const tcu::ConstPixelBufferAccess effectiveSrc =
        tcu::getEffectiveDepthStencilAccess(src.raw(), tcu::Sampler::MODE_STENCIL);
    const tcu::PixelBufferAccess effectiveDst = tcu::getEffectiveDepthStencilAccess(dst, tcu::Sampler::MODE_STENCIL);

    if (src.getNumSamples() == 1)
    {
        // fast-path for non-multisampled cases
        tcu::copy(effectiveDst,
                  MultisampleConstPixelBufferAccess::fromMultisampleAccess(effectiveSrc).toSinglesampleAccess());
    }
    else
    {
        // Resolve by selecting one
        for (int y = 0; y < dst.getHeight(); y++)
            for (int x = 0; x < dst.getWidth(); x++)
                effectiveDst.setPixStencil(effectiveSrc.getPixStencil(0, x, y), x, y);
    }
}

void resolveMultisampleBuffer(const tcu::PixelBufferAccess &dst, const MultisampleConstPixelBufferAccess &src)
{
    switch (src.raw().getFormat().order)
    {
    case tcu::TextureFormat::D:
        resolveMultisampleDepthBuffer(dst, src);
        return;

    case tcu::TextureFormat::S:
        resolveMultisampleStencilBuffer(dst, src);
        return;

    case tcu::TextureFormat::DS:
        resolveMultisampleDepthBuffer(dst, src);
        resolveMultisampleStencilBuffer(dst, src);
        return;

    default:
        resolveMultisampleColorBuffer(dst, src);
        return;
    }
}

tcu::Vec4 resolveMultisamplePixel(const MultisampleConstPixelBufferAccess &access, int x, int y)
{
    tcu::Vec4 sum;
    for (int s = 0; s < access.getNumSamples(); s++)
        sum += access.raw().getPixel(s, x, y);

    return sum / (float)access.getNumSamples();
}

void clear(const MultisamplePixelBufferAccess &access, const tcu::Vec4 &color)
{
    tcu::clear(access.raw(), color);
}

void clear(const MultisamplePixelBufferAccess &access, const tcu::IVec4 &color)
{
    tcu::clear(access.raw(), color);
}

void clearDepth(const MultisamplePixelBufferAccess &access, float depth)
{
    tcu::clearDepth(access.raw(), depth);
}

void clearStencil(const MultisamplePixelBufferAccess &access, int stencil)
{
    tcu::clearStencil(access.raw(), stencil);
}

} // namespace rr

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
 * \brief Reference implementation for per-fragment operations.
 *//*--------------------------------------------------------------------*/

#include "rrFragmentOperations.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"
#include <limits>

using de::clamp;
using de::max;
using de::min;
using tcu::clamp;
using tcu::IVec2;
using tcu::IVec4;
using tcu::max;
using tcu::min;
using tcu::UVec4;
using tcu::Vec3;
using tcu::Vec4;

namespace rr
{

// Return oldValue with the bits indicated by mask replaced by corresponding bits of newValue.
static inline int maskedBitReplace(int oldValue, int newValue, uint32_t mask)
{
    return (oldValue & ~mask) | (newValue & mask);
}

static inline bool isInsideRect(const IVec2 &point, const WindowRectangle &rect)
{
    return de::inBounds(point.x(), rect.left, rect.left + rect.width) &&
           de::inBounds(point.y(), rect.bottom, rect.bottom + rect.height);
}

static inline Vec4 unpremultiply(const Vec4 &v)
{
    if (v.w() > 0.0f)
        return Vec4(v.x() / v.w(), v.y() / v.w(), v.z() / v.w(), v.w());
    else
    {
        DE_ASSERT(v.x() == 0.0f && v.y() == 0.0f && v.z() == 0.0f);
        return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

void clearMultisampleColorBuffer(const tcu::PixelBufferAccess &dst, const Vec4 &v, const WindowRectangle &r)
{
    tcu::clear(tcu::getSubregion(dst, 0, r.left, r.bottom, dst.getWidth(), r.width, r.height), v);
}
void clearMultisampleColorBuffer(const tcu::PixelBufferAccess &dst, const IVec4 &v, const WindowRectangle &r)
{
    tcu::clear(tcu::getSubregion(dst, 0, r.left, r.bottom, dst.getWidth(), r.width, r.height), v);
}
void clearMultisampleColorBuffer(const tcu::PixelBufferAccess &dst, const UVec4 &v, const WindowRectangle &r)
{
    tcu::clear(tcu::getSubregion(dst, 0, r.left, r.bottom, dst.getWidth(), r.width, r.height), v.cast<int>());
}
void clearMultisampleDepthBuffer(const tcu::PixelBufferAccess &dst, float v, const WindowRectangle &r)
{
    tcu::clearDepth(tcu::getSubregion(dst, 0, r.left, r.bottom, dst.getWidth(), r.width, r.height), v);
}
void clearMultisampleStencilBuffer(const tcu::PixelBufferAccess &dst, int v, const WindowRectangle &r)
{
    tcu::clearStencil(tcu::getSubregion(dst, 0, r.left, r.bottom, dst.getWidth(), r.width, r.height), v);
}

FragmentProcessor::FragmentProcessor(void) : m_sampleRegister()
{
}

void FragmentProcessor::executeScissorTest(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                           const WindowRectangle &scissorRect)
{
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
    {
        if (m_sampleRegister[regSampleNdx].isAlive)
        {
            int fragNdx = fragNdxOffset + regSampleNdx / numSamplesPerFragment;

            if (!isInsideRect(inputFragments[fragNdx].pixelCoord, scissorRect))
                m_sampleRegister[regSampleNdx].isAlive = false;
        }
    }
}

void FragmentProcessor::executeStencilCompare(int fragNdxOffset, int numSamplesPerFragment,
                                              const Fragment *inputFragments, const StencilState &stencilState,
                                              int numStencilBits, const tcu::ConstPixelBufferAccess &stencilBuffer)
{
#define SAMPLE_REGISTER_STENCIL_COMPARE(COMPARE_EXPRESSION)                                              \
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)                      \
    {                                                                                                    \
        if (m_sampleRegister[regSampleNdx].isAlive)                                                      \
        {                                                                                                \
            int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;                                 \
            const Fragment &frag = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment]; \
            int stencilBufferValue =                                                                     \
                stencilBuffer.getPixStencil(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());    \
            int maskedRef = stencilState.compMask & clampedStencilRef;                                   \
            int maskedBuf = stencilState.compMask & stencilBufferValue;                                  \
            DE_UNREF(maskedRef);                                                                         \
            DE_UNREF(maskedBuf);                                                                         \
                                                                                                         \
            m_sampleRegister[regSampleNdx].stencilPassed = (COMPARE_EXPRESSION);                         \
        }                                                                                                \
    }

    int clampedStencilRef = de::clamp(stencilState.ref, 0, (1 << numStencilBits) - 1);

    switch (stencilState.func)
    {
    case TESTFUNC_NEVER:
        SAMPLE_REGISTER_STENCIL_COMPARE(false) break;
    case TESTFUNC_ALWAYS:
        SAMPLE_REGISTER_STENCIL_COMPARE(true) break;
    case TESTFUNC_LESS:
        SAMPLE_REGISTER_STENCIL_COMPARE(maskedRef < maskedBuf) break;
    case TESTFUNC_LEQUAL:
        SAMPLE_REGISTER_STENCIL_COMPARE(maskedRef <= maskedBuf) break;
    case TESTFUNC_GREATER:
        SAMPLE_REGISTER_STENCIL_COMPARE(maskedRef > maskedBuf) break;
    case TESTFUNC_GEQUAL:
        SAMPLE_REGISTER_STENCIL_COMPARE(maskedRef >= maskedBuf) break;
    case TESTFUNC_EQUAL:
        SAMPLE_REGISTER_STENCIL_COMPARE(maskedRef == maskedBuf) break;
    case TESTFUNC_NOTEQUAL:
        SAMPLE_REGISTER_STENCIL_COMPARE(maskedRef != maskedBuf) break;
    default:
        DE_ASSERT(false);
    }

#undef SAMPLE_REGISTER_STENCIL_COMPARE
}

void FragmentProcessor::executeStencilSFail(int fragNdxOffset, int numSamplesPerFragment,
                                            const Fragment *inputFragments, const StencilState &stencilState,
                                            int numStencilBits, const tcu::PixelBufferAccess &stencilBuffer)
{
#define SAMPLE_REGISTER_SFAIL(SFAIL_EXPRESSION)                                                                  \
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)                              \
    {                                                                                                            \
        if (m_sampleRegister[regSampleNdx].isAlive && !m_sampleRegister[regSampleNdx].stencilPassed)             \
        {                                                                                                        \
            int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;                                         \
            const Fragment &frag = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];         \
            int stencilBufferValue =                                                                             \
                stencilBuffer.getPixStencil(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());            \
                                                                                                                 \
            stencilBuffer.setPixStencil(                                                                         \
                maskedBitReplace(stencilBufferValue, (SFAIL_EXPRESSION), stencilState.writeMask), fragSampleNdx, \
                frag.pixelCoord.x(), frag.pixelCoord.y());                                                       \
            m_sampleRegister[regSampleNdx].isAlive = false;                                                      \
        }                                                                                                        \
    }

    int clampedStencilRef = de::clamp(stencilState.ref, 0, (1 << numStencilBits) - 1);

    switch (stencilState.sFail)
    {
    case STENCILOP_KEEP:
        SAMPLE_REGISTER_SFAIL(stencilBufferValue) break;
    case STENCILOP_ZERO:
        SAMPLE_REGISTER_SFAIL(0) break;
    case STENCILOP_REPLACE:
        SAMPLE_REGISTER_SFAIL(clampedStencilRef) break;
    case STENCILOP_INCR:
        SAMPLE_REGISTER_SFAIL(de::clamp(stencilBufferValue + 1, 0, (1 << numStencilBits) - 1)) break;
    case STENCILOP_DECR:
        SAMPLE_REGISTER_SFAIL(de::clamp(stencilBufferValue - 1, 0, (1 << numStencilBits) - 1)) break;
    case STENCILOP_INCR_WRAP:
        SAMPLE_REGISTER_SFAIL((stencilBufferValue + 1) & ((1 << numStencilBits) - 1)) break;
    case STENCILOP_DECR_WRAP:
        SAMPLE_REGISTER_SFAIL((stencilBufferValue - 1) & ((1 << numStencilBits) - 1)) break;
    case STENCILOP_INVERT:
        SAMPLE_REGISTER_SFAIL((~stencilBufferValue) & ((1 << numStencilBits) - 1)) break;
    default:
        DE_ASSERT(false);
    }

#undef SAMPLE_REGISTER_SFAIL
}

void FragmentProcessor::executeDepthBoundsTest(int fragNdxOffset, int numSamplesPerFragment,
                                               const Fragment *inputFragments, const float minDepthBound,
                                               const float maxDepthBound,
                                               const tcu::ConstPixelBufferAccess &depthBuffer)
{
    if (depthBuffer.getFormat().type == tcu::TextureFormat::FLOAT ||
        depthBuffer.getFormat().type == tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV)
    {
        for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; ++regSampleNdx)
        {
            if (m_sampleRegister[regSampleNdx].isAlive)
            {
                const int fragSampleNdx = regSampleNdx % numSamplesPerFragment;
                const Fragment &frag    = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];
                const float depthBufferValue =
                    depthBuffer.getPixDepth(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());

                if (!de::inRange(depthBufferValue, minDepthBound, maxDepthBound))
                    m_sampleRegister[regSampleNdx].isAlive = false;
            }
        }
    }
    else
    {
        /* Convert float bounds to target buffer format for comparison */

        uint32_t minDepthBoundUint, maxDepthBoundUint;
        {
            uint32_t buffer[2];
            DE_ASSERT(sizeof(buffer) >= (size_t)depthBuffer.getFormat().getPixelSize());

            tcu::PixelBufferAccess access(depthBuffer.getFormat(), 1, 1, 1, &buffer);
            access.setPixDepth(minDepthBound, 0, 0, 0);
            minDepthBoundUint = access.getPixelUint(0, 0, 0).x();
        }
        {
            uint32_t buffer[2];

            tcu::PixelBufferAccess access(depthBuffer.getFormat(), 1, 1, 1, &buffer);
            access.setPixDepth(maxDepthBound, 0, 0, 0);
            maxDepthBoundUint = access.getPixelUint(0, 0, 0).x();
        }

        for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; ++regSampleNdx)
        {
            if (m_sampleRegister[regSampleNdx].isAlive)
            {
                const int fragSampleNdx = regSampleNdx % numSamplesPerFragment;
                const Fragment &frag    = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];
                const uint32_t depthBufferValue =
                    depthBuffer.getPixelUint(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y()).x();

                if (!de::inRange(depthBufferValue, minDepthBoundUint, maxDepthBoundUint))
                    m_sampleRegister[regSampleNdx].isAlive = false;
            }
        }
    }
}

void FragmentProcessor::executeDepthCompare(int fragNdxOffset, int numSamplesPerFragment,
                                            const Fragment *inputFragments, TestFunc depthFunc,
                                            const tcu::ConstPixelBufferAccess &depthBuffer)
{
#define SAMPLE_REGISTER_DEPTH_COMPARE_F(COMPARE_EXPRESSION)                                                            \
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)                                    \
    {                                                                                                                  \
        if (m_sampleRegister[regSampleNdx].isAlive)                                                                    \
        {                                                                                                              \
            int fragSampleNdx      = regSampleNdx % numSamplesPerFragment;                                             \
            const Fragment &frag   = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];             \
            float depthBufferValue = depthBuffer.getPixDepth(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y()); \
            float sampleDepthFloat = frag.sampleDepths[fragSampleNdx];                                                 \
            float sampleDepth      = de::clamp(sampleDepthFloat, 0.0f, 1.0f);                                          \
                                                                                                                       \
            m_sampleRegister[regSampleNdx].depthPassed = (COMPARE_EXPRESSION);                                         \
                                                                                                                       \
            DE_UNREF(depthBufferValue);                                                                                \
            DE_UNREF(sampleDepth);                                                                                     \
        }                                                                                                              \
    }

#define SAMPLE_REGISTER_DEPTH_COMPARE_UI(COMPARE_EXPRESSION)                                             \
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)                      \
    {                                                                                                    \
        if (m_sampleRegister[regSampleNdx].isAlive)                                                      \
        {                                                                                                \
            int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;                                 \
            const Fragment &frag = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment]; \
            uint32_t depthBufferValue =                                                                  \
                depthBuffer.getPixelUint(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y()).x();   \
            float sampleDepthFloat = frag.sampleDepths[fragSampleNdx];                                   \
                                                                                                         \
            /* Convert input float to target buffer format for comparison */                             \
                                                                                                         \
            uint32_t buffer[2];                                                                          \
                                                                                                         \
            DE_ASSERT(sizeof(buffer) >= (size_t)depthBuffer.getFormat().getPixelSize());                 \
                                                                                                         \
            tcu::PixelBufferAccess access(depthBuffer.getFormat(), 1, 1, 1, &buffer);                    \
            access.setPixDepth(sampleDepthFloat, 0, 0, 0);                                               \
            uint32_t sampleDepth = access.getPixelUint(0, 0, 0).x();                                     \
                                                                                                         \
            m_sampleRegister[regSampleNdx].depthPassed = (COMPARE_EXPRESSION);                           \
                                                                                                         \
            DE_UNREF(depthBufferValue);                                                                  \
            DE_UNREF(sampleDepth);                                                                       \
        }                                                                                                \
    }

    if (depthBuffer.getFormat().type == tcu::TextureFormat::FLOAT ||
        depthBuffer.getFormat().type == tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV)
    {

        switch (depthFunc)
        {
        case TESTFUNC_NEVER:
            SAMPLE_REGISTER_DEPTH_COMPARE_F(false) break;
        case TESTFUNC_ALWAYS:
            SAMPLE_REGISTER_DEPTH_COMPARE_F(true) break;
        case TESTFUNC_LESS:
            SAMPLE_REGISTER_DEPTH_COMPARE_F(sampleDepth < depthBufferValue) break;
        case TESTFUNC_LEQUAL:
            SAMPLE_REGISTER_DEPTH_COMPARE_F(sampleDepth <= depthBufferValue) break;
        case TESTFUNC_GREATER:
            SAMPLE_REGISTER_DEPTH_COMPARE_F(sampleDepth > depthBufferValue) break;
        case TESTFUNC_GEQUAL:
            SAMPLE_REGISTER_DEPTH_COMPARE_F(sampleDepth >= depthBufferValue) break;
        case TESTFUNC_EQUAL:
            SAMPLE_REGISTER_DEPTH_COMPARE_F(sampleDepth == depthBufferValue) break;
        case TESTFUNC_NOTEQUAL:
            SAMPLE_REGISTER_DEPTH_COMPARE_F(sampleDepth != depthBufferValue) break;
        default:
            DE_ASSERT(false);
        }
    }
    else
    {
        switch (depthFunc)
        {
        case TESTFUNC_NEVER:
            SAMPLE_REGISTER_DEPTH_COMPARE_UI(false) break;
        case TESTFUNC_ALWAYS:
            SAMPLE_REGISTER_DEPTH_COMPARE_UI(true) break;
        case TESTFUNC_LESS:
            SAMPLE_REGISTER_DEPTH_COMPARE_UI(sampleDepth < depthBufferValue) break;
        case TESTFUNC_LEQUAL:
            SAMPLE_REGISTER_DEPTH_COMPARE_UI(sampleDepth <= depthBufferValue) break;
        case TESTFUNC_GREATER:
            SAMPLE_REGISTER_DEPTH_COMPARE_UI(sampleDepth > depthBufferValue) break;
        case TESTFUNC_GEQUAL:
            SAMPLE_REGISTER_DEPTH_COMPARE_UI(sampleDepth >= depthBufferValue) break;
        case TESTFUNC_EQUAL:
            SAMPLE_REGISTER_DEPTH_COMPARE_UI(sampleDepth == depthBufferValue) break;
        case TESTFUNC_NOTEQUAL:
            SAMPLE_REGISTER_DEPTH_COMPARE_UI(sampleDepth != depthBufferValue) break;
        default:
            DE_ASSERT(false);
        }
    }

#undef SAMPLE_REGISTER_DEPTH_COMPARE_F
#undef SAMPLE_REGISTER_DEPTH_COMPARE_UI
}

void FragmentProcessor::executeDepthWrite(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                          const tcu::PixelBufferAccess &depthBuffer)
{
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
    {
        if (m_sampleRegister[regSampleNdx].isAlive && m_sampleRegister[regSampleNdx].depthPassed)
        {
            int fragSampleNdx        = regSampleNdx % numSamplesPerFragment;
            const Fragment &frag     = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];
            const float clampedDepth = de::clamp(frag.sampleDepths[fragSampleNdx], 0.0f, 1.0f);

            depthBuffer.setPixDepth(clampedDepth, fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());
        }
    }
}

void FragmentProcessor::executeStencilDpFailAndPass(int fragNdxOffset, int numSamplesPerFragment,
                                                    const Fragment *inputFragments, const StencilState &stencilState,
                                                    int numStencilBits, const tcu::PixelBufferAccess &stencilBuffer)
{
#define SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, EXPRESSION)                                                     \
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)                                 \
    {                                                                                                               \
        if (m_sampleRegister[regSampleNdx].isAlive && (CONDITION))                                                  \
        {                                                                                                           \
            int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;                                            \
            const Fragment &frag = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];            \
            int stencilBufferValue =                                                                                \
                stencilBuffer.getPixStencil(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());               \
                                                                                                                    \
            stencilBuffer.setPixStencil(maskedBitReplace(stencilBufferValue, (EXPRESSION), stencilState.writeMask), \
                                        fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());                   \
        }                                                                                                           \
    }

#define SWITCH_DPFAIL_OR_DPPASS(OP_NAME, CONDITION)                                                                  \
    switch (stencilState.OP_NAME)                                                                                    \
    {                                                                                                                \
    case STENCILOP_KEEP:                                                                                             \
        SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, stencilBufferValue) break;                                       \
    case STENCILOP_ZERO:                                                                                             \
        SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, 0) break;                                                        \
    case STENCILOP_REPLACE:                                                                                          \
        SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, clampedStencilRef) break;                                        \
    case STENCILOP_INCR:                                                                                             \
        SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, de::clamp(stencilBufferValue + 1, 0, (1 << numStencilBits) - 1)) \
        break;                                                                                                       \
    case STENCILOP_DECR:                                                                                             \
        SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, de::clamp(stencilBufferValue - 1, 0, (1 << numStencilBits) - 1)) \
        break;                                                                                                       \
    case STENCILOP_INCR_WRAP:                                                                                        \
        SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, (stencilBufferValue + 1) & ((1 << numStencilBits) - 1)) break;   \
    case STENCILOP_DECR_WRAP:                                                                                        \
        SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, (stencilBufferValue - 1) & ((1 << numStencilBits) - 1)) break;   \
    case STENCILOP_INVERT:                                                                                           \
        SAMPLE_REGISTER_DPFAIL_OR_DPPASS(CONDITION, (~stencilBufferValue) & ((1 << numStencilBits) - 1)) break;      \
    default:                                                                                                         \
        DE_ASSERT(false);                                                                                            \
    }

    int clampedStencilRef = de::clamp(stencilState.ref, 0, (1 << numStencilBits) - 1);

    SWITCH_DPFAIL_OR_DPPASS(dpFail, !m_sampleRegister[regSampleNdx].depthPassed)
    SWITCH_DPFAIL_OR_DPPASS(dpPass, m_sampleRegister[regSampleNdx].depthPassed)

#undef SWITCH_DPFAIL_OR_DPPASS
#undef SAMPLE_REGISTER_DPFAIL_OR_DPPASS
}

void FragmentProcessor::executeBlendFactorComputeRGB(const Vec4 &blendColor, const BlendState &blendRGBState)
{
#define SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, FACTOR_EXPRESSION)                 \
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)  \
    {                                                                                \
        if (m_sampleRegister[regSampleNdx].isAlive)                                  \
        {                                                                            \
            const Vec4 &src  = m_sampleRegister[regSampleNdx].clampedBlendSrcColor;  \
            const Vec4 &src1 = m_sampleRegister[regSampleNdx].clampedBlendSrc1Color; \
            const Vec4 &dst  = m_sampleRegister[regSampleNdx].clampedBlendDstColor;  \
            DE_UNREF(src);                                                           \
            DE_UNREF(src1);                                                          \
            DE_UNREF(dst);                                                           \
                                                                                     \
            m_sampleRegister[regSampleNdx].FACTOR_NAME = (FACTOR_EXPRESSION);        \
        }                                                                            \
    }

#define SWITCH_SRC_OR_DST_FACTOR_RGB(FUNC_NAME, FACTOR_NAME)                                       \
    switch (blendRGBState.FUNC_NAME)                                                               \
    {                                                                                              \
    case BLENDFUNC_ZERO:                                                                           \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(0.0f)) break;                               \
    case BLENDFUNC_ONE:                                                                            \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f)) break;                               \
    case BLENDFUNC_SRC_COLOR:                                                                      \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, src.swizzle(0, 1, 2)) break;                     \
    case BLENDFUNC_ONE_MINUS_SRC_COLOR:                                                            \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f) - src.swizzle(0, 1, 2)) break;        \
    case BLENDFUNC_DST_COLOR:                                                                      \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, dst.swizzle(0, 1, 2)) break;                     \
    case BLENDFUNC_ONE_MINUS_DST_COLOR:                                                            \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f) - dst.swizzle(0, 1, 2)) break;        \
    case BLENDFUNC_SRC_ALPHA:                                                                      \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(src.w())) break;                            \
    case BLENDFUNC_ONE_MINUS_SRC_ALPHA:                                                            \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f - src.w())) break;                     \
    case BLENDFUNC_DST_ALPHA:                                                                      \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(dst.w())) break;                            \
    case BLENDFUNC_ONE_MINUS_DST_ALPHA:                                                            \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f - dst.w())) break;                     \
    case BLENDFUNC_CONSTANT_COLOR:                                                                 \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, blendColor.swizzle(0, 1, 2)) break;              \
    case BLENDFUNC_ONE_MINUS_CONSTANT_COLOR:                                                       \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f) - blendColor.swizzle(0, 1, 2)) break; \
    case BLENDFUNC_CONSTANT_ALPHA:                                                                 \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(blendColor.w())) break;                     \
    case BLENDFUNC_ONE_MINUS_CONSTANT_ALPHA:                                                       \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f - blendColor.w())) break;              \
    case BLENDFUNC_SRC_ALPHA_SATURATE:                                                             \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(de::min(src.w(), 1.0f - dst.w()))) break;   \
    case BLENDFUNC_SRC1_COLOR:                                                                     \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, src1.swizzle(0, 1, 2)) break;                    \
    case BLENDFUNC_ONE_MINUS_SRC1_COLOR:                                                           \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f) - src1.swizzle(0, 1, 2)) break;       \
    case BLENDFUNC_SRC1_ALPHA:                                                                     \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(src1.w())) break;                           \
    case BLENDFUNC_ONE_MINUS_SRC1_ALPHA:                                                           \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, Vec3(1.0f - src1.w())) break;                    \
    default:                                                                                       \
        DE_ASSERT(false);                                                                          \
    }

    SWITCH_SRC_OR_DST_FACTOR_RGB(srcFunc, blendSrcFactorRGB)
    SWITCH_SRC_OR_DST_FACTOR_RGB(dstFunc, blendDstFactorRGB)

#undef SWITCH_SRC_OR_DST_FACTOR_RGB
#undef SAMPLE_REGISTER_BLEND_FACTOR
}

void FragmentProcessor::executeBlendFactorComputeA(const Vec4 &blendColor, const BlendState &blendAState)
{
#define SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, FACTOR_EXPRESSION)                 \
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)  \
    {                                                                                \
        if (m_sampleRegister[regSampleNdx].isAlive)                                  \
        {                                                                            \
            const Vec4 &src  = m_sampleRegister[regSampleNdx].clampedBlendSrcColor;  \
            const Vec4 &src1 = m_sampleRegister[regSampleNdx].clampedBlendSrc1Color; \
            const Vec4 &dst  = m_sampleRegister[regSampleNdx].clampedBlendDstColor;  \
            DE_UNREF(src);                                                           \
            DE_UNREF(src1);                                                          \
            DE_UNREF(dst);                                                           \
                                                                                     \
            m_sampleRegister[regSampleNdx].FACTOR_NAME = (FACTOR_EXPRESSION);        \
        }                                                                            \
    }

#define SWITCH_SRC_OR_DST_FACTOR_A(FUNC_NAME, FACTOR_NAME)                      \
    switch (blendAState.FUNC_NAME)                                              \
    {                                                                           \
    case BLENDFUNC_ZERO:                                                        \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 0.0f) break;                  \
    case BLENDFUNC_ONE:                                                         \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f) break;                  \
    case BLENDFUNC_SRC_COLOR:                                                   \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, src.w()) break;               \
    case BLENDFUNC_ONE_MINUS_SRC_COLOR:                                         \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f - src.w()) break;        \
    case BLENDFUNC_DST_COLOR:                                                   \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, dst.w()) break;               \
    case BLENDFUNC_ONE_MINUS_DST_COLOR:                                         \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f - dst.w()) break;        \
    case BLENDFUNC_SRC_ALPHA:                                                   \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, src.w()) break;               \
    case BLENDFUNC_ONE_MINUS_SRC_ALPHA:                                         \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f - src.w()) break;        \
    case BLENDFUNC_DST_ALPHA:                                                   \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, dst.w()) break;               \
    case BLENDFUNC_ONE_MINUS_DST_ALPHA:                                         \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f - dst.w()) break;        \
    case BLENDFUNC_CONSTANT_COLOR:                                              \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, blendColor.w()) break;        \
    case BLENDFUNC_ONE_MINUS_CONSTANT_COLOR:                                    \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f - blendColor.w()) break; \
    case BLENDFUNC_CONSTANT_ALPHA:                                              \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, blendColor.w()) break;        \
    case BLENDFUNC_ONE_MINUS_CONSTANT_ALPHA:                                    \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f - blendColor.w()) break; \
    case BLENDFUNC_SRC_ALPHA_SATURATE:                                          \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f) break;                  \
    case BLENDFUNC_SRC1_COLOR:                                                  \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, src1.w()) break;              \
    case BLENDFUNC_ONE_MINUS_SRC1_COLOR:                                        \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f - src1.w()) break;       \
    case BLENDFUNC_SRC1_ALPHA:                                                  \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, src1.w()) break;              \
    case BLENDFUNC_ONE_MINUS_SRC1_ALPHA:                                        \
        SAMPLE_REGISTER_BLEND_FACTOR(FACTOR_NAME, 1.0f - src1.w()) break;       \
    default:                                                                    \
        DE_ASSERT(false);                                                       \
    }

    SWITCH_SRC_OR_DST_FACTOR_A(srcFunc, blendSrcFactorA)
    SWITCH_SRC_OR_DST_FACTOR_A(dstFunc, blendDstFactorA)

#undef SWITCH_SRC_OR_DST_FACTOR_A
#undef SAMPLE_REGISTER_BLEND_FACTOR
}

void FragmentProcessor::executeBlend(const BlendState &blendRGBState, const BlendState &blendAState)
{
#define SAMPLE_REGISTER_BLENDED_COLOR(COLOR_NAME, COLOR_EXPRESSION)                 \
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++) \
    {                                                                               \
        if (m_sampleRegister[regSampleNdx].isAlive)                                 \
        {                                                                           \
            SampleData &sample   = m_sampleRegister[regSampleNdx];                  \
            const Vec4 &srcColor = sample.clampedBlendSrcColor;                     \
            const Vec4 &dstColor = sample.clampedBlendDstColor;                     \
                                                                                    \
            sample.COLOR_NAME = (COLOR_EXPRESSION);                                 \
        }                                                                           \
    }

    switch (blendRGBState.equation)
    {
    case BLENDEQUATION_ADD:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedRGB, srcColor.swizzle(0, 1, 2) * sample.blendSrcFactorRGB +
                                                      dstColor.swizzle(0, 1, 2) * sample.blendDstFactorRGB)
        break;
    case BLENDEQUATION_SUBTRACT:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedRGB, srcColor.swizzle(0, 1, 2) * sample.blendSrcFactorRGB -
                                                      dstColor.swizzle(0, 1, 2) * sample.blendDstFactorRGB)
        break;
    case BLENDEQUATION_REVERSE_SUBTRACT:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedRGB, dstColor.swizzle(0, 1, 2) * sample.blendDstFactorRGB -
                                                      srcColor.swizzle(0, 1, 2) * sample.blendSrcFactorRGB)
        break;
    case BLENDEQUATION_MIN:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedRGB, min(srcColor.swizzle(0, 1, 2), dstColor.swizzle(0, 1, 2))) break;
    case BLENDEQUATION_MAX:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedRGB, max(srcColor.swizzle(0, 1, 2), dstColor.swizzle(0, 1, 2))) break;
    default:
        DE_ASSERT(false);
    }

    switch (blendAState.equation)
    {
    case BLENDEQUATION_ADD:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedA,
                                      srcColor.w() * sample.blendSrcFactorA + dstColor.w() * sample.blendDstFactorA)
        break;
    case BLENDEQUATION_SUBTRACT:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedA,
                                      srcColor.w() * sample.blendSrcFactorA - dstColor.w() * sample.blendDstFactorA)
        break;
    case BLENDEQUATION_REVERSE_SUBTRACT:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedA,
                                      dstColor.w() * sample.blendDstFactorA - srcColor.w() * sample.blendSrcFactorA)
        break;
    case BLENDEQUATION_MIN:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedA, min(srcColor.w(), dstColor.w())) break;
    case BLENDEQUATION_MAX:
        SAMPLE_REGISTER_BLENDED_COLOR(blendedA, max(srcColor.w(), dstColor.w())) break;
    default:
        DE_ASSERT(false);
    }
#undef SAMPLE_REGISTER_BLENDED_COLOR
}

namespace advblend
{

inline float multiply(float src, float dst)
{
    return src * dst;
}
inline float screen(float src, float dst)
{
    return src + dst - src * dst;
}
inline float darken(float src, float dst)
{
    return de::min(src, dst);
}
inline float lighten(float src, float dst)
{
    return de::max(src, dst);
}
inline float difference(float src, float dst)
{
    return de::abs(dst - src);
}
inline float exclusion(float src, float dst)
{
    return src + dst - 2.0f * src * dst;
}

inline float overlay(float src, float dst)
{
    if (dst <= 0.5f)
        return 2.0f * src * dst;
    else
        return 1.0f - 2.0f * (1.0f - src) * (1.0f - dst);
}

inline float colordodge(float src, float dst)
{
    if (dst <= 0.0f)
        return 0.0f;
    else if (src < 1.0f)
        return de::min(1.0f, dst / (1.0f - src));
    else
        return 1.0f;
}

inline float colorburn(float src, float dst)
{
    if (dst >= 1.0f)
        return 1.0f;
    else if (src > 0.0f)
        return 1.0f - de::min(1.0f, (1.0f - dst) / src);
    else
        return 0.0f;
}

inline float hardlight(float src, float dst)
{
    if (src <= 0.5f)
        return 2.0f * src * dst;
    else
        return 1.0f - 2.0f * (1.0f - src) * (1.0f - dst);
}

inline float softlight(float src, float dst)
{
    if (src <= 0.5f)
        return dst - (1.0f - 2.0f * src) * dst * (1.0f - dst);
    else if (dst <= 0.25f)
        return dst + (2.0f * src - 1.0f) * dst * ((16.0f * dst - 12.0f) * dst + 3.0f);
    else
        return dst + (2.0f * src - 1.0f) * (deFloatSqrt(dst) - dst);
}

inline float minComp(const Vec3 &v)
{
    return de::min(de::min(v.x(), v.y()), v.z());
}

inline float maxComp(const Vec3 &v)
{
    return de::max(de::max(v.x(), v.y()), v.z());
}

inline float luminosity(const Vec3 &rgb)
{
    return dot(rgb, Vec3(0.3f, 0.59f, 0.11f));
}

inline float saturation(const Vec3 &rgb)
{
    return maxComp(rgb) - minComp(rgb);
}

Vec3 setLum(const Vec3 &cbase, const Vec3 &clum)
{
    const float lbase = luminosity(cbase);
    const float llum  = luminosity(clum);
    const float ldiff = llum - lbase;
    const Vec3 color  = cbase + Vec3(ldiff);
    const float minC  = minComp(color);
    const float maxC  = maxComp(color);

    if (minC < 0.0f)
        return llum + ((color - llum) * llum / (llum != minC ? (llum - minC) : 1.0f));
    else if (maxC > 1.0f)
        return llum + ((color - llum) * (1.0f - llum) / (llum != maxC ? (maxC - llum) : 1.0f));
    else
        return color;
}

Vec3 setLumSat(const Vec3 &cbase, const Vec3 &csat, const Vec3 &clum)
{
    const float minbase = minComp(cbase);
    const float sbase   = saturation(cbase);
    const float ssat    = saturation(csat);
    Vec3 color          = Vec3(0.0f);

    if (sbase > 0.0f)
        color = (cbase - minbase) * ssat / sbase;

    return setLum(color, clum);
}

} // namespace advblend

void FragmentProcessor::executeAdvancedBlend(BlendEquationAdvanced equation)
{
    using namespace advblend;

#define SAMPLE_REGISTER_ADV_BLEND(FUNCTION_NAME)                                               \
    do                                                                                         \
    {                                                                                          \
        for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)        \
        {                                                                                      \
            if (m_sampleRegister[regSampleNdx].isAlive)                                        \
            {                                                                                  \
                SampleData &sample   = m_sampleRegister[regSampleNdx];                         \
                const Vec4 &srcColor = sample.clampedBlendSrcColor;                            \
                const Vec4 &dstColor = sample.clampedBlendDstColor;                            \
                const Vec3 &bias     = sample.blendSrcFactorRGB;                               \
                const float p0       = sample.blendSrcFactorA;                                 \
                const float r        = FUNCTION_NAME(srcColor[0], dstColor[0]) * p0 + bias[0]; \
                const float g        = FUNCTION_NAME(srcColor[1], dstColor[1]) * p0 + bias[1]; \
                const float b        = FUNCTION_NAME(srcColor[2], dstColor[2]) * p0 + bias[2]; \
                                                                                               \
                sample.blendedRGB = Vec3(r, g, b);                                             \
            }                                                                                  \
        }                                                                                      \
    } while (0)

#define SAMPLE_REGISTER_ADV_BLEND_HSL(COLOR_EXPRESSION)                                 \
    do                                                                                  \
    {                                                                                   \
        for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++) \
        {                                                                               \
            if (m_sampleRegister[regSampleNdx].isAlive)                                 \
            {                                                                           \
                SampleData &sample  = m_sampleRegister[regSampleNdx];                   \
                const Vec3 srcColor = sample.clampedBlendSrcColor.swizzle(0, 1, 2);     \
                const Vec3 dstColor = sample.clampedBlendDstColor.swizzle(0, 1, 2);     \
                const Vec3 &bias    = sample.blendSrcFactorRGB;                         \
                const float p0      = sample.blendSrcFactorA;                           \
                                                                                        \
                sample.blendedRGB = (COLOR_EXPRESSION)*p0 + bias;                       \
            }                                                                           \
        }                                                                               \
    } while (0)

    // Pre-compute factors & compute alpha \todo [2014-03-18 pyry] Re-using variable names.
    // \note clampedBlend*Color contains clamped & unpremultiplied colors
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
    {
        if (m_sampleRegister[regSampleNdx].isAlive)
        {
            SampleData &sample   = m_sampleRegister[regSampleNdx];
            const Vec4 &srcColor = sample.clampedBlendSrcColor;
            const Vec4 &dstColor = sample.clampedBlendDstColor;
            const float srcA     = srcColor.w();
            const float dstA     = dstColor.w();
            const float p0       = srcA * dstA;
            const float p1       = srcA * (1.0f - dstA);
            const float p2       = dstA * (1.0f - srcA);
            const Vec3 bias(srcColor[0] * p1 + dstColor[0] * p2, srcColor[1] * p1 + dstColor[1] * p2,
                            srcColor[2] * p1 + dstColor[2] * p2);

            sample.blendSrcFactorRGB = bias;
            sample.blendSrcFactorA   = p0;
            sample.blendedA          = p0 + p1 + p2;
        }
    }

    switch (equation)
    {
    case BLENDEQUATION_ADVANCED_MULTIPLY:
        SAMPLE_REGISTER_ADV_BLEND(multiply);
        break;
    case BLENDEQUATION_ADVANCED_SCREEN:
        SAMPLE_REGISTER_ADV_BLEND(screen);
        break;
    case BLENDEQUATION_ADVANCED_OVERLAY:
        SAMPLE_REGISTER_ADV_BLEND(overlay);
        break;
    case BLENDEQUATION_ADVANCED_DARKEN:
        SAMPLE_REGISTER_ADV_BLEND(darken);
        break;
    case BLENDEQUATION_ADVANCED_LIGHTEN:
        SAMPLE_REGISTER_ADV_BLEND(lighten);
        break;
    case BLENDEQUATION_ADVANCED_COLORDODGE:
        SAMPLE_REGISTER_ADV_BLEND(colordodge);
        break;
    case BLENDEQUATION_ADVANCED_COLORBURN:
        SAMPLE_REGISTER_ADV_BLEND(colorburn);
        break;
    case BLENDEQUATION_ADVANCED_HARDLIGHT:
        SAMPLE_REGISTER_ADV_BLEND(hardlight);
        break;
    case BLENDEQUATION_ADVANCED_SOFTLIGHT:
        SAMPLE_REGISTER_ADV_BLEND(softlight);
        break;
    case BLENDEQUATION_ADVANCED_DIFFERENCE:
        SAMPLE_REGISTER_ADV_BLEND(difference);
        break;
    case BLENDEQUATION_ADVANCED_EXCLUSION:
        SAMPLE_REGISTER_ADV_BLEND(exclusion);
        break;
    case BLENDEQUATION_ADVANCED_HSL_HUE:
        SAMPLE_REGISTER_ADV_BLEND_HSL(setLumSat(srcColor, dstColor, dstColor));
        break;
    case BLENDEQUATION_ADVANCED_HSL_SATURATION:
        SAMPLE_REGISTER_ADV_BLEND_HSL(setLumSat(dstColor, srcColor, dstColor));
        break;
    case BLENDEQUATION_ADVANCED_HSL_COLOR:
        SAMPLE_REGISTER_ADV_BLEND_HSL(setLum(srcColor, dstColor));
        break;
    case BLENDEQUATION_ADVANCED_HSL_LUMINOSITY:
        SAMPLE_REGISTER_ADV_BLEND_HSL(setLum(dstColor, srcColor));
        break;
    default:
        DE_ASSERT(false);
    }

#undef SAMPLE_REGISTER_ADV_BLEND
#undef SAMPLE_REGISTER_ADV_BLEND_HSL
}

void FragmentProcessor::executeColorWrite(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                          bool isSRGB, const tcu::PixelBufferAccess &colorBuffer)
{
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
    {
        if (m_sampleRegister[regSampleNdx].isAlive)
        {
            int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;
            const Fragment &frag = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];
            Vec4 combinedColor;

            combinedColor.xyz() = m_sampleRegister[regSampleNdx].blendedRGB;
            combinedColor.w()   = m_sampleRegister[regSampleNdx].blendedA;

            if (isSRGB)
                combinedColor = tcu::linearToSRGB(combinedColor);

            colorBuffer.setPixel(combinedColor, fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());
        }
    }
}

void FragmentProcessor::executeRGBA8ColorWrite(int fragNdxOffset, int numSamplesPerFragment,
                                               const Fragment *inputFragments,
                                               const tcu::PixelBufferAccess &colorBuffer)
{
    const int fragStride   = 4;
    const int xStride      = colorBuffer.getRowPitch();
    const int yStride      = colorBuffer.getSlicePitch();
    uint8_t *const basePtr = (uint8_t *)colorBuffer.getDataPtr();

    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
    {
        if (m_sampleRegister[regSampleNdx].isAlive)
        {
            const int fragSampleNdx = regSampleNdx % numSamplesPerFragment;
            const Fragment &frag    = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];
            uint8_t *dstPtr =
                basePtr + fragSampleNdx * fragStride + frag.pixelCoord.x() * xStride + frag.pixelCoord.y() * yStride;

            dstPtr[0] = tcu::floatToU8(m_sampleRegister[regSampleNdx].blendedRGB.x());
            dstPtr[1] = tcu::floatToU8(m_sampleRegister[regSampleNdx].blendedRGB.y());
            dstPtr[2] = tcu::floatToU8(m_sampleRegister[regSampleNdx].blendedRGB.z());
            dstPtr[3] = tcu::floatToU8(m_sampleRegister[regSampleNdx].blendedA);
        }
    }
}

void FragmentProcessor::executeMaskedColorWrite(int fragNdxOffset, int numSamplesPerFragment,
                                                const Fragment *inputFragments, const Vec4 &colorMaskFactor,
                                                const Vec4 &colorMaskNegationFactor, bool isSRGB,
                                                const tcu::PixelBufferAccess &colorBuffer)
{
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
    {
        if (m_sampleRegister[regSampleNdx].isAlive)
        {
            int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;
            const Fragment &frag = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];
            Vec4 originalColor   = colorBuffer.getPixel(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());
            Vec4 newColor;

            newColor.xyz() = m_sampleRegister[regSampleNdx].blendedRGB;
            newColor.w()   = m_sampleRegister[regSampleNdx].blendedA;

            if (isSRGB)
                newColor = tcu::linearToSRGB(newColor);

            newColor = colorMaskFactor * newColor + colorMaskNegationFactor * originalColor;

            colorBuffer.setPixel(newColor, fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());
        }
    }
}

void FragmentProcessor::executeSignedValueWrite(int fragNdxOffset, int numSamplesPerFragment,
                                                const Fragment *inputFragments, const tcu::BVec4 &colorMask,
                                                const tcu::PixelBufferAccess &colorBuffer)
{
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
    {
        if (m_sampleRegister[regSampleNdx].isAlive)
        {
            int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;
            const Fragment &frag = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];
            const IVec4 originalValue =
                colorBuffer.getPixelInt(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());

            colorBuffer.setPixel(tcu::select(m_sampleRegister[regSampleNdx].signedValue, originalValue, colorMask),
                                 fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());
        }
    }
}

void FragmentProcessor::executeUnsignedValueWrite(int fragNdxOffset, int numSamplesPerFragment,
                                                  const Fragment *inputFragments, const tcu::BVec4 &colorMask,
                                                  const tcu::PixelBufferAccess &colorBuffer)
{
    for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
    {
        if (m_sampleRegister[regSampleNdx].isAlive)
        {
            int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;
            const Fragment &frag = inputFragments[fragNdxOffset + regSampleNdx / numSamplesPerFragment];
            const UVec4 originalValue =
                colorBuffer.getPixelUint(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());

            colorBuffer.setPixel(tcu::select(m_sampleRegister[regSampleNdx].unsignedValue, originalValue, colorMask),
                                 fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());
        }
    }
}

void FragmentProcessor::render(const rr::MultisamplePixelBufferAccess &msColorBuffer,
                               const rr::MultisamplePixelBufferAccess &msDepthBuffer,
                               const rr::MultisamplePixelBufferAccess &msStencilBuffer, const Fragment *inputFragments,
                               int numFragments, FaceType fragmentFacing, const FragmentOperationState &state)
{
    DE_ASSERT(fragmentFacing < FACETYPE_LAST);
    DE_ASSERT(state.numStencilBits < 32); // code bitshifts numStencilBits, avoid undefined behavior

    const tcu::PixelBufferAccess &colorBuffer   = msColorBuffer.raw();
    const tcu::PixelBufferAccess &depthBuffer   = msDepthBuffer.raw();
    const tcu::PixelBufferAccess &stencilBuffer = msStencilBuffer.raw();

    bool hasDepth   = depthBuffer.getWidth() > 0 && depthBuffer.getHeight() > 0 && depthBuffer.getDepth() > 0;
    bool hasStencil = stencilBuffer.getWidth() > 0 && stencilBuffer.getHeight() > 0 && stencilBuffer.getDepth() > 0;
    bool doDepthBoundsTest = hasDepth && state.depthBoundsTestEnabled;
    bool doDepthTest       = hasDepth && state.depthTestEnabled;
    bool doStencilTest     = hasStencil && state.stencilTestEnabled;

    tcu::TextureChannelClass colorbufferClass = tcu::getTextureChannelClass(msColorBuffer.raw().getFormat().type);
    rr::GenericVecType fragmentDataType =
        (colorbufferClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER) ?
            (rr::GENERICVECTYPE_INT32) :
            ((colorbufferClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER) ? (rr::GENERICVECTYPE_UINT32) :
                                                                               (rr::GENERICVECTYPE_FLOAT));

    DE_ASSERT((!hasDepth || colorBuffer.getWidth() == depthBuffer.getWidth()) &&
              (!hasStencil || colorBuffer.getWidth() == stencilBuffer.getWidth()));
    DE_ASSERT((!hasDepth || colorBuffer.getHeight() == depthBuffer.getHeight()) &&
              (!hasStencil || colorBuffer.getHeight() == stencilBuffer.getHeight()));
    DE_ASSERT((!hasDepth || colorBuffer.getDepth() == depthBuffer.getDepth()) &&
              (!hasStencil || colorBuffer.getDepth() == stencilBuffer.getDepth()));

    // Combined formats must be separated beforehand
    DE_ASSERT(!hasDepth || (!tcu::isCombinedDepthStencilType(depthBuffer.getFormat().type) &&
                            depthBuffer.getFormat().order == tcu::TextureFormat::D));
    DE_ASSERT(!hasStencil || (!tcu::isCombinedDepthStencilType(stencilBuffer.getFormat().type) &&
                              stencilBuffer.getFormat().order == tcu::TextureFormat::S));

    int numSamplesPerFragment = colorBuffer.getWidth();
    int totalNumSamples       = numFragments * numSamplesPerFragment;
    int numSampleGroups =
        (totalNumSamples - 1) / SAMPLE_REGISTER_SIZE + 1; // \note totalNumSamples/SAMPLE_REGISTER_SIZE rounded up.
    const StencilState &stencilState = state.stencilStates[fragmentFacing];
    Vec4 colorMaskFactor(state.colorMask[0] ? 1.0f : 0.0f, state.colorMask[1] ? 1.0f : 0.0f,
                         state.colorMask[2] ? 1.0f : 0.0f, state.colorMask[3] ? 1.0f : 0.0f);
    Vec4 colorMaskNegationFactor(state.colorMask[0] ? 0.0f : 1.0f, state.colorMask[1] ? 0.0f : 1.0f,
                                 state.colorMask[2] ? 0.0f : 1.0f, state.colorMask[3] ? 0.0f : 1.0f);
    bool sRGBTarget = state.sRGBEnabled && tcu::isSRGB(colorBuffer.getFormat());

    DE_ASSERT(SAMPLE_REGISTER_SIZE % numSamplesPerFragment == 0);

    // Divide the fragments' samples into groups of size SAMPLE_REGISTER_SIZE, and perform
    // the per-sample operations for one group at a time.

    for (int sampleGroupNdx = 0; sampleGroupNdx < numSampleGroups; sampleGroupNdx++)
    {
        // The index of the fragment of the sample at the beginning of m_sampleRegisters.
        int groupFirstFragNdx = (sampleGroupNdx * SAMPLE_REGISTER_SIZE) / numSamplesPerFragment;

        // Initialize sample data in the sample register.

        for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
        {
            int fragNdx       = groupFirstFragNdx + regSampleNdx / numSamplesPerFragment;
            int fragSampleNdx = regSampleNdx % numSamplesPerFragment;

            if (fragNdx < numFragments)
            {
                m_sampleRegister[regSampleNdx].isAlive =
                    (inputFragments[fragNdx].coverage & (1u << fragSampleNdx)) != 0;
                m_sampleRegister[regSampleNdx].depthPassed =
                    true; // \note This will stay true if depth test is disabled.
            }
            else
                m_sampleRegister[regSampleNdx].isAlive = false;
        }

        // Scissor test.

        if (state.scissorTestEnabled)
            executeScissorTest(groupFirstFragNdx, numSamplesPerFragment, inputFragments, state.scissorRectangle);

        // Depth bounds test.

        if (doDepthBoundsTest)
            executeDepthBoundsTest(groupFirstFragNdx, numSamplesPerFragment, inputFragments, state.minDepthBound,
                                   state.maxDepthBound, depthBuffer);

        // Stencil test.

        if (doStencilTest)
        {
            executeStencilCompare(groupFirstFragNdx, numSamplesPerFragment, inputFragments, stencilState,
                                  state.numStencilBits, stencilBuffer);
            executeStencilSFail(groupFirstFragNdx, numSamplesPerFragment, inputFragments, stencilState,
                                state.numStencilBits, stencilBuffer);
        }

        // Depth test.
        // \note Current value of isAlive is needed for dpPass and dpFail, so it's only updated after them and not right after depth test.

        if (doDepthTest)
        {
            executeDepthCompare(groupFirstFragNdx, numSamplesPerFragment, inputFragments, state.depthFunc, depthBuffer);

            if (state.depthMask)
                executeDepthWrite(groupFirstFragNdx, numSamplesPerFragment, inputFragments, depthBuffer);
        }

        // Do dpFail and dpPass stencil writes.

        if (doStencilTest)
            executeStencilDpFailAndPass(groupFirstFragNdx, numSamplesPerFragment, inputFragments, stencilState,
                                        state.numStencilBits, stencilBuffer);

        // Kill the samples that failed depth test.

        if (doDepthTest)
        {
            for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
                m_sampleRegister[regSampleNdx].isAlive =
                    m_sampleRegister[regSampleNdx].isAlive && m_sampleRegister[regSampleNdx].depthPassed;
        }

        // Paint fragments to target

        switch (fragmentDataType)
        {
        case rr::GENERICVECTYPE_FLOAT:
        {
            // Select min/max clamping values for blending factors and operands
            Vec4 minClampValue;
            Vec4 maxClampValue;

            if (colorbufferClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
            {
                minClampValue = Vec4(0.0f);
                maxClampValue = Vec4(1.0f);
            }
            else if (colorbufferClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT)
            {
                minClampValue = Vec4(-1.0f);
                maxClampValue = Vec4(1.0f);
            }
            else
            {
                // No clamping
                minClampValue = Vec4(-std::numeric_limits<float>::infinity());
                maxClampValue = Vec4(std::numeric_limits<float>::infinity());
            }

            // Blend calculation - only if using blend.
            if (state.blendMode == BLENDMODE_STANDARD)
            {
                // Put dst color to register, doing srgb-to-linear conversion if needed.
                for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
                {
                    if (m_sampleRegister[regSampleNdx].isAlive)
                    {
                        int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;
                        const Fragment &frag = inputFragments[groupFirstFragNdx + regSampleNdx / numSamplesPerFragment];
                        Vec4 dstColor = colorBuffer.getPixel(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());

                        m_sampleRegister[regSampleNdx].clampedBlendSrcColor =
                            clamp(frag.value.get<float>(), minClampValue, maxClampValue);
                        m_sampleRegister[regSampleNdx].clampedBlendSrc1Color =
                            clamp(frag.value1.get<float>(), minClampValue, maxClampValue);
                        m_sampleRegister[regSampleNdx].clampedBlendDstColor =
                            clamp(sRGBTarget ? tcu::sRGBToLinear(dstColor) : dstColor, minClampValue, maxClampValue);
                    }
                }

                // Calculate blend factors to register.
                executeBlendFactorComputeRGB(state.blendColor, state.blendRGBState);
                executeBlendFactorComputeA(state.blendColor, state.blendAState);

                // Compute blended color.
                executeBlend(state.blendRGBState, state.blendAState);
            }
            else if (state.blendMode == BLENDMODE_ADVANCED)
            {
                // Unpremultiply colors for blending, and do sRGB->linear if necessary
                // \todo [2014-03-17 pyry] Re-consider clampedBlend*Color var names
                for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
                {
                    if (m_sampleRegister[regSampleNdx].isAlive)
                    {
                        int fragSampleNdx    = regSampleNdx % numSamplesPerFragment;
                        const Fragment &frag = inputFragments[groupFirstFragNdx + regSampleNdx / numSamplesPerFragment];
                        const Vec4 srcColor  = frag.value.get<float>();
                        const Vec4 dstColor =
                            colorBuffer.getPixel(fragSampleNdx, frag.pixelCoord.x(), frag.pixelCoord.y());

                        m_sampleRegister[regSampleNdx].clampedBlendSrcColor =
                            unpremultiply(clamp(srcColor, minClampValue, maxClampValue));
                        m_sampleRegister[regSampleNdx].clampedBlendDstColor = unpremultiply(
                            clamp(sRGBTarget ? tcu::sRGBToLinear(dstColor) : dstColor, minClampValue, maxClampValue));
                    }
                }

                executeAdvancedBlend(state.blendEquationAdvaced);
            }
            else
            {
                // Not using blend - just put values to register as-is.
                DE_ASSERT(state.blendMode == BLENDMODE_NONE);

                for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
                {
                    if (m_sampleRegister[regSampleNdx].isAlive)
                    {
                        const Fragment &frag = inputFragments[groupFirstFragNdx + regSampleNdx / numSamplesPerFragment];

                        m_sampleRegister[regSampleNdx].blendedRGB = frag.value.get<float>().xyz();
                        m_sampleRegister[regSampleNdx].blendedA   = frag.value.get<float>().w();
                    }
                }
            }

            // Clamp result values in sample register
            if (colorbufferClass != tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
            {
                for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
                {
                    if (m_sampleRegister[regSampleNdx].isAlive)
                    {
                        m_sampleRegister[regSampleNdx].blendedRGB =
                            clamp(m_sampleRegister[regSampleNdx].blendedRGB, minClampValue.swizzle(0, 1, 2),
                                  maxClampValue.swizzle(0, 1, 2));
                        m_sampleRegister[regSampleNdx].blendedA =
                            clamp(m_sampleRegister[regSampleNdx].blendedA, minClampValue.w(), maxClampValue.w());
                    }
                }
            }

            // Finally, write the colors to the color buffer.

            if (state.colorMask[0] && state.colorMask[1] && state.colorMask[2] && state.colorMask[3])
            {
                if (colorBuffer.getFormat() ==
                    tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
                    executeRGBA8ColorWrite(groupFirstFragNdx, numSamplesPerFragment, inputFragments, colorBuffer);
                else
                    executeColorWrite(groupFirstFragNdx, numSamplesPerFragment, inputFragments, sRGBTarget,
                                      colorBuffer);
            }
            else if (state.colorMask[0] || state.colorMask[1] || state.colorMask[2] || state.colorMask[3])
                executeMaskedColorWrite(groupFirstFragNdx, numSamplesPerFragment, inputFragments, colorMaskFactor,
                                        colorMaskNegationFactor, sRGBTarget, colorBuffer);
            break;
        }
        case rr::GENERICVECTYPE_INT32:
            // Write fragments
            for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
            {
                if (m_sampleRegister[regSampleNdx].isAlive)
                {
                    const Fragment &frag = inputFragments[groupFirstFragNdx + regSampleNdx / numSamplesPerFragment];

                    m_sampleRegister[regSampleNdx].signedValue = frag.value.get<int32_t>();
                }
            }

            if (state.colorMask[0] || state.colorMask[1] || state.colorMask[2] || state.colorMask[3])
                executeSignedValueWrite(groupFirstFragNdx, numSamplesPerFragment, inputFragments, state.colorMask,
                                        colorBuffer);
            break;

        case rr::GENERICVECTYPE_UINT32:
            // Write fragments
            for (int regSampleNdx = 0; regSampleNdx < SAMPLE_REGISTER_SIZE; regSampleNdx++)
            {
                if (m_sampleRegister[regSampleNdx].isAlive)
                {
                    const Fragment &frag = inputFragments[groupFirstFragNdx + regSampleNdx / numSamplesPerFragment];

                    m_sampleRegister[regSampleNdx].unsignedValue = frag.value.get<uint32_t>();
                }
            }

            if (state.colorMask[0] || state.colorMask[1] || state.colorMask[2] || state.colorMask[3])
                executeUnsignedValueWrite(groupFirstFragNdx, numSamplesPerFragment, inputFragments, state.colorMask,
                                          colorBuffer);
            break;

        default:
            DE_ASSERT(false);
        }
    }
}

} // namespace rr

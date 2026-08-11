#ifndef _RRFRAGMENTOPERATIONS_HPP
#define _RRFRAGMENTOPERATIONS_HPP
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
 *
 * \note In this file, a multisample buffer means a tcu::PixelBufferAccess
 *         (or ConstPixelBufferAccess) where the x coordinate is the sample
 *         index and the y and z coordinates are the pixel's x and y
 *         coordinates, respectively. To prevent supplying a buffer in
 *         a wrong format the buffers are wrapped in rr::MultisamplePixelBufferAccess
 *         wrapper. FragmentProcessor::render() operates on
 *         this kind of buffers. The function fromSinglesampleAccess() can be
 *         used to get a one-sampled multisample access to a normal 2d
 *         buffer.
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "tcuVector.hpp"
#include "tcuTexture.hpp"
#include "rrRenderState.hpp"
#include "rrGenericVector.hpp"
#include "rrMultisamplePixelBufferAccess.hpp"

namespace rr
{

struct Fragment
{
    tcu::IVec2 pixelCoord;
    GenericVec4 value;
    GenericVec4 value1;
    uint32_t coverage;
    const float *sampleDepths;

    Fragment(const tcu::IVec2 &pixelCoord_, const GenericVec4 &value_, uint32_t coverage_, const float *sampleDepths_)
        : pixelCoord(pixelCoord_)
        , value(value_)
        , value1()
        , coverage(coverage_)
        , sampleDepths(sampleDepths_)
    {
    }

    Fragment(const tcu::IVec2 &pixelCoord_, const GenericVec4 &value_, const GenericVec4 &value1_, uint32_t coverage_,
             const float *sampleDepths_)
        : pixelCoord(pixelCoord_)
        , value(value_)
        , value1(value1_)
        , coverage(coverage_)
        , sampleDepths(sampleDepths_)
    {
    }

    Fragment(void) : pixelCoord(0), value(), coverage(0), sampleDepths(nullptr)
    {
    }
} DE_WARN_UNUSED_TYPE;

// These functions are for clearing only a specific pixel rectangle in a multisample buffer.
// When clearing the entire buffer, tcu::clear, tcu::clearDepth and tcu::clearStencil can be used.
void clearMultisampleColorBuffer(const tcu::PixelBufferAccess &dst, const tcu::Vec4 &value,
                                 const WindowRectangle &rect);
void clearMultisampleColorBuffer(const tcu::PixelBufferAccess &dst, const tcu::IVec4 &value,
                                 const WindowRectangle &rect);
void clearMultisampleColorBuffer(const tcu::PixelBufferAccess &dst, const tcu::UVec4 &value,
                                 const WindowRectangle &rect);
void clearMultisampleDepthBuffer(const tcu::PixelBufferAccess &dst, float value, const WindowRectangle &rect);
void clearMultisampleStencilBuffer(const tcu::PixelBufferAccess &dst, int value, const WindowRectangle &rect);

/*--------------------------------------------------------------------*//*!
 * \brief Reference fragment renderer.
 *
 * FragmentProcessor.render() draws a given set of fragments. No two
 * fragments given in one render() call should have the same pixel
 * coordinates coordinates, and they must all have the same facing.
 *//*--------------------------------------------------------------------*/
class FragmentProcessor
{
public:
    FragmentProcessor(void);

    void render(const rr::MultisamplePixelBufferAccess &colorMultisampleBuffer,
                const rr::MultisamplePixelBufferAccess &depthMultisampleBuffer,
                const rr::MultisamplePixelBufferAccess &stencilMultisampleBuffer, const Fragment *fragments,
                int numFragments, FaceType fragmentFacing, const FragmentOperationState &state);

private:
    enum
    {
        SAMPLE_REGISTER_SIZE = 64
    };
    struct SampleData
    {
        bool isAlive;
        bool stencilPassed;
        bool depthPassed;
        tcu::Vec4 clampedBlendSrcColor;
        tcu::Vec4 clampedBlendSrc1Color;
        tcu::Vec4 clampedBlendDstColor;
        tcu::Vec3 blendSrcFactorRGB;
        float blendSrcFactorA;
        tcu::Vec3 blendDstFactorRGB;
        float blendDstFactorA;
        tcu::Vec3 blendedRGB;
        float blendedA;
        tcu::Vector<int32_t, 4> signedValue;    //!< integer targets
        tcu::Vector<uint32_t, 4> unsignedValue; //!< unsigned integer targets
    };

    // These functions operate on the values in m_sampleRegister and, in some cases, the buffers.

    void executeScissorTest(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                            const WindowRectangle &scissorRect);
    void executeStencilCompare(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                               const StencilState &stencilState, int numStencilBits,
                               const tcu::ConstPixelBufferAccess &stencilBuffer);
    void executeStencilSFail(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                             const StencilState &stencilState, int numStencilBits,
                             const tcu::PixelBufferAccess &stencilBuffer);
    void executeDepthBoundsTest(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                const float minDepthBound, const float maxDepthBound,
                                const tcu::ConstPixelBufferAccess &depthBuffer);
    void executeDepthCompare(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                             TestFunc depthFunc, const tcu::ConstPixelBufferAccess &depthBuffer);
    void executeDepthWrite(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                           const tcu::PixelBufferAccess &depthBuffer);
    void executeStencilDpFailAndPass(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                     const StencilState &stencilState, int numStencilBits,
                                     const tcu::PixelBufferAccess &stencilBuffer);
    void executeBlendFactorComputeRGB(const tcu::Vec4 &blendColor, const BlendState &blendRGBState);
    void executeBlendFactorComputeA(const tcu::Vec4 &blendColor, const BlendState &blendAState);
    void executeBlend(const BlendState &blendRGBState, const BlendState &blendAState);
    void executeAdvancedBlend(BlendEquationAdvanced equation);

    void executeColorWrite(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments, bool isSRGB,
                           const tcu::PixelBufferAccess &colorBuffer);
    void executeRGBA8ColorWrite(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                const tcu::PixelBufferAccess &colorBuffer);
    void executeMaskedColorWrite(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                 const tcu::Vec4 &colorMaskFactor, const tcu::Vec4 &colorMaskNegationFactor,
                                 bool isSRGB, const tcu::PixelBufferAccess &colorBuffer);
    void executeSignedValueWrite(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                 const tcu::BVec4 &colorMask, const tcu::PixelBufferAccess &colorBuffer);
    void executeUnsignedValueWrite(int fragNdxOffset, int numSamplesPerFragment, const Fragment *inputFragments,
                                   const tcu::BVec4 &colorMask, const tcu::PixelBufferAccess &colorBuffer);

    SampleData m_sampleRegister[SAMPLE_REGISTER_SIZE];
} DE_WARN_UNUSED_TYPE;

} // namespace rr

#endif // _RRFRAGMENTOPERATIONS_HPP

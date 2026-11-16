#ifndef _RRRASTERIZER_HPP
#define _RRRASTERIZER_HPP
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
 * \brief Reference rasterizer
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "tcuVector.hpp"
#include "rrRenderState.hpp"
#include "rrFragmentPacket.hpp"

namespace rr
{

//! Rasterizer configuration
enum
{
    RASTERIZER_MAX_SAMPLES_PER_FRAGMENT = 16
};

//! Get coverage bit value.
inline uint64_t getCoverageBit(int numSamples, int x, int y, int sampleNdx)
{
    const int numBits    = (int)sizeof(uint64_t) * 8;
    const int maxSamples = numBits / 4;
    DE_STATIC_ASSERT(maxSamples >= RASTERIZER_MAX_SAMPLES_PER_FRAGMENT);
    DE_ASSERT(de::inRange(numSamples, 1, maxSamples) && de::inBounds(x, 0, 2) && de::inBounds(y, 0, 2));
    return 1ull << ((x * 2 + y) * numSamples + sampleNdx);
}

//! Get all sample bits for fragment
inline uint64_t getCoverageFragmentSampleBits(int numSamples, int x, int y)
{
    DE_ASSERT(de::inBounds(x, 0, 2) && de::inBounds(y, 0, 2));
    const uint64_t fragMask = (1ull << numSamples) - 1;
    return fragMask << (x * 2 + y) * numSamples;
}

//! Set bit in coverage mask.
inline uint64_t setCoverageValue(uint64_t mask, int numSamples, int x, int y, int sampleNdx, bool val)
{
    const uint64_t bit = getCoverageBit(numSamples, x, y, sampleNdx);
    return val ? (mask | bit) : (mask & ~bit);
}

//! Get coverage bit value in mask.
inline bool getCoverageValue(uint64_t mask, int numSamples, int x, int y, int sampleNdx)
{
    return (mask & getCoverageBit(numSamples, x, y, sampleNdx)) != 0;
}

//! Test if any sample for fragment is live
inline bool getCoverageAnyFragmentSampleLive(uint64_t mask, int numSamples, int x, int y)
{
    return (mask & getCoverageFragmentSampleBits(numSamples, x, y)) != 0;
}

//! Get position of first coverage bit of fragment - equivalent to deClz64(getCoverageFragmentSampleBits(numSamples, x, y)).
inline int getCoverageOffset(int numSamples, int x, int y)
{
    return (x * 2 + y) * numSamples;
}

/*--------------------------------------------------------------------*//*!
 * \brief Edge function
 *
 * Edge function can be evaluated for point P (in fixed-point coordinates
 * with SUBPIXEL_BITS fractional part) by computing
 *  D = a*Px + b*Py + c
 *
 * D will be fixed-point value where lower (SUBPIXEL_BITS*2) bits will
 * be fractional part.
 *
 * a and b are stored with SUBPIXEL_BITS fractional part, while c is stored
 * with SUBPIXEL_BITS*2 fractional bits.
 *//*--------------------------------------------------------------------*/
struct EdgeFunction
{
    inline EdgeFunction(void) : a(0), b(0), c(0), inclusive(false)
    {
    }

    int64_t a;
    int64_t b;
    int64_t c;
    bool inclusive; //!< True if edge is inclusive according to fill rules.
};

/*--------------------------------------------------------------------*//*!
 * \brief Triangle rasterizer
 *
 * Triangle rasterizer implements following features:
 *  - Rasterization using fixed-point coordinates
 *  - 1, 4, and 16 -sample rasterization
 *  - Depth interpolation
 *  - Perspective-correct barycentric computation for interpolation
 *  - Visible face determination
 *
 * It does not (and will not) implement following:
 *  - Triangle setup
 *  - Clipping
 *  - Degenerate elimination
 *  - Coordinate transformation (inputs are in screen-space)
 *  - Culling - logic can be implemented outside by querying visible face
 *  - Scissoring (this can be done by controlling viewport rectangle)
 *  - Any per-fragment operations
 *//*--------------------------------------------------------------------*/
class TriangleRasterizer
{
public:
    TriangleRasterizer(const tcu::IVec4 &viewport, const int numSamples, const RasterizationState &state,
                       const int suppixelBits);

    void init(const tcu::Vec4 &v0, const tcu::Vec4 &v1, const tcu::Vec4 &v2);

    // Following functions are only available after init()
    FaceType getVisibleFace(void) const
    {
        return m_face;
    }
    void rasterize(FragmentPacket *const fragmentPackets, float *const depthValues, const int maxFragmentPackets,
                   int &numPacketsRasterized);

private:
    void rasterizeSingleSample(FragmentPacket *const fragmentPackets, float *const depthValues,
                               const int maxFragmentPackets, int &numPacketsRasterized);

    template <int NumSamples>
    void rasterizeMultiSample(FragmentPacket *const fragmentPackets, float *const depthValues,
                              const int maxFragmentPackets, int &numPacketsRasterized);

    // Constant rasterization state.
    const tcu::IVec4 m_viewport;
    const int m_numSamples;
    const Winding m_winding;
    const HorizontalFill m_horizontalFill;
    const VerticalFill m_verticalFill;
    const int m_subpixelBits;

    // Per-triangle rasterization state.
    tcu::Vec4 m_v0;
    tcu::Vec4 m_v1;
    tcu::Vec4 m_v2;
    EdgeFunction m_edge01;
    EdgeFunction m_edge12;
    EdgeFunction m_edge20;
    FaceType m_face;                           //!< Triangle orientation, eg. visible face.
    tcu::IVec2 m_bboxMin;                      //!< Bounding box min (inclusive).
    tcu::IVec2 m_bboxMax;                      //!< Bounding box max (inclusive).
    tcu::IVec2 m_curPos;                       //!< Current rasterization position.
    ViewportOrientation m_viewportOrientation; //!< Direction of +x+y axis
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Single sample line rasterizer
 *
 * Line rasterizer implements following features:
 *  - Rasterization using fixed-point coordinates
 *  - Depth interpolation
 *  - Perspective-correct interpolation
 *
 * It does not (and will not) implement following:
 *  - Clipping
 *  - Multisampled line rasterization
 *//*--------------------------------------------------------------------*/
class SingleSampleLineRasterizer
{
public:
    SingleSampleLineRasterizer(const tcu::IVec4 &viewport, const int subpixelBits);
    ~SingleSampleLineRasterizer(void);

    void init(const tcu::Vec4 &v0, const tcu::Vec4 &v1, float lineWidth, uint32_t stippleFactor,
              uint16_t stipplePattern);

    // only available after init()
    void rasterize(FragmentPacket *const fragmentPackets, float *const depthValues, const int maxFragmentPackets,
                   int &numPacketsRasterized);

    void resetStipple()
    {
        m_stippleCounter = 0;
    }

private:
    SingleSampleLineRasterizer(const SingleSampleLineRasterizer &);            // not allowed
    SingleSampleLineRasterizer &operator=(const SingleSampleLineRasterizer &); // not allowed

    // Constant rasterization state.
    const tcu::IVec4 m_viewport;
    const int m_subpixelBits;

    // Per-line rasterization state.
    tcu::Vec4 m_v0;
    tcu::Vec4 m_v1;
    tcu::IVec2 m_bboxMin;     //!< Bounding box min (inclusive).
    tcu::IVec2 m_bboxMax;     //!< Bounding box max (inclusive).
    tcu::IVec2 m_curPos;      //!< Current rasterization position.
    int32_t m_curRowFragment; //!< Current rasterization position of one fragment in column of lineWidth fragments
    float m_lineWidth;
    uint32_t m_stippleFactor;
    uint16_t m_stipplePattern;
    uint32_t m_stippleCounter;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Multisampled line rasterizer
 *
 * Line rasterizer implements following features:
 *  - Rasterization using fixed-point coordinates
 *  - Depth interpolation
 *  - Perspective-correct interpolation
 *
 * It does not (and will not) implement following:
 *  - Clipping
 *  - Aliased line rasterization
 *//*--------------------------------------------------------------------*/
class MultiSampleLineRasterizer
{
public:
    MultiSampleLineRasterizer(const int numSamples, const tcu::IVec4 &viewport, const int subpixelBits);
    ~MultiSampleLineRasterizer();

    void init(const tcu::Vec4 &v0, const tcu::Vec4 &v1, float lineWidth);

    // only available after init()
    void rasterize(FragmentPacket *const fragmentPackets, float *const depthValues, const int maxFragmentPackets,
                   int &numPacketsRasterized);

private:
    MultiSampleLineRasterizer(const MultiSampleLineRasterizer &);            // not allowed
    MultiSampleLineRasterizer &operator=(const MultiSampleLineRasterizer &); // not allowed

    // Constant rasterization state.
    const int m_numSamples;

    // Per-line rasterization state.
    TriangleRasterizer
        m_triangleRasterizer0; //!< not in array because we want to initialize these in the initialization list
    TriangleRasterizer m_triangleRasterizer1;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Pixel diamond
 *
 * Structure representing a diamond a line exits.
 *//*--------------------------------------------------------------------*/
struct LineExitDiamond
{
    tcu::IVec2 position;
};

/*--------------------------------------------------------------------*//*!
 * \brief Line exit diamond generator
 *
 * For a given line, generates list of diamonds the line exits using the
 * line-exit rules of the line rasterization. Does not do scissoring.
 *
 * \note Not used by rr, but provided to prevent test cases requiring
 *       accurate diamonds from abusing SingleSampleLineRasterizer.
 *//*--------------------------------------------------------------------*/
class LineExitDiamondGenerator
{
public:
    LineExitDiamondGenerator(const int subpixelBits);
    ~LineExitDiamondGenerator(void);

    void init(const tcu::Vec4 &v0, const tcu::Vec4 &v1);

    // only available after init()
    void rasterize(LineExitDiamond *const lineDiamonds, const int maxDiamonds, int &numWritten);

private:
    LineExitDiamondGenerator(const LineExitDiamondGenerator &);            // not allowed
    LineExitDiamondGenerator &operator=(const LineExitDiamondGenerator &); // not allowed

    const int m_subpixelBits;

    // Per-line rasterization state.
    tcu::Vec4 m_v0;
    tcu::Vec4 m_v1;
    tcu::IVec2 m_bboxMin; //!< Bounding box min (inclusive).
    tcu::IVec2 m_bboxMax; //!< Bounding box max (inclusive).
    tcu::IVec2 m_curPos;  //!< Current rasterization position.
};

} // namespace rr

#endif // _RRRASTERIZER_HPP

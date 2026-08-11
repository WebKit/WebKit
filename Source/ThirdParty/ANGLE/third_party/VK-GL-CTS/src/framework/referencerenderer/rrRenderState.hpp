#ifndef _RRRENDERSTATE_HPP
#define _RRRENDERSTATE_HPP
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
 * \brief Reference renderer render state.
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "rrMultisamplePixelBufferAccess.hpp"
#include "tcuTexture.hpp"

namespace rr
{

//! Horizontal fill rule
enum HorizontalFill
{
    FILL_LEFT,
    FILL_RIGHT
};

//! Vertical fill rule
enum VerticalFill
{
    FILL_TOP,
    FILL_BOTTOM,
};

//! Winding mode
enum Winding
{
    WINDING_CCW = 0, //!< Counter-clockwise winding
    WINDING_CW,      //!< Clockwise winding

    WINDING_LAST
};

//! Triangle cull mode
enum CullMode
{
    CULLMODE_NONE,
    CULLMODE_BACK,
    CULLMODE_FRONT,
    CULLMODE_FRONT_AND_BACK,

    CULLMODE_LAST
};

//! Viewport Orientation of renderer this will be compared against
enum ViewportOrientation
{
    VIEWPORTORIENTATION_LOWER_LEFT = 0, //<! Corresponds to GL
    VIEWPORTORIENTATION_UPPER_LEFT,     //<! Corresponds to Vulkan

    VIEWPORTORIENTATION_LAST
};

struct RasterizationState
{
    RasterizationState(void)
        : winding(WINDING_CCW)
        , horizontalFill(FILL_LEFT)
        , verticalFill(FILL_BOTTOM)
        , viewportOrientation(VIEWPORTORIENTATION_LAST)
    {
    }

    Winding winding;
    HorizontalFill horizontalFill;
    VerticalFill verticalFill;
    ViewportOrientation viewportOrientation;
};

enum TestFunc
{
    TESTFUNC_NEVER = 0,
    TESTFUNC_ALWAYS,
    TESTFUNC_LESS,
    TESTFUNC_LEQUAL,
    TESTFUNC_GREATER,
    TESTFUNC_GEQUAL,
    TESTFUNC_EQUAL,
    TESTFUNC_NOTEQUAL,

    TESTFUNC_LAST
};

enum StencilOp
{
    STENCILOP_KEEP = 0,
    STENCILOP_ZERO,
    STENCILOP_REPLACE,
    STENCILOP_INCR, //!< Increment with saturation.
    STENCILOP_DECR, //!< Decrement with saturation.
    STENCILOP_INCR_WRAP,
    STENCILOP_DECR_WRAP,
    STENCILOP_INVERT,

    STENCILOP_LAST
};

enum BlendMode
{
    BLENDMODE_NONE = 0, //!< No blending.
    BLENDMODE_STANDARD, //!< Standard blending.
    BLENDMODE_ADVANCED, //!< Advanced blending mode, as defined in GL_KHR_blend_equation_advanced.

    BLENDMODE_LAST
};

enum BlendEquation
{
    BLENDEQUATION_ADD = 0,
    BLENDEQUATION_SUBTRACT,
    BLENDEQUATION_REVERSE_SUBTRACT,
    BLENDEQUATION_MIN,
    BLENDEQUATION_MAX,

    BLENDEQUATION_LAST
};

enum BlendEquationAdvanced
{
    BLENDEQUATION_ADVANCED_MULTIPLY = 0,
    BLENDEQUATION_ADVANCED_SCREEN,
    BLENDEQUATION_ADVANCED_OVERLAY,
    BLENDEQUATION_ADVANCED_DARKEN,
    BLENDEQUATION_ADVANCED_LIGHTEN,
    BLENDEQUATION_ADVANCED_COLORDODGE,
    BLENDEQUATION_ADVANCED_COLORBURN,
    BLENDEQUATION_ADVANCED_HARDLIGHT,
    BLENDEQUATION_ADVANCED_SOFTLIGHT,
    BLENDEQUATION_ADVANCED_DIFFERENCE,
    BLENDEQUATION_ADVANCED_EXCLUSION,
    BLENDEQUATION_ADVANCED_HSL_HUE,
    BLENDEQUATION_ADVANCED_HSL_SATURATION,
    BLENDEQUATION_ADVANCED_HSL_COLOR,
    BLENDEQUATION_ADVANCED_HSL_LUMINOSITY,

    BLENDEQUATION_ADVANCED_LAST
};

enum BlendFunc
{
    BLENDFUNC_ZERO = 0,
    BLENDFUNC_ONE,
    BLENDFUNC_SRC_COLOR,
    BLENDFUNC_ONE_MINUS_SRC_COLOR,
    BLENDFUNC_DST_COLOR,
    BLENDFUNC_ONE_MINUS_DST_COLOR,
    BLENDFUNC_SRC_ALPHA,
    BLENDFUNC_ONE_MINUS_SRC_ALPHA,
    BLENDFUNC_DST_ALPHA,
    BLENDFUNC_ONE_MINUS_DST_ALPHA,
    BLENDFUNC_CONSTANT_COLOR,
    BLENDFUNC_ONE_MINUS_CONSTANT_COLOR,
    BLENDFUNC_CONSTANT_ALPHA,
    BLENDFUNC_ONE_MINUS_CONSTANT_ALPHA,
    BLENDFUNC_SRC_ALPHA_SATURATE,
    BLENDFUNC_SRC1_COLOR,
    BLENDFUNC_ONE_MINUS_SRC1_COLOR,
    BLENDFUNC_SRC1_ALPHA,
    BLENDFUNC_ONE_MINUS_SRC1_ALPHA,

    BLENDFUNC_LAST
};

struct StencilState
{
    TestFunc func;
    int ref;
    uint32_t compMask;
    StencilOp sFail;
    StencilOp dpFail;
    StencilOp dpPass;
    uint32_t writeMask;

    StencilState(void)
        : func(TESTFUNC_ALWAYS)
        , ref(0)
        , compMask(~0U)
        , sFail(STENCILOP_KEEP)
        , dpFail(STENCILOP_KEEP)
        , dpPass(STENCILOP_KEEP)
        , writeMask(~0U)
    {
    }
};

struct BlendState
{
    BlendEquation equation;
    BlendFunc srcFunc;
    BlendFunc dstFunc;

    BlendState(void) : equation(BLENDEQUATION_ADD), srcFunc(BLENDFUNC_ONE), dstFunc(BLENDFUNC_ZERO)
    {
    }
};

struct WindowRectangle
{
    int left;
    int bottom;
    int width;
    int height;

    WindowRectangle(int left_, int bottom_, int width_, int height_)
        : left(left_)
        , bottom(bottom_)
        , width(width_)
        , height(height_)
    {
    }
};

struct FragmentOperationState
{
    // Variables corresponding to GL state variables.

    bool scissorTestEnabled;
    WindowRectangle scissorRectangle;

    bool stencilTestEnabled;
    StencilState stencilStates[2]; //!< Indexed with FACETYPE_FRONT and FACETYPE_BACK.

    bool depthTestEnabled;
    TestFunc depthFunc;
    bool depthMask;

    bool depthBoundsTestEnabled;
    float minDepthBound;
    float maxDepthBound;

    BlendMode blendMode;

    // Standard blending state
    BlendState blendRGBState;
    BlendState blendAState;
    tcu::Vec4 blendColor; //!< Components should be in range [0, 1].

    BlendEquationAdvanced blendEquationAdvaced;

    bool sRGBEnabled;

    bool depthClampEnabled;

    bool polygonOffsetEnabled;
    float polygonOffsetFactor;
    float polygonOffsetUnits;

    tcu::BVec4 colorMask;

    // Variables not corresponding to configurable GL state, but other GL variables.

    int numStencilBits;

    FragmentOperationState(void)
        : scissorTestEnabled(false)
        , scissorRectangle(0, 0, 1, 1)

        , stencilTestEnabled(false)
        // \note stencilStates[] members get default-constructed.

        , depthTestEnabled(false)
        , depthFunc(TESTFUNC_LESS)
        , depthMask(true)

        , depthBoundsTestEnabled(false)
        , minDepthBound(0.0f)
        , maxDepthBound(1.0f)

        , blendMode(BLENDMODE_NONE)
        , blendRGBState()
        , blendAState()
        , blendColor(0.0f)
        , blendEquationAdvaced(BLENDEQUATION_ADVANCED_LAST)

        , sRGBEnabled(true)

        , depthClampEnabled(false)

        , polygonOffsetEnabled(false)
        , polygonOffsetFactor(0.0f)
        , polygonOffsetUnits(0.0f)

        , colorMask(true)

        , numStencilBits(8)
    {
    }
};

struct PointState
{
    float pointSize;

    PointState(void) : pointSize(1.0f)
    {
    }
};

struct LineState
{
    float lineWidth;

    LineState(void) : lineWidth(1.0f)
    {
    }
};

struct ViewportState
{
    WindowRectangle rect;
    float zn;
    float zf;

    explicit ViewportState(const WindowRectangle &rect_) : rect(rect_), zn(0.0f), zf(1.0f)
    {
    }

    explicit ViewportState(const rr::MultisampleConstPixelBufferAccess &multisampleBuffer)
        : rect(0, 0, multisampleBuffer.raw().getHeight(), multisampleBuffer.raw().getDepth())
        , zn(0.0f)
        , zf(1.0f)
    {
    }
};

struct RestartState
{
    bool enabled;
    uint32_t restartIndex;

    RestartState(void) : enabled(false), restartIndex(0xFFFFFFFFul)
    {
    }
};

//! Rasterizer configuration
struct RenderState
{
    explicit RenderState(const ViewportState &viewport_, const int subpixelBits_,
                         ViewportOrientation viewportOrientation_ = VIEWPORTORIENTATION_LOWER_LEFT,
                         CullMode cullMode_                       = CULLMODE_NONE)
        : cullMode(cullMode_)
        , provokingVertexConvention(PROVOKINGVERTEX_LAST)
        , viewport(viewport_)
        , viewportOrientation(viewportOrientation_)
        , subpixelBits(subpixelBits_)
    {
        rasterization.viewportOrientation = viewportOrientation;
    }

    enum
    {
        DEFAULT_SUBPIXEL_BITS = 8
    };

    CullMode cullMode;
    ProvokingVertex provokingVertexConvention;
    RasterizationState rasterization;
    FragmentOperationState fragOps;
    PointState point;
    ViewportState viewport;
    LineState line;
    RestartState restart;
    ViewportOrientation viewportOrientation;
    const int subpixelBits;
};

} // namespace rr

#endif // _RRRENDERSTATE_HPP

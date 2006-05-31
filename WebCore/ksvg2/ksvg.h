/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_H
#define KSVG_H
#if SVG_SUPPORT

/**
 * @short General namespace specific definitions.
 */
namespace WebCore
{
    /**
     * All SVG constants
     */
    enum SVGExceptionCode
    {
        SVG_WRONG_TYPE_ERR            = 1,
        SVG_INVALID_VALUE_ERR        = 2,
        SVG_MATRIX_NOT_INVERTABLE    = 3
    };
    
    enum SVGUnitTypes
    {
        SVG_UNIT_TYPE_UNKNOWN                = 0,
        SVG_UNIT_TYPE_USERSPACEONUSE        = 1,
        SVG_UNIT_TYPE_OBJECTBOUNDINGBOX        = 2
    };

    enum SVGRenderingIntentType
    {
        RENDERING_INTENT_UNKNOWN                = 0,
        RENDERING_INTENT_AUTO                    = 1,
        RENDERING_INTENT_PERCEPTUAL                = 2,
        RENDERING_INTENT_RELATIVE_COLORIMETRIC    = 3,
        RENDERING_INTENT_SATURATION                = 4,
        RENDERING_INTENT_ABSOLUTE_COLORIMETRIC    = 5
    };
    
    enum SVGTransformType {
        SVG_TRANSFORM_UNKNOWN           = 0,
        SVG_TRANSFORM_MATRIX            = 1,
        SVG_TRANSFORM_TRANSLATE         = 2,
        SVG_TRANSFORM_SCALE             = 3,
        SVG_TRANSFORM_ROTATE            = 4,
        SVG_TRANSFORM_SKEWX             = 5,
        SVG_TRANSFORM_SKEWY             = 6
    };

    enum SVGCSSRuleType
    {
        COLOR_PROFILE_RULE = 7
    };

    enum SVGPreserveAspectRatioType
    {
        SVG_PRESERVEASPECTRATIO_UNKNOWN        = 0,
        SVG_PRESERVEASPECTRATIO_NONE        = 1,
        SVG_PRESERVEASPECTRATIO_XMINYMIN    = 2,
        SVG_PRESERVEASPECTRATIO_XMIDYMIN    = 3,
        SVG_PRESERVEASPECTRATIO_XMAXYMIN    = 4,
        SVG_PRESERVEASPECTRATIO_XMINYMID    = 5,
        SVG_PRESERVEASPECTRATIO_XMIDYMID    = 6,
        SVG_PRESERVEASPECTRATIO_XMAXYMID    = 7,
        SVG_PRESERVEASPECTRATIO_XMINYMAX    = 8,
        SVG_PRESERVEASPECTRATIO_XMIDYMAX    = 9,
        SVG_PRESERVEASPECTRATIO_XMAXYMAX    = 10
    };

    enum SVGMeetOrSliceType
    {
        SVG_MEETORSLICE_UNKNOWN    = 0,
        SVG_MEETORSLICE_MEET    = 1,
        SVG_MEETORSLICE_SLICE    = 2
    };

    enum SVGPathSegType
    {
        PATHSEG_UNKNOWN                            = 0,
        PATHSEG_CLOSEPATH                        = 1,
        PATHSEG_MOVETO_ABS                        = 2,
        PATHSEG_MOVETO_REL                        = 3,
        PATHSEG_LINETO_ABS                        = 4,
        PATHSEG_LINETO_REL                        = 5,
        PATHSEG_CURVETO_CUBIC_ABS                = 6,
        PATHSEG_CURVETO_CUBIC_REL                = 7,
        PATHSEG_CURVETO_QUADRATIC_ABS            = 8,
        PATHSEG_CURVETO_QUADRATIC_REL            = 9,
        PATHSEG_ARC_ABS                            = 10,
        PATHSEG_ARC_REL                            = 11,
        PATHSEG_LINETO_HORIZONTAL_ABS            = 12,
        PATHSEG_LINETO_HORIZONTAL_REL            = 13,
        PATHSEG_LINETO_VERTICAL_ABS                = 14,
        PATHSEG_LINETO_VERTICAL_REL                = 15,
        PATHSEG_CURVETO_CUBIC_SMOOTH_ABS        = 16,
        PATHSEG_CURVETO_CUBIC_SMOOTH_REL        = 17,
        PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS    = 18,
        PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL    = 19
    };

    enum SVGLengthAdjustType
    {
        LENGTHADJUST_UNKNOWN            = 0,
        LENGTHADJUST_SPACING            = 1,
        LENGTHADJUST_SPACINGANDGLYPHS    = 2
    };

    enum SVGTextPathMethodType
    {
        TEXTPATH_METHODTYPE_UNKNOWN    = 0,
        TEXTPATH_METHODTYPE_ALIGN    = 1,
        TEXTPATH_METHODTYPE_STRETCH    = 2
    };

    enum SVGTextPathSpacingType
    {
        TEXTPATH_SPACINGTYPE_UNKNOWN    = 0,
        TEXTPATH_SPACINGTYPE_AUTO        = 1,
        TEXTPATH_SPACINGTYPE_EXACT        = 2
    };

    enum SVGPaintType
    {
        SVG_PAINTTYPE_UNKNOWN               = 0,
        SVG_PAINTTYPE_RGBCOLOR              = 1,
        SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR     = 2,
        SVG_PAINTTYPE_NONE                  = 101,
        SVG_PAINTTYPE_CURRENTCOLOR          = 102,
        SVG_PAINTTYPE_URI_NONE              = 103,
        SVG_PAINTTYPE_URI_CURRENTCOLOR      = 104,
        SVG_PAINTTYPE_URI_RGBCOLOR          = 105,
        SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR = 106,
        SVG_PAINTTYPE_URI                   = 107
    };

    enum SVGMarkerUnitsType
    {
        SVG_MARKERUNITS_UNKNOWN            = 0,
        SVG_MARKERUNITS_USERSPACEONUSE    = 1,
        SVG_MARKERUNITS_STROKEWIDTH        = 2
    };

    enum SVGMarkerOrientType
    {
        SVG_MARKER_ORIENT_UNKNOWN    = 0,
        SVG_MARKER_ORIENT_AUTO        = 1,
        SVG_MARKER_ORIENT_ANGLE        = 2
    };

    enum SVGGradientType
    {
        SVG_SPREADMETHOD_UNKNOWN = 0,
        SVG_SPREADMETHOD_PAD     = 1,
        SVG_SPREADMETHOD_REFLECT = 2,
        SVG_SPREADMETHOD_REPEAT  = 3
    };

    enum SVGZoomAndPanType
    {
        SVG_ZOOMANDPAN_UNKNOWN = 0,
        SVG_ZOOMANDPAN_DISABLE = 1,
        SVG_ZOOMANDPAN_MAGNIFY = 2
    };

    enum SVGBlendModeType
    {
        SVG_FEBLEND_MODE_UNKNOWN  = 0,
        SVG_FEBLEND_MODE_NORMAL   = 1,
        SVG_FEBLEND_MODE_MULTIPLY = 2,
        SVG_FEBLEND_MODE_SCREEN   = 3,
        SVG_FEBLEND_MODE_DARKEN   = 4,
        SVG_FEBLEND_MODE_LIGHTEN  = 5
    };

    enum SVGColorMatrixType
    {
        SVG_FECOLORMATRIX_TYPE_UNKNOWN          = 0,
        SVG_FECOLORMATRIX_TYPE_MATRIX           = 1,
        SVG_FECOLORMATRIX_TYPE_SATURATE         = 2,
        SVG_FECOLORMATRIX_TYPE_HUEROTATE        = 3,
        SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA = 4
    };

    enum SVGComponentTransferType
    {
        SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN  = 0,
        SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY = 1,
        SVG_FECOMPONENTTRANSFER_TYPE_TABLE    = 2,
        SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE = 3,
        SVG_FECOMPONENTTRANSFER_TYPE_LINEAR   = 4,
        SVG_FECOMPONENTTRANSFER_TYPE_GAMMA    = 5
    };

    enum SVGCompositeOperators
    {
        SVG_FECOMPOSITE_OPERATOR_UNKNOWN    = 0,
        SVG_FECOMPOSITE_OPERATOR_OVER       = 1,
        SVG_FECOMPOSITE_OPERATOR_IN         = 2,
        SVG_FECOMPOSITE_OPERATOR_OUT        = 3,
        SVG_FECOMPOSITE_OPERATOR_ATOP       = 4,
        SVG_FECOMPOSITE_OPERATOR_XOR        = 5,
        SVG_FECOMPOSITE_OPERATOR_ARITHMETIC = 6
    };

    enum SVGEdgeModes
    {
        SVG_EDGEMODE_UNKNOWN   = 0,
        SVG_EDGEMODE_DUPLICATE = 1,
        SVG_EDGEMODE_WRAP      = 2,
        SVG_EDGEMODE_NONE      = 3
    };

    enum SVGChannelSelectors
    {
        SVG_CHANNEL_UNKNOWN = 0,
        SVG_CHANNEL_R       = 1,
        SVG_CHANNEL_G       = 2,
        SVG_CHANNEL_B       = 3,
        SVG_CHANNEL_A       = 4
    };

    enum SVGMorphologyOperators
    {
        SVG_MORPHOLOGY_OPERATOR_UNKNOWN = 0,
        SVG_MORPHOLOGY_OPERATOR_ERODE   = 1,
        SVG_MORPHOLOGY_OPERATOR_DILATE  = 2
    };

    enum SVGTurbulenceType
    {
        SVG_TURBULENCE_TYPE_UNKNOWN      = 0,
        SVG_TURBULENCE_TYPE_FRACTALNOISE = 1,
        SVG_TURBULENCE_TYPE_TURBULENCE   = 2
    };

    enum SVGStitchOptions
    {
        SVG_STITCHTYPE_UNKNOWN  = 0,
        SVG_STITCHTYPE_STITCH   = 1,
        SVG_STITCHTYPE_NOSTITCH = 2
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet

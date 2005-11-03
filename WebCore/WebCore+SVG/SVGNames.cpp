/*
 * This file is part of the SVG DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
 #define DOM_SVGNAMES_HIDE_GLOBALS 1

#include "config.h"
#include "SVGNames.h"

namespace DOM { namespace SVGNames {

// Define a properly-sized array of pointers to avoid static initialization.
// Use an array of pointers instead of an array of char in case there is some alignment issue.

#define DEFINE_UNINITIALIZED_GLOBAL(type, name) void *name[(sizeof(type) + sizeof(void *) - 1) / sizeof(void *)];

DEFINE_UNINITIALIZED_GLOBAL(AtomicString, svgNamespaceURI)

#define DEFINE_TAG_GLOBAL(name) DEFINE_UNINITIALIZED_GLOBAL(QualifiedName, name##Tag)
DOM_SVGNAMES_FOR_EACH_TAG(DEFINE_TAG_GLOBAL)

#define DEFINE_ATTR_GLOBAL(name) DEFINE_UNINITIALIZED_GLOBAL(QualifiedName, name##Attr)
DOM_SVGNAMES_FOR_EACH_ATTR(DEFINE_ATTR_GLOBAL)

void init()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    
    // Use placement new to initialize the globals.

    AtomicString svgNS("http://www.w3.org/2000/svg");

    // Namespace
    new (&svgNamespaceURI) AtomicString(svgNS);
    // Tags    #define DEFINE_TAG_STRING(name) const char *name##TagString = #name;
    DOM_SVGNAMES_FOR_EACH_TAG(DEFINE_TAG_STRING)

    color_profileTagString = "color-profile";
    definition_srcTagString = "definition-src";
    font_faceTagString = "font-face";
    font_face_formatTagString = "font-face_format";
    font_face_nameTagString = "font-face_name";
    font_face_srcTagString = "font-face_src";
    font_face_uriTagString = "font-face_uri";
    missing_glyphTagString = "missing-glyph";

    #define INITIALIZE_TAG_GLOBAL(name) new (&name##Tag) QualifiedName(nullAtom, name##TagString, svgNS);
    DOM_SVGNAMES_FOR_EACH_TAG(INITIALIZE_TAG_GLOBAL)

    // Attributes    #define DEFINE_ATTR_STRING(name) const char *name##AttrString = #name;
    DOM_SVGNAMES_FOR_EACH_ATTR(DEFINE_ATTR_STRING)

    accent_heightAttrString = "accent-height";
    alignment_baselineAttrString = "alignment-baseline";
    arabic_formAttrString = "arabic-form";
    baseline_shiftAttrString = "baseline-shift";
    cap_heightAttrString = "cap-height";
    clip_pathAttrString = "clip-path";
    clip_ruleAttrString = "clip-rule";
    color_interpolationAttrString = "color-interpolation";
    color_interpolation_filtersAttrString = "color-interpolation_filters";
    color_profileAttrString = "color-profile";
    color_renderingAttrString = "color-rendering";
    dominant_baselineAttrString = "dominant-baseline";
    enable_backgroundAttrString = "enable-background";
    fill_opacityAttrString = "fill-opacity";
    fill_ruleAttrString = "fill-rule";
    flood_colorAttrString = "flood-color";
    flood_opacityAttrString = "flood-opacity";
    font_familyAttrString = "font-family";
    font_sizeAttrString = "font-size";
    font_size_adjustAttrString = "font-size_adjust";
    font_stretchAttrString = "font-stretch";
    font_styleAttrString = "font-style";
    font_variantAttrString = "font-variant";
    font_weightAttrString = "font-weight";
    glyph_nameAttrString = "glyph-name";
    glyph_orientation_horizontalAttrString = "glyph-orientation_horizontal";
    glyph_orientation_verticalAttrString = "glyph-orientation_vertical";
    horiz_adv_xAttrString = "horiz-adv_x";
    horiz_origin_xAttrString = "horiz-origin_x";
    horiz_origin_yAttrString = "horiz-origin_y";
    image_renderingAttrString = "image-rendering";
    letter_spacingAttrString = "letter-spacing";
    lighting_colorAttrString = "lighting-color";
    marker_endAttrString = "marker-end";
    marker_midAttrString = "marker-mid";
    marker_startAttrString = "marker-start";
    overline_positionAttrString = "overline-position";
    overline_thicknessAttrString = "overline-thickness";
    panose_1AttrString = "panose-1";
    pointer_eventsAttrString = "pointer-events";
    rendering_intentAttrString = "rendering-intent";
    shape_renderingAttrString = "shape-rendering";
    stop_colorAttrString = "stop-color";
    stop_opacityAttrString = "stop-opacity";
    strikethrough_positionAttrString = "strikethrough-position";
    strikethrough_thicknessAttrString = "strikethrough-thickness";
    stroke_dasharrayAttrString = "stroke-dasharray";
    stroke_dashoffsetAttrString = "stroke-dashoffset";
    stroke_linecapAttrString = "stroke-linecap";
    stroke_linejoinAttrString = "stroke-linejoin";
    stroke_miterlimitAttrString = "stroke-miterlimit";
    stroke_opacityAttrString = "stroke-opacity";
    stroke_widthAttrString = "stroke-width";
    text_anchorAttrString = "text-anchor";
    text_decorationAttrString = "text-decoration";
    text_renderingAttrString = "text-rendering";
    underline_positionAttrString = "underline-position";
    underline_thicknessAttrString = "underline-thickness";
    unicode_bidiAttrString = "unicode-bidi";
    unicode_rangeAttrString = "unicode-range";
    units_per_emAttrString = "units-per_em";
    v_alphabeticAttrString = "v-alphabetic";
    v_hangingAttrString = "v-hanging";
    v_ideographicAttrString = "v-ideographic";
    v_mathematicalAttrString = "v-mathematical";
    vert_adv_yAttrString = "vert-adv_y";
    vert_origin_xAttrString = "vert-origin_x";
    vert_origin_yAttrString = "vert-origin_y";
    word_spacingAttrString = "word-spacing";
    writing_modeAttrString = "writing-mode";
    x_heightAttrString = "x-height";

    #define INITIALIZE_ATTR_GLOBAL(name) new (&name##Attr) QualifiedName(nullAtom, name##AttrString, nullAtom);
    DOM_SVGNAMES_FOR_EACH_ATTR(INITIALIZE_ATTR_GLOBAL)

}

} }
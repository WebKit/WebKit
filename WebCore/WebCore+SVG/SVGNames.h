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
 #ifndef DOM_SVGNAMES_H
#define DOM_SVGNAMES_H

#include "dom_qname.h"
namespace DOM { namespace SVGNames {
#define DOM_SVGNAMES_FOR_EACH_TAG(macro) \
    macro(a) \
    macro(altglyph) \
    macro(altglyphdef) \
    macro(altglyphitem) \
    macro(animate) \
    macro(animatecolor) \
    macro(animatemotion) \
    macro(animatetransform) \
    macro(circle) \
    macro(clippath) \
    macro(color_profile) \
    macro(cursor) \
    macro(definition_src) \
    macro(defs) \
    macro(desc) \
    macro(ellipse) \
    macro(feblend) \
    macro(fecolormatrix) \
    macro(fecomponenttransfer) \
    macro(fecomposite) \
    macro(feconvolvematrix) \
    macro(fediffuselighting) \
    macro(fedisplacementmap) \
    macro(fedistantlight) \
    macro(feflood) \
    macro(fefunca) \
    macro(fefuncb) \
    macro(fefuncg) \
    macro(fefuncr) \
    macro(fegaussianblur) \
    macro(feimage) \
    macro(femerge) \
    macro(femergenode) \
    macro(femorphology) \
    macro(feoffset) \
    macro(fepointlight) \
    macro(fespecularlighting) \
    macro(fespotlight) \
    macro(fetile) \
    macro(feturbulence) \
    macro(filter) \
    macro(font) \
    macro(font_face) \
    macro(font_face_format) \
    macro(font_face_name) \
    macro(font_face_src) \
    macro(font_face_uri) \
    macro(foreignobject) \
    macro(g) \
    macro(glyph) \
    macro(glyphref) \
    macro(hkern) \
    macro(image) \
    macro(line) \
    macro(lineargradient) \
    macro(marker) \
    macro(mask) \
    macro(metadata) \
    macro(missing_glyph) \
    macro(mpath) \
    macro(path) \
    macro(pattern) \
    macro(polygon) \
    macro(polyline) \
    macro(radialgradient) \
    macro(rect) \
    macro(script) \
    macro(set) \
    macro(stop) \
    macro(style) \
    macro(svg) \
    macro(switch) \
    macro(symbol) \
    macro(text) \
    macro(textpath) \
    macro(title) \
    macro(tref) \
    macro(tspan) \
    macro(use) \
    macro(view) \
    macro(vkern) \
// end of macro

#define DOM_SVGNAMES_FOR_EACH_ATTR(macro) \
    macro(accent_height) \
    macro(accumulate) \
    macro(additive) \
    macro(alignment_baseline) \
    macro(alphabetic) \
    macro(amplitude) \
    macro(animate) \
    macro(arabic_form) \
    macro(ascent) \
    macro(attributename) \
    macro(attributetype) \
    macro(azimuth) \
    macro(basefrequency) \
    macro(baseline_shift) \
    macro(baseprofile) \
    macro(bbox) \
    macro(begin) \
    macro(bias) \
    macro(by) \
    macro(calcmode) \
    macro(cap_height) \
    macro(clip) \
    macro(clip_path) \
    macro(clip_rule) \
    macro(clippathunits) \
    macro(color) \
    macro(color_interpolation) \
    macro(color_interpolation_filters) \
    macro(color_profile) \
    macro(color_rendering) \
    macro(contentscripttype) \
    macro(contentstyletype) \
    macro(cursor) \
    macro(cx) \
    macro(cy) \
    macro(d) \
    macro(descent) \
    macro(diffuseconstant) \
    macro(direction) \
    macro(display) \
    macro(divisor) \
    macro(dominant_baseline) \
    macro(dur) \
    macro(dx) \
    macro(dy) \
    macro(edgemode) \
    macro(elevation) \
    macro(enable_background) \
    macro(end) \
    macro(exponent) \
    macro(externalresourcesrequired) \
    macro(fecolormatrix) \
    macro(fecomposite) \
    macro(fegaussianblur) \
    macro(femorphology) \
    macro(fetile) \
    macro(fill) \
    macro(fill_opacity) \
    macro(fill_rule) \
    macro(filter) \
    macro(filterres) \
    macro(filterunits) \
    macro(flood_color) \
    macro(flood_opacity) \
    macro(font_family) \
    macro(font_size) \
    macro(font_size_adjust) \
    macro(font_stretch) \
    macro(font_style) \
    macro(font_variant) \
    macro(font_weight) \
    macro(format) \
    macro(from) \
    macro(fx) \
    macro(fy) \
    macro(g1) \
    macro(g2) \
    macro(glyph_name) \
    macro(glyph_orientation_horizontal) \
    macro(glyph_orientation_vertical) \
    macro(glyphref) \
    macro(gradienttransform) \
    macro(gradientunits) \
    macro(hanging) \
    macro(height) \
    macro(horiz_adv_x) \
    macro(horiz_origin_x) \
    macro(horiz_origin_y) \
    macro(href) \
    macro(ideographic) \
    macro(image_rendering) \
    macro(in) \
    macro(in2) \
    macro(intercept) \
    macro(k) \
    macro(k1) \
    macro(k2) \
    macro(k3) \
    macro(k4) \
    macro(kernelmatrix) \
    macro(kernelunitlength) \
    macro(kerning) \
    macro(keypoints) \
    macro(keysplines) \
    macro(keytimes) \
    macro(lang) \
    macro(lengthadjust) \
    macro(letter_spacing) \
    macro(lighting_color) \
    macro(limitingconeangle) \
    macro(local) \
    macro(marker_end) \
    macro(marker_mid) \
    macro(marker_start) \
    macro(markerheight) \
    macro(markerunits) \
    macro(markerwidth) \
    macro(mask) \
    macro(maskcontentunits) \
    macro(maskunits) \
    macro(mathematical) \
    macro(max) \
    macro(media) \
    macro(method) \
    macro(min) \
    macro(mode) \
    macro(name) \
    macro(numoctaves) \
    macro(offset) \
    macro(onbegin) \
    macro(onend) \
    macro(onrepeat) \
    macro(onzoom) \
    macro(opacity) \
    macro(operator) \
    macro(order) \
    macro(orient) \
    macro(orientation) \
    macro(origin) \
    macro(overflow) \
    macro(overline_position) \
    macro(overline_thickness) \
    macro(panose_1) \
    macro(path) \
    macro(pathlength) \
    macro(patterncontentunits) \
    macro(patterntransform) \
    macro(patternunits) \
    macro(pointer_events) \
    macro(points) \
    macro(pointsatx) \
    macro(pointsaty) \
    macro(pointsatz) \
    macro(preservealpha) \
    macro(preserveaspectratio) \
    macro(primitiveunits) \
    macro(r) \
    macro(radius) \
    macro(refx) \
    macro(refy) \
    macro(rendering_intent) \
    macro(repeatcount) \
    macro(repeatdur) \
    macro(requiredextensions) \
    macro(requiredfeatures) \
    macro(restart) \
    macro(result) \
    macro(rotate) \
    macro(rx) \
    macro(ry) \
    macro(scale) \
    macro(seed) \
    macro(shape_rendering) \
    macro(slope) \
    macro(space) \
    macro(spacing) \
    macro(specularconstant) \
    macro(specularexponent) \
    macro(spreadmethod) \
    macro(startoffset) \
    macro(stddeviation) \
    macro(stemh) \
    macro(stemv) \
    macro(stitchtiles) \
    macro(stop_color) \
    macro(stop_opacity) \
    macro(strikethrough_position) \
    macro(strikethrough_thickness) \
    macro(stroke) \
    macro(stroke_dasharray) \
    macro(stroke_dashoffset) \
    macro(stroke_linecap) \
    macro(stroke_linejoin) \
    macro(stroke_miterlimit) \
    macro(stroke_opacity) \
    macro(stroke_width) \
    macro(style) \
    macro(surfacescale) \
    macro(systemlanguage) \
    macro(tablevalues) \
    macro(target) \
    macro(targetx) \
    macro(targety) \
    macro(text_anchor) \
    macro(text_decoration) \
    macro(text_rendering) \
    macro(textlength) \
    macro(title) \
    macro(to) \
    macro(transform) \
    macro(type) \
    macro(u1) \
    macro(u2) \
    macro(underline_position) \
    macro(underline_thickness) \
    macro(unicode) \
    macro(unicode_bidi) \
    macro(unicode_range) \
    macro(units_per_em) \
    macro(v_alphabetic) \
    macro(v_hanging) \
    macro(v_ideographic) \
    macro(v_mathematical) \
    macro(values) \
    macro(version) \
    macro(vert_adv_y) \
    macro(vert_origin_x) \
    macro(vert_origin_y) \
    macro(viewbox) \
    macro(viewtarget) \
    macro(visibility) \
    macro(width) \
    macro(widths) \
    macro(word_spacing) \
    macro(writing_mode) \
    macro(x) \
    macro(x_height) \
    macro(x1) \
    macro(x2) \
    macro(xchannelselector) \
    macro(y) \
    macro(y1) \
    macro(y2) \
    macro(ychannelselector) \
    macro(z) \
    macro(zoomandpan) \
// end of macro

#if !DOM_SVGNAMES_HIDE_GLOBALS
    // Namespace
    extern const AtomicString svgNamespaceURI;

    // Tags
    #define DOM_NAMES_DEFINE_TAG_GLOBAL(name) extern const QualifiedName name##Tag;
    DOM_NAMES_FOR_EACH_TAG(DOM_NAMES_DEFINE_TAG_GLOBAL)
    #undef DOM_NAMES_DEFINE_TAG_GLOBAL

    // Attributes
    #define DOM_NAMES_DEFINE_ATTR_GLOBAL(name) extern const QualifiedName name##Attr;
    DOM_NAMES_FOR_EACH_ATTR(DOM_NAMES_DEFINE_ATTR_GLOBAL)
    #undef DOM_NAMES_DEFINE_ATTR_GLOBAL
#endif

    void init();
} }

#endif


/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.CSSKeywordCompletions = {};

WebInspector.CSSKeywordCompletions.forProperty = function(propertyName)
{
    let acceptedKeywords = ["initial", "unset", "revert", "var()"];
    let isNotPrefixed = propertyName.charAt(0) !== "-";

    if (propertyName in WebInspector.CSSKeywordCompletions._propertyKeywordMap)
        acceptedKeywords = acceptedKeywords.concat(WebInspector.CSSKeywordCompletions._propertyKeywordMap[propertyName]);
    else if (isNotPrefixed && ("-webkit-" + propertyName) in WebInspector.CSSKeywordCompletions._propertyKeywordMap)
        acceptedKeywords = acceptedKeywords.concat(WebInspector.CSSKeywordCompletions._propertyKeywordMap["-webkit-" + propertyName]);

    if (propertyName in WebInspector.CSSKeywordCompletions._colorAwareProperties)
        acceptedKeywords = acceptedKeywords.concat(WebInspector.CSSKeywordCompletions._colors);
    else if (isNotPrefixed && ("-webkit-" + propertyName) in WebInspector.CSSKeywordCompletions._colorAwareProperties)
        acceptedKeywords = acceptedKeywords.concat(WebInspector.CSSKeywordCompletions._colors);
    else if (propertyName.endsWith("color"))
        acceptedKeywords = acceptedKeywords.concat(WebInspector.CSSKeywordCompletions._colors);

    // Only suggest "inherit" on inheritable properties even though it is valid on all properties.
    if (propertyName in WebInspector.CSSKeywordCompletions.InheritedProperties)
        acceptedKeywords.push("inherit");
    else if (isNotPrefixed && ("-webkit-" + propertyName) in WebInspector.CSSKeywordCompletions.InheritedProperties)
        acceptedKeywords.push("inherit");

    if (acceptedKeywords.includes(WebInspector.CSSKeywordCompletions.AllPropertyNamesPlaceholder) && WebInspector.CSSCompletions.cssNameCompletions) {
        acceptedKeywords.remove(WebInspector.CSSKeywordCompletions.AllPropertyNamesPlaceholder);
        acceptedKeywords = acceptedKeywords.concat(WebInspector.CSSCompletions.cssNameCompletions.values);
    }

    return new WebInspector.CSSCompletions(acceptedKeywords, true);
};

WebInspector.CSSKeywordCompletions.addCustomCompletions = function(properties)
{
    for (var property of properties) {
        if (property.values)
            WebInspector.CSSKeywordCompletions.addPropertyCompletionValues(property.name, property.values);
    }
};

WebInspector.CSSKeywordCompletions.addPropertyCompletionValues = function(propertyName, newValues)
{
    var existingValues = WebInspector.CSSKeywordCompletions._propertyKeywordMap[propertyName];
    if (!existingValues) {
        WebInspector.CSSKeywordCompletions._propertyKeywordMap[propertyName] = newValues;
        return;
    }

    var union = new Set;
    for (var value of existingValues)
        union.add(value);
    for (var value of newValues)
        union.add(value);

    WebInspector.CSSKeywordCompletions._propertyKeywordMap[propertyName] = [...union.values()];
};

WebInspector.CSSKeywordCompletions.AllPropertyNamesPlaceholder = "__all-properties__";

WebInspector.CSSKeywordCompletions.InheritedProperties = [
    "azimuth", "border-collapse", "border-spacing", "caption-side", "clip-rule", "color", "color-interpolation",
    "color-interpolation-filters", "color-rendering", "cursor", "direction", "elevation", "empty-cells", "fill",
    "fill-opacity", "fill-rule", "font", "font-family", "font-size", "font-style", "font-variant", "font-variant-numeric", "font-weight",
    "glyph-orientation-horizontal", "glyph-orientation-vertical", "hanging-punctuation", "image-rendering", "kerning", "letter-spacing",
    "line-height", "list-style", "list-style-image", "list-style-position", "list-style-type", "marker", "marker-end",
    "marker-mid", "marker-start", "orphans", "pitch", "pitch-range", "pointer-events", "quotes", "resize", "richness",
    "shape-rendering", "speak", "speak-header", "speak-numeral", "speak-punctuation", "speech-rate", "stress", "stroke",
    "stroke-dasharray", "stroke-dashoffset", "stroke-linecap", "stroke-linejoin", "stroke-miterlimit", "stroke-opacity",
    "stroke-width", "tab-size", "text-align", "text-anchor", "text-decoration", "text-indent", "text-rendering",
    "text-shadow", "text-transform", "visibility", "voice-family", "volume", "white-space", "widows", "word-break",
    "word-spacing", "word-wrap", "writing-mode", "-webkit-aspect-ratio", "-webkit-border-horizontal-spacing",
    "-webkit-border-vertical-spacing", "-webkit-box-direction", "-webkit-color-correction", "font-feature-settings",
    "-webkit-font-kerning", "-webkit-font-smoothing", "-webkit-font-variant-ligatures",
    "-webkit-hyphenate-character", "-webkit-hyphenate-limit-after", "-webkit-hyphenate-limit-before",
    "-webkit-hyphenate-limit-lines", "-webkit-hyphens", "-webkit-line-align", "-webkit-line-box-contain",
    "-webkit-line-break", "-webkit-line-grid", "-webkit-line-snap", "-webkit-locale", "-webkit-nbsp-mode",
    "-webkit-print-color-adjust", "-webkit-rtl-ordering", "-webkit-text-combine", "-webkit-text-decorations-in-effect",
    "-webkit-text-emphasis", "-webkit-text-emphasis-color", "-webkit-text-emphasis-position", "-webkit-text-emphasis-style",
    "-webkit-text-fill-color", "-webkit-text-orientation", "-webkit-text-security", "-webkit-text-size-adjust",
    "-webkit-text-stroke", "-webkit-text-stroke-color", "-webkit-text-stroke-width", "-webkit-user-modify",
    "-webkit-user-select", "-webkit-writing-mode", "-webkit-cursor-visibility", "image-orientation", "image-resolution",
    "overflow-wrap", "-webkit-text-align-last", "-webkit-text-justify", "-webkit-ruby-position", "-webkit-text-decoration-line",
    "font-synthesis",

    // iOS Properties
    "-webkit-overflow-scrolling", "-webkit-touch-callout", "-webkit-tap-highlight-color"
].keySet();

WebInspector.CSSKeywordCompletions._colors = [
    "aqua", "black", "blue", "fuchsia", "gray", "green", "lime", "maroon", "navy", "olive", "orange", "purple", "red",
    "silver", "teal", "white", "yellow", "transparent", "currentcolor", "grey", "aliceblue", "antiquewhite",
    "aquamarine", "azure", "beige", "bisque", "blanchedalmond", "blueviolet", "brown", "burlywood", "cadetblue",
    "chartreuse", "chocolate", "coral", "cornflowerblue", "cornsilk", "crimson", "cyan", "darkblue", "darkcyan",
    "darkgoldenrod", "darkgray", "darkgreen", "darkgrey", "darkkhaki", "darkmagenta", "darkolivegreen", "darkorange",
    "darkorchid", "darkred", "darksalmon", "darkseagreen", "darkslateblue", "darkslategray", "darkslategrey",
    "darkturquoise", "darkviolet", "deeppink", "deepskyblue", "dimgray", "dimgrey", "dodgerblue", "firebrick",
    "floralwhite", "forestgreen", "gainsboro", "ghostwhite", "gold", "goldenrod", "greenyellow", "honeydew", "hotpink",
    "indianred", "indigo", "ivory", "khaki", "lavender", "lavenderblush", "lawngreen", "lemonchiffon", "lightblue",
    "lightcoral", "lightcyan", "lightgoldenrodyellow", "lightgray", "lightgreen", "lightgrey", "lightpink",
    "lightsalmon", "lightseagreen", "lightskyblue", "lightslategray", "lightslategrey", "lightsteelblue", "lightyellow",
    "limegreen", "linen", "magenta", "mediumaquamarine", "mediumblue", "mediumorchid", "mediumpurple", "mediumseagreen",
    "mediumslateblue", "mediumspringgreen", "mediumturquoise", "mediumvioletred", "midnightblue", "mintcream",
    "mistyrose", "moccasin", "navajowhite", "oldlace", "olivedrab", "orangered", "orchid", "palegoldenrod", "palegreen",
    "paleturquoise", "palevioletred", "papayawhip", "peachpuff", "peru", "pink", "plum", "powderblue", "rebeccapurple", "rosybrown",
    "royalblue", "saddlebrown", "salmon", "sandybrown", "seagreen", "seashell", "sienna", "skyblue", "slateblue",
    "slategray", "slategrey", "snow", "springgreen", "steelblue", "tan", "thistle", "tomato", "turquoise", "violet",
    "wheat", "whitesmoke", "yellowgreen", "rgb()", "rgba()", "hsl()", "hsla()"
];

WebInspector.CSSKeywordCompletions._colorAwareProperties = [
    "background", "background-color", "background-image", "border", "border-color", "border-top", "border-right", "border-bottom",
    "border-left", "border-top-color", "border-right-color", "border-bottom-color", "border-left-color", "box-shadow", "color",
    "fill", "outline", "outline-color", "stroke", "text-line-through", "text-line-through-color", "text-overline", "text-overline-color",
    "text-shadow", "text-underline", "text-underline-color", "-webkit-box-shadow", "-webkit-column-rule", "-webkit-column-rule-color",
    "-webkit-text-emphasis", "-webkit-text-emphasis-color", "-webkit-text-fill-color", "-webkit-text-stroke", "-webkit-text-stroke-color",
    "-webkit-text-decoration-color",

    // iOS Properties
    "-webkit-tap-highlight-color"
].keySet();

WebInspector.CSSKeywordCompletions._propertyKeywordMap = {
    "table-layout": [
        "auto", "fixed"
    ],
    "visibility": [
        "hidden", "visible", "collapse"
    ],
    "text-underline": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave"
    ],
    "content": [
        "list-item", "close-quote", "no-close-quote", "no-open-quote", "open-quote", "attr()", "counter()", "counters()", "url()", "linear-gradient()", "radial-gradient()", "repeating-linear-gradient()", "repeating-radial-gradient()", "-webkit-canvas()", "cross-fade()", "image-set()"
    ],
    "list-style-image": [
        "none", "url()", "linear-gradient()", "radial-gradient()", "repeating-linear-gradient()", "repeating-radial-gradient()", "-webkit-canvas()", "cross-fade()", "image-set()"
    ],
    "clear": [
        "none", "left", "right", "both"
    ],
    "stroke-linejoin": [
        "round", "miter", "bevel"
    ],
    "baseline-shift": [
        "baseline", "sub", "super"
    ],
    "border-bottom-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "margin-top-collapse": [
        "collapse", "separate", "discard"
    ],
    "-webkit-box-orient": [
        "horizontal", "vertical", "inline-axis", "block-axis"
    ],
    "font-stretch": [
        "normal", "wider", "narrower", "ultra-condensed", "extra-condensed", "condensed", "semi-condensed",
        "semi-expanded", "expanded", "extra-expanded", "ultra-expanded"
    ],
    "-webkit-color-correction": [
        "default", "srgb"
    ],
    "border-left-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "-webkit-writing-mode": [
        "lr", "rl", "tb", "lr-tb", "rl-tb", "tb-rl", "horizontal-tb", "vertical-rl", "vertical-lr", "horizontal-bt"
    ],
    "text-line-through-mode": [
        "continuous", "skip-white-space"
    ],
    "text-overline-mode": [
        "continuous", "skip-white-space"
    ],
    "text-underline-mode": [
        "continuous", "skip-white-space"
    ],
    "text-line-through-style": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave"
    ],
    "text-overline-style": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave"
    ],
    "text-underline-style": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave"
    ],
    "border-collapse": [
        "collapse", "separate"
    ],
    "border-top-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "outline-color": [
        "invert", "-webkit-focus-ring-color"
    ],
    "outline-style": [
        "none", "hidden", "inset", "groove", "ridge", "outset", "dotted", "dashed", "solid", "double", "auto"
    ],
    "cursor": [
        "none", "copy", "auto", "crosshair", "default", "pointer", "move", "vertical-text", "cell", "context-menu",
        "alias", "progress", "no-drop", "not-allowed", "zoom-in", "zoom-out", "e-resize", "ne-resize",
        "nw-resize", "n-resize", "se-resize", "sw-resize", "s-resize", "w-resize", "ew-resize", "ns-resize",
        "nesw-resize", "nwse-resize", "col-resize", "row-resize", "text", "wait", "help", "all-scroll", "-webkit-grab",
        "-webkit-zoom-in", "-webkit-zoom-out",
        "-webkit-grabbing", "url()", "image-set()"
    ],
    "border-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "size": [
        "a3", "a4", "a5", "b4", "b5", "landscape", "ledger", "legal", "letter", "portrait"
    ],
    "background": [
        "none", "url()", "linear-gradient()", "radial-gradient()", "repeating-linear-gradient()", "repeating-radial-gradient()", "-webkit-canvas()", "cross-fade()", "image-set()",
        "repeat", "repeat-x", "repeat-y", "no-repeat", "space", "round",
        "scroll", "fixed", "local",
        "auto", "contain", "cover",
        "top", "right", "left", "bottom", "center",
        "border-box", "padding-box", "content-box"
    ],
    "background-image": [
        "none", "url()", "linear-gradient()", "radial-gradient()", "repeating-linear-gradient()", "repeating-radial-gradient()", "-webkit-canvas()", "cross-fade()", "image-set()"
    ],
    "background-size": [
        "auto", "contain", "cover"
    ],
    "background-attachment": [
        "scroll", "fixed", "local"
    ],
    "background-repeat": [
        "repeat", "repeat-x", "repeat-y", "no-repeat", "space", "round"
    ],
    "background-blend-mode": [
        "normal", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity"
    ],
    "background-position": [
        "top", "right", "left", "bottom", "center"
    ],
    "background-origin": [
        "border-box", "padding-box", "content-box"
    ],
    "background-clip": [
        "border-box", "padding-box", "content-box"
    ],
    "direction": [
        "ltr", "rtl"
    ],
    "enable-background": [
        "accumulate", "new"
    ],
    "float": [
        "none", "left", "right"
    ],
    "hanging-punctuation": [
        "none", "first", "last", "allow-end", "force-end"
    ],
    "overflow-x": [
        "hidden", "auto", "visible", "overlay", "scroll", "marquee"
    ],
    "overflow-y": [
        "hidden", "auto", "visible", "overlay", "scroll", "marquee", "-webkit-paged-x", "-webkit-paged-y"
    ],
    "overflow": [
        "hidden", "auto", "visible", "overlay", "scroll", "marquee", "-webkit-paged-x", "-webkit-paged-y"
    ],
    "margin-bottom-collapse": [
        "collapse",  "separate", "discard"
    ],
    "-webkit-box-reflect": [
        "none", "left", "right", "above", "below"
    ],
    "text-rendering": [
        "auto", "optimizeSpeed", "optimizeLegibility", "geometricPrecision"
    ],
    "text-align": [
        "-webkit-auto", "left", "right", "center", "justify", "-webkit-left", "-webkit-right", "-webkit-center", "-webkit-match-parent", "start", "end"
    ],
    "list-style-position": [
        "outside", "inside"
    ],
    "margin-bottom": [
        "auto"
    ],
    "color-interpolation": [
        "linearrgb"
    ],
    "word-wrap": [
        "normal", "break-word"
    ],
    "font-weight": [
        "normal", "bold", "bolder", "lighter", "100", "200", "300", "400", "500", "600", "700", "800", "900"
    ],
    "font-synthesis": [
        "none", "weight", "style"
    ],
    "margin-before-collapse": [
        "collapse", "separate", "discard"
    ],
    "text-overline-width": [
        "normal", "medium", "auto", "thick", "thin", "calc()"
    ],
    "text-transform": [
        "none", "capitalize", "uppercase", "lowercase"
    ],
    "border-right-style": [
        "none", "hidden", "inset", "groove", "ridge", "outset", "dotted", "dashed", "solid", "double"
    ],
    "border-left-style": [
        "none", "hidden", "inset", "groove", "ridge", "outset", "dotted", "dashed", "solid", "double"
    ],
    "font-style": [
        "italic", "oblique", "normal"
    ],
    "speak": [
        "none", "normal", "spell-out", "digits", "literal-punctuation", "no-punctuation"
    ],
    "text-line-through": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave", "continuous", "skip-white-space"
    ],
    "color-rendering": [
        "auto", "optimizeSpeed", "optimizeQuality"
    ],
    "list-style-type": [
        "none", "disc", "circle", "square", "decimal", "decimal-leading-zero", "arabic-indic", "binary", "bengali",
        "cambodian", "khmer", "devanagari", "gujarati", "gurmukhi", "kannada", "lower-hexadecimal", "lao", "malayalam",
        "mongolian", "myanmar", "octal", "oriya", "persian", "urdu", "telugu", "tibetan", "thai", "upper-hexadecimal",
        "lower-roman", "upper-roman", "lower-greek", "lower-alpha", "lower-latin", "upper-alpha", "upper-latin", "afar",
        "ethiopic-halehame-aa-et", "ethiopic-halehame-aa-er", "amharic", "ethiopic-halehame-am-et", "amharic-abegede",
        "ethiopic-abegede-am-et", "cjk-earthly-branch", "cjk-heavenly-stem", "ethiopic", "ethiopic-halehame-gez",
        "ethiopic-abegede", "ethiopic-abegede-gez", "hangul-consonant", "hangul", "lower-norwegian", "oromo",
        "ethiopic-halehame-om-et", "sidama", "ethiopic-halehame-sid-et", "somali", "ethiopic-halehame-so-et", "tigre",
        "ethiopic-halehame-tig", "tigrinya-er", "ethiopic-halehame-ti-er", "tigrinya-er-abegede",
        "ethiopic-abegede-ti-er", "tigrinya-et", "ethiopic-halehame-ti-et", "tigrinya-et-abegede",
        "ethiopic-abegede-ti-et", "upper-greek", "upper-norwegian", "asterisks", "footnotes", "hebrew", "armenian",
        "lower-armenian", "upper-armenian", "georgian", "cjk-ideographic", "hiragana", "katakana", "hiragana-iroha",
        "katakana-iroha"
    ],
    "-webkit-text-combine": [
        "none", "horizontal"
    ],
    "outline": [
        "none", "hidden", "inset", "groove", "ridge", "outset", "dotted", "dashed", "solid", "double"
    ],
    "font": [
        "caption", "icon", "menu", "message-box", "small-caption", "-webkit-mini-control", "-webkit-small-control",
        "-webkit-control", "status-bar", "italic", "oblique", "small-caps", "normal", "bold", "bolder", "lighter",
        "100", "200", "300", "400", "500", "600", "700", "800", "900", "xx-small", "x-small", "small", "medium",
        "large", "x-large", "xx-large", "-webkit-xxx-large", "smaller", "larger", "serif", "sans-serif", "cursive",
        "fantasy", "monospace", "-webkit-body", "-webkit-pictograph", "-apple-system",
        "-apple-system-headline", "-apple-system-body", "-apple-system-subheadline", "-apple-system-footnote",
        "-apple-system-caption1", "-apple-system-caption2", "-apple-system-short-headline", "-apple-system-short-body",
        "-apple-system-short-subheadline", "-apple-system-short-footnote", "-apple-system-short-caption1",
        "-apple-system-tall-body", "-apple-system-title1", "-apple-system-title2", "-apple-system-title3"
    ],
    "dominant-baseline": [
        "middle", "auto", "central", "text-before-edge", "text-after-edge", "ideographic", "alphabetic", "hanging",
        "mathematical", "use-script", "no-change", "reset-size"
    ],
    "display": [
        "none", "inline", "block", "list-item", "compact", "inline-block", "table", "inline-table",
        "table-row-group", "table-header-group", "table-footer-group", "table-row", "table-column-group",
        "table-column", "table-cell", "table-caption", "-webkit-box", "-webkit-inline-box", "-wap-marquee",
        "flex", "inline-flex", "grid", "inline-grid"
    ],
    "image-rendering": [
        "auto", "optimizeSpeed", "optimizeQuality", "-webkit-crisp-edges", "-webkit-optimize-contrast", "crisp-edges", "pixelated"
    ],
    "alignment-baseline": [
        "baseline", "middle", "auto", "before-edge", "after-edge", "central", "text-before-edge", "text-after-edge",
        "ideographic", "alphabetic", "hanging", "mathematical"
    ],
    "outline-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "text-line-through-width": [
        "normal", "medium", "auto", "thick", "thin"
    ],
    "box-align": [
        "baseline", "center", "stretch", "start", "end"
    ],
    "box-shadow": [
        "none"
    ],
    "text-shadow": [
        "none"
    ],
    "-webkit-box-shadow": [
        "none"
    ],
    "border-right-width": [
        "medium", "thick", "thin"
    ],
    "border-top-style": [
        "none", "hidden", "inset", "groove", "ridge", "outset", "dotted", "dashed", "solid", "double"
    ],
    "line-height": [
        "normal"
    ],
    "counter-increment": [
        "none"
    ],
    "counter-reset": [
        "none"
    ],
    "text-overflow": [
        "clip", "ellipsis"
    ],
    "-webkit-box-direction": [
        "normal", "reverse"
    ],
    "margin-after-collapse": [
        "collapse", "separate", "discard"
    ],
    "break-after": [
         "left", "right", "recto", "verso", "auto", "avoid", "page", "column", "region", "avoid-page", "avoid-column", "avoid-region"
    ],
    "break-before": [
          "left", "right", "recto", "verso", "auto", "avoid", "page", "column", "region", "avoid-page", "avoid-column", "avoid-region"
    ],
    "break-inside": [
          "auto", "avoid", "avoid-page", "avoid-column", "avoid-region"
    ],
    "page-break-after": [
        "left", "right", "auto", "always", "avoid"
    ],
    "page-break-before": [
        "left", "right", "auto", "always", "avoid"
    ],
    "page-break-inside": [
        "auto", "avoid"
    ],
    "-webkit-column-break-after": [
        "left", "right", "auto", "always", "avoid"
    ],
    "-webkit-column-break-before": [
        "left", "right", "auto", "always", "avoid"
    ],
    "-webkit-column-break-inside": [
        "auto", "avoid"
    ],
    "-webkit-hyphens": [
        "none", "auto", "manual"
    ],
    "border-image": [
        "repeat", "stretch", "url()", "linear-gradient()", "radial-gradient()", "repeating-linear-gradient()", "repeating-radial-gradient()", "-webkit-canvas()", "cross-fade()", "image-set()"
    ],
    "border-image-repeat": [
        "repeat", "stretch", "space", "round"
    ],
    "-webkit-mask-box-image-repeat": [
        "repeat", "stretch", "space", "round"
    ],
    "position": [
        "absolute", "fixed", "relative", "static", "-webkit-sticky"
    ],
    "font-family": [
        "serif", "sans-serif", "cursive", "fantasy", "monospace", "-webkit-body", "-webkit-pictograph",
        "-apple-system", "-apple-system-headline", "-apple-system-body",
        "-apple-system-subheadline", "-apple-system-footnote", "-apple-system-caption1", "-apple-system-caption2",
        "-apple-system-short-headline", "-apple-system-short-body", "-apple-system-short-subheadline",
        "-apple-system-short-footnote", "-apple-system-short-caption1", "-apple-system-tall-body",
        "-apple-system-title1", "-apple-system-title2", "-apple-system-title3"
    ],
    "text-overflow-mode": [
        "clip", "ellipsis"
    ],
    "border-bottom-style": [
        "none", "hidden", "inset", "groove", "ridge", "outset", "dotted", "dashed", "solid", "double"
    ],
    "unicode-bidi": [
        "normal", "bidi-override", "embed", "-webkit-plaintext", "-webkit-isolate", "-webkit-isolate-override"
    ],
    "clip-rule": [
        "nonzero", "evenodd"
    ],
    "margin-left": [
        "auto"
    ],
    "margin-top": [
        "auto"
    ],
    "zoom": [
        "normal", "document", "reset"
    ],
    "z-index": [
        "auto"
    ],
    "width": [
        "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()"
    ],
    "height": [
        "intrinsic", "min-intrinsic", "calc()"
    ],
    "max-width": [
        "none", "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()"
    ],
    "min-width": [
        "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()"
    ],
    "max-height": [
        "none", "intrinsic", "min-intrinsic", "calc()"
    ],
    "min-height": [
        "intrinsic", "min-intrinsic", "calc()"
    ],
    "-webkit-logical-width": [
        "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()"
    ],
    "-webkit-logical-height": [
        "intrinsic", "min-intrinsic", "calc()"
    ],
    "-webkit-max-logical-width": [
        "none", "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()"
    ],
    "-webkit-min-logical-width": [
        "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()"
    ],
    "-webkit-max-logical-height": [
        "none", "intrinsic", "min-intrinsic", "calc()"
    ],
    "-webkit-min-logical-height": [
        "intrinsic", "min-intrinsic", "calc()"
    ],
    "empty-cells": [
        "hide", "show"
    ],
    "pointer-events": [
        "none", "all", "auto", "visible", "visiblepainted", "visiblefill", "visiblestroke", "painted", "fill", "stroke"
    ],
    "letter-spacing": [
        "normal", "calc()"
    ],
    "word-spacing": [
        "normal", "calc()"
    ],
    "-webkit-font-kerning": [
        "auto", "normal", "none"
    ],
    "-webkit-font-smoothing": [
        "none", "auto", "antialiased", "subpixel-antialiased"
    ],
    "border": [
        "none", "hidden", "inset", "groove", "ridge", "outset", "dotted", "dashed", "solid", "double"
    ],
    "font-size": [
        "xx-small", "x-small", "small", "medium", "large", "x-large", "xx-large", "-webkit-xxx-large", "smaller", "larger"
    ],
    "font-variant": [
        "small-caps", "normal"
    ],
    "font-variant-numeric": [
        "normal", "ordinal", "slashed-zero", "lining-nums", "oldstyle-nums", "proportional-nums", "tabular-nums",
        "diagonal-fractions", "stacked-fractions"
    ],
    "vertical-align": [
        "baseline", "middle", "sub", "super", "text-top", "text-bottom", "top", "bottom", "-webkit-baseline-middle"
    ],
    "white-space": [
        "normal", "nowrap", "pre", "pre-line", "pre-wrap"
    ],
    "word-break": [
        "normal", "break-all", "break-word"
    ],
    "text-underline-width": [
        "normal", "medium", "auto", "thick", "thin", "calc()"
    ],
    "text-indent": [
        "-webkit-each-line", "-webkit-hanging"
    ],
    "-webkit-box-lines": [
        "single", "multiple"
    ],
    "clip": [
        "auto", "rect()"
    ],
    "clip-path": [
        "none", "url()", "circle()", "ellipse()", "inset()", "polygon()", "margin-box", "border-box", "padding-box", "content-box"
    ],
    "-webkit-shape-outside": [
        "none", "url()", "circle()", "ellipse()", "inset()", "polygon()", "margin-box", "border-box", "padding-box", "content-box"
    ],
    "orphans": [
        "auto"
    ],
    "widows": [
        "auto"
    ],
    "margin": [
        "auto"
    ],
    "page": [
        "auto"
    ],
    "perspective": [
        "none"
    ],
    "perspective-origin": [
        "none", "left", "right", "bottom", "top", "center"
    ],
    "-webkit-marquee-increment": [
        "small", "large", "medium"
    ],
    "-webkit-marquee-direction": [
        "left", "right", "auto", "reverse", "forwards", "backwards", "ahead", "up", "down"
    ],
    "-webkit-marquee-style": [
        "none", "scroll", "slide", "alternate"
    ],
    "-webkit-marquee-repetition": [
        "infinite"
    ],
    "-webkit-marquee-speed": [
        "normal", "slow", "fast"
    ],
    "margin-right": [
        "auto"
    ],
    "marquee-speed": [
        "normal", "slow", "fast"
    ],
    "-webkit-text-emphasis": [
        "circle", "filled", "open", "dot", "double-circle", "triangle", "sesame"
    ],
    "-webkit-text-emphasis-style": [
        "circle", "filled", "open", "dot", "double-circle", "triangle", "sesame"
    ],
    "-webkit-text-emphasis-position": [
        "over", "under", "left", "right"
    ],
    "transform": [
        "none",
        "scale()", "scaleX()", "scaleY()", "scale3d()", "rotate()", "rotateX()", "rotateY()", "rotateZ()", "rotate3d()", "skew()", "skewX()", "skewY()",
        "translate()", "translateX()", "translateY()", "translateZ()", "translate3d()", "matrix()", "matrix3d()", "perspective()"
    ],
    "transform-style": [
        "flat", "preserve-3d"
    ],
    "-webkit-cursor-visibility": [
        "auto", "auto-hide"
    ],
    "text-decoration": [
        "none", "underline", "overline", "line-through", "blink"
    ],
    "-webkit-text-decorations-in-effect": [
        "none", "underline", "overline", "line-through", "blink"
    ],
    "-webkit-text-decoration-line": [
        "none", "underline", "overline", "line-through", "blink"
    ],
    "-webkit-text-decoration-style": [
        "solid", "double", "dotted", "dashed", "wavy"
    ],
    "-webkit-text-decoration-skip": [
        "auto", "none", "objects", "ink"
    ],
    "-webkit-text-underline-position": [
        "auto", "alphabetic", "under"
    ],
    "image-resolution": [
        "from-image", "snap"
    ],
    "-webkit-blend-mode": [
        "normal", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference", "exclusion", "plus-darker", "plus-lighter", "hue", "saturation", "color", "luminosity",
    ],
    "mix-blend-mode": [
        "normal", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference", "exclusion", "plus-darker", "plus-lighter", "hue", "saturation", "color", "luminosity",
    ],
    "mix": [
        "auto",
        "normal", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference", "exclusion", "plus-darker", "plus-lighter", "hue", "saturation", "color", "luminosity",
        "clear", "copy", "destination", "source-over", "destination-over", "source-in", "destination-in", "source-out", "destination-out", "source-atop", "destination-atop", "xor"
    ],
    "geometry": [
        "detached", "attached", "grid()"
    ],
    "overflow-wrap": [
        "normal", "break-word"
    ],
    "transition": [
        "none", "ease", "linear", "ease-in", "ease-out", "ease-in-out", "step-start", "step-end", "steps()", "cubic-bezier()", "spring()", "all", WebInspector.CSSKeywordCompletions.AllPropertyNamesPlaceholder
    ],
    "transition-timing-function": [
        "ease", "linear", "ease-in", "ease-out", "ease-in-out", "step-start", "step-end", "steps()", "cubic-bezier()", "spring()"
    ],
    "transition-property": [
        "all", "none", WebInspector.CSSKeywordCompletions.AllPropertyNamesPlaceholder
    ],
    "-webkit-column-progression": [
        "normal", "reverse"
    ],
    "-webkit-box-decoration-break": [
        "slice", "clone"
    ],
    "align-content": [
        "auto",
        "baseline", "last-baseline",
        "space-between", "space-around", "space-evenly", "stretch",
        "center", "start", "end", "flex-start", "flex-end", "left", "right",
        "true", "safe"
    ],
    "justify-content": [
        "auto",
        "baseline", "last-baseline", "space-between", "space-around", "space-evenly", "stretch",
        "center", "start", "end", "flex-start", "flex-end", "left", "right",
        "true", "safe"
    ],
    "align-items": [
        "auto", "stretch",
        "baseline", "last-baseline",
        "center", "start", "end", "self-start", "self-end", "flex-start", "flex-end", "left", "right",
        "true", "safe"
    ],
    "align-self": [
        "auto", "stretch",
        "baseline", "last-baseline",
        "center", "start", "end", "self-start", "self-end", "flex-start", "flex-end", "left", "right",
        "true", "safe"
    ],
    "justify-items": [
        "auto", "stretch",
        "baseline", "last-baseline",
        "center", "start", "end", "self-start", "self-end", "flex-start", "flex-end", "left", "right",
        "true", "safe"
    ],
    "justify-self": [
        "auto", "stretch",
        "baseline", "last-baseline",
        "center", "start", "end", "self-start", "self-end", "flex-start", "flex-end", "left", "right",
        "true", "safe"
    ],
    "flex-direction": [
        "row", "row-reverse", "column", "column-reverse"
    ],
    "flex-wrap": [
        "nowrap", "wrap", "wrap-reverse"
    ],
    "flex-flow": [
        "row", "row-reverse", "column", "column-reverse",
        "nowrap", "wrap", "wrap-reverse"
    ],
    "flex": [
        "none"
    ],
    "flex-basis": [
        "auto"
    ],
    "grid": [
        "none"
    ],
    "grid-area": [
        "auto"
    ],
    "grid-auto-columns": [
        "auto", "-webkit-max-content", "-webkit-min-content", "minmax()",
    ],
    "grid-auto-flow": [
        "row", "column", "dense"
    ],
    "grid-auto-rows": [
        "auto", "-webkit-max-content", "-webkit-min-content", "minmax()",
    ],
    "grid-column": [
        "auto"
    ],
    "grid-column-start": [
        "auto"
    ],
    "grid-column-end": [
        "auto"
    ],
    "grid-row": [
        "auto"
    ],
    "grid-row-start": [
        "auto"
    ],
    "grid-row-end": [
        "auto"
    ],
    "grid-template": [
        "none"
    ],
    "grid-template-areas": [
        "none"
    ],
    "grid-template-columns": [
        "none", "auto", "-webkit-max-content", "-webkit-min-content", "minmax()", "repeat()"
    ],
    "grid-template-rows": [
        "none", "auto", "-webkit-max-content", "-webkit-min-content", "minmax()", "repeat()"
    ],
    "-webkit-ruby-position": [
        "after", "before", "inter-character"
    ],
    "-webkit-text-align-last": [
        "auto", "start", "end", "left", "right", "center", "justify"
    ],
    "-webkit-text-justify": [
        "auto", "none", "inter-word", "inter-ideograph", "inter-cluster", "distribute", "kashida"
    ],
    "max-zoom": [
        "auto"
    ],
    "min-zoom": [
        "auto"
    ],
    "orientation": [
        "auto", "portait", "landscape"
    ],
    "user-zoom": [
        "zoom", "fixed"
    ],
    "-webkit-app-region": [
        "drag", "no-drag"
    ],
    "-webkit-line-break": [
        "auto", "loose", "normal", "strict", "after-white-space"
    ],
    "-webkit-background-composite": [
        "clear", "copy", "source-over", "source-in", "source-out", "source-atop", "destination-over", "destination-in", "destination-out", "destination-atop", "xor", "plus-darker", "plus-lighter"
    ],
    "-webkit-mask-composite": [
        "clear", "copy", "source-over", "source-in", "source-out", "source-atop", "destination-over", "destination-in", "destination-out", "destination-atop", "xor", "plus-darker", "plus-lighter"
    ],
    "-webkit-animation-direction": [
        "normal", "alternate", "reverse", "alternate-reverse"
    ],
    "-webkit-animation-fill-mode": [
        "none", "forwards", "backwards", "both"
    ],
    "-webkit-animation-iteration-count": [
        "infinite"
    ],
    "-webkit-animation-play-state": [
        "paused", "running"
    ],
    "-webkit-animation-timing-function": [
        "ease", "linear", "ease-in", "ease-out", "ease-in-out", "step-start", "step-end", "steps()", "cubic-bezier()", "spring()"
    ],
    "-webkit-column-span": [
        "all", "none", "calc()"
    ],
    "-webkit-region-break-after": [
        "auto", "always", "avoid", "left", "right"
    ],
    "-webkit-region-break-before": [
        "auto", "always", "avoid", "left", "right"
    ],
    "-webkit-region-break-inside": [
        "auto", "avoid"
    ],
    "-webkit-region-overflow": [
        "auto", "break"
    ],
    "-webkit-backface-visibility": [
        "visible", "hidden"
    ],
    "resize": [
        "none", "both", "horizontal", "vertical", "auto"
    ],
    "caption-side": [
        "top", "bottom", "left", "right"
    ],
    "box-sizing": [
        "border-box", "content-box"
    ],
    "-webkit-alt": [
        "attr()"
    ],
    "-webkit-border-fit": [
        "border", "lines"
    ],
    "-webkit-line-align": [
        "none", "edges"
    ],
    "-webkit-line-snap": [
        "none", "baseline", "contain"
    ],
    "-webkit-nbsp-mode": [
        "normal", "space"
    ],
    "-webkit-print-color-adjust": [
        "exact", "economy"
    ],
    "-webkit-rtl-ordering": [
        "logical", "visual"
    ],
    "-webkit-text-security": [
        "disc", "circle", "square", "none"
    ],
    "-webkit-user-drag": [
        "auto", "none", "element"
    ],
    "-webkit-user-modify": [
        "read-only", "read-write", "read-write-plaintext-only"
    ],
    "-webkit-user-select": [
        "auto", "none", "text", "all"
    ],
    "-webkit-text-stroke-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "-webkit-border-start-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "-webkit-border-end-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "-webkit-border-before-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "-webkit-border-after-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "-webkit-column-rule-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "-webkit-aspect-ratio": [
        "auto", "from-dimensions", "from-intrinsic", "/"
    ],
    "filter": [
        "none", "grayscale()", "sepia()", "saturate()", "hue-rotate()", "invert()", "opacity()", "brightness()", "contrast()", "blur()", "drop-shadow()", "custom()"
    ],
    "-webkit-backdrop-filter": [
        "none", "grayscale()", "sepia()", "saturate()", "hue-rotate()", "invert()", "opacity()", "brightness()", "contrast()", "blur()", "drop-shadow()", "custom()"
    ],
    "-webkit-column-count": [
        "auto", "calc()"
    ],
    "-webkit-column-gap": [
        "normal", "calc()"
    ],
    "-webkit-column-axis": [
        "horizontal", "vertical", "auto"
    ],
    "-webkit-column-width": [
        "auto", "calc()"
    ],
    "-webkit-column-fill": [
        "auto", "balance"
    ],
    "-webkit-hyphenate-character": [
        "none"
    ],
    "-webkit-hyphenate-limit-after": [
        "auto"
    ],
    "-webkit-hyphenate-limit-before": [
        "auto"
    ],
    "-webkit-hyphenate-limit-lines": [
        "no-limit"
    ],
    "-webkit-line-grid": [
        "none"
    ],
    "-webkit-locale": [
        "auto"
    ],
    "-webkit-text-orientation": [
        "sideways", "sideways-right", "vertical-right", "upright"
    ],
    "-webkit-line-box-contain": [
        "block", "inline", "font", "glyphs", "replaced", "inline-box", "none"
    ],
    "font-feature-settings": [
        "normal"
    ],
    "-webkit-font-variant-ligatures": [
        "normal", "common-ligatures", "no-common-ligatures", "discretionary-ligatures", "no-discretionary-ligatures", "historical-ligatures", "no-historical-ligatures"
    ],
    /*
    "-webkit-appearance": [
        "none", "checkbox", "radio", "push-button", "square-button", "button", "button-bevel", "default-button", "inner-spin-button", "listbox", "listitem", "media-enter-fullscreen-button", "media-exit-fullscreen-button", "media-fullscreen-volume-slider", "media-fullscreen-volume-slider-thumb", "media-mute-button", "media-play-button", "media-overlay-play-button", "media-seek-back-button", "media-seek-forward-button", "media-rewind-button", "media-return-to-realtime-button", "media-toggle-closed-captions-button", "media-slider", "media-sliderthumb", "media-volume-slider-container", "media-volume-slider", "media-volume-sliderthumb", "media-volume-slider-mute-button", "media-controls-background", "media-controls-fullscreen-background", "media-current-time-display", "media-time-remaining-display", "menulist", "menulist-button", "menulist-text", "menulist-textfield", "meter", "progress-bar", "progress-bar-value", "slider-horizontal", "slider-vertical", "sliderthumb-horizontal", "sliderthumb-vertical", "caret", "searchfield", "searchfield-decoration", "searchfield-results-decoration", "searchfield-results-button", "searchfield-cancel-button", "snapshotted-plugin-overlay", "textfield", "relevancy-level-indicator", "continuous-capacity-level-indicator", "discrete-capacity-level-indicator", "rating-level-indicator", "textarea", "attachment", "caps-lock-indicator"
    ],
    */
    "-webkit-animation-trigger": [
        "auto", "container-scroll()"
    ],
    "-webkit-scroll-snap-type": [
        "none", "mandatory", "proximity"
    ],
    "-webkit-scroll-snap-points-x": [
        "elements", "repeat()"
    ],
    "-webkit-scroll-snap-points-y": [
        "elements", "repeat()"
    ],
    "-webkit-scroll-snap-destination": [
        "none", "left", "right", "bottom", "top", "center"
    ],
    "-webkit-scroll-snap-coordinate": [
        "none", "left", "right", "bottom", "top", "center"
    ],

    // iOS Properties
    "-webkit-text-size-adjust": [
        "none", "auto"
    ],
    "-webkit-touch-callout": [
        "default", "none"
    ],
    "-webkit-overflow-scrolling": [
        "auto", "touch"
    ]
};

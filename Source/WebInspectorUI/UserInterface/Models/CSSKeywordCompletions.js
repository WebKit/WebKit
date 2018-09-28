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

WI.CSSKeywordCompletions = {};

WI.CSSKeywordCompletions.forProperty = function(propertyName)
{
    let acceptedKeywords = ["initial", "unset", "revert", "var()", "env()"];

    function addKeywordsForName(name) {
        let isNotPrefixed = name.charAt(0) !== "-";

        if (name in WI.CSSKeywordCompletions._propertyKeywordMap)
            acceptedKeywords = acceptedKeywords.concat(WI.CSSKeywordCompletions._propertyKeywordMap[name]);
        else if (isNotPrefixed && ("-webkit-" + name) in WI.CSSKeywordCompletions._propertyKeywordMap)
            acceptedKeywords = acceptedKeywords.concat(WI.CSSKeywordCompletions._propertyKeywordMap["-webkit-" + name]);

        if (name in WI.CSSKeywordCompletions._colorAwareProperties)
            acceptedKeywords = acceptedKeywords.concat(WI.CSSKeywordCompletions._colors);
        else if (isNotPrefixed && ("-webkit-" + name) in WI.CSSKeywordCompletions._colorAwareProperties)
            acceptedKeywords = acceptedKeywords.concat(WI.CSSKeywordCompletions._colors);
        else if (name.endsWith("color"))
            acceptedKeywords = acceptedKeywords.concat(WI.CSSKeywordCompletions._colors);

        // Only suggest "inherit" on inheritable properties even though it is valid on all properties.
        if (WI.CSSKeywordCompletions.InheritedProperties.has(name))
            acceptedKeywords.push("inherit");
        else if (isNotPrefixed && WI.CSSKeywordCompletions.InheritedProperties.has("-webkit-" + name))
            acceptedKeywords.push("inherit");
    }

    addKeywordsForName(propertyName);

    let unaliasedName = WI.CSSKeywordCompletions.PropertyNameForAlias.get(propertyName);
    if (unaliasedName)
        addKeywordsForName(unaliasedName);

    let longhandNames = WI.CSSKeywordCompletions.LonghandNamesForShorthandProperty.get(propertyName);
    if (longhandNames) {
        for (let longhandName of longhandNames)
            addKeywordsForName(longhandName);
    }

    if (acceptedKeywords.includes(WI.CSSKeywordCompletions.AllPropertyNamesPlaceholder) && WI.CSSCompletions.cssNameCompletions) {
        acceptedKeywords.remove(WI.CSSKeywordCompletions.AllPropertyNamesPlaceholder);
        acceptedKeywords = acceptedKeywords.concat(WI.CSSCompletions.cssNameCompletions.values);
    }

    return new WI.CSSCompletions(Array.from(new Set(acceptedKeywords)), true);
};

WI.CSSKeywordCompletions.forFunction = function(functionName)
{
    let suggestions = ["var()"];

    if (functionName === "var")
        suggestions = [];
    else if (functionName === "calc" || functionName === "min" || functionName === "max")
        suggestions = suggestions.concat(["calc()", "min()", "max()"]);
    else if (functionName === "env")
        suggestions = suggestions.concat(["safe-area-inset-top", "safe-area-inset-right", "safe-area-inset-bottom", "safe-area-inset-left"]);
    else if (functionName === "image-set")
        suggestions.push("url()");
    else if (functionName === "repeat")
        suggestions = suggestions.concat(["auto", "auto-fill", "auto-fit", "min-content", "max-content"]);
    else if (functionName.endsWith("gradient")) {
        suggestions = suggestions.concat(["to", "left", "right", "top", "bottom"]);
        suggestions = suggestions.concat(WI.CSSKeywordCompletions._colors);
    }

    return new WI.CSSCompletions(suggestions, true);
};

WI.CSSKeywordCompletions.addCustomCompletions = function(properties)
{
    for (var property of properties) {
        if (property.aliases) {
            for (let alias of property.aliases)
                WI.CSSKeywordCompletions.PropertyNameForAlias.set(alias, property.name);
        }

        if (property.values)
            WI.CSSKeywordCompletions.addPropertyCompletionValues(property.name, property.values);

        if (property.inherited)
            WI.CSSKeywordCompletions.InheritedProperties.add(property.name);

        if (property.longhands)
            WI.CSSKeywordCompletions.LonghandNamesForShorthandProperty.set(property.name, property.longhands);
    }
};

WI.CSSKeywordCompletions.addPropertyCompletionValues = function(propertyName, newValues)
{
    var existingValues = WI.CSSKeywordCompletions._propertyKeywordMap[propertyName];
    if (!existingValues) {
        WI.CSSKeywordCompletions._propertyKeywordMap[propertyName] = newValues;
        return;
    }

    var union = new Set;
    for (var value of existingValues)
        union.add(value);
    for (var value of newValues)
        union.add(value);

    WI.CSSKeywordCompletions._propertyKeywordMap[propertyName] = [...union.values()];
};

WI.CSSKeywordCompletions.AllPropertyNamesPlaceholder = "__all-properties__";

// Populated by CSS.getSupportedCSSProperties.
WI.CSSKeywordCompletions.PropertyNameForAlias = new Map;
WI.CSSKeywordCompletions.LonghandNamesForShorthandProperty = new Map;

WI.CSSKeywordCompletions.InheritedProperties = new Set([
    // Compatibility (iOS 12): `inherited` didn't exist on `CSSPropertyInfo`
    "-apple-color-filter",
    "-apple-trailing-word",
    "-webkit-animation-trigger",
    "-webkit-aspect-ratio",
    "-webkit-border-horizontal-spacing",
    "-webkit-border-vertical-spacing",
    "-webkit-box-direction",
    "-webkit-cursor-visibility",
    "-webkit-font-kerning",
    "-webkit-font-smoothing",
    "-webkit-hyphenate-character",
    "-webkit-hyphenate-limit-after",
    "-webkit-hyphenate-limit-before",
    "-webkit-hyphenate-limit-lines",
    "-webkit-hyphens",
    "-webkit-line-align",
    "-webkit-line-break",
    "-webkit-line-box-contain",
    "-webkit-line-grid",
    "-webkit-line-snap",
    "-webkit-locale",
    "-webkit-nbsp-mode",
    "-webkit-overflow-scrolling",
    "-webkit-print-color-adjust",
    "-webkit-rtl-ordering",
    "-webkit-ruby-position",
    "-webkit-text-align-last",
    "-webkit-text-combine",
    "-webkit-text-decoration-skip",
    "-webkit-text-decorations-in-effect",
    "-webkit-text-emphasis",
    "-webkit-text-emphasis-color",
    "-webkit-text-emphasis-position",
    "-webkit-text-emphasis-style",
    "-webkit-text-fill-color",
    "-webkit-text-justify",
    "-webkit-text-orientation",
    "-webkit-text-security",
    "-webkit-text-size-adjust",
    "-webkit-text-stroke",
    "-webkit-text-stroke-color",
    "-webkit-text-stroke-width",
    "-webkit-text-underline-position",
    "-webkit-text-zoom",
    "-webkit-touch-callout",
    "-webkit-user-modify",
    "-webkit-user-select",
    "border-collapse",
    "border-spacing",
    "caption-side",
    "caret-color",
    "clip-rule",
    "color",
    "color-interpolation",
    "color-interpolation-filters",
    "color-rendering",
    "cursor",
    "direction",
    "empty-cells",
    "fill",
    "fill-opacity",
    "fill-rule",
    "font",
    "font-family",
    "font-feature-settings",
    "font-optical-sizing",
    "font-size",
    "font-stretch",
    "font-style",
    "font-synthesis",
    "font-variant",
    "font-variant-alternates",
    "font-variant-caps",
    "font-variant-east-asian",
    "font-variant-ligatures",
    "font-variant-numeric",
    "font-variant-position",
    "font-variation-settings",
    "font-weight",
    "glyph-orientation-horizontal",
    "glyph-orientation-vertical",
    "hanging-punctuation",
    "image-orientation",
    "image-rendering",
    "image-resolution",
    "kerning",
    "letter-spacing",
    "line-break",
    "line-height",
    "list-style",
    "list-style-image",
    "list-style-position",
    "list-style-type",
    "marker",
    "marker-end",
    "marker-mid",
    "marker-start",
    "orphans",
    "pointer-events",
    "quotes",
    "resize",
    "shape-rendering",
    "speak-as",
    "stroke",
    "stroke-color",
    "stroke-dasharray",
    "stroke-dashoffset",
    "stroke-linecap",
    "stroke-linejoin",
    "stroke-miterlimit",
    "stroke-opacity",
    "stroke-width",
    "tab-size",
    "text-align",
    "text-anchor",
    "text-indent",
    "text-rendering",
    "text-shadow",
    "text-transform",
    "visibility",
    "white-space",
    "widows",
    "word-break",
    "word-spacing",
    "word-wrap",
    "writing-mode",
]);

WI.CSSKeywordCompletions._colors = [
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

WI.CSSKeywordCompletions._colorAwareProperties = [
    "background", "background-color", "background-image", "border", "border-color", "border-top", "border-right", "border-bottom",
    "border-left", "border-top-color", "border-right-color", "border-bottom-color", "border-left-color", "box-shadow", "color",
    "fill", "outline", "outline-color", "stroke", "text-line-through", "text-line-through-color", "text-overline", "text-overline-color",
    "text-shadow", "text-underline", "text-underline-color", "-webkit-box-shadow", "-webkit-column-rule", "-webkit-column-rule-color",
    "-webkit-text-emphasis", "-webkit-text-emphasis-color", "-webkit-text-fill-color", "-webkit-text-stroke", "-webkit-text-stroke-color",
    "-webkit-text-decoration-color",

    // iOS Properties
    "-webkit-tap-highlight-color"
].keySet();

WI.CSSKeywordCompletions._propertyKeywordMap = {
    "content": [
        "list-item", "close-quote", "no-close-quote", "no-open-quote", "open-quote", "attr()", "counter()", "counters()", "url()", "linear-gradient()", "radial-gradient()", "repeating-linear-gradient()", "repeating-radial-gradient()", "-webkit-canvas()", "cross-fade()", "image-set()"
    ],
    "list-style-image": [
        "none", "url()", "linear-gradient()", "radial-gradient()", "repeating-linear-gradient()", "repeating-radial-gradient()", "-webkit-canvas()", "cross-fade()", "image-set()"
    ],
    "baseline-shift": [
        "baseline", "sub", "super"
    ],
    "border-bottom-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "font-stretch": [
        "normal", "wider", "narrower", "ultra-condensed", "extra-condensed", "condensed", "semi-condensed",
        "semi-expanded", "expanded", "extra-expanded", "ultra-expanded"
    ],
    "border-left-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "border-top-width": [
        "medium", "thick", "thin", "calc()"
    ],
    "outline-color": [
        "invert", "-webkit-focus-ring-color"
    ],
    "cursor": [
        "auto", "default", "none", "context-menu", "help", "pointer", "progress", "wait", "cell", "crosshair", "text", "vertical-text",
        "alias", "copy", "move", "no-drop", "not-allowed", "grab", "grabbing",
        "e-resize", "n-resize", "ne-resize", "nw-resize", "s-resize", "se-resize", "sw-resize", "w-resize", "ew-resize", "ns-resize", "nesw-resize", "nwse-resize",
        "col-resize", "row-resize", "all-scroll", "zoom-in", "zoom-out",
        "-webkit-grab", "-webkit-grabbing", "-webkit-zoom-in", "-webkit-zoom-out",
        "url()", "image-set()"
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
    "enable-background": [
        "accumulate", "new"
    ],
    "hanging-punctuation": [
        "none", "first", "last", "allow-end", "force-end"
    ],
    "overflow": [
        "hidden", "auto", "visible", "scroll", "marquee", "-webkit-paged-x", "-webkit-paged-y"
    ],
    "-webkit-box-reflect": [
        "none", "left", "right", "above", "below"
    ],
    "margin-bottom": [
        "auto"
    ],
    "font-weight": [
        "normal", "bold", "bolder", "lighter", "100", "200", "300", "400", "500", "600", "700", "800", "900"
    ],
    "font-synthesis": [
        "none", "weight", "style"
    ],
    "font-style": [
        "italic", "oblique", "normal"
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
        "-apple-system-tall-body", "-apple-system-title0", "-apple-system-title1", "-apple-system-title2", "-apple-system-title3", "-apple-system-title4", "system-ui"
    ],
    "outline-width": [
        "medium", "thick", "thin", "calc()"
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
    "line-height": [
        "normal"
    ],
    "counter-increment": [
        "none"
    ],
    "counter-reset": [
        "none"
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
    "border-image": [
        "repeat", "stretch", "url()", "linear-gradient()", "radial-gradient()", "repeating-linear-gradient()", "repeating-radial-gradient()", "-webkit-canvas()", "cross-fade()", "image-set()"
    ],
    "border-image-repeat": [
        "repeat", "stretch", "space", "round"
    ],
    "-webkit-mask-box-image-repeat": [
        "repeat", "stretch", "space", "round"
    ],
    "font-family": [
        "serif", "sans-serif", "cursive", "fantasy", "monospace", "-webkit-body", "-webkit-pictograph",
        "-apple-system", "-apple-system-headline", "-apple-system-body",
        "-apple-system-subheadline", "-apple-system-footnote", "-apple-system-caption1", "-apple-system-caption2",
        "-apple-system-short-headline", "-apple-system-short-body", "-apple-system-short-subheadline",
        "-apple-system-short-footnote", "-apple-system-short-caption1", "-apple-system-tall-body",
        "-apple-system-title0", "-apple-system-title1", "-apple-system-title2", "-apple-system-title3", "-apple-system-title4", "system-ui"
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
    "letter-spacing": [
        "normal", "calc()"
    ],
    "word-spacing": [
        "normal", "calc()"
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
    "text-indent": [
        "-webkit-each-line", "-webkit-hanging"
    ],
    "clip": [
        "auto", "rect()"
    ],
    "clip-path": [
        "none", "url()", "circle()", "ellipse()", "inset()", "polygon()", "margin-box", "border-box", "padding-box", "content-box"
    ],
    "shape-outside": [
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
    "-webkit-marquee-repetition": [
        "infinite"
    ],
    "-webkit-marquee-speed": [
        "normal", "slow", "fast"
    ],
    "margin-right": [
        "auto"
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
    "text-decoration": [
        "none", "underline", "overline", "line-through", "blink"
    ],
    "-webkit-text-decorations-in-effect": [
        "none", "underline", "overline", "line-through", "blink"
    ],
    "-webkit-text-decoration-line": [
        "none", "underline", "overline", "line-through", "blink"
    ],
    "-webkit-text-decoration-skip": [
        "auto", "none", "objects", "ink"
    ],
    "-webkit-text-underline-position": [
        "auto", "alphabetic", "under"
    ],
    "transition": [
        "none", "ease", "linear", "ease-in", "ease-out", "ease-in-out", "step-start", "step-end", "steps()", "cubic-bezier()", "spring()", "all", WI.CSSKeywordCompletions.AllPropertyNamesPlaceholder
    ],
    "transition-timing-function": [
        "ease", "linear", "ease-in", "ease-out", "ease-in-out", "step-start", "step-end", "steps()", "cubic-bezier()", "spring()"
    ],
    "transition-property": [
        "all", "none", WI.CSSKeywordCompletions.AllPropertyNamesPlaceholder
    ],
    "animation-direction": [
        "normal", "alternate", "reverse", "alternate-reverse"
    ],
    "animation-fill-mode": [
        "none", "forwards", "backwards", "both"
    ],
    "animation-iteration-count": [
        "infinite"
    ],
    "animation-play-state": [
        "paused", "running"
    ],
    "animation-timing-function": [
        "ease", "linear", "ease-in", "ease-out", "ease-in-out", "step-start", "step-end", "steps()", "cubic-bezier()", "spring()"
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
    "scroll-snap-align": [
        "none", "start", "center", "end"
    ],
    "scroll-snap-type": [
        "none", "mandatory", "proximity", "x", "y", "inline", "block", "both"
    ],
    "-webkit-background-composite": [
        "clear", "copy", "source-over", "source-in", "source-out", "source-atop", "destination-over", "destination-in", "destination-out", "destination-atop", "xor", "plus-darker", "plus-lighter"
    ],
    "-webkit-mask-composite": [
        "clear", "copy", "source-over", "source-in", "source-out", "source-atop", "destination-over", "destination-in", "destination-out", "destination-atop", "xor", "plus-darker", "plus-lighter"
    ],
    "-webkit-text-stroke-width": [
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
    "-webkit-line-box-contain": [
        "block", "inline", "font", "glyphs", "replaced", "inline-box", "none"
    ],
    "font-feature-settings": [
        "normal"
    ],
    "-webkit-animation-trigger": [
        "auto", "container-scroll()"
    ],

    // iOS Properties
    "-webkit-text-size-adjust": [
        "none", "auto"
    ],

    // Compatibility (iOS 12): `inherited` didn't exist on `CSSPropertyInfo`
    "-apple-pay-button-style": [
        "black", "white", "white-outline",
    ],
    "-apple-pay-button-type": [
        "plain", "buy", "set-up", "donate", "check-out", "book", "subscribe",
    ],
    "-apple-trailing-word": [
        "auto", "-webkit-partially-balanced",
    ],
    "-webkit-alt": [
        "attr()",
    ],
    "-webkit-animation-direction": [
        "normal", "alternate", "reverse", "alternate-reverse",
    ],
    "-webkit-animation-fill-mode": [
        "none", "forwards", "backwards", "both",
    ],
    "-webkit-animation-iteration-count": [
        "infinite",
    ],
    "-webkit-animation-play-state": [
        "paused", "running",
    ],
    "-webkit-animation-timing-function": [
        "ease", "linear", "ease-in", "ease-out", "ease-in-out", "step-start", "step-end", "steps()", "cubic-bezier()", "spring()",
    ],
    "-webkit-appearance": [
        "none", "checkbox", "radio", "push-button", "square-button", "button", "button-bevel", "default-button", "inner-spin-button", "listbox", "listitem", "media-controls-background", "media-controls-dark-bar-background", "media-controls-fullscreen-background", "media-controls-light-bar-background", "media-current-time-display", "media-enter-fullscreen-button", "media-exit-fullscreen-button", "media-fullscreen-volume-slider", "media-fullscreen-volume-slider-thumb", "media-mute-button", "media-overlay-play-button", "media-play-button", "media-return-to-realtime-button", "media-rewind-button", "media-seek-back-button", "media-seek-forward-button", "media-slider", "media-sliderthumb", "media-time-remaining-display", "media-toggle-closed-captions-button", "media-volume-slider", "media-volume-slider-container", "media-volume-slider-mute-button", "media-volume-sliderthumb", "menulist", "menulist-button", "menulist-text", "menulist-textfield", "meter", "progress-bar", "progress-bar-value", "slider-horizontal", "slider-vertical", "sliderthumb-horizontal", "sliderthumb-vertical", "caret", "searchfield", "searchfield-decoration", "searchfield-results-decoration", "searchfield-results-button", "searchfield-cancel-button", "snapshotted-plugin-overlay", "textfield", "relevancy-level-indicator", "continuous-capacity-level-indicator", "discrete-capacity-level-indicator", "rating-level-indicator", "image-controls-button", "-apple-pay-button", "textarea", "attachment", "borderless-attachment", "caps-lock-indicator",
    ],
    "-webkit-backface-visibility": [
        "hidden", "visible",
    ],
    "-webkit-border-after-width": [
        "medium", "thick", "thin", "calc()",
    ],
    "-webkit-border-before-width": [
        "medium", "thick", "thin", "calc()",
    ],
    "-webkit-border-end-width": [
        "medium", "thick", "thin", "calc()",
    ],
    "-webkit-border-fit": [
        "border", "lines",
    ],
    "-webkit-border-start-width": [
        "medium", "thick", "thin", "calc()",
    ],
    "-webkit-box-align": [
        "baseline", "center", "stretch", "start", "end",
    ],
    "-webkit-box-decoration-break": [
        "clone", "slice",
    ],
    "-webkit-box-direction": [
        "normal", "reverse",
    ],
    "-webkit-box-lines": [
        "single", "multiple",
    ],
    "-webkit-box-orient": [
        "horizontal", "vertical", "inline-axis", "block-axis",
    ],
    "-webkit-box-pack": [
        "center", "justify", "start", "end",
    ],
    "-webkit-column-axis": [
        "auto", "horizontal", "vertical",
    ],
    "-webkit-column-count": [
        "auto", "calc()",
    ],
    "-webkit-column-fill": [
        "auto", "balance",
    ],
    "-webkit-column-gap": [
        "normal", "calc()",
    ],
    "-webkit-column-progression": [
        "normal", "reverse",
    ],
    "-webkit-column-rule-width": [
        "medium", "thick", "thin", "calc()",
    ],
    "-webkit-column-span": [
        "all", "none", "calc()",
    ],
    "-webkit-column-width": [
        "auto", "calc()",
    ],
    "-webkit-cursor-visibility": [
        "auto", "auto-hide",
    ],
    "-webkit-font-kerning": [
        "none", "normal", "auto",
    ],
    "-webkit-font-smoothing": [
        "none", "auto", "antialiased", "subpixel-antialiased",
    ],
    "-webkit-hyphens": [
        "none", "auto", "manual",
    ],
    "-webkit-line-align": [
        "none", "edges",
    ],
    "-webkit-line-break": [
        "auto", "loose", "normal", "strict", "after-white-space",
    ],
    "-webkit-line-snap": [
        "none", "baseline", "contain",
    ],
    "-webkit-logical-height": [
        "intrinsic", "min-intrinsic", "calc()",
    ],
    "-webkit-logical-width": [
        "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()",
    ],
    "-webkit-margin-after-collapse": [
        "collapse", "separate", "discard",
    ],
    "-webkit-margin-before-collapse": [
        "collapse", "separate", "discard",
    ],
    "-webkit-margin-bottom-collapse": [
        "collapse", "separate", "discard",
    ],
    "-webkit-margin-top-collapse": [
        "collapse", "separate", "discard",
    ],
    "-webkit-marquee-direction": [
        "left", "right", "auto", "reverse", "forwards", "backwards", "ahead", "up", "down",
    ],
    "-webkit-marquee-style": [
        "none", "scroll", "slide", "alternate",
    ],
    "-webkit-max-logical-height": [
        "none", "intrinsic", "min-intrinsic", "calc()",
    ],
    "-webkit-max-logical-width": [
        "none", "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()",
    ],
    "-webkit-min-logical-height": [
        "intrinsic", "min-intrinsic", "calc()",
    ],
    "-webkit-min-logical-width": [
        "intrinsic", "min-intrinsic", "-webkit-min-content", "-webkit-max-content", "-webkit-fill-available", "-webkit-fit-content", "calc()",
    ],
    "-webkit-nbsp-mode": [
        "normal", "space",
    ],
    "-webkit-overflow-scrolling": [
        "auto", "touch",
    ],
    "-webkit-print-color-adjust": [
        "economy", "exact",
    ],
    "-webkit-rtl-ordering": [
        "logical", "visual",
    ],
    "-webkit-ruby-position": [
        "after", "before", "inter-character",
    ],
    "-webkit-text-align-last": [
        "auto", "start", "end", "left", "right", "center", "justify",
    ],
    "-webkit-text-combine": [
        "none", "horizontal",
    ],
    "-webkit-text-decoration-style": [
        "dotted", "dashed", "solid", "double", "wavy",
    ],
    "-webkit-text-justify": [
        "auto", "none", "inter-word", "inter-ideograph", "inter-cluster", "distribute", "kashida",
    ],
    "-webkit-text-orientation": [
        "sideways", "sideways-right", "upright", "mixed",
    ],
    "-webkit-text-security": [
        "none", "disc", "circle", "square",
    ],
    "-webkit-text-zoom": [
        "normal", "reset",
    ],
    "-webkit-transform-style": [
        "flat", "preserve-3d",
    ],
    "-webkit-user-drag": [
        "none", "auto", "element",
    ],
    "-webkit-user-modify": [
        "read-only", "read-write", "read-write-plaintext-only",
    ],
    "-webkit-user-select": [
        "none", "all", "auto", "text",
    ],
    "-webkit-writing-mode": [
        "lr", "rl", "tb", "lr-tb", "rl-tb", "tb-rl", "horizontal-tb", "vertical-rl", "vertical-lr", "horizontal-bt",
    ],
    "alignment-baseline": [
        "baseline", "middle", "auto", "alphabetic", "before-edge", "after-edge", "central", "text-before-edge", "text-after-edge", "ideographic", "hanging", "mathematical",
    ],
    "border-block-end-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "border-block-start-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "border-bottom-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "border-collapse": [
        "collapse", "separate",
    ],
    "border-inline-end-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "border-inline-start-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "border-left-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "border-right-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "border-top-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "box-sizing": [
        "border-box", "content-box",
    ],
    "break-after": [
        "left", "right", "auto", "avoid", "column", "avoid-column", "avoid-page", "page", "recto", "verso",
    ],
    "break-before": [
        "left", "right", "auto", "avoid", "column", "avoid-column", "avoid-page", "page", "recto", "verso",
    ],
    "break-inside": [
        "auto", "avoid", "avoid-column", "avoid-page",
    ],
    "buffered-rendering": [
        "auto", "static", "dynamic",
    ],
    "caption-side": [
        "top", "bottom", "left", "right",
    ],
    "clear": [
        "none", "left", "right", "both",
    ],
    "clip-rule": [
        "nonzero", "evenodd",
    ],
    "color-interpolation": [
        "auto", "sRGB", "linearRGB",
    ],
    "color-interpolation-filters": [
        "auto", "sRGB", "linearRGB",
    ],
    "color-rendering": [
        "auto", "optimizeSpeed", "optimizeQuality",
    ],
    "column-fill": [
        "auto", "balance",
    ],
    "column-rule-style": [
        "none", "hidden", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double",
    ],
    "direction": [
        "ltr", "rtl",
    ],
    "display": [
        "none", "inline", "block", "list-item", "compact", "inline-block", "table", "inline-table", "table-row-group", "table-header-group", "table-footer-group", "table-row", "table-column-group", "table-column", "table-cell", "table-caption", "-webkit-box", "-webkit-inline-box", "flex", "-webkit-flex", "inline-flex", "-webkit-inline-flex", "contents", "grid", "inline-grid",
    ],
    "dominant-baseline": [
        "middle", "auto", "alphabetic", "central", "text-before-edge", "text-after-edge", "ideographic", "hanging", "mathematical", "use-script", "no-change", "reset-size",
    ],
    "empty-cells": [
        "hide", "show",
    ],
    "fill-rule": [
        "nonzero", "evenodd",
    ],
    "flex-direction": [
        "row", "row-reverse", "column", "column-reverse",
    ],
    "flex-wrap": [
        "nowrap", "wrap-reverse", "wrap",
    ],
    "float": [
        "none", "left", "right",
    ],
    "font-optical-sizing": [
        "none", "auto",
    ],
    "font-variant-alternates": [
        "historical-forms", "normal",
    ],
    "font-variant-caps": [
        "small-caps", "all-small-caps", "petite-caps", "all-petite-caps", "unicase", "titling-caps", "normal",
    ],
    "font-variant-position": [
        "normal", "sub", "super",
    ],
    "image-rendering": [
        "auto", "optimizeSpeed", "optimizeQuality", "crisp-edges", "pixelated", "-webkit-crisp-edges", "-webkit-optimize-contrast",
    ],
    "image-resolution": [
        "from-image", "snap",
    ],
    "isolation": [
        "auto", "isolate",
    ],
    "line-break": [
        "normal", "auto", "loose", "strict", "after-white-space",
    ],
    "list-style-position": [
        "outside", "inside",
    ],
    "list-style-type": [
        "none", "disc", "circle", "square", "decimal", "decimal-leading-zero", "arabic-indic", "binary", "bengali", "cambodian", "khmer", "devanagari", "gujarati", "gurmukhi", "kannada", "lower-hexadecimal", "lao", "malayalam", "mongolian", "myanmar", "octal", "oriya", "persian", "urdu", "telugu", "tibetan", "thai", "upper-hexadecimal", "lower-roman", "upper-roman", "lower-greek", "lower-alpha", "lower-latin", "upper-alpha", "upper-latin", "afar", "ethiopic-halehame-aa-et", "ethiopic-halehame-aa-er", "amharic", "ethiopic-halehame-am-et", "amharic-abegede", "ethiopic-abegede-am-et", "cjk-earthly-branch", "cjk-heavenly-stem", "ethiopic", "ethiopic-halehame-gez", "ethiopic-abegede", "ethiopic-abegede-gez", "hangul-consonant", "hangul", "lower-norwegian", "oromo", "ethiopic-halehame-om-et", "sidama", "ethiopic-halehame-sid-et", "somali", "ethiopic-halehame-so-et", "tigre", "ethiopic-halehame-tig", "tigrinya-er", "ethiopic-halehame-ti-er", "tigrinya-er-abegede", "ethiopic-abegede-ti-er", "tigrinya-et", "ethiopic-halehame-ti-et", "tigrinya-et-abegede", "ethiopic-abegede-ti-et", "upper-greek", "upper-norwegian", "asterisks", "footnotes", "hebrew", "armenian", "lower-armenian", "upper-armenian", "georgian", "cjk-ideographic", "hiragana", "katakana", "hiragana-iroha", "katakana-iroha",
    ],
    "mask-type": [
        "alpha", "luminance",
    ],
    "max-zoom": [
        "auto",
    ],
    "min-zoom": [
        "auto",
    ],
    "mix-blend-mode": [
        "normal", "plus-darker", "plus-lighter", "overlay", "multiply", "screen", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity",
    ],
    "object-fit": [
        "none", "contain", "cover", "fill", "scale-down",
    ],
    "orientation": [
        "auto", "portait", "landscape",
    ],
    "outline-style": [
        "none", "inset", "groove", "outset", "ridge", "dotted", "dashed", "solid", "double", "auto",
    ],
    "overflow-wrap": [
        "normal", "break-word",
    ],
    "overflow-x": [
        "hidden", "auto", "visible", "scroll",
    ],
    "overflow-y": [
        "hidden", "auto", "visible", "scroll", "-webkit-paged-x", "-webkit-paged-y",
    ],
    "pointer-events": [
        "none", "all", "auto", "visible", "visiblePainted", "visibleFill", "visibleStroke", "painted", "fill", "stroke",
    ],
    "position": [
        "absolute", "fixed", "relative", "static", "-webkit-sticky",
    ],
    "resize": [
        "none", "auto", "both", "horizontal", "vertical",
    ],
    "shape-rendering": [
        "auto", "optimizeSpeed", "geometricPrecision", "crispedges",
    ],
    "stroke-linecap": [
        "square", "round", "butt",
    ],
    "stroke-linejoin": [
        "round", "miter", "bevel",
    ],
    "table-layout": [
        "auto", "fixed",
    ],
    "text-align": [
        "-webkit-auto", "left", "right", "center", "justify", "-webkit-left", "-webkit-right", "-webkit-center", "-webkit-match-parent", "start", "end",
    ],
    "text-anchor": [
        "middle", "start", "end",
    ],
    "text-line-through": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave", "continuous", "skip-white-space",
    ],
    "text-line-through-mode": [
        "continuous", "skip-white-space",
    ],
    "text-line-through-style": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave",
    ],
    "text-line-through-width": [
        "normal", "medium", "auto", "thick", "thin",
    ],
    "text-overflow": [
        "clip", "ellipsis",
    ],
    "text-overline-mode": [
        "continuous", "skip-white-space",
    ],
    "text-overline-style": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave",
    ],
    "text-overline-width": [
        "normal", "medium", "auto", "thick", "thin", "calc()",
    ],
    "text-rendering": [
        "auto", "optimizeSpeed", "optimizeLegibility", "geometricPrecision",
    ],
    "text-transform": [
        "none", "capitalize", "uppercase", "lowercase",
    ],
    "text-underline": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave",
    ],
    "text-underline-mode": [
        "continuous", "skip-white-space",
    ],
    "text-underline-style": [
        "none", "dotted", "dashed", "solid", "double", "dot-dash", "dot-dot-dash", "wave",
    ],
    "text-underline-width": [
        "normal", "medium", "auto", "thick", "thin", "calc()",
    ],
    "transform-style": [
        "flat", "preserve-3d",
    ],
    "unicode-bidi": [
        "normal", "bidi-override", "embed", "isolate-override", "plaintext", "-webkit-isolate", "-webkit-isolate-override", "-webkit-plaintext", "isolate",
    ],
    "user-zoom": [
        "zoom", "fixed",
    ],
    "vector-effect": [
        "none",
    ],
    "visibility": [
        "hidden", "visible", "collapse",
    ],
    "white-space": [
        "normal", "nowrap", "pre", "pre-line", "pre-wrap",
    ],
    "word-break": [
        "normal", "break-all", "keep-all", "break-word",
    ],
    "word-wrap": [
        "normal", "break-word",
    ],
    "writing-mode": [
        "lr", "rl", "tb", "lr-tb", "rl-tb", "tb-rl", "horizontal-tb", "vertical-rl", "vertical-lr", "horizontal-bt",
    ],
};

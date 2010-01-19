/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.CSSSourceSyntaxHighlighter = function(table, sourceFrame) {
    WebInspector.SourceSyntaxHighlighter.call(this, table, sourceFrame);

    this.LexState = {
        Initial: 1,
        DeclarationProperty: 2,
        DeclarationValue: 3,
        AtMedia: 4,
        AtRule: 5,
        AtKeyframes: 6
    };
    this.ContinueState = {
        None: 0,
        Comment: 1
    };
    
    this.nonToken = "";
    this.cursor = 0;
    this.lineIndex = -1;
    this.lineCode = "";
    this.newLine = null;
    this.lexState = this.LexState.Initial;
    this.continueState = this.ContinueState.None;
    
    const urlPattern = /^url\(\s*(?:(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*')|(?:[!#$%&*-~\w]|(?:\\[\da-f]{1,6}\s?|\.))*)\s*\)/i;
    const stringPattern = /^(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*')/i;
    const identPattern = /^-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*/i;
    const startBlockPattern = /^{/i;
    const endBlockPattern = /^}/i;

    const propertyKeywords = [
        "background", "background-attachment", "background-clip", "background-color", "background-image",
        "background-origin", "background-position", "background-position-x", "background-position-y",
        "background-repeat", "background-repeat-x", "background-repeat-y", "background-size", "border",
        "border-bottom", "border-bottom-color", "border-bottom-left-radius", "border-bottom-right-radius",
        "border-bottom-style", "border-bottom-width", "border-collapse", "border-color", "border-left",
        "border-left-color", "border-left-style", "border-left-width", "border-radius", "border-right",
        "border-right-color", "border-right-style", "border-right-width", "border-spacing", "border-style",
        "border-top", "border-top-color", "border-top-left-radius", "border-top-right-radius", "border-top-style",
        "border-top-width", "border-width", "bottom", "caption-side", "clear", "clip", "color", "content",
        "counter-increment", "counter-reset", "cursor", "direction", "display", "empty-cells", "float",
        "font", "font-family", "font-size", "font-stretch", "font-style", "font-variant", "font-weight",
        "height", "left", "letter-spacing", "line-height", "list-style", "list-style-image", "list-style-position",
        "list-style-type", "margin", "margin-bottom", "margin-left", "margin-right", "margin-top", "max-height",
        "max-width", "min-height", "min-width", "opacity", "orphans", "outline", "outline-color", "outline-offset",
        "outline-style", "outline-width", "overflow", "overflow-x", "overflow-y", "padding", "padding-bottom",
        "padding-left", "padding-right", "padding-top", "page", "page-break-after", "page-break-before",
        "page-break-inside", "pointer-events", "position", "quotes", "resize", "right", "size", "src",
        "table-layout", "text-align", "text-decoration", "text-indent", "text-line-through", "text-line-through-color",
        "text-line-through-mode", "text-line-through-style", "text-line-through-width", "text-overflow", "text-overline",
        "text-overline-color", "text-overline-mode", "text-overline-style", "text-overline-width", "text-rendering",
        "text-shadow", "text-transform", "text-underline", "text-underline-color", "text-underline-mode",
        "text-underline-style", "text-underline-width", "top", "unicode-bidi", "unicode-range", "vertical-align",
        "visibility", "white-space", "widows", "width", "word-break", "word-spacing", "word-wrap", "z-index", "zoom",
        "-webkit-animation", "-webkit-animation-delay", "-webkit-animation-direction", "-webkit-animation-duration",
        "-webkit-animation-iteration-count", "-webkit-animation-name", "-webkit-animation-play-state",
        "-webkit-animation-timing-function", "-webkit-appearance", "-webkit-backface-visibility",
        "-webkit-background-clip", "-webkit-background-composite", "-webkit-background-origin", "-webkit-background-size",
        "-webkit-binding", "-webkit-border-fit", "-webkit-border-horizontal-spacing", "-webkit-border-image",
        "-webkit-border-radius", "-webkit-border-vertical-spacing", "-webkit-box-align", "-webkit-box-direction",
        "-webkit-box-flex", "-webkit-box-flex-group", "-webkit-box-lines", "-webkit-box-ordinal-group",
        "-webkit-box-orient", "-webkit-box-pack", "-webkit-box-reflect", "-webkit-box-shadow", "-webkit-box-sizing",
        "-webkit-column-break-after", "-webkit-column-break-before", "-webkit-column-break-inside", "-webkit-column-count",
        "-webkit-column-gap", "-webkit-column-rule", "-webkit-column-rule-color", "-webkit-column-rule-style",
        "-webkit-column-rule-width", "-webkit-column-width", "-webkit-columns", "-webkit-font-size-delta",
        "-webkit-font-smoothing", "-webkit-highlight", "-webkit-line-break", "-webkit-line-clamp",
        "-webkit-margin-bottom-collapse", "-webkit-margin-collapse", "-webkit-margin-start", "-webkit-margin-top-collapse",
        "-webkit-marquee", "-webkit-marquee-direction", "-webkit-marquee-increment", "-webkit-marquee-repetition",
        "-webkit-marquee-speed", "-webkit-marquee-style", "-webkit-mask", "-webkit-mask-attachment",
        "-webkit-mask-box-image", "-webkit-mask-clip", "-webkit-mask-composite", "-webkit-mask-image",
        "-webkit-mask-origin", "-webkit-mask-position", "-webkit-mask-position-x", "-webkit-mask-position-y",
        "-webkit-mask-repeat", "-webkit-mask-repeat-x", "-webkit-mask-repeat-y", "-webkit-mask-size",
        "-webkit-match-nearest-mail-blockquote-color", "-webkit-nbsp-mode", "-webkit-padding-start",
        "-webkit-perspective", "-webkit-perspective-origin", "-webkit-perspective-origin-x", "-webkit-perspective-origin-y",
        "-webkit-rtl-ordering", "-webkit-text-decorations-in-effect", "-webkit-text-fill-color", "-webkit-text-security",
        "-webkit-text-size-adjust", "-webkit-text-stroke", "-webkit-text-stroke-color", "-webkit-text-stroke-width",
        "-webkit-transform", "-webkit-transform-origin", "-webkit-transform-origin-x", "-webkit-transform-origin-y",
        "-webkit-transform-origin-z", "-webkit-transform-style", "-webkit-transition", "-webkit-transition-delay",
        "-webkit-transition-duration", "-webkit-transition-property", "-webkit-transition-timing-function",
        "-webkit-user-drag", "-webkit-user-modify", "-webkit-user-select", "-webkit-variable-declaration-block"
    ].keySet();
    
    const valueKeywords = [
        "above", "absolute", "activeborder", "activecaption", "afar", "after-white-space", "ahead", "alias", "all", "all-scroll",
        "alternate", "always","amharic", "amharic-abegede", "antialiased", "appworkspace", "aqua", "armenian", "auto", "avoid",
        "background", "backwards", "baseline", "below", "bidi-override", "black", "blink", "block", "block-axis", "blue", "bold",
        "bolder", "border", "border-box", "both", "bottom", "break-all", "break-word", "button", "button-bevel", "buttonface",
        "buttonhighlight", "buttonshadow", "buttontext", "capitalize", "caps-lock-indicator", "caption", "captiontext", "caret", "cell",
        "center", "checkbox", "circle", "cjk-earthly-branch", "cjk-heavenly-stem", "cjk-ideographic", "clear", "clip", "close-quote",
        "col-resize", "collapse", "compact", "condensed", "contain", "content", "content-box", "context-menu", "continuous", "copy",
        "cover", "crop", "cross", "crosshair", "currentcolor", "cursive", "dashed", "decimal", "decimal-leading-zero", "default",
        "default-button", "destination-atop", "destination-in", "destination-out", "destination-over", "disc", "discard", "document",
        "dot-dash", "dot-dot-dash", "dotted", "double", "down", "e-resize", "ease", "ease-in", "ease-in-out", "ease-out", "element",
        "ellipsis", "embed", "end", "ethiopic", "ethiopic-abegede", "ethiopic-abegede-am-et", "ethiopic-abegede-gez",
        "ethiopic-abegede-ti-er", "ethiopic-abegede-ti-et", "ethiopic-halehame-aa-er", "ethiopic-halehame-aa-et",
        "ethiopic-halehame-am-et", "ethiopic-halehame-gez", "ethiopic-halehame-om-et", "ethiopic-halehame-sid-et",
        "ethiopic-halehame-so-et", "ethiopic-halehame-ti-er", "ethiopic-halehame-ti-et", "ethiopic-halehame-tig", "ew-resize", "expanded",
        "extra-condensed", "extra-expanded", "fantasy", "fast", "fill", "fixed", "flat", "forwards", "fuchsia", "geometricPrecision",
        "georgian", "gray", "graytext", "green", "grey", "groove", "hand", "hangul", "hangul-consonant", "hebrew", "help", "hidden", "hide",
        "higher", "highlight", "highlighttext", "hiragana", "hiragana-iroha", "horizontal", "icon", "ignore",
        "inactiveborder", "inactivecaption", "inactivecaptiontext", "infinite", "infobackground", "infotext", "inherit", "initial", "inline",
        "inline-axis", "inline-block", "inline-table", "inset", "inside", "intrinsic", "invert", "italic", "justify", "katakana",
        "katakana-iroha", "landscape", "large", "larger", "left", "level", "lighter", "lime", "line-through", "linear", "lines",
        "list-button", "list-item", "listbox", "listitem", "local", "logical", "loud", "lower", "lower-alpha", "lower-greek", "lower-latin",
        "lower-norwegian", "lower-roman", "lowercase", "ltr", "maroon", "match", "media-controls-background", "media-current-time-display",
        "media-fullscreen-button", "media-mute-button", "media-play-button", "media-return-to-realtime-button", "media-rewind-button",
        "media-seek-back-button", "media-seek-forward-button", "media-slider", "media-sliderthumb", "media-time-remaining-display",
        "media-volume-slider", "media-volume-slider-container", "media-volume-sliderthumb", "medium", "menu", "menulist", "menulist-button",
        "menulist-text", "menulist-textfield", "menutext", "message-box", "middle", "min-intrinsic", "mix", "monospace", "move", "multiple",
        "n-resize", "narrower", "navy", "ne-resize", "nesw-resize", "no-close-quote", "no-drop", "no-open-quote", "no-repeat", "none",
        "normal", "not-allowed", "nowrap", "ns-resize", "nw-resize", "nwse-resize", "oblique", "olive", "open-quote", "optimizeLegibility",
        "optimizeSpeed", "orange", "oromo", "outset", "outside", "overlay", "overline", "padding", "padding-box", "painted", "paused",
        "plus-darker", "plus-lighter", "pointer", "portrait", "pre", "pre-line", "pre-wrap", "preserve-3d", "progress", "purple",
        "push-button", "radio", "read-only", "read-write", "read-write-plaintext-only", "red", "relative", "repeat", "repeat-x",
        "repeat-y", "reset", "reverse", "ridge", "right", "round", "row-resize", "rtl", "run-in", "running", "s-resize", "sans-serif",
        "scroll", "scrollbar", "se-resize", "searchfield", "searchfield-cancel-button", "searchfield-decoration", "searchfield-results-button",
        "searchfield-results-decoration", "semi-condensed", "semi-expanded", "separate", "serif", "show", "sidama", "silver", "single",
        "skip-white-space", "slide", "slider-horizontal", "slider-vertical", "sliderthumb-horizontal", "sliderthumb-vertical", "slow",
        "small", "small-caps", "small-caption", "smaller", "solid", "somali", "source-atop", "source-in", "source-out", "source-over",
        "space", "square", "square-button", "start", "static", "status-bar", "stretch", "stroke", "sub", "subpixel-antialiased", "super",
        "sw-resize", "table", "table-caption", "table-cell", "table-column", "table-column-group", "table-footer-group", "table-header-group",
        "table-row", "table-row-group", "teal", "text", "text-bottom", "text-top", "textarea", "textfield", "thick", "thin", "threeddarkshadow",
        "threedface", "threedhighlight", "threedlightshadow", "threedshadow", "tigre", "tigrinya-er", "tigrinya-er-abegede", "tigrinya-et",
        "tigrinya-et-abegede", "top", "transparent", "ultra-condensed", "ultra-expanded", "underline", "up", "upper-alpha", "upper-greek",
        "upper-latin", "upper-norwegian", "upper-roman", "uppercase", "vertical", "vertical-text", "visible", "visibleFill", "visiblePainted",
        "visibleStroke", "visual", "w-resize", "wait", "wave", "white", "wider", "window", "windowframe", "windowtext", "x-large", "x-small",
        "xor", "xx-large", "xx-small", "yellow", "-wap-marquee", "-webkit-activelink", "-webkit-auto", "-webkit-baseline-middle", "-webkit-body",
        "-webkit-box", "-webkit-center", "-webkit-control", "-webkit-focus-ring-color", "-webkit-grab", "-webkit-grabbing", "-webkit-inline-box",
        "-webkit-left", "-webkit-link", "-webkit-marquee", "-webkit-mini-control", "-webkit-nowrap", "-webkit-right", "-webkit-small-control",
        "-webkit-text", "-webkit-xxx-large", "-webkit-zoom-in", "-webkit-zoom-out",
    ].keySet();

    const mediaTypes = ["all", "aural", "braille", "embossed", "handheld", "print", "projection", "screen", "tty", "tv"].keySet();

    this.rules = [{
        name: "commentAction",
        pattern: /^\/\*[^\*]*\*+([^\/*][^*]*\*+)*\//i,
        style: "webkit-css-comment"
    }, {
        name: "commentStartAction",
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*)/i,
        style: "webkit-css-comment",
        postContinueState: this.ContinueState.Comment
    }, {
        name: "commentEndAction",
        pattern: /^(?:(?:[^\*]|\*[^\/])*\*+\/)/i,
        style: "webkit-css-comment",
        preContinueState: this.ContinueState.Comment,
        postContinueState: this.ContinueState.None
    }, {
        name: "commentMiddleAction",
        pattern: /^.*/i,
        style: "webkit-css-comment",
        preContinueState: this.ContinueState.Comment
    }, {
        name: "selectorAction",
        pattern: /^(?:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\*)(?:#-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\.-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\[\s*-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*(?:(?:=|~=|\|=)\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*'))\s*)?\]|:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\(\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*)?\)))*|(?:#-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\.-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\[\s*-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*(?:(?:=|~=|\|=)\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*'))\s*)?\]|:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\(\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*)?\)))+)/i,
        style: "webkit-css-selector",
        preLexState: this.LexState.Initial
    }, {
        name: "startRulesetBlockAction",
        pattern: startBlockPattern,
        preLexState: this.LexState.Initial,
        postLexState: this.LexState.DeclarationProperty
    }, {
        name: "propertyAction",
        pattern: identPattern,
        style: "webkit-css-property",
        keywords: propertyKeywords,
        preLexState: this.LexState.DeclarationProperty,
    }, {
        name: "declarationColonAction",
        pattern: /^:/i,
        preLexState: this.LexState.DeclarationProperty,
        postLexState: this.LexState.DeclarationValue
    }, {
        name: "colorAction",
        pattern: /^(?:#(?:[\da-f]{6}|[\da-f]{3})|rgba\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|hsla\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|rgb\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|hsl\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\))/i,
        style: "webkit-css-color",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "numvalueAction",
        pattern: /^(?:-?(?:\d+|\d*\.\d+)(?:em|rem|__qem|ex|px|cm|mm|in|pt|pc|deg|rad|grad|turn|ms|s|Hz|kHz|%)?)/i,
        style: "webkit-css-number",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "urlAction",
        pattern: urlPattern,
        style: "webkit-css-url",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "stringAction",
        pattern: stringPattern,
        style: "webkit-css-string",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "importantAction",
        pattern: /^!\s*important/i,
        style: "webkit-css-important",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "valueIdentAction",
        pattern: identPattern,
        keywords: valueKeywords,
        style: "webkit-css-keyword",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "declarationSemicolonAction",
        pattern: /^;/i,
        preLexState: this.LexState.DeclarationValue,
        postLexState: this.LexState.DeclarationProperty
    }, {
        name: "endRulesetBlockAction",
        pattern: endBlockPattern,
        preLexState: this.LexState.DeclarationProperty,
        postLexState: this.LexState.Initial
    }, {
        name: "atMediaAction",
        pattern: /^@media/i,
        style: "webkit-css-at-rule",
        preLexState: this.LexState.Initial,
        postLexState: this.LexState.AtMedia
    }, {
        name: "startAtMediaBlockAction",
        pattern: startBlockPattern,
        preLexState: this.LexState.AtMedia,
        postLexState: this.LexState.Initial
    }, {
        name: "atKeyframesAction",
        pattern: /^@-webkit-keyframes/i,
        style: "webkit-css-at-rule",
        preLexState: this.LexState.Initial,
        postLexState: this.LexState.AtKeyframes
    }, {
        name: "startAtKeyframesBlockAction",
        pattern: startBlockPattern,
        preLexState: this.LexState.AtKeyframes,
        postLexState: this.LexState.Initial
    }, {
        name: "atRuleAction",
        style: "webkit-css-at-rule",
        pattern: /^@-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*/i,
        preLexState: this.LexState.Initial,
        postLexState: this.LexState.AtRule
    }, {
        name: "endAtRuleAction",
        pattern: /^;/i,
        preLexState: this.LexState.AtRule,
        postLexState: this.LexState.Initial
    }, {
        name: "urlAction",
        pattern: urlPattern,
        style: "webkit-css-url",
        preLexState: this.LexState.AtRule
    }, {
        name: "stringAction",
        pattern: stringPattern,
        style: "webkit-css-string",
        preLexState: this.LexState.AtRule
    }, {
        name: "stringAction",
        pattern: stringPattern,
        style: "webkit-css-string",
        preLexState: this.LexState.AtKeyframes
    }, {
        name: "atRuleIdentAction",
        pattern: identPattern,
        keywords: mediaTypes,
        style: "webkit-css-keyword",
        preLexState: this.LexState.AtRule
    }, {
        name: "atRuleIdentAction",
        pattern: identPattern,
        keywords: mediaTypes,
        style: "webkit-css-keyword",
        preLexState: this.LexState.AtMedia,
    }, {
        name: "startAtRuleBlockAction",
        pattern: startBlockPattern,
        preLexState: this.LexState.AtRule,
        postLexState: this.LexState.DeclarationProperty
    }];
}

WebInspector.CSSSourceSyntaxHighlighter.prototype.__proto__ = WebInspector.SourceSyntaxHighlighter.prototype;

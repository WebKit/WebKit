description("Test the computed style of the -webkit-filter property.");

// These have to be global for the test helpers to see them.
var stylesheet, filterStyle, subRule;
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
stylesheet = styleElement.sheet;

function testComputedFilterRule(description, rule, expectedLength, expectedTypes, expectedTexts)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet.insertRule("body { -webkit-filter: " + rule + "; }", 0);

    filterStyle = window.getComputedStyle(document.body).getPropertyCSSValue('-webkit-filter');
    shouldBe("filterStyle.length", "" + expectedLength);
    for (var i = 0; i < expectedLength; i++) {
        subRule = filterStyle[i];
        shouldBe("subRule.operationType", expectedTypes[i]);
        shouldBe("subRule.cssText", "'" + expectedTexts[i] + "'");
    }
    stylesheet.deleteRule(0);
}

testComputedFilterRule("Basic reference",
                       "url('#a')", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_REFERENCE"],
                       ["url(#a)"]);

testComputedFilterRule("Bare unquoted reference converting to quoted form",
                       "url(#a)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_REFERENCE"],
                       ["url(#a)"]);

testComputedFilterRule("Multiple references",
                       "url('#a') url('#b')", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_REFERENCE", "WebKitCSSFilterValue.CSS_FILTER_REFERENCE"],
                       ["url(#a)", "url(#b)"]);

testComputedFilterRule("Reference as 2nd value",
                       "grayscale(1) url('#a')", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE", "WebKitCSSFilterValue.CSS_FILTER_REFERENCE"],
                       ["grayscale(1)", "url(#a)"]);

testComputedFilterRule("Integer value",
                       "grayscale(1)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["grayscale(1)"]);

testComputedFilterRule("Float value converts to integer",
                       "grayscale(1.0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["grayscale(1)"]);

testComputedFilterRule("Zero value",
                       "grayscale(0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["grayscale(0)"]);

testComputedFilterRule("No values",
                       "grayscale()", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["grayscale(1)"]);

testComputedFilterRule("Multiple values",
                       "grayscale(0.5) grayscale(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["grayscale(0.5)", "grayscale(0.25)"]);

testComputedFilterRule("Integer value",
                       "sepia(1)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
                       ["sepia(1)"]);

testComputedFilterRule("Float value converts to integer",
                       "sepia(1.0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
                       ["sepia(1)"]);

testComputedFilterRule("Zero value",
                       "sepia(0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
                       ["sepia(0)"]);

testComputedFilterRule("No values",
                       "sepia()", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
                       ["sepia(1)"]);

testComputedFilterRule("Multiple values",
                       "sepia(0.5) sepia(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_SEPIA", "WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
                       ["sepia(0.5)", "sepia(0.25)"]);

testComputedFilterRule("Rule combinations",
                       "sepia(0.5) grayscale(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_SEPIA", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["sepia(0.5)", "grayscale(0.25)"]);

testComputedFilterRule("Integer value",
                       "saturate(1)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
                       ["saturate(1)"]);

testComputedFilterRule("Float value converts to integer",
                       "saturate(1.0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
                       ["saturate(1)"]);

testComputedFilterRule("Zero value",
                       "saturate(0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
                       ["saturate(0)"]);

testComputedFilterRule("No values",
                       "saturate()", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
                       ["saturate(1)"]);

testComputedFilterRule("Multiple values",
                       "saturate(0.5) saturate(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_SATURATE", "WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
                       ["saturate(0.5)", "saturate(0.25)"]);

testComputedFilterRule("Rule combinations",
                       "saturate(0.5) grayscale(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_SATURATE", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["saturate(0.5)", "grayscale(0.25)"]);

testComputedFilterRule("Degrees value as integer",
                       "hue-rotate(10deg)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
                       ["hue-rotate(10deg)"]);

testComputedFilterRule("Degrees float value converts to integer",
                       "hue-rotate(10.0deg)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
                       ["hue-rotate(10deg)"]);

testComputedFilterRule("Radians value",
                       "hue-rotate(10rad)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
                       ["hue-rotate(572.9577951308232deg)"]);

testComputedFilterRule("Gradians value",
                       "hue-rotate(10grad)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
                       ["hue-rotate(9deg)"]);

testComputedFilterRule("Turns value",
                       "hue-rotate(0.5turn)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
                       ["hue-rotate(180deg)"]);

testComputedFilterRule("Zero value",
                       "hue-rotate(0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
                       ["hue-rotate(0deg)"]);

testComputedFilterRule("No values",
                       "hue-rotate()", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
                       ["hue-rotate(0deg)"]);

testComputedFilterRule("Rule combinations",
                       "hue-rotate(10deg) grayscale(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["hue-rotate(10deg)", "grayscale(0.25)"]);

testComputedFilterRule("Integer value",
                       "invert(1)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
                       ["invert(1)"]);

testComputedFilterRule("Float value converts to integer",
                       "invert(1.0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
                       ["invert(1)"]);

testComputedFilterRule("Zero value",
                       "invert(0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
                       ["invert(0)"]);

testComputedFilterRule("No values",
                       "invert()", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
                       ["invert(1)"]);

testComputedFilterRule("Multiple values",
                       "invert(0.5) invert(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_INVERT", "WebKitCSSFilterValue.CSS_FILTER_INVERT"],
                       ["invert(0.5)", "invert(0.25)"]);

testComputedFilterRule("Rule combinations",
                       "invert(0.5) grayscale(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_INVERT", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["invert(0.5)", "grayscale(0.25)"]);

testComputedFilterRule("Integer value",
                       "opacity(1)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
                       ["opacity(1)"]);

testComputedFilterRule("Float value converts to integer",
                       "opacity(1.0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
                       ["opacity(1)"]);

testComputedFilterRule("Zero value",
                       "opacity(0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
                       ["opacity(0)"]);

testComputedFilterRule("No values",
                       "opacity()", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
                       ["opacity(1)"]);

testComputedFilterRule("Multiple values",
                       "opacity(0.5) opacity(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_OPACITY", "WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
                       ["opacity(0.5)", "opacity(0.25)"]);

testComputedFilterRule("Rule combinations",
                       "opacity(0.5) grayscale(0.25)", 2,
                       ["WebKitCSSFilterValue.CSS_FILTER_OPACITY", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                       ["opacity(0.5)", "grayscale(0.25)"]);

testComputedFilterRule("Integer value",
                      "brightness(1)", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
                      ["brightness(1)"]);

testComputedFilterRule("Float value converts to integer",
                      "brightness(1.0)", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
                      ["brightness(1)"]);

testComputedFilterRule("Zero value",
                      "brightness(0)", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
                      ["brightness(0)"]);

testComputedFilterRule("No values",
                      "brightness()", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
                      ["brightness(0)"]);

testComputedFilterRule("Multiple values",
                      "brightness(0.5) brightness(0.25)", 2,
                      ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS", "WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
                      ["brightness(0.5)", "brightness(0.25)"]);

testComputedFilterRule("Rule combinations",
                      "brightness(0.5) grayscale(0.25)", 2,
                      ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                      ["brightness(0.5)", "grayscale(0.25)"]);

testComputedFilterRule("Integer value",
                      "contrast(1)", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
                      ["contrast(1)"]);

testComputedFilterRule("Value greater than 1",
                      "contrast(2)", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
                      ["contrast(2)"]);

testComputedFilterRule("Float value converts to integer",
                      "contrast(1.0)", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
                      ["contrast(1)"]);

testComputedFilterRule("Zero value",
                      "contrast(0)", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
                      ["contrast(0)"]);

testComputedFilterRule("No values",
                      "contrast()", 1,
                      ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
                      ["contrast(1)"]);

testComputedFilterRule("Multiple values",
                      "contrast(0.5) contrast(0.25)", 2,
                      ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST", "WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
                      ["contrast(0.5)", "contrast(0.25)"]);

testComputedFilterRule("Rule combinations",
                      "contrast(0.5) grayscale(0.25)", 2,
                      ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
                      ["contrast(0.5)", "grayscale(0.25)"]);

testComputedFilterRule("One zero to px",
                       "blur(0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_BLUR"],
                       ["blur(0px)"]);

testComputedFilterRule("One length",
                       "blur(2em)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_BLUR"],
                       ["blur(32px)"]);

testComputedFilterRule("One length",
                       "blur(5px)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_BLUR"],
                       ["blur(5px)"]);

testComputedFilterRule("No values",
                       "blur()", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_BLUR"],
                       ["blur(0px)"]);

testComputedFilterRule("Color then three values",
                       "drop-shadow(red 1px 2px 3px)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
                       ["drop-shadow(rgb(255, 0, 0) 1px 2px 3px)"]);

testComputedFilterRule("Three values then color",
                       "drop-shadow(1px 2px 3px red)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
                       ["drop-shadow(rgb(255, 0, 0) 1px 2px 3px)"]);

testComputedFilterRule("Color then three values with zero length",
                       "drop-shadow(#abc 0 0 0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
                       ["drop-shadow(rgb(170, 187, 204) 0px 0px 0px)"]);

testComputedFilterRule("Three values with zero length",
                       "drop-shadow(0 0 0)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
                       ["drop-shadow(rgba(0, 0, 0, 0) 0px 0px 0px)"]);

testComputedFilterRule("Two values no color",
                       "drop-shadow(1px 2px)", 1,
                       ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
                       ["drop-shadow(rgba(0, 0, 0, 0) 1px 2px 0px)"]);

testComputedFilterRule("Multiple operations",
                       "grayscale(0.5) sepia(0.25) saturate(0.75) hue-rotate(35deg) invert(0.2) opacity(0.9) blur(5px)", 7,
                       [
                           "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE",
                           "WebKitCSSFilterValue.CSS_FILTER_SEPIA",
                           "WebKitCSSFilterValue.CSS_FILTER_SATURATE",
                           "WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE",
                           "WebKitCSSFilterValue.CSS_FILTER_INVERT",
                           "WebKitCSSFilterValue.CSS_FILTER_OPACITY",
                           "WebKitCSSFilterValue.CSS_FILTER_BLUR",
               ],
                       [
                           "grayscale(0.5)",
                           "sepia(0.25)",
                           "saturate(0.75)",
                           "hue-rotate(35deg)",
                           "invert(0.2)",
                           "opacity(0.9)",
                           "blur(5px)"
               ]);

testComputedFilterRule("Percentage values",
                      "grayscale(50%) sepia(25%) saturate(75%) invert(20%) opacity(90%) brightness(60%) contrast(30%)", 7,
                      [
                          "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE",
                          "WebKitCSSFilterValue.CSS_FILTER_SEPIA",
                          "WebKitCSSFilterValue.CSS_FILTER_SATURATE",
                          "WebKitCSSFilterValue.CSS_FILTER_INVERT",
                          "WebKitCSSFilterValue.CSS_FILTER_OPACITY",
                          "WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS",
                          "WebKitCSSFilterValue.CSS_FILTER_CONTRAST"
              ],
                      [
                          "grayscale(0.5)",
                          "sepia(0.25)",
                          "saturate(0.75)",
                          "invert(0.2)",
                          "opacity(0.9)",
                          "brightness(0.6)",
                          "contrast(0.3)"
              ]);

successfullyParsed = true;

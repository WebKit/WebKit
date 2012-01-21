description("Test the parsing of the -webkit-filter property.");

function jsWrapperClass(node)
{
    if (!node)
        return "[null]";
    var string = Object.prototype.toString.apply(node);
    return string.substr(8, string.length - 9);
}

function shouldBeType(expression, className, prototypeName, constructorName)
{
    if (!prototypeName)
        prototypeName = className + "Prototype";
    if (!constructorName)
        constructorName = className + "Constructor";
    shouldBe("jsWrapperClass(" + expression + ")", "'" + className + "'");
    shouldBe("jsWrapperClass(" + expression + ".__proto__)", "'" + prototypeName + "'");
    shouldBe("jsWrapperClass(" + expression + ".constructor)", "'" + constructorName + "'");
}

// These have to be global for the test helpers to see them.
var stylesheet, cssRule, declaration, filterRule, subRule;

function testFilterRule(description, rule, expectedLength, expectedValue, expectedTypes, expectedTexts)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet = document.styleSheets.item(0);
    stylesheet.insertRule("body { -webkit-filter: " + rule + "; }", 0);
    cssRule = stylesheet.cssRules.item(0);
  
    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", "1");
    shouldBe("declaration.getPropertyValue('-webkit-filter')", "'" + expectedValue + "'");

    filterRule = declaration.getPropertyCSSValue('-webkit-filter');
    shouldBeType("filterRule", "CSSValueList");
  
    shouldBe("filterRule.length", "" + expectedLength); // shouldBe expects string arguments
  
    if (filterRule) {
        for (var i = 0; i < expectedLength; i++) {
            subRule = filterRule[i];
            shouldBe("subRule.operationType", expectedTypes[i]);
            shouldBe("subRule.cssText", "'" + expectedTexts[i] + "'");
        }
    }
}

testFilterRule("Basic reference",
               "url('#a')", 1, "url(\\'#a\\')",
               ["WebKitCSSFilterValue.CSS_FILTER_REFERENCE"],
               ["url(\\'#a\\')"]);

testFilterRule("Bare unquoted reference converting to quoted form",
               "url(#a)", 1, "url(\\'#a\\')",
               ["WebKitCSSFilterValue.CSS_FILTER_REFERENCE"],
               ["url(\\'#a\\')"]);

testFilterRule("Multiple references",
               "url('#a') url('#b')", 2, "url(\\'#a\\') url(\\'#b\\')",
               ["WebKitCSSFilterValue.CSS_FILTER_REFERENCE", "WebKitCSSFilterValue.CSS_FILTER_REFERENCE"],
               ["url(\\'#a\\')", "url(\\'#b\\')"]);

testFilterRule("Reference as 2nd value",
               "grayscale(1) url('#a')", 2, "grayscale(1) url(\\'#a\\')",
               ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE", "WebKitCSSFilterValue.CSS_FILTER_REFERENCE"],
               ["grayscale(1)", "url(\\'#a\\')"]);

testFilterRule("Integer value",
               "grayscale(1)", 1, "grayscale(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["grayscale(1)"]);

testFilterRule("Percentage value",
               "grayscale(50%)", 1, "grayscale(50%)",
               ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["grayscale(50%)"]);

testFilterRule("Float value converts to integer",
               "grayscale(1.0)", 1, "grayscale(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["grayscale(1)"]);

testFilterRule("Zero value",
               "grayscale(0)", 1, "grayscale(0)",
               ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["grayscale(0)"]);

testFilterRule("No values",
               "grayscale()", 1, "grayscale()",
               ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["grayscale()"]);

testFilterRule("Multiple values",
               "grayscale(0.5) grayscale(0.25)", 2, "grayscale(0.5) grayscale(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["grayscale(0.5)", "grayscale(0.25)"]);

testFilterRule("Integer value",
               "sepia(1)", 1, "sepia(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
               ["sepia(1)"]);

testFilterRule("Percentage value",
               "sepia(50%)", 1, "sepia(50%)",
               ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
               ["sepia(50%)"]);

testFilterRule("Float value converts to integer",
               "sepia(1.0)", 1, "sepia(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
               ["sepia(1)"]);

testFilterRule("Zero value",
               "sepia(0)", 1, "sepia(0)",
               ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
               ["sepia(0)"]);

testFilterRule("No values",
               "sepia()", 1, "sepia()",
               ["WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
               ["sepia()"]);

testFilterRule("Multiple values",
               "sepia(0.5) sepia(0.25)", 2, "sepia(0.5) sepia(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_SEPIA", "WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
               ["sepia(0.5)", "sepia(0.25)"]);

testFilterRule("Rule combinations",
               "sepia(0.5) grayscale(0.25)", 2, "sepia(0.5) grayscale(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_SEPIA", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["sepia(0.5)", "grayscale(0.25)"]);

testFilterRule("Integer value",
               "saturate(1)", 1, "saturate(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
               ["saturate(1)"]);

testFilterRule("Percentage value",
               "saturate(50%)", 1, "saturate(50%)",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
               ["saturate(50%)"]);

testFilterRule("Percentage value > 1",
               "saturate(250%)", 1, "saturate(250%)",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
               ["saturate(250%)"]);

testFilterRule("Float value converts to integer",
               "saturate(1.0)", 1, "saturate(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
               ["saturate(1)"]);

testFilterRule("Input value > 1",
               "saturate(5.5)", 1, "saturate(5.5)",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
               ["saturate(5.5)"]);

testFilterRule("Zero value",
               "saturate(0)", 1, "saturate(0)",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
               ["saturate(0)"]);

testFilterRule("No values",
               "saturate()", 1, "saturate()",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
               ["saturate()"]);

testFilterRule("Multiple values",
               "saturate(0.5) saturate(0.25)", 2, "saturate(0.5) saturate(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE", "WebKitCSSFilterValue.CSS_FILTER_SATURATE"],
               ["saturate(0.5)", "saturate(0.25)"]);

testFilterRule("Rule combinations",
               "saturate(0.5) grayscale(0.25)", 2, "saturate(0.5) grayscale(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_SATURATE", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["saturate(0.5)", "grayscale(0.25)"]);

testFilterRule("Degrees value as integer",
               "hue-rotate(10deg)", 1, "hue-rotate(10deg)",
               ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
               ["hue-rotate(10deg)"]);

testFilterRule("Degrees float value converts to integer",
               "hue-rotate(10.0deg)", 1, "hue-rotate(10deg)",
               ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
               ["hue-rotate(10deg)"]);

testFilterRule("Radians value",
               "hue-rotate(10rad)", 1, "hue-rotate(10rad)",
               ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
               ["hue-rotate(10rad)"]);

testFilterRule("Gradians value",
               "hue-rotate(10grad)", 1, "hue-rotate(10grad)",
               ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
               ["hue-rotate(10grad)"]);

testFilterRule("Turns value",
               "hue-rotate(0.5turn)", 1, "hue-rotate(0.5turn)",
               ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
               ["hue-rotate(0.5turn)"]);

testFilterRule("Zero value",
               "hue-rotate(0)", 1, "hue-rotate(0deg)",
               ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
               ["hue-rotate(0deg)"]);

testFilterRule("No values",
               "hue-rotate()", 1, "hue-rotate()",
               ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE"],
               ["hue-rotate()"]);

testFilterRule("Rule combinations",
               "hue-rotate(10deg) grayscale(0.25)", 2, "hue-rotate(10deg) grayscale(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["hue-rotate(10deg)", "grayscale(0.25)"]);

testFilterRule("Integer value",
               "invert(1)", 1, "invert(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
               ["invert(1)"]);

testFilterRule("Percentage value",
               "invert(50%)", 1, "invert(50%)",
               ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
               ["invert(50%)"]);

testFilterRule("Float value converts to integer",
               "invert(1.0)", 1, "invert(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
               ["invert(1)"]);

testFilterRule("Zero value",
               "invert(0)", 1, "invert(0)",
               ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
               ["invert(0)"]);

testFilterRule("No values",
               "invert()", 1, "invert()",
               ["WebKitCSSFilterValue.CSS_FILTER_INVERT"],
               ["invert()"]);

testFilterRule("Multiple values",
               "invert(0.5) invert(0.25)", 2, "invert(0.5) invert(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_INVERT", "WebKitCSSFilterValue.CSS_FILTER_INVERT"],
               ["invert(0.5)", "invert(0.25)"]);

testFilterRule("Rule combinations",
               "invert(0.5) grayscale(0.25)", 2, "invert(0.5) grayscale(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_INVERT", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["invert(0.5)", "grayscale(0.25)"]);

testFilterRule("Integer value",
               "opacity(1)", 1, "opacity(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
               ["opacity(1)"]);

testFilterRule("Percentage value",
               "opacity(50%)", 1, "opacity(50%)",
               ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
               ["opacity(50%)"]);

testFilterRule("Float value converts to integer",
               "opacity(1.0)", 1, "opacity(1)",
               ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
               ["opacity(1)"]);

testFilterRule("Zero value",
               "opacity(0)", 1, "opacity(0)",
               ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
               ["opacity(0)"]);

testFilterRule("No values",
               "opacity()", 1, "opacity()",
               ["WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
               ["opacity()"]);

testFilterRule("Multiple values",
               "opacity(0.5) opacity(0.25)", 2, "opacity(0.5) opacity(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_OPACITY", "WebKitCSSFilterValue.CSS_FILTER_OPACITY"],
               ["opacity(0.5)", "opacity(0.25)"]);

testFilterRule("Rule combinations",
               "opacity(0.5) grayscale(0.25)", 2, "opacity(0.5) grayscale(0.25)",
               ["WebKitCSSFilterValue.CSS_FILTER_OPACITY", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
               ["opacity(0.5)", "grayscale(0.25)"]);

testFilterRule("Integer value",
              "brightness(1)", 1, "brightness(1)",
              ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
              ["brightness(1)"]);

testFilterRule("Percentage value",
              "brightness(50%)", 1, "brightness(50%)",
              ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
              ["brightness(50%)"]);

testFilterRule("Float value converts to integer",
              "brightness(1.0)", 1, "brightness(1)",
              ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
              ["brightness(1)"]);

testFilterRule("Zero value",
              "brightness(0)", 1, "brightness(0)",
              ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
              ["brightness(0)"]);

testFilterRule("No values",
              "brightness()", 1, "brightness()",
              ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
              ["brightness()"]);

testFilterRule("Multiple values",
              "brightness(0.5) brightness(0.25)", 2, "brightness(0.5) brightness(0.25)",
              ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS", "WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
              ["brightness(0.5)", "brightness(0.25)"]);

testFilterRule("Rule combinations",
              "brightness(0.5) grayscale(0.25)", 2, "brightness(0.5) grayscale(0.25)",
              ["WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
              ["brightness(0.5)", "grayscale(0.25)"]);

testFilterRule("Rule combinations",
              "grayscale(0.25) brightness(0.5)", 2, "grayscale(0.25) brightness(0.5)",
              [ "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE", "WebKitCSSFilterValue.CSS_FILTER_BRIGHTNESS"],
              ["grayscale(0.25)", "brightness(0.5)"]);

testFilterRule("Integer value",
              "contrast(1)", 1, "contrast(1)",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["contrast(1)"]);

testFilterRule("Percentage value",
              "contrast(50%)", 1, "contrast(50%)",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["contrast(50%)"]);

testFilterRule("Percentage value > 1",
              "contrast(250%)", 1, "contrast(250%)",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["contrast(250%)"]);

testFilterRule("Float value converts to integer",
              "contrast(1.0)", 1, "contrast(1)",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["contrast(1)"]);

testFilterRule("Zero value",
              "contrast(0)", 1, "contrast(0)",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["contrast(0)"]);

testFilterRule("No values",
              "contrast()", 1, "contrast()",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["contrast()"]);

testFilterRule("Value greater than one",
              "contrast(2)", 1, "contrast(2)",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["contrast(2)"]);

testFilterRule("Multiple values",
              "contrast(0.5) contrast(0.25)", 2, "contrast(0.5) contrast(0.25)",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST", "WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["contrast(0.5)", "contrast(0.25)"]);

testFilterRule("Rule combinations",
              "contrast(0.5) grayscale(0.25)", 2, "contrast(0.5) grayscale(0.25)",
              ["WebKitCSSFilterValue.CSS_FILTER_CONTRAST", "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE"],
              ["contrast(0.5)", "grayscale(0.25)"]);

testFilterRule("Rule combinations",
              "grayscale(0.25) contrast(0.5)", 2, "grayscale(0.25) contrast(0.5)",
              [ "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE", "WebKitCSSFilterValue.CSS_FILTER_CONTRAST"],
              ["grayscale(0.25)", "contrast(0.5)"]);

testFilterRule("One zero to px",
               "blur(0)", 1, "blur(0px)",
               ["WebKitCSSFilterValue.CSS_FILTER_BLUR"],
               ["blur(0px)"]);

testFilterRule("One length",
               "blur(10px)", 1, "blur(10px)",
               ["WebKitCSSFilterValue.CSS_FILTER_BLUR"],
               ["blur(10px)"]);

testFilterRule("No values",
               "blur()", 1, "blur()",
               ["WebKitCSSFilterValue.CSS_FILTER_BLUR"],
               ["blur()"]);

testFilterRule("Color then three values",
              "drop-shadow(red 1px 2px 3px)", 1, "drop-shadow(red 1px 2px 3px)",
              ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
              ["drop-shadow(red 1px 2px 3px)"]);

testFilterRule("Three values then color",
              "drop-shadow(1px 2px 3px red)", 1, "drop-shadow(red 1px 2px 3px)",
              ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
              ["drop-shadow(red 1px 2px 3px)"]);

testFilterRule("Color then three values with zero length",
              "drop-shadow(#abc 0 0 0)", 1, "drop-shadow(rgb(170, 187, 204) 0px 0px 0px)",
              ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
              ["drop-shadow(rgb(170, 187, 204) 0px 0px 0px)"]);

testFilterRule("Three values with zero length",
              "drop-shadow(0 0 0)", 1, "drop-shadow(0px 0px 0px)",
              ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
              ["drop-shadow(0px 0px 0px)"]);

testFilterRule("Two values no color",
              "drop-shadow(1px 2px)", 1, "drop-shadow(1px 2px)",
              ["WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"],
              ["drop-shadow(1px 2px)"]);

testFilterRule("Multiple operations",
               "grayscale(0.5) sepia(0.25) saturate(0.75) hue-rotate(35deg) invert(0.2) opacity(0.9) blur(5px) drop-shadow(green 1px 2px 3px)", 8,
               "grayscale(0.5) sepia(0.25) saturate(0.75) hue-rotate(35deg) invert(0.2) opacity(0.9) blur(5px) drop-shadow(green 1px 2px 3px)",
               [
                   "WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE",
                   "WebKitCSSFilterValue.CSS_FILTER_SEPIA",
                   "WebKitCSSFilterValue.CSS_FILTER_SATURATE",
                   "WebKitCSSFilterValue.CSS_FILTER_HUE_ROTATE",
                   "WebKitCSSFilterValue.CSS_FILTER_INVERT",
                   "WebKitCSSFilterValue.CSS_FILTER_OPACITY",
                   "WebKitCSSFilterValue.CSS_FILTER_BLUR",
                   "WebKitCSSFilterValue.CSS_FILTER_DROP_SHADOW"
               ],
               [
                   "grayscale(0.5)",
                   "sepia(0.25)",
                   "saturate(0.75)",
                   "hue-rotate(35deg)",
                   "invert(0.2)",
                   "opacity(0.9)",
                   "blur(5px)",
                   "drop-shadow(green 1px 2px 3px)"
               ]);

successfullyParsed = true;

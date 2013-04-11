// Requires custom-filter-parsing-common.js.

description("Test mix property parsing in the @-webkit-filter at-rule.");

// These have to be global for the test helpers to see them.
var filterAtRule, styleDeclaration, mixPropertyValue, subValue;

function testMixProperty(description, propertyValue, expectedValue, expectedTexts)
{
    var mixProperty = "mix: " + propertyValue + ";"
    debug("\n" + description + "\n" + mixProperty);

    stylesheet.insertRule("@-webkit-filter my-filter { " + mixProperty + " }", 0);
    filterAtRule = stylesheet.cssRules.item(0);
    shouldBe("filterAtRule.type", "CSSRule.WEBKIT_FILTER_RULE");

    styleDeclaration = filterAtRule.style;
    shouldBe("styleDeclaration.length", "1");
    shouldBe("removeBaseURL(styleDeclaration.getPropertyValue('mix'))", "\"" + expectedValue + "\"");

    mixPropertyValue = styleDeclaration.getPropertyCSSValue('mix');
    shouldHaveConstructor("mixPropertyValue", "CSSValueList");

    shouldBe("mixPropertyValue.length", expectedTexts.length.toString()); // shouldBe expects string arguments
  
    if (mixPropertyValue) {
        for (var i = 0; i < expectedTexts.length; i++) {
            subValue = mixPropertyValue[i];
            shouldBe("subValue.cssValueType", "CSSValue.CSS_PRIMITIVE_VALUE");
            shouldBe("removeBaseURL(subValue.cssText)", "\"" + expectedTexts[i] + "\"");
        }
    }
}

heading("Test blend mode keywords.");
testMixProperty("Test multiply.", "multiply", "multiply", ["multiply"]);
testMixProperty("Test screen.", "screen", "screen", ["screen"]);
testMixProperty("Test overlay.", "overlay", "overlay", ["overlay"]);
testMixProperty("Test darken.", "darken", "darken", ["darken"]);
testMixProperty("Test lighten.", "lighten", "lighten", ["lighten"]);
testMixProperty("Test color-dodge.", "color-dodge", "color-dodge", ["color-dodge"]);
testMixProperty("Test color-burn.", "color-burn", "color-burn", ["color-burn"]);
testMixProperty("Test hard-light.", "hard-light", "hard-light", ["hard-light"]);
testMixProperty("Test soft-light.", "soft-light", "soft-light", ["soft-light"]);
testMixProperty("Test difference.", "difference", "difference", ["difference"]);
testMixProperty("Test exclusion.", "exclusion", "exclusion", ["exclusion"]);
testMixProperty("Test hue.", "hue", "hue", ["hue"]);
testMixProperty("Test saturation.", "saturation", "saturation", ["saturation"]);
testMixProperty("Test color.", "color", "color", ["color"]);
testMixProperty("Test luminosity.", "luminosity", "luminosity", ["luminosity"]);

heading("Test composite keywords.");
testMixProperty("Test clear.", "clear", "clear", ["clear"]);
testMixProperty("Test copy.", "copy", "copy", ["copy"]);
testMixProperty("Test source-over.", "source-over", "source-over", ["source-over"]);
testMixProperty("Test source-in.", "source-in", "source-in", ["source-in"]);
testMixProperty("Test source-out.", "source-out", "source-out", ["source-out"]);
testMixProperty("Test source-atop.", "source-atop", "source-atop", ["source-atop"]);
testMixProperty("Test destination-over.", "destination-over", "destination-over", ["destination-over"]);
testMixProperty("Test destination-in.", "destination-in", "destination-in", ["destination-in"]);
testMixProperty("Test destination-out.", "destination-out", "destination-out", ["destination-out"]);
testMixProperty("Test destination-atop.", "destination-atop", "destination-atop", ["destination-atop"]);
testMixProperty("Test xor.", "xor", "xor", ["xor"]);

testMixProperty("Blend mode and composite together.",
    "screen source-over",
    "screen source-over",
    ["screen", "source-over"]);

testMixProperty("Composite and blend mode together.",
    "source-over screen",
    "source-over screen",
    ["source-over", "screen"]);

heading("Capitalization tests.");
testMixProperty("All tokens capitalized.",
    "SCREEN SOURCE-OVER",
    "screen source-over",
    ["screen", "source-over"]);

heading("Whitespace tests.");
testMixProperty("Extra leading, middle, and trailing whitespace. ",
    "   screen   source-over   ",
    "screen source-over",
    ["screen", "source-over"]);

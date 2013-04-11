// Requires custom-filter-parsing-common.js.

description("Test mix property parsing in the @-webkit-filter at-rule.");

// These have to be global for the test helpers to see them.
var filterAtRule, styleDeclaration;

function testInvalidMixProperty(description, propertyValue)
{
    var mixProperty = "mix: " + propertyValue + ";";
    debug("\n" + description + "\n" + mixProperty);

    stylesheet.insertRule("@-webkit-filter my-filter { " + mixProperty + " }", 0);

    filterAtRule = stylesheet.cssRules.item(0);
    shouldBe("filterAtRule.type", "CSSRule.WEBKIT_FILTER_RULE");

    styleDeclaration = filterAtRule.style;
    shouldBe("styleDeclaration.length", "0");
    shouldBe("styleDeclaration.getPropertyValue('mix')", "null");
}

heading("Value test.");
testInvalidMixProperty("No property value.", "");
testInvalidMixProperty("Three values.", "screen source-over screen");
testInvalidMixProperty("Three values.", "screen source-over source-over");
testInvalidMixProperty("Two blend modes.", "screen screen");
testInvalidMixProperty("Two composite operators.", "source-over source-over");
testInvalidMixProperty("Two blend modes.", "screen screen");
testInvalidMixProperty("Comma between values.", "screen, source-over");

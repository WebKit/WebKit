description("Test that the custom() function of the -webkit-filter property is not parsed when CSS Custom Filter is disabled.");

if (window.layoutTestController)
    window.layoutTestController.overridePreference("WebKitCSSCustomFilterEnabled", false);

// These have to be global for the test helpers to see them.
var stylesheet, cssRule, declaration;

function testInvalidFilterRule(description, rule)
{
    debug(description + " : " + rule);

    stylesheet = document.styleSheets.item(0);
    stylesheet.insertRule("body { -webkit-filter: " + rule + "; }", 0);
    cssRule = stylesheet.cssRules.item(0);
  
    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", "0");
    shouldBe("declaration.getPropertyValue('-webkit-filter')", "null");
}

testInvalidFilterRule("Custom with vertex shader", "custom(url(vertex.shader))");
testInvalidFilterRule("Multiple with fragment shader", "grayscale() custom(none url(fragment.shader)) sepia()", "grayscale() custom(none url(fragment.shader)) sepia()");
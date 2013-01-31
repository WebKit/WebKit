description("Test the parsing of the custom() function of the -webkit-filter property.");

// These have to be global for the test helpers to see them.
var styleRule, styleDeclaration, filterPropertyValue, subValue;

function testFilterProperty(description, propertyValue, expectedValue, expectedTypes, expectedTexts)
{
    debug("\n" + description + "\n" + propertyValue);

    stylesheet.insertRule("body { -webkit-filter: " + propertyValue + "; }", 0);
    styleRule = stylesheet.cssRules.item(0);
    shouldBe("styleRule.type", "CSSRule.STYLE_RULE");

    styleDeclaration = styleRule.style;
    shouldBe("styleDeclaration.length", "1");
    shouldBe("styleDeclaration.getPropertyValue('-webkit-filter')", "'" + expectedValue + "'");

    filterPropertyValue = styleDeclaration.getPropertyCSSValue('-webkit-filter');
    shouldHaveConstructor("filterPropertyValue", "CSSValueList");

    if (!expectedTypes) {
        expectedTypes = ["WebKitCSSFilterValue.CSS_FILTER_CUSTOM"];
        expectedTexts = [expectedValue];
    }
    
    shouldBe("filterPropertyValue.length", "" + expectedTypes.length); // shouldBe expects string arguments
  
    if (filterPropertyValue) {
        for (var i = 0; i < expectedTypes.length; i++) {
            subValue = filterPropertyValue[i];
            shouldBe("subValue.operationType", expectedTypes[i]);
            shouldBe("subValue.cssText", "'" + expectedTexts[i] + "'");
        }
    }
}

heading("Custom function tests.");
testFilterProperty("Custom function in CAPS.",
    "CUSTOM(my-filter)",
    "custom(my-filter)");

heading("Filter chain tests.")
testFilterProperty("Custom function in middle of filter chain.",
    "grayscale() custom(my-filter) sepia()", "grayscale() custom(my-filter) sepia()",
    ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE",
    "WebKitCSSFilterValue.CSS_FILTER_CUSTOM",
    "WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
    ["grayscale()",
    "custom(my-filter)",
    "sepia()"]);

heading("Filter name tests.");
testFilterProperty("Filter name only.",
    "custom(my-filter)",
    "custom(my-filter)");
testFilterProperty("Filter name as CSS 'none' keyword.",
    "custom(none)",
    "custom(none)");

heading("Number parameter tests.")
testFilterProperty("Integer parameters.",
    "custom(my-filter, n1 1, n2 2 3, n3 3 4 5, n4 4 5 6 7)",
    "custom(my-filter, n1 1, n2 2 3, n3 3 4 5, n4 4 5 6 7)");
testFilterProperty("Float parameters.",
    "custom(my-filter, n1 1.1, n2 2.2 3.3, n3 3.1 4.1 5.1, n4 4.1 5.2 6.3 7.4)",
    "custom(my-filter, n1 1.1, n2 2.2 3.3, n3 3.1 4.1 5.1, n4 4.1 5.2 6.3 7.4)");
testFilterProperty("Parameter name same as CSS keyword.",
    "custom(my-filter, background 0 1 0 1)",
    "custom(my-filter, background 0 1 0 1)");

heading("Transform parameter tests.")
testFilterProperty("Transform parameter with one transform function.",
    "custom(my-filter, t rotate(0deg))",
    "custom(my-filter, t rotate(0deg))");
testFilterProperty("Transform parameter with multiple transform functions.",
    "custom(my-filter, t rotate(0deg) perspective(0) scale(0, 0) translate(0px, 0px))",
    "custom(my-filter, t rotate(0deg) perspective(0) scale(0, 0) translate(0px, 0px))");
testFilterProperty("Mulitple transform parameters.",
    "custom(my-filter, t1 rotate(0deg), t2 rotate(0deg))",
    "custom(my-filter, t1 rotate(0deg), t2 rotate(0deg))");

heading("Array parameter tests.");
testFilterProperty("Array parameter with name 'array'.",
    "custom(my-filter, array array(1))",
    "custom(my-filter, array array(1))");
testFilterProperty("Array parameter with one positive integer.",
    "custom(my-filter, a array(1))",
    "custom(my-filter, a array(1))");
testFilterProperty("Array parameter with one negative float.",
    "custom(my-filter, a array(-1.01))",
    "custom(my-filter, a array(-1.01))");
testFilterProperty("Array parameter with multiple positive integers.",
    "custom(my-filter, a array(1, 2, 3, 4, 5))",
    "custom(my-filter, a array(1, 2, 3, 4, 5))");
testFilterProperty("Array parameter with multiple signed floats.",
    "custom(my-filter, a array(1, -2.2, 3.14, 0.4, 5))",
    "custom(my-filter, a array(1, -2.2, 3.14, 0.4, 5))");
testFilterProperty("Multiple array parameters.",
    "custom(my-filter, a1 array(1, -2.2, 3.14, 0.4, 5), a2 array(1, 2, 3))",
    "custom(my-filter, a1 array(1, -2.2, 3.14, 0.4, 5), a2 array(1, 2, 3))");

heading("Combined parameter tests.");
testFilterProperty("Number parameter, transform parameter, and array parameter.",
    "custom(my-filter, n 1, t rotate(0deg), a array(1))",
    "custom(my-filter, n 1, t rotate(0deg), a array(1))");

// Requires custom-filter-parsing-common.js.

description("Test geometry property parsing in the @-webkit-filter at-rule.");

// These have to be global for the test helpers to see them.
var filterAtRule, styleDeclaration, geometryPropertyValue, subValue;

function testGeometryProperty(description, propertyValue, expectedValue, expectedTexts)
{
    var geometryProperty = "geometry: " + propertyValue + ";"
    debug("\n" + description + "\n" + geometryProperty);

    stylesheet.insertRule("@-webkit-filter my-filter { " + geometryProperty + " }", 0);
    filterAtRule = stylesheet.cssRules.item(0);
    shouldBe("filterAtRule.type", "CSSRule.WEBKIT_FILTER_RULE");

    styleDeclaration = filterAtRule.style;
    shouldBe("styleDeclaration.length", "1");
    shouldBe("removeBaseURL(styleDeclaration.getPropertyValue('geometry'))", "\"" + expectedValue + "\"");

    geometryPropertyValue = styleDeclaration.getPropertyCSSValue('geometry');
    shouldHaveConstructor("geometryPropertyValue", "CSSValueList");

    shouldBe("geometryPropertyValue.length", expectedTexts.length.toString()); // shouldBe expects string arguments
}

heading("Geometry property.");

testGeometryProperty("Geometry with full mesh and attached.",
    "grid(1 1 attached)",
    "1 1 attached",
    ["1", "1", "attached"]);

testGeometryProperty("Geometry with full mesh and detached.",
    "grid(1 1 detached)",
    "1 1 detached",
    ["1", "1", "detached"]);

testGeometryProperty("Geometry with full mesh and no attached, nor detached.",
    "grid(1 1)",
    "1 1",
    ["1", "1"]);

testGeometryProperty("Geometry with only one mesh value.",
    "grid(1)",
    "1",
    ["1"]);

testGeometryProperty("Geometry with one mesh value and attached.",
    "grid(1 attached)",
    "1 attached",
    ["1", "attached"]);

testGeometryProperty("Geometry with one mesh value and detached.",
    "grid(1 detached)",
    "1 detached",
    ["1", "detached"]);

testGeometryProperty("Geometry with attached before one mesh value.",
    "grid(attached 1)",
    "attached 1",
    ["attached", "1"]);

testGeometryProperty("Geometry with attached before two mesh values.",
    "grid(attached 1 1)",
    "attached 1 1",
    ["attached", "1", "1"]);

testGeometryProperty("Geometry with detached before one mesh value.",
    "grid(detached 1)",
    "detached 1",
    ["detached", "1"]);

testGeometryProperty("Geometry with detached before two mesh values.",
    "grid(detached 1 1)",
    "detached 1 1",
    ["detached", "1", "1"]);

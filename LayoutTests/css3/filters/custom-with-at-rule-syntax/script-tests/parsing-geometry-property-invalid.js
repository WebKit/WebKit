// Requires custom-filter-parsing-common.js.

description("Test geometry property parsing in the @-webkit-filter at-rule.");

// These have to be global for the test helpers to see them.
var filterAtRule, styleDeclaration;

function testInvalidGeometryProperty(description, propertyValue)
{
    var geometryProperty = "geometry: " + propertyValue + ";";
    debug("\n" + description + "\n" + geometryProperty);

    stylesheet.insertRule("@-webkit-filter my-filter { " + geometryProperty + " }", 0);

    filterAtRule = stylesheet.cssRules.item(0);
    shouldBe("filterAtRule.type", "CSSRule.WEBKIT_FILTER_RULE");

    styleDeclaration = filterAtRule.style;
    shouldBe("styleDeclaration.length", "0");
    shouldBe("styleDeclaration.getPropertyValue('geometry')", "null");
}

heading("Geometry tests.");
testInvalidGeometryProperty("No property value.", "");
testInvalidGeometryProperty("Empty grid.", "grid()");
testInvalidGeometryProperty("Mispelled grid function.", "griid(1 1)");
testInvalidGeometryProperty("Single negative value in grid.", "grid(-1)");
testInvalidGeometryProperty("Negative value in grid along with a valid value.", "grid(1 -1)");
testInvalidGeometryProperty("Negative value in grid along with a valid value, inverted.", "grid(-1 1)");
testInvalidGeometryProperty("Two negative values.", "grid(-1 -1)");
testInvalidGeometryProperty("Single zero value.", "grid(0)");
testInvalidGeometryProperty("Zero with a valid value.", "grid(0 1)");
testInvalidGeometryProperty("Zero with a valid value, inverted.", "grid(1 0)");
testInvalidGeometryProperty("Attached value alone.", "grid(attached)");
testInvalidGeometryProperty("Detached value alone.", "grid(detached)");
testInvalidGeometryProperty("Detached and attached.", "grid(attached attached)");
testInvalidGeometryProperty("Too many values.", "grid(1 1 attached detached)");
testInvalidGeometryProperty("Mispelled attached.", "grid(1 1 attacd)");
testInvalidGeometryProperty("Mispelled detached.", "grid(1 1 detache)");
testInvalidGeometryProperty("Invalid divider.", "grid(1, 1 attached)");
testInvalidGeometryProperty("Invalid divider, reloaded.", "grid(1 1, attached)");
testInvalidGeometryProperty("Non integer values.", "grid(1.3 1)");
testInvalidGeometryProperty("Non integer values.", "grid(1 1.3)");
testInvalidGeometryProperty("Three integers.", "grid(1 2 3)");
testInvalidGeometryProperty("Attached in between two integers.", "grid(1 attached 1)");
testInvalidGeometryProperty("Detached in between two integers.", "grid(1 detached 1)");

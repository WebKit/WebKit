description("This test checks the SVGAnimatedEnumeration API - utilizing the clipPathUnits property of SVGClipPathElement");

var clipPathElement = document.createElementNS("http://www.w3.org/2000/svg", "clipPath");

debug("");
debug("Check initial clipPathUnits value");
shouldBeEqualToString("clipPathElement.clipPathUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(clipPathElement.clipPathUnits.baseVal)", "number");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Check that enumerations are static, caching value in a local variable and modifying it, should have no effect");
var enumRef = clipPathElement.clipPathUnits.baseVal;
enumRef = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
shouldBe("enumRef", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Check assigning various valid and invalid values");
shouldThrow("clipPathElement.clipPathUnits.baseVal = 3");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldThrow("clipPathElement.clipPathUnits.baseVal = -1");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

// ECMA-262, 9.7, "ToUint16"
shouldBeEqualToString("clipPathElement.clipPathUnits.baseVal = '1'", SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE.toString());
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

// ECMA-262, 9.7, "ToUint16"
shouldThrow("clipPathElement.clipPathUnits.baseVal = 'aString'");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

shouldBe("clipPathElement.clipPathUnits.baseVal = 2", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldThrow("clipPathElement.clipPathUnits.baseVal = clipPathElement");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");

successfullyParsed = true;

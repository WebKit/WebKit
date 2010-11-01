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
enumRef = SVGUnitTypes.SVG_UNIT_TYPE_UNKNOWN;
shouldBe("enumRef", "SVGUnitTypes.SVG_UNIT_TYPE_UNKNOWN");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Check assigning various valid and invalid values");
shouldThrow("clipPathElement.clipPathUnits.baseVal = 3"); // FIXME: Doesn't throw in WebKit, we're not clamping to the allowed range.
shouldBe("clipPathElement.clipPathUnits.baseVal = 2", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldThrow("clipPathElement.clipPathUnits.baseVal = -1"); // FIXME: Doesn't throw in WebKit, we're not clamping to the allowed range.
shouldThrow("clipPathElement.clipPathUnits.baseVal = 'aString'");
shouldThrow("clipPathElement.clipPathUnits.baseVal = clipPathElement");

debug("");
debug("Check that the clipPathUnits value remained objectBBox");
shouldBe("clipPathElement.clipPathUnits.baseVal = 2", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");

successfullyParsed = true;

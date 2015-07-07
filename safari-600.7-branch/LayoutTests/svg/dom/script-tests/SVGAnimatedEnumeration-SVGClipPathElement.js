description("This test checks the use of SVGAnimatedEnumeration within SVGClipPathElement");

var clipPathElement = document.createElementNS("http://www.w3.org/2000/svg", "clipPath");
clipPathElement.setAttribute("clipPathUnits", "userSpaceOnUse");

debug("");
debug("Check initial 'clipPathUnits' value");
shouldBeEqualToString("clipPathElement.clipPathUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(clipPathElement.clipPathUnits.baseVal)", "number");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Switch to 'objectBoundingBox'");
shouldBe("clipPathElement.clipPathUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("clipPathElement.getAttribute('clipPathUnits')", "objectBoundingBox");

debug("");
debug("Try setting invalid values");
shouldThrow("clipPathElement.clipPathUnits.baseVal = 3");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("clipPathElement.getAttribute('clipPathUnits')", "objectBoundingBox");

shouldThrow("clipPathElement.clipPathUnits.baseVal = -1");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("clipPathElement.getAttribute('clipPathUnits')", "objectBoundingBox");

shouldThrow("clipPathElement.clipPathUnits.baseVal = 0");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("clipPathElement.getAttribute('clipPathUnits')", "objectBoundingBox");

debug("");
debug("Switch to 'userSpaceOnUse'");
shouldBe("clipPathElement.clipPathUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBeEqualToString("clipPathElement.getAttribute('clipPathUnits')", "userSpaceOnUse");

successfullyParsed = true;

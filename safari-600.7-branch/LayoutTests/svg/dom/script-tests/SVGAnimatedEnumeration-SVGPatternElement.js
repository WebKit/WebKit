description("This test checks the use of SVGAnimatedEnumeration within SVGPatternElement");

var patternElement = document.createElementNS("http://www.w3.org/2000/svg", "pattern");
patternElement.setAttribute("patternUnits", "userSpaceOnUse");
patternElement.setAttribute("patternContentUnits", "userSpaceOnUse");

// patternUnits
debug("");
debug("Check initial 'patternUnits' value");
shouldBeEqualToString("patternElement.patternUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(patternElement.patternUnits.baseVal)", "number");
shouldBe("patternElement.patternUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Switch to 'objectBoundingBox'");
shouldBe("patternElement.patternUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("patternElement.patternUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("patternElement.getAttribute('patternUnits')", "objectBoundingBox");

debug("");
debug("Try setting invalid values");
shouldThrow("patternElement.patternUnits.baseVal = 3");
shouldBe("patternElement.patternUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("patternElement.getAttribute('patternUnits')", "objectBoundingBox");

shouldThrow("patternElement.patternUnits.baseVal = -1");
shouldBe("patternElement.patternUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("patternElement.getAttribute('patternUnits')", "objectBoundingBox");

shouldThrow("patternElement.patternUnits.baseVal = 0");
shouldBe("patternElement.patternUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("patternElement.getAttribute('patternUnits')", "objectBoundingBox");

debug("");
debug("Switch to 'userSpaceOnUse'");
shouldBe("patternElement.patternUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBe("patternElement.patternUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBeEqualToString("patternElement.getAttribute('patternUnits')", "userSpaceOnUse");

// patternContentUnits
debug("");
debug("Check initial 'patternContentUnits' value");
shouldBeEqualToString("patternElement.patternContentUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(patternElement.patternContentUnits.baseVal)", "number");
shouldBe("patternElement.patternContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Switch to 'objectBoundingBox'");
shouldBe("patternElement.patternContentUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("patternElement.patternContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("patternElement.getAttribute('patternContentUnits')", "objectBoundingBox");

debug("");
debug("Try setting invalid values");
shouldThrow("patternElement.patternContentUnits.baseVal = 3");
shouldBe("patternElement.patternContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("patternElement.getAttribute('patternContentUnits')", "objectBoundingBox");

shouldThrow("patternElement.patternContentUnits.baseVal = -1");
shouldBe("patternElement.patternContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("patternElement.getAttribute('patternContentUnits')", "objectBoundingBox");

shouldThrow("patternElement.patternContentUnits.baseVal = 0");
shouldBe("patternElement.patternContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("patternElement.getAttribute('patternContentUnits')", "objectBoundingBox");

debug("");
debug("Switch to 'userSpaceOnUse'");
shouldBe("patternElement.patternContentUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBe("patternElement.patternContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBeEqualToString("patternElement.getAttribute('patternContentUnits')", "userSpaceOnUse");

successfullyParsed = true;

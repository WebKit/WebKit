description("This test checks the use of SVGAnimatedEnumeration within SVGMaskElement");

var maskElement = document.createElementNS("http://www.w3.org/2000/svg", "mask");
maskElement.setAttribute("maskUnits", "userSpaceOnUse");
maskElement.setAttribute("maskContentUnits", "userSpaceOnUse");

// maskUnits
debug("");
debug("Check initial 'maskUnits' value");
shouldBeEqualToString("maskElement.maskUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(maskElement.maskUnits.baseVal)", "number");
shouldBe("maskElement.maskUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Switch to 'objectBoundingBox'");
shouldBe("maskElement.maskUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("maskElement.maskUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("maskElement.getAttribute('maskUnits')", "objectBoundingBox");

debug("");
debug("Try setting invalid values");
shouldThrow("maskElement.maskUnits.baseVal = 3");
shouldBe("maskElement.maskUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("maskElement.getAttribute('maskUnits')", "objectBoundingBox");

shouldThrow("maskElement.maskUnits.baseVal = -1");
shouldBe("maskElement.maskUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("maskElement.getAttribute('maskUnits')", "objectBoundingBox");

shouldThrow("maskElement.maskUnits.baseVal = 0");
shouldBe("maskElement.maskUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("maskElement.getAttribute('maskUnits')", "objectBoundingBox");

debug("");
debug("Switch to 'userSpaceOnUse'");
shouldBe("maskElement.maskUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBe("maskElement.maskUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBeEqualToString("maskElement.getAttribute('maskUnits')", "userSpaceOnUse");

// maskContentUnits
debug("");
debug("Check initial 'maskContentUnits' value");
shouldBeEqualToString("maskElement.maskContentUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(maskElement.maskContentUnits.baseVal)", "number");
shouldBe("maskElement.maskContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Switch to 'objectBoundingBox'");
shouldBe("maskElement.maskContentUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("maskElement.maskContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("maskElement.getAttribute('maskContentUnits')", "objectBoundingBox");

debug("");
debug("Try setting invalid values");
shouldThrow("maskElement.maskContentUnits.baseVal = 3");
shouldBe("maskElement.maskContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("maskElement.getAttribute('maskContentUnits')", "objectBoundingBox");

shouldThrow("maskElement.maskContentUnits.baseVal = -1");
shouldBe("maskElement.maskContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("maskElement.getAttribute('maskContentUnits')", "objectBoundingBox");

shouldThrow("maskElement.maskContentUnits.baseVal = 0");
shouldBe("maskElement.maskContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("maskElement.getAttribute('maskContentUnits')", "objectBoundingBox");

debug("");
debug("Switch to 'userSpaceOnUse'");
shouldBe("maskElement.maskContentUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBe("maskElement.maskContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBeEqualToString("maskElement.getAttribute('maskContentUnits')", "userSpaceOnUse");

successfullyParsed = true;

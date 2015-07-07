description("This test checks the use of SVGAnimatedEnumeration within SVGFilterElement");

var filterElement = document.createElementNS("http://www.w3.org/2000/svg", "filter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("primitiveUnits", "userSpaceOnUse");

// filterUnits
debug("");
debug("Check initial 'filterUnits' value");
shouldBeEqualToString("filterElement.filterUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(filterElement.filterUnits.baseVal)", "number");
shouldBe("filterElement.filterUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Switch to 'objectBoundingBox'");
shouldBe("filterElement.filterUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("filterElement.filterUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("filterElement.getAttribute('filterUnits')", "objectBoundingBox");

debug("");
debug("Try setting invalid values");
shouldThrow("filterElement.filterUnits.baseVal = 3");
shouldBe("filterElement.filterUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("filterElement.getAttribute('filterUnits')", "objectBoundingBox");

shouldThrow("filterElement.filterUnits.baseVal = -1");
shouldBe("filterElement.filterUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("filterElement.getAttribute('filterUnits')", "objectBoundingBox");

shouldThrow("filterElement.filterUnits.baseVal = 0");
shouldBe("filterElement.filterUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("filterElement.getAttribute('filterUnits')", "objectBoundingBox");

debug("");
debug("Switch to 'userSpaceOnUse'");
shouldBe("filterElement.filterUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBe("filterElement.filterUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBeEqualToString("filterElement.getAttribute('filterUnits')", "userSpaceOnUse");

// primitiveUnits
debug("");
debug("Check initial 'primitiveUnits' value");
shouldBeEqualToString("filterElement.primitiveUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(filterElement.primitiveUnits.baseVal)", "number");
shouldBe("filterElement.primitiveUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Switch to 'objectBoundingBox'");
shouldBe("filterElement.primitiveUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("filterElement.primitiveUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("filterElement.getAttribute('primitiveUnits')", "objectBoundingBox");

debug("");
debug("Try setting invalid values");
shouldThrow("filterElement.primitiveUnits.baseVal = 3");
shouldBe("filterElement.primitiveUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("filterElement.getAttribute('primitiveUnits')", "objectBoundingBox");

shouldThrow("filterElement.primitiveUnits.baseVal = -1");
shouldBe("filterElement.primitiveUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("filterElement.getAttribute('primitiveUnits')", "objectBoundingBox");

shouldThrow("filterElement.primitiveUnits.baseVal = 0");
shouldBe("filterElement.primitiveUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("filterElement.getAttribute('primitiveUnits')", "objectBoundingBox");

debug("");
debug("Switch to 'userSpaceOnUse'");
shouldBe("filterElement.primitiveUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBe("filterElement.primitiveUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBeEqualToString("filterElement.getAttribute('primitiveUnits')", "userSpaceOnUse");


successfullyParsed = true;

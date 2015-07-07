description("This test checks the use of SVGAnimatedEnumeration within SVGGradientElement");

var gradientElement = document.createElementNS("http://www.w3.org/2000/svg", "linearGradient");
gradientElement.setAttribute("gradientUnits", "userSpaceOnUse");
gradientElement.setAttribute("spreadMethod", "pad");

// gradientUnits
debug("");
debug("Check initial 'gradientUnits' value");
shouldBeEqualToString("gradientElement.gradientUnits.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(gradientElement.gradientUnits.baseVal)", "number");
shouldBe("gradientElement.gradientUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

debug("");
debug("Switch to 'objectBoundingBox'");
shouldBe("gradientElement.gradientUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBe("gradientElement.gradientUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("gradientElement.getAttribute('gradientUnits')", "objectBoundingBox");

debug("");
debug("Try setting invalid values");
shouldThrow("gradientElement.gradientUnits.baseVal = 3");
shouldBe("gradientElement.gradientUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("gradientElement.getAttribute('gradientUnits')", "objectBoundingBox");

shouldThrow("gradientElement.gradientUnits.baseVal = -1");
shouldBe("gradientElement.gradientUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("gradientElement.getAttribute('gradientUnits')", "objectBoundingBox");

shouldThrow("gradientElement.gradientUnits.baseVal = 0");
shouldBe("gradientElement.gradientUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
shouldBeEqualToString("gradientElement.getAttribute('gradientUnits')", "objectBoundingBox");

debug("");
debug("Switch to 'userSpaceOnUse'");
shouldBe("gradientElement.gradientUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBe("gradientElement.gradientUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
shouldBeEqualToString("gradientElement.getAttribute('gradientUnits')", "userSpaceOnUse");

// spreadMethod
debug("");
debug("Check initial 'spreadMethod' value");
shouldBeEqualToString("gradientElement.spreadMethod.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(gradientElement.spreadMethod.baseVal)", "number");
shouldBe("gradientElement.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_PAD");

debug("");
debug("Switch to 'reflect' value");
shouldBe("gradientElement.spreadMethod.baseVal = SVGGradientElement.SVG_SPREADMETHOD_REFLECT", "SVGGradientElement.SVG_SPREADMETHOD_REFLECT");
shouldBe("gradientElement.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_REFLECT");
shouldBeEqualToString("gradientElement.getAttribute('spreadMethod')", "reflect");

debug("");
debug("Switch to 'repeat' value");
shouldBe("gradientElement.spreadMethod.baseVal = SVGGradientElement.SVG_SPREADMETHOD_REPEAT", "SVGGradientElement.SVG_SPREADMETHOD_REPEAT");
shouldBe("gradientElement.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_REPEAT");
shouldBeEqualToString("gradientElement.getAttribute('spreadMethod')", "repeat");

debug("");
debug("Try setting invalid values");
shouldThrow("gradientElement.spreadMethod.baseVal = 4");
shouldBe("gradientElement.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_REPEAT");
shouldBeEqualToString("gradientElement.getAttribute('spreadMethod')", "repeat");

shouldThrow("gradientElement.spreadMethod.baseVal = -1");
shouldBe("gradientElement.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_REPEAT");
shouldBeEqualToString("gradientElement.getAttribute('spreadMethod')", "repeat");

shouldThrow("gradientElement.spreadMethod.baseVal = 0");
shouldBe("gradientElement.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_REPEAT");
shouldBeEqualToString("gradientElement.getAttribute('spreadMethod')", "repeat");

debug("");
debug("Switch to 'pad'");
shouldBe("gradientElement.spreadMethod.baseVal = SVGGradientElement.SVG_SPREADMETHOD_PAD", "SVGGradientElement.SVG_SPREADMETHOD_PAD");
shouldBe("gradientElement.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_PAD");
shouldBeEqualToString("gradientElement.getAttribute('spreadMethod')", "pad");

successfullyParsed = true;

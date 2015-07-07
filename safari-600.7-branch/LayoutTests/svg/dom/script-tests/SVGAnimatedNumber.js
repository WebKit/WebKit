description("This test checks the SVGAnimatedNumber API - utilizing the surfaceScale property of SVGFESpecularLightingElement");

var feSpecularLightingElement = document.createElementNS("http://www.w3.org/2000/svg", "feSpecularLighting");

debug("");
debug("Check initial surfaceScale value");
shouldBeEqualToString("feSpecularLightingElement.surfaceScale.toString()", "[object SVGAnimatedNumber]");
shouldBeEqualToString("typeof(feSpecularLightingElement.surfaceScale.baseVal)", "number");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal", "1");

debug("");
debug("Check that integers are static, caching value in a local variable and modifying it, should have no effect");
var numRef = feSpecularLightingElement.surfaceScale.baseVal;
numRef = 100;
shouldBe("numRef", "100");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal", "1");

debug("");
debug("Check assigning various valid and invalid values");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal = -1", "-1"); // Negative values are allowed from SVG DOM, but should lead to an error when rendering (disable the filter)
shouldBe("feSpecularLightingElement.surfaceScale.baseVal = 300", "300");
// ECMA-262, 9.3, "ToNumber"
shouldBe("feSpecularLightingElement.surfaceScale.baseVal = 'aString'", "'aString'");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal", "NaN");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal = 0", "0");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal = feSpecularLightingElement", "feSpecularLightingElement");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal", "NaN");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal = 300", "300");

debug("");
debug("Check that the surfaceScale value remained 300");
shouldBe("feSpecularLightingElement.surfaceScale.baseVal", "300");

successfullyParsed = true;

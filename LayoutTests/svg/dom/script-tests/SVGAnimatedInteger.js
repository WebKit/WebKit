description("This test checks the SVGAnimatedInteger API - utilizing the filterResX property of SVGFilterElement");

var filterElement = document.createElementNS("http://www.w3.org/2000/svg", "filter");

debug("");
debug("Check initial filterResX value");
shouldBeEqualToString("filterElement.filterResX.toString()", "[object SVGAnimatedInteger]");
shouldBeEqualToString("typeof(filterElement.filterResX.baseVal)", "number");
shouldBe("filterElement.filterResX.baseVal", "0");

debug("");
debug("Check that integers are static, caching value in a local variable and modifying it, should have no effect");
var numRef = filterElement.filterResX.baseVal;
numRef = 100;
shouldBe("numRef", "100");
shouldBe("filterElement.filterResX.baseVal", "0");

debug("");
debug("Check assigning various valid and invalid values");
shouldBe("filterElement.filterResX.baseVal = -1", "-1"); // Negative values are allowed from SVG DOM, but should lead to an error when rendering (disable the filter)
shouldBe("filterElement.filterResX.baseVal = 300", "300");
// ECMA-262, 9.5, "ToInt32"
shouldBe("filterElement.filterResX.baseVal = 'aString'", "'aString'");
shouldBe("filterElement.filterResX.baseVal", "0");
shouldBe("filterElement.filterResX.baseVal = filterElement", "filterElement");
shouldBe("filterElement.filterResX.baseVal", "0");
shouldBe("filterElement.filterResX.baseVal = 300", "300");

debug("");
debug("Check that the filterResX value remained 300");
shouldBe("filterElement.filterResX.baseVal", "300");

successfullyParsed = true;

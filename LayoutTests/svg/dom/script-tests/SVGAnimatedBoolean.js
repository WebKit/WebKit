description("This test checks the SVGAnimatedBoolean API - utilizing the externalResourcesRequired property of SVGRectElement");

var rectElement = document.createElementNS("http://www.w3.org/2000/svg", "rect");
debug("");
debug("Check initial SVGExternalResourcesRequired value");
shouldBe("rectElement.externalResourcesRequired.baseVal", "false");

debug("");
debug("Set value to true");
shouldBe("rectElement.externalResourcesRequired.baseVal = true", "true");

debug("");
debug("Caching baseVal in local variable");
var baseVal = rectElement.externalResourcesRequired.baseVal;
shouldBe("baseVal", "true");

debug("");
debug("Modify local baseVal variable to true");
shouldBeFalse("baseVal = false");

debug("");
debug("Assure that rectElement.externalResourcesRequired has not been changed, but the local baseVal variable");
shouldBe("baseVal", "false");
shouldBe("rectElement.externalResourcesRequired.baseVal", "true");

debug("");
debug("Check assigning values of various types");
// ECMA-262, 9.2, "ToBoolean"
shouldBe("rectElement.externalResourcesRequired.baseVal = rectElement.externalResourcesRequired", "rectElement.externalResourcesRequired");
shouldBe("rectElement.externalResourcesRequired.baseVal", "true");
shouldBeNull("rectElement.externalResourcesRequired.baseVal = null");
shouldBe("rectElement.externalResourcesRequired.baseVal", "false");
shouldBe("rectElement.externalResourcesRequired.baseVal = 'aString'", "'aString'");
shouldBe("rectElement.externalResourcesRequired.baseVal", "true");
rectElement.externalResourcesRequired.baseVal = false;
shouldBe("rectElement.externalResourcesRequired.baseVal = rectElement", "rectElement");
shouldBe("rectElement.externalResourcesRequired.baseVal", "true");

successfullyParsed = true;

description("This test checks the SVGExternalResourcesRequired API");

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
debug("Check assigning invalid values");
shouldThrow("rectElement.externalResourcesRequired.baseVal = rectElement.externalResourcesRequired");
shouldBeNull("rectElement.externalResourcesRequired.baseVal = null");
shouldThrow("rectElement.externalResourcesRequired.baseVal = 'aString'");
shouldThrow("rectElement.externalResourcesRequired.baseVal = rectElement");

debug("");
debug("Check that the value is now false");
shouldBe("rectElement.externalResourcesRequired.baseVal", "false");

successfullyParsed = true;

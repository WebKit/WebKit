description("This test checks the SVGStringList API - utilizing the requiredFeatures property of SVGRectElement");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
rect.setAttribute("requiredFeatures", "foo bar");

debug("");
debug("Check initial requiredFeatures values");
shouldBe("rect.requiredFeatures.numberOfItems", "2");
shouldBeEqualToString("rect.requiredFeatures.getItem(0)", "foo");
shouldBeEqualToString("rect.requiredFeatures.getItem(1)", "bar");

debug("");
debug("Check that getItem() does NOT return live strings, as the IDL defines the return types as plain DOMString");
var firstItem = rect.requiredFeatures.getItem(0);
shouldBeEqualToString("firstItem = 'test'", "test");
shouldBe("rect.requiredFeatures.numberOfItems", "2");
shouldBeEqualToString("rect.requiredFeatures.getItem(0)", "foo");
shouldBeEqualToString("rect.requiredFeatures.getItem(1)", "bar");

successfullyParsed = true;

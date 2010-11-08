description("This test checks the SVGPoint API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var point = svgElement.createSVGPoint();

debug("");
debug("Check initial point values");
shouldBe("point.x", "0");
shouldBe("point.y", "0");

debug("");
debug("Check assigning points");
shouldBe("point.x = 100", "100");
shouldBe("point.y = 200", "200");

debug("");
debug("Check assigning invalid points");
shouldThrow("point.x = point");
shouldBeNull("point.y = null");

debug("");
debug("Check that the point is still containing the correct values");
shouldBe("point.x", "100");
shouldBe("point.y", "0");

successfullyParsed = true;

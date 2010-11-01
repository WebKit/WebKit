description("This test checks the SVGAnimatedRect API - utilizing the viewBox property of SVGSVGElement");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");

debug("");
debug("Check initial viewBox value");
shouldBeEqualToString("svgElement.viewBox.toString()", "[object SVGAnimatedRect]");
shouldBeEqualToString("svgElement.viewBox.baseVal.toString()", "[object SVGRect]");
shouldBe("svgElement.viewBox.baseVal.x", "0");

debug("");
debug("Check that rects are dynamic, caching value in a local variable and modifying it, should take effect");
var numRef = svgElement.viewBox.baseVal;
numRef.x = 100;
shouldBe("numRef.x", "100");
shouldBe("svgElement.viewBox.baseVal.x", "100");

debug("");
debug("Check that assigning to baseVal has no effect, as no setter is defined");
shouldBe("svgElement.viewBox.baseVal = -1", "-1");
shouldBeEqualToString("svgElement.viewBox.baseVal = 'aString'", "aString");
shouldBe("svgElement.viewBox.baseVal = svgElement", "svgElement");

debug("");
debug("Check that the viewBox x remained 100, and the baseVal type has not been changed");
shouldBeEqualToString("svgElement.viewBox.baseVal.toString()", "[object SVGRect]");
shouldBe("svgElement.viewBox.baseVal.x", "100");

successfullyParsed = true;

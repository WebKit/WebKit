description("This test checks the SVGAnimatedLength API - utilizing the width property of SVGRectElement");

var rectElement = document.createElementNS("http://www.w3.org/2000/svg", "rect");

debug("");
debug("Check initial width value");
shouldBeEqualToString("rectElement.width.toString()", "[object SVGAnimatedLength]");
shouldBeEqualToString("rectElement.width.baseVal.toString()", "[object SVGLength]");
shouldBe("rectElement.width.baseVal.value", "0");

debug("");
debug("Check that lengths are dynamic, caching value in a local variable and modifying it, should take effect");
var numRef = rectElement.width.baseVal;
numRef.value = 100;
shouldBe("numRef.value", "100");
shouldBe("rectElement.width.baseVal.value", "100");

debug("");
debug("Check that assigning to baseVal has no effect, as no setter is defined");
shouldBe("rectElement.width.baseVal = -1", "-1");
shouldBeEqualToString("rectElement.width.baseVal = 'aString'", "aString");
shouldBe("rectElement.width.baseVal = rectElement", "rectElement");

debug("");
debug("Check that the width value remained 100, and the baseVal type has not been changed");
shouldBeEqualToString("rectElement.width.baseVal.toString()", "[object SVGLength]");
shouldBe("rectElement.width.baseVal.value", "100");

successfullyParsed = true;

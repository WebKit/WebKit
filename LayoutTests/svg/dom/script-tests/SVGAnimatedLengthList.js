description("This test checks the SVGAnimatedLengthList API - utilizing the dx property of SVGTextElement");

var textElement = document.createElementNS("http://www.w3.org/2000/svg", "text");
textElement.setAttribute("dx", "50");

debug("");
debug("Check initial dx value");
shouldBeEqualToString("textElement.dx.toString()", "[object SVGAnimatedLengthList]");
shouldBeEqualToString("textElement.dx.baseVal.toString()", "[object SVGLengthList]");
shouldBe("textElement.dx.baseVal.getItem(0).value", "50");

debug("");
debug("Check that length lists are dynamic, caching value in a local variable and modifying it, should take effect");
var numRef = textElement.dx.baseVal;
numRef.getItem(0).value = 100;
shouldBe("numRef.getItem(0).value", "100");
shouldBe("textElement.dx.baseVal.getItem(0).value", "100");

debug("");
debug("Check that assigning to baseVal has no effect, as no setter is defined");
shouldBe("textElement.dx.baseVal = -1", "-1");
shouldBeEqualToString("textElement.dx.baseVal = 'aString'", "aString");
shouldBe("textElement.dx.baseVal = textElement", "textElement");

debug("");
debug("Check that the dx value remained 100, and the baseVal type has not been changed");
shouldBeEqualToString("textElement.dx.baseVal.toString()", "[object SVGLengthList]");
shouldBe("textElement.dx.baseVal.getItem(0).value", "100");

successfullyParsed = true;

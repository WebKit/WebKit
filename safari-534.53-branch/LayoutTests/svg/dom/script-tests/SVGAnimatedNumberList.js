description("This test checks the SVGAnimatedNumberList API - utilizing the rotate property of SVGTextElement");

var textElement = document.createElementNS("http://www.w3.org/2000/svg", "text");
textElement.setAttribute("rotate", "50");

debug("");
debug("Check initial rotate value");
shouldBeEqualToString("textElement.rotate.toString()", "[object SVGAnimatedNumberList]");
shouldBeEqualToString("textElement.rotate.baseVal.toString()", "[object SVGNumberList]");
shouldBe("textElement.rotate.baseVal.getItem(0).value", "50");

debug("");
debug("Check that number lists are dynamic, caching value in a local variable and modifying it, should take effect");
var numRef = textElement.rotate.baseVal;
numRef.getItem(0).value = 100;
shouldBe("numRef.getItem(0).value", "100");
shouldBe("textElement.rotate.baseVal.getItem(0).value", "100");

debug("");
debug("Check that assigning to baseVal has no effect, as no setter is defined");
shouldBe("textElement.rotate.baseVal = -1", "-1");
shouldBeEqualToString("textElement.rotate.baseVal = 'aString'", "aString");
shouldBe("textElement.rotate.baseVal = textElement", "textElement");

debug("");
debug("Check that the rotate value remained 100, and the baseVal type has not been changed");
shouldBeEqualToString("textElement.rotate.baseVal.toString()", "[object SVGNumberList]");
shouldBe("textElement.rotate.baseVal.getItem(0).value", "100");

successfullyParsed = true;

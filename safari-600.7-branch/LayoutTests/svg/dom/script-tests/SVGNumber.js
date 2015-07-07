description("This test checks the SVGNumber API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var num = svgElement.createSVGNumber();

debug("");
debug("Check initial number value");
shouldBe("num.value", "0");

debug("");
debug("Check assigning number");
shouldBe("num.value = 100", "100");
shouldBe("num.value = -100", "-100");
shouldBe("num.value = 12345678", "12345678");
shouldBe("num.value = -num.value", "-12345678");

debug("");
debug("Check that numbers are static, caching value in a local variable and modifying it, should have no effect");
var numRef = num.value;
numRef = 1000;
shouldBe("numRef", "1000");
shouldBe("num.value", "-12345678");

debug("");
debug("Check assigning invalid number, number should be 0 afterwards");
shouldBe("num.value = num", "num");
shouldBe("num.value = 'aString'", "'aString'");
shouldBe("num.value = svgElement", "svgElement");
shouldBe("num.value", "NaN");
shouldBeNull("num.value = null");

debug("");
debug("Check that the number is now null");
shouldBe("num.value", "0");

successfullyParsed = true;

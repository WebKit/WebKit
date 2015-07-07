description("This test checks the SVGLength API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var length = svgElement.createSVGLength();

debug("");
debug("Check initial length values");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_NUMBER");
shouldBe("length.value", "0");
shouldBe("length.valueInSpecifiedUnits", "0");
shouldBeEqualToString("length.valueAsString", "0");

debug("");
debug("Set value to be 2px");
length.valueAsString = "2px";
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");
shouldBe("length.value", "2");
shouldBe("length.valueInSpecifiedUnits", "2");
shouldBeEqualToString("length.valueAsString", "2px");

debug("");
debug("Check invalid arguments for 'convertToSpecifiedUnits'");
shouldThrow("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_UNKNOWN)");
shouldThrow("length.convertToSpecifiedUnits(-1)");
shouldThrow("length.convertToSpecifiedUnits(11)");
shouldThrow("length.convertToSpecifiedUnits('aString')");
shouldThrow("length.convertToSpecifiedUnits(length)");
shouldThrow("length.convertToSpecifiedUnits(svgElement)");
shouldThrow("length.convertToSpecifiedUnits()");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");
shouldBe("length.value", "2");
shouldBe("length.valueInSpecifiedUnits", "2");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");

debug("");
debug("Check invalid arguments for 'newValueSpecifiedUnits'");
shouldThrow("length.newValueSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_UNKNOWN, 4)");
shouldThrow("length.newValueSpecifiedUnits(-1, 4)");
shouldThrow("length.newValueSpecifiedUnits(11, 4)");
// ECMA-262, 9.3, "ToNumber"
shouldBeUndefined("length.newValueSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PX, 'aString')");
shouldBe("length.value", "NaN");
shouldBeUndefined("length.newValueSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PX, 0)");
shouldBeUndefined("length.newValueSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PX, length)");
shouldBe("length.value", "NaN");
shouldBeUndefined("length.newValueSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PX, 0)");
shouldBeUndefined("length.newValueSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PX, svgElement)");
shouldBe("length.value", "NaN");
shouldThrow("length.newValueSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PX)");
// Reset to original value above.
length.valueAsString = "2px";
shouldThrow("length.newValueSpecifiedUnits('aString', 4)");
shouldThrow("length.newValueSpecifiedUnits(length, 4)");
shouldThrow("length.newValueSpecifiedUnits(svgElement, 4)");
shouldThrow("length.newValueSpecifiedUnits('aString', 'aString')");
shouldThrow("length.newValueSpecifiedUnits(length, length)");
shouldThrow("length.newValueSpecifiedUnits(svgElement, svgElement)");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");
shouldBe("length.value", "2");
shouldBe("length.valueInSpecifiedUnits", "2");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");

successfullyParsed = true;

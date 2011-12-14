description("This test checks SVGLength - converting from px to all other unit types");

function calculateDPI()
{
    // Crude hack to determine the DPI, instead of hardcoding our 96 dpi here.
    var divElement = document.createElement("div");
    divElement.setAttribute("style", "height: 1in");
    document.getElementById("description").appendChild(divElement);
    var cssPixelsPerInch = divElement.offsetHeight;
    document.getElementById("description").removeChild(divElement);

    // Crude hack to make this test pass with Opera/Mac
    if (navigator.userAgent.indexOf("Opera") != -1) {
        if (navigator.userAgent.indexOf("Macintosh") != -1) {    
            cssPixelsPerInch = 72;
        }
    }

    return cssPixelsPerInch;
}

var cssPixelsPerInch = calculateDPI();

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var length = svgElement.createSVGLength();

debug("");
debug("Set value to be 2px");
length.valueAsString = "2px";
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");
shouldBe("length.value", "2");
shouldBe("length.valueInSpecifiedUnits", "2");
shouldBeEqualToString("length.valueAsString", "2px");

debug("");
debug("Convert from px to unitless");
shouldBeUndefined("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_NUMBER)");
shouldBeEqualToString("length.valueAsString", "2");
shouldBe("length.value", "2");
shouldBe("length.valueInSpecifiedUnits", "2");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_NUMBER");

debug("");
debug("Reset to 2px");
length.valueAsString = "2px";

debug("");
debug("Try converting from px to percentage, should fail as the SVGLength is not associated with a SVGSVGElement, and thus no viewport information is available");
shouldThrow("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PERCENTAGE)");
shouldBeEqualToString("length.valueAsString", "2px");
shouldBe("length.value", "2");
shouldBe("length.valueInSpecifiedUnits", "2");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");

debug("");
debug("Reset to 2px");
length.valueAsString = "2px";

debug("");
debug("Try converting from px to ems, should fail as the SVGLength is not associated with a SVGSVGElement, and thus no font-size information is available");
shouldThrow("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_EMS)");
shouldBeEqualToString("length.valueAsString", "2px");
shouldBe("length.value", "2");
shouldBe("length.valueInSpecifiedUnits", "2");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");

debug("");
debug("Reset to 2px");
length.valueAsString = "2px";

debug("");
debug("Try converting from px to exs, should fail as the SVGLength is not associated with a SVGSVGElement, and thus no font-size information is available");
shouldThrow("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_EXS)");
shouldBeEqualToString("length.valueAsString", "2px");
shouldBe("length.value", "2");
shouldBe("length.valueInSpecifiedUnits", "2");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PX");

debug("");
debug("Reset to 2px");
length.valueAsString = "2px";

debug("");
debug("Convert from px to cm");
shouldBeUndefined("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_CM)");
referenceValue = Number(2 * 2.54 / cssPixelsPerInch).toFixed(7);
shouldBeEqualToString("length.valueAsString", referenceValue + "cm");
shouldBeEqualToString("length.valueInSpecifiedUnits.toFixed(7)", referenceValue);
shouldBeEqualToString("length.value.toFixed(1)", "2.0");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_CM");

debug("");
debug("Reset to 2px");
length.valueAsString = "2px";

debug("");
debug("Convert from px to mm");
shouldBeUndefined("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_MM)");
referenceValue = Number(2 * 25.4 / cssPixelsPerInch).toFixed(6);
shouldBeEqualToString("length.valueAsString", referenceValue + "mm");
shouldBeEqualToString("length.valueInSpecifiedUnits.toFixed(6)", referenceValue);
shouldBeEqualToString("length.value.toFixed(1)", "2.0");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_MM");

debug("");
debug("Reset to 2px");
length.valueAsString = "2px";

debug("");
debug("Convert from px to in");
shouldBeUndefined("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_IN)");
referenceValue = Number(2 / cssPixelsPerInch).toFixed(7);
shouldBeEqualToString("length.valueAsString", referenceValue + "in");
shouldBeEqualToString("length.valueInSpecifiedUnits.toFixed(7)", referenceValue);
shouldBeEqualToString("length.value.toFixed(1)", "2.0");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_IN");

debug("");
debug("Reset to 2px");
length.valueAsString = "2px";

debug("");
debug("Convert from px to pt");
shouldBeUndefined("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PT)");
referenceValue = Number(2 / cssPixelsPerInch * 72);
shouldBeEqualToString("length.valueAsString", referenceValue + "pt");
shouldBe("length.valueInSpecifiedUnits", referenceValue.toString());
shouldBeEqualToString("length.value.toFixed(1)", "2.0");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PT");

debug("");
debug("Reset to 2px");
length.valueAsString = "2px";

debug("");
debug("Convert from px to pc");
shouldBeUndefined("length.convertToSpecifiedUnits(SVGLength.SVG_LENGTHTYPE_PC)");
referenceValue = Number(2 / cssPixelsPerInch * 6).toFixed(3);
// Don't check valueAsString here, it's unreliable across browsers.
shouldBeEqualToString("length.valueInSpecifiedUnits.toFixed(3)", referenceValue);
shouldBeEqualToString("length.value.toFixed(1)", "2.0");
shouldBe("length.unitType", "SVGLength.SVG_LENGTHTYPE_PC");

successfullyParsed = true;

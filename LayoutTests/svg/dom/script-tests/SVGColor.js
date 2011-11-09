description("This test checks the SVGColor API");

// Setup a real SVG document, as we want to access CSS style information.
var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
svgElement.setAttribute("width", "150");
svgElement.setAttribute("height", "50");

var stopElement = document.createElementNS("http://www.w3.org/2000/svg", "stop");
stopElement.setAttribute("style", "stop-color: red; color: green");
svgElement.appendChild(stopElement);
document.getElementById("description").appendChild(svgElement);

function checkStopColor(type, red, green, blue) {
    shouldBe("stopColor.colorType", type);
    shouldBeEqualToString("(rgbColor = stopColor.rgbColor).toString()", "[object RGBColor]");
    shouldBe("rgbColor.red.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + red);
    shouldBe("rgbColor.green.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + green);
    shouldBe("rgbColor.blue.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + blue);
}

debug("");
debug("Check initial color values");
shouldBeEqualToString("(stopColor = stopElement.style.getPropertyCSSValue('stop-color')).toString()", "[object SVGColor]");
checkStopColor("SVGColor.SVG_COLORTYPE_RGBCOLOR", 255, 0, 0);
shouldBeEqualToString("stopElement.style.stopColor", "#ff0000");
shouldBeEqualToString("document.defaultView.getComputedStyle(stopElement).stopColor", "rgb(255, 0, 0)");

debug("");
debug("Try invalid arguments for setColor()");
shouldThrow("stopColor.setColor(null, null, null)");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_RGBCOLOR, svgElement, '');");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_RGBCOLOR, '', '')");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_CURRENTCOLOR + 1, '', '');");
shouldThrow("stopColor.setColor()");
shouldThrow("stopColor.setColor(stopColor)");

debug("");
debug("Try assigning to the readonly colorType property, which silently fails");
shouldBeUndefined("stopColor.colorType = SVGColor.SVG_COLORTYPE_UNKKNOWN;");
shouldBe("stopColor.colorType", "SVGColor.SVG_COLORTYPE_RGBCOLOR");
shouldBeEqualToString("stopElement.style.stopColor", "#ff0000");
shouldBeEqualToString("document.defaultView.getComputedStyle(stopElement).stopColor", "rgb(255, 0, 0)");

debug("");
debug("Test using setColor() and SVG_COLORTYPE_UNKNOWN");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_UNKKNOWN, '', '')");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_UNKKNOWN, 'rgb(0,128,128)', '')");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_UNKKNOWN, '', 'icc-color(myRGB, 0, 1, 2)')");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_UNKKNOWN, 'rgb(0,0,0)', 'icc-color(myRGB, 0, 1, 2)')");

debug("");
debug("Test using setColor() and SVG_COLORTYPE_RGBCOLOR_ICCCOLOR");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_RGBCOLOR_ICCCOLOR, 'rgb(77,0,77)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("stopColor.colorType", "SVGColor.SVG_COLORTYPE_RGBCOLOR");
// FIXME: No support for ICC colors.
checkStopColor("SVGColor.SVG_COLORTYPE_RGBCOLOR", 255, 0, 0);
shouldBeEqualToString("stopElement.style.stopColor", "#ff0000");
shouldBeEqualToString("document.defaultView.getComputedStyle(stopElement).stopColor", "rgb(255, 0, 0)");

debug("");
debug("Test using setColor() and SVG_COLORTYPE_CURRENTCOLOR");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_CURRENTCOLOR, 'rgb(77,0,77)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("stopColor.colorType", "SVGColor.SVG_COLORTYPE_RGBCOLOR");
checkStopColor("SVGColor.SVG_COLORTYPE_RGBCOLOR", 255, 0, 0);
shouldBeEqualToString("stopElement.style.stopColor", "#ff0000");
shouldBeEqualToString("document.defaultView.getComputedStyle(stopElement).stopColor", "rgb(255, 0, 0)");

debug("");
debug("Test using setColor() and SVG_COLORTYPE_RGBCOLOR");
shouldThrow("stopColor.setColor(SVGColor.SVG_COLORTYPE_RGBCOLOR, 'rgb(0,77,0)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("stopColor.colorType", "SVGColor.SVG_COLORTYPE_RGBCOLOR");
checkStopColor("SVGColor.SVG_COLORTYPE_RGBCOLOR", 255, 0, 0);
shouldBeEqualToString("document.defaultView.getComputedStyle(stopElement).stopColor", "rgb(255, 0, 0)");

successfullyParsed = true;

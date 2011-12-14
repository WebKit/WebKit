description("This test checks the SVGPaint API");

// Setup a real SVG document, as we want to access CSS style information.
var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
svgElement.setAttribute("width", "150");
svgElement.setAttribute("height", "50");

var rectElement = document.createElementNS("http://www.w3.org/2000/svg", "rect");
rectElement.setAttribute("style", "color: lime; fill: green");
rectElement.setAttribute("width", "150");
rectElement.setAttribute("height", "50");
svgElement.appendChild(rectElement);
document.getElementById("description").appendChild(svgElement);

// FIXME: Most tests that examine the computed style after modifying the SVGPaint object fail, as we don't support
// invalidation of the style, if a CSSValue changes, it's also dangerous, as the CSSValues are possibly shared.
// We would need copy-on-write support for CSSValue first, to make this work. It's low priority though.
// No-one supports modifying the SVGPaint object.

function resetStyle() {
    debug("");
    debug("Reset style to initial value");
    rectElement.setAttribute("style", "color: lime; fill: green");
    shouldBeEqualToString("(fillPaint = rectElement.style.getPropertyCSSValue('fill')).toString()", "[object SVGPaint]");
    shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_RGBCOLOR");
    shouldBeEqualToString("rectElement.style.fill", "#008000");
    shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "#008000");
}

function checkFillColor(type, red, green, blue) {
    shouldBe("fillPaint.colorType", type);
    shouldBeEqualToString("(fillColor = fillPaint.rgbColor).toString()", "[object RGBColor]");
    shouldBe("fillColor.red.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + red);
    shouldBe("fillColor.green.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + green);
    shouldBe("fillColor.blue.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + blue);
}

debug("");
debug("Check initial paint values");
shouldBeEqualToString("(fillPaint = rectElement.style.getPropertyCSSValue('fill')).toString()", "[object SVGPaint]");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_RGBCOLOR");
shouldBeEqualToString("fillPaint.uri", "");
checkFillColor("SVGColor.SVG_COLORTYPE_RGBCOLOR", 0, 128, 0);
shouldBeEqualToString("rectElement.style.fill", "#008000");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "#008000");

debug("");
debug("Try invalid arguments for setPaint()");
shouldThrow("fillPaint.setPaint(null, null, null, null)");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_RGBCOLOR, svgElement, '', '');");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_RGBCOLOR, '', '')");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR + 1, '', '', '');");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_NONE - 1, '', '', '');");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI + 1, '', '', '');");
shouldThrow("fillPaint.setPaint()");
shouldThrow("fillPaint.setPaint(fillPaint)");

debug("");
debug("Try invalid arguments for setUri()");
shouldThrow("fillPaint.setUri()");

debug("");
debug("Try assigning to the readonly paintType property, which silently fails");
shouldBeUndefined("fillPaint.paintType = SVGPaint.SVG_PAINTTYPE_UNKKNOWN;");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_RGBCOLOR");
shouldBeEqualToString("rectElement.style.fill", "#008000");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "#008000");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_UNKNOWN");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_UNKKNOWN, '', '', '')");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_UNKKNOWN, 'url(#foo)', '', '')");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_UNKKNOWN, '', 'rgb(0,128,128)', '')");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_UNKKNOWN, '', '', 'icc-color(myRGB, 0, 1, 2)')");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_UNKKNOWN, 'url(#foo)', 'rgb(0,0,0)', 'icc-color(myRGB, 0, 1, 2)')");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_NONE - a");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_NONE, '', '', '')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_NONE");
shouldBeEqualToString("rectElement.style.fill", "none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "none");
resetStyle();

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_NONE - b");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_NONE, 'url(#foo)', '', '')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_NONE");
shouldBeEqualToString("rectElement.style.fill", "none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "none");
resetStyle();

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_NONE - c");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_NONE, '', 'rgb(0,128,128)', '')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_NONE");
shouldBeEqualToString("rectElement.style.fill", "none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "none");
resetStyle();

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_NONE - d");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_NONE, '', '', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_NONE");
shouldBeEqualToString("rectElement.style.fill", "none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "none");
resetStyle();

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_NONE - e");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_NONE, 'url(#foo)', 'rgb(0,0,0)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBeEqualToString("rectElement.style.fill", "none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "none");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI - a");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI, '', '', '')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_NONE");
shouldBeEqualToString("rectElement.style.fill", "none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "none");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI - b");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI, '', 'rgb(0,128,128)', '')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_NONE");
shouldBeEqualToString("rectElement.style.fill", "none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "none");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI - c");
shouldThrow("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI, '', '', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_NONE");
shouldBeEqualToString("rectElement.style.fill", "none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "none");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI - d");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI, 'url(#test)', '', '')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_URI");
shouldBeEqualToString("fillPaint.uri", "url(#test)");
checkFillColor("SVGColor.SVG_COLORTYPE_UNKNOWN", 0, 0, 0);
shouldBeEqualToString("rectElement.style.fill", "url(#test)");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "url(#test)");
resetStyle();

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI - e");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI, 'url(#foo)', 'rgb(0,0,0)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_URI");
shouldBeEqualToString("fillPaint.uri", "url(#foo)");
checkFillColor("SVGColor.SVG_COLORTYPE_UNKNOWN", 0, 0, 0);
shouldBeEqualToString("rectElement.style.fill", "url(#foo)");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "url(#foo)");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI_NONE");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI_NONE, 'url(#test)', 'rgb(0,0,0)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_URI_NONE");
shouldBeEqualToString("fillPaint.uri", "url(#test)");
checkFillColor("SVGColor.SVG_COLORTYPE_UNKNOWN", 0, 0, 0);
shouldBeEqualToString("rectElement.style.fill", "url(#test) none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "url(#test) none");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI_CURRENTCOLOR");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI_CURRENTCOLOR, 'url(#foo)', 'rgb(0,0,0)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_URI_CURRENTCOLOR");
shouldBeEqualToString("fillPaint.uri", "url(#foo)");
checkFillColor("SVGColor.SVG_COLORTYPE_CURRENTCOLOR", 0, 0, 0);
shouldBeEqualToString("rectElement.style.fill", "url(#foo) currentColor");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "url(#foo) #00ff00");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI_RGBCOLOR");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI_RGBCOLOR, 'url(#test)', 'rgb(77,0,77)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_URI_RGBCOLOR");
shouldBeEqualToString("fillPaint.uri", "url(#test)");
checkFillColor("SVGColor.SVG_COLORTYPE_RGBCOLOR", 77, 0, 77);
shouldBeEqualToString("rectElement.style.fill", "url(#test) #4d004d");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "url(#test) #4d004d");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR, 'url(#foo)', 'rgb(77,0,77)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR");
shouldBeEqualToString("fillPaint.uri", "url(#foo)");
// FIXME: No support for ICC colors.
checkFillColor("SVGColor.SVG_COLORTYPE_RGBCOLOR_ICCCOLOR", 77, 0, 77);
shouldBeEqualToString("rectElement.style.fill", "url(#foo) #4d004d");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "url(#foo) #4d004d");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR, 'url(#test)', 'rgb(77,0,77)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR");
shouldBeEqualToString("fillPaint.uri", "");
// FIXME: No support for ICC colors.
checkFillColor("SVGColor.SVG_COLORTYPE_RGBCOLOR_ICCCOLOR", 77, 0, 77);
shouldBeEqualToString("rectElement.style.fill", "#4d004d");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "#4d004d");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_CURRENTCOLOR");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_CURRENTCOLOR, 'url(#foo)', 'rgb(77,0,77)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_CURRENTCOLOR");
shouldBeEqualToString("fillPaint.uri", "");
checkFillColor("SVGColor.SVG_COLORTYPE_CURRENTCOLOR", 0, 0, 0);
shouldBeEqualToString("rectElement.style.fill", "currentColor");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "#00ff00");

debug("");
debug("Test using setPaint() and SVG_PAINTTYPE_RGBCOLOR");
shouldBeUndefined("fillPaint.setPaint(SVGPaint.SVG_PAINTTYPE_RGBCOLOR, 'url(#test)', 'rgb(0,77,0)', 'icc-color(myRGB, 0, 1, 2)')");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_RGBCOLOR");
shouldBeEqualToString("fillPaint.uri", "");
checkFillColor("SVGColor.SVG_COLORTYPE_RGBCOLOR", 0, 77, 0);
shouldBeEqualToString("rectElement.style.fill", "#004d00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "#004d00");

debug("");
debug("Test using setUri()");
shouldBeUndefined("fillPaint.setUri('url(#foobar)');");
shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_URI_NONE");
shouldBeEqualToString("fillPaint.uri", "url(#foobar)");
checkFillColor("SVGColor.SVG_COLORTYPE_UNKNOWN", 0, 0, 0);
shouldBeEqualToString("rectElement.style.fill", "url(#foobar) none");
shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement).fill", "url(#foobar) none");

successfullyParsed = true;

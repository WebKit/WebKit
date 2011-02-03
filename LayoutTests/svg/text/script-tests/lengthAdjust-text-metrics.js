// Test that assures that SVGTextContentElement API takes lengthAdjust & co into account.
description("Complete test of the SVGTextContentElement API");

// Prepare testcase
var svgNS = "http://www.w3.org/2000/svg";

var svgRoot = document.createElementNS(svgNS, "svg");
document.documentElement.appendChild(svgRoot);

var svgText = document.createElementNS(svgNS, "text");
svgText.style.fontFamily = "Arial";
svgText.style.fontSize = "20px";
svgText.setAttribute("x", "10");
svgText.setAttribute("y", "20");
svgText.setAttribute("lengthAdjust", "spacingAndGlyphs");
svgText.setAttribute("textLength", "200");
svgText.appendChild(document.createTextNode("Test"));
svgRoot.appendChild(svgText);

// Helper functions
function pointToString(point) {
    return "(" + point.x.toFixed(1) + "," + point.y.toFixed(1) + ")";
}

function rectToString(rect) {
    return "(" + rect.x.toFixed(1) + "," + rect.y.toFixed(1) + ")-(" + rect.width.toFixed(1) + "x" + rect.height.toFixed(1) + ")";
}

debug("Test SVGTextContentElement SVG DOM properties");
shouldBeEqualToString("svgText.textLength.baseVal.value.toFixed(1)", "200.0");
shouldBe("svgText.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");

debug("\nTest getNumberOfChars() API");
shouldBe("svgText.getNumberOfChars()", "4");

debug("\nTest getComputedTextLength() API");
shouldBeEqualToString("svgText.getComputedTextLength().toFixed(1)", "200.0");

debug("\nTest getSubStringLength() API");
shouldBeEqualToString("svgText.getSubStringLength(0, 1).toFixed(1)", "61.5");
shouldBeEqualToString("svgText.getSubStringLength(0, 2).toFixed(1)", "117.9");
shouldBeEqualToString("svgText.getSubStringLength(0, 3).toFixed(1)", "169.2");;
shouldBeEqualToString("svgText.getSubStringLength(0, 4).toFixed(1)", "200.0");
shouldBeEqualToString("svgText.getSubStringLength(1, 1).toFixed(1)", "56.4");
shouldBeEqualToString("svgText.getSubStringLength(1, 2).toFixed(1)", "107.7");
shouldBeEqualToString("svgText.getSubStringLength(1, 3).toFixed(1)", "138.5");
shouldBeEqualToString("svgText.getSubStringLength(2, 1).toFixed(1)", "51.3");
shouldBeEqualToString("svgText.getSubStringLength(2, 2).toFixed(1)", "82.1");
shouldBeEqualToString("svgText.getSubStringLength(3, 1).toFixed(1)", "30.8");

debug("\nTest getStartPositionOfChar() API");
shouldBeEqualToString("pointToString(svgText.getStartPositionOfChar(0))", "(10.0,20.0)");
shouldBeEqualToString("pointToString(svgText.getStartPositionOfChar(1))", "(71.5,20.0)");
shouldBeEqualToString("pointToString(svgText.getStartPositionOfChar(2))", "(127.9,20.0)");
shouldBeEqualToString("pointToString(svgText.getStartPositionOfChar(3))", "(179.2,20.0)");

debug("\nTest getEndPositionOfChar() API");
shouldBeEqualToString("pointToString(svgText.getEndPositionOfChar(0))", "(71.5,20.0)");
shouldBeEqualToString("pointToString(svgText.getEndPositionOfChar(1))", "(127.9,20.0)");
shouldBeEqualToString("pointToString(svgText.getEndPositionOfChar(2))", "(179.2,20.0)");
shouldBeEqualToString("pointToString(svgText.getEndPositionOfChar(3))", "(210.0,20.0)");

debug("\nTest getExtentOfChar() API");
shouldBeEqualToString("rectToString(svgText.getExtentOfChar(0))", "(10.0,1.9)-(61.5x22.3)");
shouldBeEqualToString("rectToString(svgText.getExtentOfChar(1))", "(71.5,1.9)-(56.4x22.3)");
shouldBeEqualToString("rectToString(svgText.getExtentOfChar(2))", "(127.9,1.9)-(51.3x22.3)");
shouldBeEqualToString("rectToString(svgText.getExtentOfChar(3))", "(179.2,1.9)-(30.8x22.3)");

debug("\nTest getRotationOfChar() API");
shouldBeEqualToString("svgText.getRotationOfChar(0).toFixed(1)", "0.0");
shouldBeEqualToString("svgText.getRotationOfChar(1).toFixed(1)", "0.0");
shouldBeEqualToString("svgText.getRotationOfChar(2).toFixed(1)", "0.0");
shouldBeEqualToString("svgText.getRotationOfChar(3).toFixed(1)", "0.0");

var point = svgRoot.createSVGPoint();
point.y = 10.0;

debug("\nTest getCharNumAtPosition() API");

point.x = 0.0;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "-1");

point.x = 9.9;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "-1");

point.x = 10.1;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "0");

point.x = 71.4;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "0");

point.x = 71.6;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "1");

point.x = 127.8;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "1");

point.x = 128.0;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "2");

point.x = 179.1;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "2");

point.x = 179.3;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "3");

point.x = 209.9;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "3");

point.x = 210.1;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "-1");

point.x = 250.0;
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "-1");

// Cleanup
document.documentElement.removeChild(svgRoot);
var successfullyParsed = true;

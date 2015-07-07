description("This test checks the SVGPoint API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var point = svgElement.createSVGPoint();

debug("");
debug("Check initial point values");
shouldBe("point.x", "0");
shouldBe("point.y", "0");

debug("");
debug("Check assigning points");
shouldBe("point.x = 100", "100");
shouldBe("point.y = 200", "200");

debug("");
debug("Check assigning invalid points");
shouldBe("point.x = point", "point");
shouldBeNull("point.y = null");

debug("");
debug("Check that the point contains the correct values");
shouldBe("point.x", "NaN");
shouldBe("point.y", "0");

debug("");
debug("Reset to -50, 100");
point.x = -50;
point.y = 100;

debug("");
debug("Check 'matrixTransform' method - multiply with -1,0,0,2,10,10 matrix, should flip x coordinate, multiply y by two and translate each coordinate by 10");
var ctm = svgElement.createSVGMatrix();
ctm.a = -1;
ctm.d = 2;
ctm.e = 10;
ctm.f = 10;
shouldBeEqualToString("(newPoint = point.matrixTransform(ctm)).toString()", "[object SVGPoint]");
shouldBe("newPoint.x", "60");
shouldBe("newPoint.y", "210");

debug("");
debug("Check invalid arguments for 'matrixTransform'");
shouldThrow("point.matrixTransform()");
shouldThrow("point.matrixTransform(-1)");
shouldThrow("point.matrixTransform(5)");
shouldThrow("point.matrixTransform('aString')");
shouldThrow("point.matrixTransform(point)");
shouldThrow("point.matrixTransform(svgElement)");

successfullyParsed = true;

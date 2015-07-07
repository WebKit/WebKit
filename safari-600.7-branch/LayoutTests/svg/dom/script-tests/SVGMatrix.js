description("This test checks the SVGMatrix API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var matrix = svgElement.createSVGMatrix();

debug("");
debug("Check initial matrix values");
shouldBe("matrix.a", "1");
shouldBe("matrix.b", "0");
shouldBe("matrix.c", "0");
shouldBe("matrix.d", "1");
shouldBe("matrix.e", "0");
shouldBe("matrix.f", "0");

debug("");
debug("Check assigning matrices");
shouldBe("matrix.a = 2", "2");
shouldBe("matrix.f = 200", "200");

debug("");
debug("Check assigning invalid matrices");
shouldBe("matrix.a = matrix", "matrix");
shouldBe("matrix.a", "NaN");
shouldBe("matrix.a = 0", "0");
shouldBe("matrix.a = svgElement", "svgElement");
shouldBe("matrix.a", "NaN");
shouldBe("matrix.a = 0", "0");
shouldBe("matrix.a = 'aString'", "'aString'");
shouldBe("matrix.a", "NaN");
// Reset to previous value.
shouldBe("matrix.a = 2", "2");

shouldBe("matrix.b = matrix", "matrix");
shouldBe("matrix.b", "NaN");
shouldBe("matrix.b = 0", "0");
shouldBe("matrix.b = svgElement", "svgElement");
shouldBe("matrix.b", "NaN");
shouldBe("matrix.b = 0", "0");
shouldBe("matrix.b = 'aString'", "'aString'");
shouldBe("matrix.b", "NaN");
// Reset to previous value.
shouldBe("matrix.b = 0", "0");

shouldBe("matrix.c = matrix", "matrix");
shouldBe("matrix.c", "NaN");
shouldBe("matrix.c = 0", "0");
shouldBe("matrix.c = svgElement", "svgElement");
shouldBe("matrix.c", "NaN");
shouldBe("matrix.c = 0", "0");
shouldBe("matrix.c = 'aString'", "'aString'");
shouldBe("matrix.c", "NaN");
// Reset to previous value.
shouldBe("matrix.c = 0", "0");

shouldBe("matrix.d = matrix", "matrix");
shouldBe("matrix.d", "NaN");
shouldBe("matrix.d = 0", "0");
shouldBe("matrix.d = svgElement", "svgElement");
shouldBe("matrix.d", "NaN");
shouldBe("matrix.d = 0", "0");
shouldBe("matrix.d = 'aString'", "'aString'");
shouldBe("matrix.d", "NaN");
// Reset to previous value.
shouldBe("matrix.d = 1", "1");

shouldBe("matrix.e = matrix", "matrix");
shouldBe("matrix.e", "NaN");
shouldBe("matrix.e = 0", "0");
shouldBe("matrix.e = svgElement", "svgElement");
shouldBe("matrix.e", "NaN");
shouldBe("matrix.e = 0", "0");
shouldBe("matrix.e = 'aString'", "'aString'");
shouldBe("matrix.e", "NaN");
// Reset to previous value.
shouldBe("matrix.e = 0", "0");

shouldBe("matrix.f = matrix", "matrix");
shouldBe("matrix.f", "NaN");
shouldBe("matrix.f = 0", "0");
shouldBe("matrix.f = svgElement", "svgElement");
shouldBe("matrix.f", "NaN");
shouldBe("matrix.f = 0", "0");
shouldBe("matrix.f = 'aString'", "'aString'");
shouldBe("matrix.f", "NaN");
// Reset to previous value.
shouldBe("matrix.f = 200", "200");

debug("");
debug("Check that the matrix is still containing the correct values");
shouldBe("matrix.a", "2");
shouldBe("matrix.b", "0");
shouldBe("matrix.c", "0");
shouldBe("matrix.d", "1");
shouldBe("matrix.e", "0");
shouldBe("matrix.f", "200");

debug("");
debug("Check assigning null works as expected");
shouldBeNull("matrix.f = null");
shouldBe("matrix.a", "2");
shouldBe("matrix.b", "0");
shouldBe("matrix.c", "0");
shouldBe("matrix.d", "1");
shouldBe("matrix.e", "0");
shouldBe("matrix.f", "0");

debug("");
debug("Check calling 'multiply' with invalid arguments");
shouldThrow("matrix.multiply()");
shouldThrow("matrix.multiply(true)");
shouldThrow("matrix.multiply(2)");
shouldThrow("matrix.multiply('aString')");
shouldThrow("matrix.multiply(svgElement)");

debug("");
debug("Check calling 'translate' with invalid arguments");
shouldThrow("matrix.translate()");
shouldThrow("matrix.translate(true)");
shouldThrow("matrix.translate(2)");
shouldThrow("matrix.translate('aString')");
shouldThrow("matrix.translate(svgElement)");
// The following string and object arguments convert to NaN
// per ECMA-262, 9.3, "ToNumber".
shouldBeNonNull("matrix.translate('aString', 'aString')");
shouldBeNonNull("matrix.translate(svgElement, svgElement)");
shouldBeNonNull("matrix.translate(2, 'aString')");
shouldBeNonNull("matrix.translate(2, svgElement)");
shouldBeNonNull("matrix.translate('aString', 2)");
shouldBeNonNull("matrix.translate(svgElement, 2)");

debug("");
debug("Check calling 'scale' with invalid arguments");
shouldThrow("matrix.scale()");
shouldBeNonNull("matrix.scale('aString')");
shouldBeNonNull("matrix.scale(svgElement)");


debug("");
debug("Check calling 'scaleNonUniform' with invalid arguments");
shouldThrow("matrix.scaleNonUniform()");
shouldThrow("matrix.scaleNonUniform(true)");
shouldThrow("matrix.scaleNonUniform(2)");
shouldThrow("matrix.scaleNonUniform('aString')");
shouldThrow("matrix.scaleNonUniform(svgElement)");
shouldBeNonNull("matrix.scaleNonUniform('aString', 'aString')");
shouldBeNonNull("matrix.scaleNonUniform(svgElement, svgElement)");
shouldBeNonNull("matrix.scaleNonUniform(2, 'aString')");
shouldBeNonNull("matrix.scaleNonUniform(2, svgElement)");
shouldBeNonNull("matrix.scaleNonUniform('aString', 2)");
shouldBeNonNull("matrix.scaleNonUniform(svgElement, 2)");

debug("");
debug("Check calling 'rotate' with invalid arguments");
shouldThrow("matrix.rotate()");
shouldBeNonNull("matrix.rotate('aString')");
shouldBeNonNull("matrix.rotate(svgElement)");

debug("");
debug("Check calling 'rotateFromVector' with invalid arguments");
shouldThrow("matrix.rotateFromVector()");
shouldThrow("matrix.rotateFromVector(true)");
shouldThrow("matrix.rotateFromVector(2)");
shouldThrow("matrix.rotateFromVector('aString')");
shouldThrow("matrix.rotateFromVector(svgElement)");
shouldBeNonNull("matrix.rotateFromVector('aString', 'aString')");
shouldBeNonNull("matrix.rotateFromVector(svgElement, svgElement)");
shouldBeNonNull("matrix.rotateFromVector(2, 'aString')");
shouldBeNonNull("matrix.rotateFromVector(2, svgElement)");
shouldBeNonNull("matrix.rotateFromVector('aString', 2)");
shouldBeNonNull("matrix.rotateFromVector(svgElement, 2)");

debug("");
debug("Check calling 'skewX' with invalid arguments");
shouldThrow("matrix.skewX()");
shouldBeNonNull("matrix.skewX('aString')");
shouldBeNonNull("matrix.skewX(svgElement)");

debug("");
debug("Check calling 'skewY' with invalid arguments");
shouldThrow("matrix.skewY()");
shouldBeNonNull("matrix.skewY('aString')");
shouldBeNonNull("matrix.skewY(svgElement)");

successfullyParsed = true;

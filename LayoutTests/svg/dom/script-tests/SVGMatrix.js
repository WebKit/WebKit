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
shouldThrow("matrix.a = matrix");
shouldThrow("matrix.a = svgElement");
shouldThrow("matrix.a = 'aString'");

shouldThrow("matrix.b = matrix");
shouldThrow("matrix.b = svgElement");
shouldThrow("matrix.b = 'aString'");

shouldThrow("matrix.c = matrix");
shouldThrow("matrix.c = svgElement");
shouldThrow("matrix.c = 'aString'");

shouldThrow("matrix.d = matrix");
shouldThrow("matrix.d = svgElement");
shouldThrow("matrix.d = 'aString'");

shouldThrow("matrix.e = matrix");
shouldThrow("matrix.e = svgElement");
shouldThrow("matrix.e = 'aString'");

shouldThrow("matrix.f = matrix");
shouldThrow("matrix.f = svgElement");
shouldThrow("matrix.f = 'aString'");

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
shouldThrow("matrix.translate('aString', 'aString')");
shouldThrow("matrix.translate(svgElement, svgElement)");
shouldThrow("matrix.translate(2, 'aString')");
shouldThrow("matrix.translate(2, svgElement)");
shouldThrow("matrix.translate('aString', 2)");
shouldThrow("matrix.translate(svgElement, 2)");

debug("");
debug("Check calling 'scale' with invalid arguments");
shouldThrow("matrix.scale()");
shouldThrow("matrix.scale('aString')");
shouldThrow("matrix.scale(svgElement)");


debug("");
debug("Check calling 'scaleNonUniform' with invalid arguments");
shouldThrow("matrix.scaleNonUniform()");
shouldThrow("matrix.scaleNonUniform(true)");
shouldThrow("matrix.scaleNonUniform(2)");
shouldThrow("matrix.scaleNonUniform('aString')");
shouldThrow("matrix.scaleNonUniform(svgElement)");
shouldThrow("matrix.scaleNonUniform('aString', 'aString')");
shouldThrow("matrix.scaleNonUniform(svgElement, svgElement)");
shouldThrow("matrix.scaleNonUniform(2, 'aString')");
shouldThrow("matrix.scaleNonUniform(2, svgElement)");
shouldThrow("matrix.scaleNonUniform('aString', 2)");
shouldThrow("matrix.scaleNonUniform(svgElement, 2)");

debug("");
debug("Check calling 'rotate' with invalid arguments");
shouldThrow("matrix.rotate()");
shouldThrow("matrix.rotate('aString')");
shouldThrow("matrix.rotate(svgElement)");

debug("");
debug("Check calling 'rotateFromVector' with invalid arguments");
shouldThrow("matrix.rotateFromVector()");
shouldThrow("matrix.rotateFromVector(true)");
shouldThrow("matrix.rotateFromVector(2)");
shouldThrow("matrix.rotateFromVector('aString')");
shouldThrow("matrix.rotateFromVector(svgElement)");
shouldThrow("matrix.rotateFromVector('aString', 'aString')");
shouldThrow("matrix.rotateFromVector(svgElement, svgElement)");
shouldThrow("matrix.rotateFromVector(2, 'aString')");
shouldThrow("matrix.rotateFromVector(2, svgElement)");
shouldThrow("matrix.rotateFromVector('aString', 2)");
shouldThrow("matrix.rotateFromVector(svgElement, 2)");

debug("");
debug("Check calling 'skewX' with invalid arguments");
shouldThrow("matrix.skewX()");
shouldThrow("matrix.skewX('aString')");
shouldThrow("matrix.skewX(svgElement)");

debug("");
debug("Check calling 'skewY' with invalid arguments");
shouldThrow("matrix.skewY()");
shouldThrow("matrix.skewY('aString')");
shouldThrow("matrix.skewY(svgElement)");

successfullyParsed = true;

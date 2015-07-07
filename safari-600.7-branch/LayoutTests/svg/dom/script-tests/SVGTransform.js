description("This test checks the SVGTransform API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var transform = svgElement.createSVGTransform();

debug("");
debug("Check initial transform values");
shouldBe("transform.type", "SVGTransform.SVG_TRANSFORM_MATRIX");
shouldBe("transform.angle", "0");
shouldBe("transform.matrix.a", "1");
shouldBe("transform.matrix.b", "0");
shouldBe("transform.matrix.c", "0");
shouldBe("transform.matrix.d", "1");
shouldBe("transform.matrix.e", "0");
shouldBe("transform.matrix.f", "0");

debug("");
debug("Change to skewX transformation");
shouldBeUndefined("transform.setSkewX(45)");
shouldBe("transform.type", "SVGTransform.SVG_TRANSFORM_SKEWX");
shouldBe("transform.angle", "45");
shouldBe("transform.matrix.a", "1");
shouldBe("transform.matrix.b", "0");
shouldBeEqualToString("transform.matrix.c.toFixed(1)", "1.0");
shouldBe("transform.matrix.d", "1");
shouldBe("transform.matrix.e", "0");
shouldBe("transform.matrix.f", "0");

debug("");
debug("Changing matrix.e to 100, should reset transformation type to MATRIX, and angle should be 0");
shouldBe("transform.matrix.e = 100", "100");
shouldBe("transform.type", "SVGTransform.SVG_TRANSFORM_MATRIX");
shouldBe("transform.angle", "0");
shouldBe("transform.matrix.a", "1");
shouldBe("transform.matrix.b", "0");
shouldBeEqualToString("transform.matrix.c.toFixed(1)", "1.0");
shouldBe("transform.matrix.d", "1");
shouldBe("transform.matrix.e", "100");
shouldBe("transform.matrix.f", "0");

debug("");
debug("Now revert to initial matrix");
shouldBeNull("transform.matrix.c = null");
shouldBe("transform.matrix.e = 0", "0");
shouldBe("transform.type", "SVGTransform.SVG_TRANSFORM_MATRIX");
shouldBe("transform.angle", "0");
shouldBe("transform.matrix.a", "1");
shouldBe("transform.matrix.b", "0");
shouldBe("transform.matrix.c", "0");
shouldBe("transform.matrix.d", "1");
shouldBe("transform.matrix.e", "0");
shouldBe("transform.matrix.f", "0");

debug("");
debug("Check passing invalid arguments to 'setMatrix'");
shouldThrow("transform.setMatrix()");
shouldThrow("transform.setMatrix(transform)");
shouldThrow("transform.setMatrix(svgElement)");
shouldThrow("transform.setMatrix('aString')");
shouldThrow("transform.setMatrix(1)");
shouldThrow("transform.setMatrix(false)");

debug("");
debug("Check passing invalid arguments to 'setTranslate'");
shouldThrow("transform.setTranslate()");
shouldThrow("transform.setTranslate(transform)");
shouldThrow("transform.setTranslate(svgElement)");
shouldThrow("transform.setTranslate('aString')");
shouldBeUndefined("transform.setTranslate(1, transform)");
shouldBeUndefined("transform.setTranslate(1, svgElement)");
shouldBeUndefined("transform.setTranslate(1, 'aString')");
shouldBeUndefined("transform.setTranslate(transform, 1)");
shouldBeUndefined("transform.setTranslate(svgElement, 1)");
shouldBeUndefined("transform.setTranslate('aString', 1)");
shouldBeUndefined("transform.setTranslate(transform, transform)");
shouldBeUndefined("transform.setTranslate(svgElement, svgElement)");
shouldBeUndefined("transform.setTranslate('aString', 'aString')");

debug("");
debug("Check passing invalid arguments to 'setScale'");
shouldThrow("transform.setScale()");
shouldThrow("transform.setScale(transform)");
shouldThrow("transform.setScale(svgElement)");
shouldThrow("transform.setScale('aString')");
shouldBeUndefined("transform.setScale(1, transform)");
shouldBeUndefined("transform.setScale(1, svgElement)");
shouldBeUndefined("transform.setScale(1, 'aString')");
shouldBeUndefined("transform.setScale(transform, 1)");
shouldBeUndefined("transform.setScale(svgElement, 1)");
shouldBeUndefined("transform.setScale('aString', 1)");
shouldBeUndefined("transform.setScale(transform, transform)");
shouldBeUndefined("transform.setScale(svgElement, svgElement)");
shouldBeUndefined("transform.setScale('aString', 'aString')");

debug("");
debug("Check passing invalid arguments to 'setRotate'");
shouldThrow("transform.setRotate()");
shouldThrow("transform.setRotate(transform)");
shouldThrow("transform.setRotate(svgElement)");
shouldThrow("transform.setRotate('aString')");
shouldThrow("transform.setRotate(1, transform)");
shouldThrow("transform.setRotate(1, svgElement)");
shouldThrow("transform.setRotate(1, 'aString')");
shouldBeUndefined("transform.setRotate(1, 1, transform)");
shouldBeUndefined("transform.setRotate(1, 1, svgElement)");
shouldBeUndefined("transform.setRotate(1, 1, 'aString')");

debug("");
debug("Check passing invalid arguments to 'setSkewX'");
shouldThrow("transform.setSkewX()");
shouldBeUndefined("transform.setSkewX(transform)");
shouldBeUndefined("transform.setSkewX(svgElement)");
shouldBeUndefined("transform.setSkewX('aString')");

debug("");
debug("Check passing invalid arguments to 'setSkewY'");
shouldThrow("transform.setSkewY()");
shouldBeUndefined("transform.setSkewY(transform)");
shouldBeUndefined("transform.setSkewY(svgElement)");
shouldBeUndefined("transform.setSkewY('aString')");

successfullyParsed = true;

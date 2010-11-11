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
shouldThrow("transform.setTranslate(1, transform)");
shouldThrow("transform.setTranslate(1, svgElement)");
shouldThrow("transform.setTranslate(1, 'aString')");
shouldThrow("transform.setTranslate(transform, 1)");
shouldThrow("transform.setTranslate(svgElement, 1)");
shouldThrow("transform.setTranslate('aString', 1)");
shouldThrow("transform.setTranslate(transform, transform)");
shouldThrow("transform.setTranslate(svgElement, svgElement)");
shouldThrow("transform.setTranslate('aString', 'aString')");

debug("");
debug("Check passing invalid arguments to 'setScale'");
shouldThrow("transform.setScale()");
shouldThrow("transform.setScale(transform)");
shouldThrow("transform.setScale(svgElement)");
shouldThrow("transform.setScale('aString')");
shouldThrow("transform.setScale(1, transform)");
shouldThrow("transform.setScale(1, svgElement)");
shouldThrow("transform.setScale(1, 'aString')");
shouldThrow("transform.setScale(transform, 1)");
shouldThrow("transform.setScale(svgElement, 1)");
shouldThrow("transform.setScale('aString', 1)");
shouldThrow("transform.setScale(transform, transform)");
shouldThrow("transform.setScale(svgElement, svgElement)");
shouldThrow("transform.setScale('aString', 'aString')");

debug("");
debug("Check passing invalid arguments to 'setRotate'");
shouldThrow("transform.setRotate()");
shouldThrow("transform.setRotate(transform)");
shouldThrow("transform.setRotate(svgElement)");
shouldThrow("transform.setRotate('aString')");
shouldThrow("transform.setRotate(1, transform)");
shouldThrow("transform.setRotate(1, svgElement)");
shouldThrow("transform.setRotate(1, 'aString')");
shouldThrow("transform.setRotate(1, 1, transform)");
shouldThrow("transform.setRotate(1, 1, svgElement)");
shouldThrow("transform.setRotate(1, 1, 'aString')");

debug("");
debug("Check passing invalid arguments to 'setSkewX'");
shouldThrow("transform.setSkewX()");
shouldThrow("transform.setSkewX(transform)");
shouldThrow("transform.setSkewX(svgElement)");
shouldThrow("transform.setSkewX('aString')");

debug("");
debug("Check passing invalid arguments to 'setSkewY'");
shouldThrow("transform.setSkewY()");
shouldThrow("transform.setSkewY(transform)");
shouldThrow("transform.setSkewY(svgElement)");
shouldThrow("transform.setSkewY('aString')");

successfullyParsed = true;

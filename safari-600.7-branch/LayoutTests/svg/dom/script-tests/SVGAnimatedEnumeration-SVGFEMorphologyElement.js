description("This test checks the use of SVGAnimatedEnumeration within SVGFEMorphologyElement");

var feMorphologyElement = document.createElementNS("http://www.w3.org/2000/svg", "feMorphology");
feMorphologyElement.setAttribute("operator", "erode");

debug("");
debug("Check initial 'operator' value");
shouldBeEqualToString("feMorphologyElement.operator.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feMorphologyElement.operator.baseVal)", "number");
shouldBe("feMorphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_ERODE");

debug("");
debug("Switch to 'dilate'");
shouldBe("feMorphologyElement.operator.baseVal = SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");
shouldBe("feMorphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");
shouldBeEqualToString("feMorphologyElement.getAttribute('operator')", "dilate");

debug("");
debug("Try setting invalid values");
shouldThrow("feMorphologyElement.operator.baseVal = 4");
shouldBe("feMorphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");
shouldBeEqualToString("feMorphologyElement.getAttribute('operator')", "dilate");

shouldThrow("feMorphologyElement.operator.baseVal = -1");
shouldBe("feMorphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");
shouldBeEqualToString("feMorphologyElement.getAttribute('operator')", "dilate");

shouldThrow("feMorphologyElement.operator.baseVal = 0");
shouldBe("feMorphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");
shouldBeEqualToString("feMorphologyElement.getAttribute('operator')", "dilate");

debug("");
debug("Switch to 'erode'");
shouldBe("feMorphologyElement.operator.baseVal = SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_ERODE", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_ERODE");
shouldBe("feMorphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_ERODE");
shouldBeEqualToString("feMorphologyElement.getAttribute('operator')", "erode");

successfullyParsed = true;

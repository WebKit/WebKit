description("This test checks the use of SVGAnimatedEnumeration within SVGFEColorMatrixElement");

var feColorMatrixElement = document.createElementNS("http://www.w3.org/2000/svg", "feColorMatrix");
feColorMatrixElement.setAttribute("type", "matrix");

debug("");
debug("Check initial 'type; value");
shouldBeEqualToString("feColorMatrixElement.type.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feColorMatrixElement.type.baseVal)", "number");
shouldBe("feColorMatrixElement.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX");

debug("");
debug("Switch to 'saturate'");
shouldBe("feColorMatrixElement.type.baseVal = SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_SATURATE", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_SATURATE");
shouldBe("feColorMatrixElement.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_SATURATE");
shouldBeEqualToString("feColorMatrixElement.getAttribute('type')", "saturate");

debug("");
debug("Switch to 'hueRotate'");
shouldBe("feColorMatrixElement.type.baseVal = SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_HUEROTATE", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_HUEROTATE");
shouldBe("feColorMatrixElement.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_HUEROTATE");
shouldBeEqualToString("feColorMatrixElement.getAttribute('type')", "hueRotate");

debug("");
debug("Switch to 'luminanceToAlpha'");
shouldBe("feColorMatrixElement.type.baseVal = SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA");
shouldBe("feColorMatrixElement.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA");
shouldBeEqualToString("feColorMatrixElement.getAttribute('type')", "luminanceToAlpha");

debug("");
debug("Try setting invalid values");
shouldThrow("feColorMatrixElement.type.baseVal = 5");
shouldBe("feColorMatrixElement.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA");
shouldBeEqualToString("feColorMatrixElement.getAttribute('type')", "luminanceToAlpha");

shouldThrow("feColorMatrixElement.type.baseVal = -1");
shouldBe("feColorMatrixElement.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA");
shouldBeEqualToString("feColorMatrixElement.getAttribute('type')", "luminanceToAlpha");

shouldThrow("feColorMatrixElement.type.baseVal = 0");
shouldBe("feColorMatrixElement.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA");
shouldBeEqualToString("feColorMatrixElement.getAttribute('type')", "luminanceToAlpha");

debug("");
debug("Switch to 'matrix'");
shouldBe("feColorMatrixElement.type.baseVal = SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX");
shouldBe("feColorMatrixElement.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX");
shouldBeEqualToString("feColorMatrixElement.getAttribute('type')", "matrix");

successfullyParsed = true;

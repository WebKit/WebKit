description("This test checks the use of SVGAnimatedEnumeration within SVGFEConvolveMatrixElement");

var feConvolveMatrixElement = document.createElementNS("http://www.w3.org/2000/svg", "feConvolveMatrix");
feConvolveMatrixElement.setAttribute("edgeMode", "duplicate");

debug("");
debug("Check initial 'edgeMode' value");
shouldBeEqualToString("feConvolveMatrixElement.edgeMode.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feConvolveMatrixElement.edgeMode.baseVal)", "number");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_DUPLICATE");

debug("");
debug("Switch to 'wrap'");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal = SVGFEConvolveMatrixElement.SVG_EDGEMODE_WRAP", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_WRAP");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_WRAP");
shouldBeEqualToString("feConvolveMatrixElement.getAttribute('edgeMode')", "wrap");

debug("");
debug("Switch to 'none'");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal = SVGFEConvolveMatrixElement.SVG_EDGEMODE_NONE", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_NONE");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_NONE");
shouldBeEqualToString("feConvolveMatrixElement.getAttribute('edgeMode')", "none");

debug("");
debug("Try setting invalid values");
shouldThrow("feConvolveMatrixElement.edgeMode.baseVal = 4");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_NONE");
shouldBeEqualToString("feConvolveMatrixElement.getAttribute('edgeMode')", "none");

shouldThrow("feConvolveMatrixElement.edgeMode.baseVal = -1");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_NONE");
shouldBeEqualToString("feConvolveMatrixElement.getAttribute('edgeMode')", "none");

shouldThrow("feConvolveMatrixElement.edgeMode.baseVal = 0");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_NONE");
shouldBeEqualToString("feConvolveMatrixElement.getAttribute('edgeMode')", "none");

debug("");
debug("Switch to 'duplicate'");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal = SVGFEConvolveMatrixElement.SVG_EDGEMODE_DUPLICATE", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_DUPLICATE");
shouldBe("feConvolveMatrixElement.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_DUPLICATE");
shouldBeEqualToString("feConvolveMatrixElement.getAttribute('edgeMode')", "duplicate");

successfullyParsed = true;

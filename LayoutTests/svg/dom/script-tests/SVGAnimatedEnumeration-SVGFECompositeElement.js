description("This test checks the use of SVGAnimatedEnumeration within SVGFECompositeElement");

var feCompositeElement = document.createElementNS("http://www.w3.org/2000/svg", "feComposite");
feCompositeElement.setAttribute("operator", "over");

debug("");
debug("Check initial 'operator' value");
shouldBeEqualToString("feCompositeElement.operator.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feCompositeElement.operator.baseVal)", "number");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_OVER");

debug("");
debug("Switch to 'in'");
shouldBe("feCompositeElement.operator.baseVal = SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_IN", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_IN");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_IN");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "in");

debug("");
debug("Switch to 'over'");
shouldBe("feCompositeElement.operator.baseVal = SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_OVER", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_OVER");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_OVER");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "over");

debug("");
debug("Switch to 'out'");
shouldBe("feCompositeElement.operator.baseVal = SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_OUT", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_OUT");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_OUT");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "out");

debug("");
debug("Switch to 'atop'");
shouldBe("feCompositeElement.operator.baseVal = SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ATOP", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ATOP");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ATOP");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "atop");

debug("");
debug("Switch to 'xor'");
shouldBe("feCompositeElement.operator.baseVal = SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_XOR", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_XOR");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_XOR");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "xor");

debug("");
debug("Switch to 'arithmetic'");
shouldBe("feCompositeElement.operator.baseVal = SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ARITHMETIC", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ARITHMETIC");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ARITHMETIC");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "arithmetic");

// Compositing modes added to SVG 2 do not expose their enumeration values through
// the IDL the way older modes did. Therefore, lighter cannot be selected by
// setting operator.baseVal and SVG_FECOMPOSITE_OPERATOR_UNKNOWN is returned
// for all new modes.

debug("");
debug("Try setting invalid values");
shouldThrow("feCompositeElement.operator.baseVal = 7");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ARITHMETIC");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "arithmetic");

shouldThrow("feCompositeElement.operator.baseVal = -1");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ARITHMETIC");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "arithmetic");

shouldThrow("feCompositeElement.operator.baseVal = 0");
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_ARITHMETIC");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "arithmetic");

debug("");
debug("Switch to 'lighter'");
feCompositeElement.setAttribute('operator', 'lighter');
shouldBe("feCompositeElement.operator.baseVal", "SVGFECompositeElement.SVG_FECOMPOSITE_OPERATOR_UNKNOWN");
shouldBeEqualToString("feCompositeElement.getAttribute('operator')", "lighter");

successfullyParsed = true;

description("This test checks the use of SVGAnimatedEnumeration within SVGFEBlendElement");

var feBlendElement = document.createElementNS("http://www.w3.org/2000/svg", "feBlend");
feBlendElement.setAttribute("mode", "normal");

debug("");
debug("Check initial 'mode' value");
shouldBeEqualToString("feBlendElement.mode.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feBlendElement.mode.baseVal)", "number");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_NORMAL");

debug("");
debug("Switch to 'multiply'");
shouldBe("feBlendElement.mode.baseVal = SVGFEBlendElement.SVG_FEBLEND_MODE_MULTIPLY", "SVGFEBlendElement.SVG_FEBLEND_MODE_MULTIPLY");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_MULTIPLY");
shouldBeEqualToString("feBlendElement.getAttribute('mode')", "multiply");

debug("");
debug("Switch to 'screen'");
shouldBe("feBlendElement.mode.baseVal = SVGFEBlendElement.SVG_FEBLEND_MODE_SCREEN", "SVGFEBlendElement.SVG_FEBLEND_MODE_SCREEN");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_SCREEN");
shouldBeEqualToString("feBlendElement.getAttribute('mode')", "screen");

debug("");
debug("Switch to 'darken'");
shouldBe("feBlendElement.mode.baseVal = SVGFEBlendElement.SVG_FEBLEND_MODE_DARKEN", "SVGFEBlendElement.SVG_FEBLEND_MODE_DARKEN");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_DARKEN");
shouldBeEqualToString("feBlendElement.getAttribute('mode')", "darken");

debug("");
debug("Switch to 'lighten'");
shouldBe("feBlendElement.mode.baseVal = SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
shouldBeEqualToString("feBlendElement.getAttribute('mode')", "lighten");

debug("");
debug("Try setting invalid values");
shouldThrow("feBlendElement.mode.baseVal = 6");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
shouldBeEqualToString("feBlendElement.getAttribute('mode')", "lighten");

shouldThrow("feBlendElement.mode.baseVal = -1");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
shouldBeEqualToString("feBlendElement.getAttribute('mode')", "lighten");

shouldThrow("feBlendElement.mode.baseVal = 0");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
shouldBeEqualToString("feBlendElement.getAttribute('mode')", "lighten");

debug("");
debug("Switch to 'normal'");
shouldBe("feBlendElement.mode.baseVal = SVGFEBlendElement.SVG_FEBLEND_MODE_NORMAL", "SVGFEBlendElement.SVG_FEBLEND_MODE_NORMAL");
shouldBe("feBlendElement.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_NORMAL");
shouldBeEqualToString("feBlendElement.getAttribute('mode')", "normal");

successfullyParsed = true;

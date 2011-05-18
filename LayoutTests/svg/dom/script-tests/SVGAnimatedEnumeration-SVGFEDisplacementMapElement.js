description("This test checks the use of SVGAnimatedEnumeration within SVGFEDisplacementMapElement");

var feDisplacementMapElement = document.createElementNS("http://www.w3.org/2000/svg", "feDisplacementMap");
feDisplacementMapElement.setAttribute("xChannelSelector", "R");
feDisplacementMapElement.setAttribute("yChannelSelector", "R");

// xChannelSelector
debug("");
debug("Check initial 'xChannelSelector' value");
shouldBeEqualToString("feDisplacementMapElement.xChannelSelector.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feDisplacementMapElement.xChannelSelector.baseVal)", "number");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_R");

debug("");
debug("Switch to 'G'");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal = SVGFEDisplacementMapElement.SVG_CHANNEL_G", "SVGFEDisplacementMapElement.SVG_CHANNEL_G");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_G");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('xChannelSelector')", "G");

debug("");
debug("Switch to 'B'");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal = SVGFEDisplacementMapElement.SVG_CHANNEL_B", "SVGFEDisplacementMapElement.SVG_CHANNEL_B");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_B");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('xChannelSelector')", "B");

debug("");
debug("Switch to 'A'");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal = SVGFEDisplacementMapElement.SVG_CHANNEL_A", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('xChannelSelector')", "A");

debug("");
debug("Try setting invalid values");
shouldThrow("feDisplacementMapElement.xChannelSelector.baseVal = 5");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('xChannelSelector')", "A");

shouldThrow("feDisplacementMapElement.xChannelSelector.baseVal = -1");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('xChannelSelector')", "A");

shouldThrow("feDisplacementMapElement.xChannelSelector.baseVal = 0");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('xChannelSelector')", "A");

debug("");
debug("Switch to 'R'");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal = SVGFEDisplacementMapElement.SVG_CHANNEL_R", "SVGFEDisplacementMapElement.SVG_CHANNEL_R");
shouldBe("feDisplacementMapElement.xChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_R");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('xChannelSelector')", "R");

// yChannelSelector
debug("");
debug("Check initial 'yChannelSelector' value");
shouldBeEqualToString("feDisplacementMapElement.yChannelSelector.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feDisplacementMapElement.yChannelSelector.baseVal)", "number");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_R");

debug("");
debug("Switch to 'G'");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal = SVGFEDisplacementMapElement.SVG_CHANNEL_G", "SVGFEDisplacementMapElement.SVG_CHANNEL_G");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_G");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('yChannelSelector')", "G");

debug("");
debug("Switch to 'B'");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal = SVGFEDisplacementMapElement.SVG_CHANNEL_B", "SVGFEDisplacementMapElement.SVG_CHANNEL_B");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_B");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('yChannelSelector')", "B");

debug("");
debug("Switch to 'A'");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal = SVGFEDisplacementMapElement.SVG_CHANNEL_A", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('yChannelSelector')", "A");

debug("");
debug("Try setting invalid values");
shouldThrow("feDisplacementMapElement.yChannelSelector.baseVal = 5");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('yChannelSelector')", "A");

shouldThrow("feDisplacementMapElement.yChannelSelector.baseVal = -1");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('yChannelSelector')", "A");

shouldThrow("feDisplacementMapElement.yChannelSelector.baseVal = 0");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_A");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('yChannelSelector')", "A");

debug("");
debug("Switch to 'R'");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal = SVGFEDisplacementMapElement.SVG_CHANNEL_R", "SVGFEDisplacementMapElement.SVG_CHANNEL_R");
shouldBe("feDisplacementMapElement.yChannelSelector.baseVal", "SVGFEDisplacementMapElement.SVG_CHANNEL_R");
shouldBeEqualToString("feDisplacementMapElement.getAttribute('yChannelSelector')", "R");

successfullyParsed = true;

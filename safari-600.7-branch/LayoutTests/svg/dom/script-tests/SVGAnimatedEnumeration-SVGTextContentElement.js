description("This test checks the use of SVGAnimatedEnumeration within SVGTextContentElement");

var textContentElement = document.createElementNS("http://www.w3.org/2000/svg", "text");
textContentElement.setAttribute("lengthAdjust", "spacing");

debug("");
debug("Check initial 'lengthAdjust' value");
shouldBeEqualToString("textContentElement.lengthAdjust.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(textContentElement.lengthAdjust.baseVal)", "number");
shouldBe("textContentElement.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACING");

debug("");
debug("Switch to 'spacingAndGlyphs'");
shouldBe("textContentElement.lengthAdjust.baseVal = SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");
shouldBe("textContentElement.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");
shouldBeEqualToString("textContentElement.getAttribute('lengthAdjust')", "spacingAndGlyphs");

debug("");
debug("Try setting invalid values");
shouldThrow("textContentElement.lengthAdjust.baseVal = 3");
shouldBe("textContentElement.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");
shouldBeEqualToString("textContentElement.getAttribute('lengthAdjust')", "spacingAndGlyphs");

shouldThrow("textContentElement.lengthAdjust.baseVal = -1");
shouldBe("textContentElement.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");
shouldBeEqualToString("textContentElement.getAttribute('lengthAdjust')", "spacingAndGlyphs");

shouldThrow("textContentElement.lengthAdjust.baseVal = 0");
shouldBe("textContentElement.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");
shouldBeEqualToString("textContentElement.getAttribute('lengthAdjust')", "spacingAndGlyphs");

debug("");
debug("Switch to 'spacing'");
shouldBe("textContentElement.lengthAdjust.baseVal = SVGTextContentElement.LENGTHADJUST_SPACING", "SVGTextContentElement.LENGTHADJUST_SPACING");
shouldBe("textContentElement.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACING");
shouldBeEqualToString("textContentElement.getAttribute('lengthAdjust')", "spacing");

successfullyParsed = true;

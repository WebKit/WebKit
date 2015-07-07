description("This test checks the use of SVGAnimatedEnumeration within SVGTextPathElement");

var textPathElement = document.createElementNS("http://www.w3.org/2000/svg", "textPath");
textPathElement.setAttribute("method", "align");
textPathElement.setAttribute("spacing", "auto");

// method
debug("");
debug("Check initial 'method' value");
shouldBeEqualToString("textPathElement.method.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(textPathElement.method.baseVal)", "number");
shouldBe("textPathElement.method.baseVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_ALIGN");

debug("");
debug("Switch to 'stretch'");
shouldBe("textPathElement.method.baseVal = SVGTextPathElement.TEXTPATH_METHODTYPE_STRETCH", "SVGTextPathElement.TEXTPATH_METHODTYPE_STRETCH");
shouldBe("textPathElement.method.baseVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_STRETCH");
shouldBeEqualToString("textPathElement.getAttribute('method')", "stretch");

debug("");
debug("Try setting invalid values");
shouldThrow("textPathElement.method.baseVal = 3");
shouldBe("textPathElement.method.baseVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_STRETCH");
shouldBeEqualToString("textPathElement.getAttribute('method')", "stretch");

shouldThrow("textPathElement.method.baseVal = -1");
shouldBe("textPathElement.method.baseVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_STRETCH");
shouldBeEqualToString("textPathElement.getAttribute('method')", "stretch");

shouldThrow("textPathElement.method.baseVal = 0");
shouldBe("textPathElement.method.baseVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_STRETCH");
shouldBeEqualToString("textPathElement.getAttribute('method')", "stretch");

debug("");
debug("Switch to 'align'");
shouldBe("textPathElement.method.baseVal = SVGTextPathElement.TEXTPATH_METHODTYPE_ALIGN", "SVGTextPathElement.TEXTPATH_METHODTYPE_ALIGN");
shouldBe("textPathElement.method.baseVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_ALIGN");
shouldBeEqualToString("textPathElement.getAttribute('method')", "align");

// spacing
debug("");
debug("Check initial 'spacing' value");
shouldBeEqualToString("textPathElement.spacing.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(textPathElement.spacing.baseVal)", "number");
shouldBe("textPathElement.spacing.baseVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_AUTO");

debug("");
debug("Switch to 'exact'");
shouldBe("textPathElement.spacing.baseVal = SVGTextPathElement.TEXTPATH_SPACINGTYPE_EXACT", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_EXACT");
shouldBe("textPathElement.spacing.baseVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_EXACT");
shouldBeEqualToString("textPathElement.getAttribute('spacing')", "exact");

debug("");
debug("Try setting invalid values");
shouldThrow("textPathElement.spacing.baseVal = 3");
shouldBe("textPathElement.spacing.baseVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_EXACT");
shouldBeEqualToString("textPathElement.getAttribute('spacing')", "exact");

shouldThrow("textPathElement.spacing.baseVal = -1");
shouldBe("textPathElement.spacing.baseVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_EXACT");
shouldBeEqualToString("textPathElement.getAttribute('spacing')", "exact");

shouldThrow("textPathElement.spacing.baseVal = 0");
shouldBe("textPathElement.spacing.baseVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_EXACT");
shouldBeEqualToString("textPathElement.getAttribute('spacing')", "exact");

debug("");
debug("Switch to 'auto'");
shouldBe("textPathElement.spacing.baseVal = SVGTextPathElement.TEXTPATH_SPACINGTYPE_AUTO", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_AUTO");
shouldBe("textPathElement.spacing.baseVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_AUTO");
shouldBeEqualToString("textPathElement.getAttribute('spacing')", "auto");

successfullyParsed = true;

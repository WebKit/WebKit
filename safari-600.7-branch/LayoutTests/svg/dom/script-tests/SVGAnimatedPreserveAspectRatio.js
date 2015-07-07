description("This test checks the SVGAnimatedPreserveAspectRatio API - utilizing the preserveAspectRatio property of SVGSVGElement");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");

debug("");
debug("Check initial preserveAspectRatio value");
shouldBeEqualToString("svgElement.preserveAspectRatio.toString()", "[object SVGAnimatedPreserveAspectRatio]");
shouldBeEqualToString("svgElement.preserveAspectRatio.baseVal.toString()", "[object SVGPreserveAspectRatio]");
shouldBe("svgElement.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMIDYMID");
shouldBe("svgElement.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_MEET");

debug("");
debug("Check that preserveAspectRatios are dynamic, caching value in a local variable and modifying it, should take effect");
var aspectRef = svgElement.preserveAspectRatio.baseVal;
aspectRef.align = SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMAXYMIN;
aspectRef.meetOrSlice = SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE;
shouldBe("aspectRef.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMAXYMIN");
shouldBe("aspectRef.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");
shouldBe("svgElement.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMAXYMIN");
shouldBe("svgElement.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");

debug("");
debug("Check that assigning to baseVal has no effect, as no setter is defined");
shouldBe("svgElement.preserveAspectRatio.baseVal = -1", "-1");
shouldBeEqualToString("svgElement.preserveAspectRatio.baseVal = 'aString'", "aString");
shouldBe("svgElement.preserveAspectRatio.baseVal = svgElement", "svgElement");

debug("");
debug("Check that the preserveAspectRatio align/meetOrSlice remained xMaxYMin/slice, and the baseVal type has not been changed");
shouldBeEqualToString("svgElement.preserveAspectRatio.baseVal.toString()", "[object SVGPreserveAspectRatio]");
shouldBe("svgElement.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMAXYMIN");
shouldBe("svgElement.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");

successfullyParsed = true;

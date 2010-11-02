description("This test checks the SVGPreserveAspectRatio API - utilizing the preserveAspectRatio property of SVGSVGElement");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var preserveAspectRatio = svgElement.preserveAspectRatio.baseVal;

debug("");
debug("Check initial align/meetOrSlice values");
shouldBe("preserveAspectRatio.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMIDYMID");
shouldBe("preserveAspectRatio.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_MEET");

debug("");
debug("Check assigning align/meetOrSlice values");
shouldBe("preserveAspectRatio.align = SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMIDYMIN", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMIDYMIN");
shouldBe("preserveAspectRatio.meetOrSlice = SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE", "SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");

debug("");
debug("Check assigning invalid align values");
shouldThrow("preserveAspectRatio.align = preserveAspectRatio");
shouldThrow("preserveAspectRatio.align = null");
shouldThrow("preserveAspectRatio.align = 'aString'");
shouldThrow("preserveAspectRatio.align = svgElement");
shouldThrow("preserveAspectRatio.align = SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_UNKNOWN");
shouldThrow("preserveAspectRatio.align = -1");
shouldThrow("preserveAspectRatio.align = 11"); // SVG_PRESERVEASPECTRATIO_XMAXYMAX + 1 (last value + 1)

debug("");
debug("Check assigning invalid meetOrSlice values");
shouldThrow("preserveAspectRatio.meetOrSlice = preserveAspectRatio");
shouldThrow("preserveAspectRatio.meetOrSlice = null");
shouldThrow("preserveAspectRatio.meetOrSlice = 'aString'");
shouldThrow("preserveAspectRatio.meetOrSlice = svgElement");
shouldThrow("preserveAspectRatio.meetOrSlice = SVGPreserveAspectRatio.SVG_MEETORSLICE_UNKNOWN");
shouldThrow("preserveAspectRatio.meetOrSlice = -1");
shouldThrow("preserveAspectRatio.meetOrSlice = 3"); // SVG_MEETORSLICE_SLICE + 1 (last value + 1)

debug("");
debug("Check that the preserveAspectRatio remained correct");
shouldBe("preserveAspectRatio.align = SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMIDYMIN", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMIDYMIN");
shouldBe("preserveAspectRatio.meetOrSlice = SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE", "SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");

successfullyParsed = true;

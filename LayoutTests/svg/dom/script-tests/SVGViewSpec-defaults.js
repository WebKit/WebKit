description("This test checks the SVGViewSpec API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var currentView = svgElement.currentView;
svgElement.setAttribute("id", "test");
svgElement.setAttribute("width", "150");
svgElement.setAttribute("height", "50");
svgElement.setAttribute("display", "block");

debug("");
debug("Check initial SVGSVGElement.currentView values on a SVGSVGElement");
shouldBe("currentView.transform.numberOfItems", "0");
shouldBeNull("currentView.viewTarget");
shouldBe("currentView.zoomAndPan", "SVGZoomAndPan.SVG_ZOOMANDPAN_MAGNIFY");
shouldBe("currentView.viewBox.baseVal.x", "0");
shouldBe("currentView.viewBox.baseVal.y", "0");
shouldBe("currentView.viewBox.baseVal.width", "0");
shouldBe("currentView.viewBox.baseVal.height", "0");
shouldBe("currentView.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMIDYMID");
shouldBe("currentView.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_MEET");
shouldBeEqualToString("currentView.viewBoxString", "0 0 0 0");
shouldBeEqualToString("currentView.preserveAspectRatioString", "xMidYMid meet");
shouldBeEqualToString("currentView.transformString", "");
shouldBeEqualToString("currentView.viewTargetString", "");
shouldBe("currentView.zoomAndPan", "SVGZoomAndPan.SVG_ZOOMANDPAN_MAGNIFY");

debug("");
debug("Try changing zoomAndPan - none of these will work, as SVGViewSpec is fully readonly - even the animated properties it inherits from parent classes like SVGZoomAndPan/SVGFitToViewBox");
shouldThrow("currentView.zoomAndPan = SVGZoomAndPan.SVG_ZOOMANDPAN_DISABLE");
shouldBe("currentView.zoomAndPan", "SVGZoomAndPan.SVG_ZOOMANDPAN_MAGNIFY");

debug("");
debug("Try changing viewBox - this has no affect on the SVGSVGElement the viewSpec belongs to - it exposed all its properties as read-only");
shouldThrow("currentView.viewBox.baseVal.x = 10");
shouldBe("currentView.viewBox.baseVal.x", "0");
shouldThrow("currentView.viewBox.baseVal.y = 20");
shouldBe("currentView.viewBox.baseVal.y", "0");
shouldThrow("currentView.viewBox.baseVal.width = 50");
shouldBe("currentView.viewBox.baseVal.width", "0");
shouldThrow("currentView.viewBox.baseVal.height = 100");
shouldBe("currentView.viewBox.baseVal.height", "0");
shouldBeEqualToString("currentView.viewBoxString", "0 0 0 0");

debug("");
debug("Try changing viewBoxString");
shouldBeEqualToString("currentView.viewBoxString = '1 2 3 4'", "1 2 3 4");
shouldBeEqualToString("currentView.viewBoxString", "0 0 0 0");

debug("");
debug("Try changing preserveAspectRatio");
shouldThrow("currentView.preserveAspectRatio.baseVal.align = SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMINYMIN");
shouldBe("currentView.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMIDYMID");
shouldThrow("currentView.preserveAspectRatio.baseVal.meetOrSlice = SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");
shouldBe("currentView.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_MEET");

debug("");
debug("Try changing preserveAspectRatioString");
shouldBeEqualToString("currentView.preserveAspectRatioString = 'xMinYMin slice'", "xMinYMin slice");
shouldBeEqualToString("currentView.preserveAspectRatioString", "xMidYMid meet");


debug("");
debug("Try changing transformString");
shouldBeEqualToString("currentView.transformString = 'rotate(90)'", "rotate(90)");
shouldBeEqualToString("currentView.transformString", "");


debug("");
debug("Try changing viewTarget");
shouldBe("currentView.viewTarget = svgElement", "svgElement");
shouldBeNull("currentView.viewTarget");


debug("");
debug("Try changing viewTargetString");
shouldBeEqualToString("currentView.viewTargetString = '#test'", "#test");
shouldBeEqualToString("currentView.viewTargetString", "");

debug("");
debug("Try changing transform");
shouldThrow("currentView.transform.clear()");
shouldBe("currentView.transform.numberOfItems", "0");

successfullyParsed = true;

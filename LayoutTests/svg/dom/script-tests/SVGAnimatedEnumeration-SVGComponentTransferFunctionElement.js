description("This test checks the use of SVGAnimatedEnumeration within SVGComponentTransferFunctionElement");

var feFuncRElement = document.createElementNS("http://www.w3.org/2000/svg", "feFuncR");
feFuncRElement.setAttribute("type", "identity");

debug("");
debug("Check initial 'type' value");
shouldBeEqualToString("feFuncRElement.type.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feFuncRElement.type.baseVal)", "number");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY");

debug("");
debug("Switch to 'table'");
shouldBe("feFuncRElement.type.baseVal = SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_TABLE", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_TABLE");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_TABLE");
shouldBeEqualToString("feFuncRElement.getAttribute('type')", "table");

debug("");
debug("Switch to 'discrete'");
shouldBe("feFuncRElement.type.baseVal = SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE");
shouldBeEqualToString("feFuncRElement.getAttribute('type')", "discrete");

debug("");
debug("Switch to 'linear'");
shouldBe("feFuncRElement.type.baseVal = SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_LINEAR", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_LINEAR");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_LINEAR");
shouldBeEqualToString("feFuncRElement.getAttribute('type')", "linear");

debug("");
debug("Switch to 'gamma'");
shouldBe("feFuncRElement.type.baseVal = SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_GAMMA", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_GAMMA");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_GAMMA");
shouldBeEqualToString("feFuncRElement.getAttribute('type')", "gamma");

debug("");
debug("Try setting invalid values");
shouldThrow("feFuncRElement.type.baseVal = 6");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_GAMMA");
shouldBeEqualToString("feFuncRElement.getAttribute('type')", "gamma");

shouldThrow("feFuncRElement.type.baseVal = -1");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_GAMMA");
shouldBeEqualToString("feFuncRElement.getAttribute('type')", "gamma");

shouldThrow("feFuncRElement.type.baseVal = 0");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_GAMMA");
shouldBeEqualToString("feFuncRElement.getAttribute('type')", "gamma");

debug("");
debug("Switch to 'identity'");
shouldBe("feFuncRElement.type.baseVal = SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY");
shouldBe("feFuncRElement.type.baseVal", "SVGComponentTransferFunctionElement.SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY");
shouldBeEqualToString("feFuncRElement.getAttribute('type')", "identity");

successfullyParsed = true;

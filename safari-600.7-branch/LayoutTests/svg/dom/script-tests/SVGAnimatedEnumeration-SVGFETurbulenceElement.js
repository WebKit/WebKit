description("This test checks the use of SVGAnimatedEnumeration within SVGFETurbulenceElement");

var feTurbulenceElement = document.createElementNS("http://www.w3.org/2000/svg", "feTurbulence");
feTurbulenceElement.setAttribute("type", "fractalNoise");
feTurbulenceElement.setAttribute("stitchTiles", "stitch");

// type
debug("");
debug("Check initial 'type' value");
shouldBeEqualToString("feTurbulenceElement.type.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feTurbulenceElement.type.baseVal)", "number");
shouldBe("feTurbulenceElement.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_FRACTALNOISE");

debug("");
debug("Switch to 'turbulence'");
shouldBe("feTurbulenceElement.type.baseVal = SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE");
shouldBe("feTurbulenceElement.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE");
shouldBeEqualToString("feTurbulenceElement.getAttribute('type')", "turbulence");

debug("");
debug("Try setting invalid values");
shouldThrow("feTurbulenceElement.type.baseVal = 3");
shouldBe("feTurbulenceElement.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE");
shouldBeEqualToString("feTurbulenceElement.getAttribute('type')", "turbulence");

shouldThrow("feTurbulenceElement.type.baseVal = -1");
shouldBe("feTurbulenceElement.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE");
shouldBeEqualToString("feTurbulenceElement.getAttribute('type')", "turbulence");

shouldThrow("feTurbulenceElement.type.baseVal = 0");
shouldBe("feTurbulenceElement.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE");
shouldBeEqualToString("feTurbulenceElement.getAttribute('type')", "turbulence");

debug("");
debug("Switch to 'fractalNoise'");
shouldBe("feTurbulenceElement.type.baseVal = SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_FRACTALNOISE", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_FRACTALNOISE");
shouldBe("feTurbulenceElement.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_FRACTALNOISE");
shouldBeEqualToString("feTurbulenceElement.getAttribute('type')", "fractalNoise");

// stitchTiles
debug("");
debug("Check initial 'stitchTiles' value");
shouldBeEqualToString("feTurbulenceElement.stitchTiles.toString()", "[object SVGAnimatedEnumeration]");
shouldBeEqualToString("typeof(feTurbulenceElement.stitchTiles.baseVal)", "number");
shouldBe("feTurbulenceElement.stitchTiles.baseVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_STITCH");

debug("");
debug("Switch to 'noStitch'");
shouldBe("feTurbulenceElement.stitchTiles.baseVal = SVGFETurbulenceElement.SVG_STITCHTYPE_NOSTITCH", "SVGFETurbulenceElement.SVG_STITCHTYPE_NOSTITCH");
shouldBe("feTurbulenceElement.stitchTiles.baseVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_NOSTITCH");
shouldBeEqualToString("feTurbulenceElement.getAttribute('stitchTiles')", "noStitch");

debug("");
debug("Try setting invalid values");
shouldThrow("feTurbulenceElement.stitchTiles.baseVal = 3");
shouldBe("feTurbulenceElement.stitchTiles.baseVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_NOSTITCH");
shouldBeEqualToString("feTurbulenceElement.getAttribute('stitchTiles')", "noStitch");

shouldThrow("feTurbulenceElement.stitchTiles.baseVal = -1");
shouldBe("feTurbulenceElement.stitchTiles.baseVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_NOSTITCH");
shouldBeEqualToString("feTurbulenceElement.getAttribute('stitchTiles')", "noStitch");

shouldThrow("feTurbulenceElement.stitchTiles.baseVal = 0");
shouldBe("feTurbulenceElement.stitchTiles.baseVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_NOSTITCH");
shouldBeEqualToString("feTurbulenceElement.getAttribute('stitchTiles')", "noStitch");

debug("");
debug("Switch to 'stitch'");
shouldBe("feTurbulenceElement.stitchTiles.baseVal = SVGFETurbulenceElement.SVG_STITCHTYPE_STITCH", "SVGFETurbulenceElement.SVG_STITCHTYPE_STITCH");
shouldBe("feTurbulenceElement.stitchTiles.baseVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_STITCH");
shouldBeEqualToString("feTurbulenceElement.getAttribute('stitchTiles')", "stitch");

successfullyParsed = true;

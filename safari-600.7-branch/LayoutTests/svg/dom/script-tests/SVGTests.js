description("This test checks the SVGTests API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var foreignObject = document.createElementNS("http://www.w3.org/2000/svg", "foreignObject");

debug("");
debug("Check the requiredFeatures, requiredExtensions and systemLanguage attributes");
shouldBeTrue("foreignObject.requiredFeatures instanceof SVGStringList");
shouldBeTrue("foreignObject.requiredExtensions instanceof SVGStringList");
shouldBeTrue("foreignObject.systemLanguage instanceof SVGStringList");

debug("");
debug("Check the hasExtension function");
shouldBeTrue("foreignObject.hasExtension('http://www.w3.org/1998/Math/MathML')");
shouldBeTrue("foreignObject.hasExtension('http://www.w3.org/1999/xhtml')");
shouldBeFalse("foreignObject.hasExtension('')");
shouldBeFalse("foreignObject.hasExtension('unknownExtension')");
shouldBeFalse("foreignObject.hasExtension('HTTP://WWW.W3.ORG/1999/XHTML')");
shouldBeFalse("foreignObject.hasExtension('http://www.w3.org/1998/Math/MathML http://www.w3.org/1999/xhtml')");

successfullyParsed = true;

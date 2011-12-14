description("Tests whether createSVGTransform creates a SVGTransform of the right SVGTransform type.");

var svgDoc = document.implementation.createDocument("http://www.w3.org/2000/svg", "svg", null);
var transform = svgDoc.documentElement.createSVGTransform();
shouldBe("transform.type", "SVGTransform.SVG_TRANSFORM_MATRIX");

var successfullyParsed = true;

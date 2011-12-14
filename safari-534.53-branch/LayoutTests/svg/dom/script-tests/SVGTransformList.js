description("This test checks the SVGTransformList API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var rectElement = document.createElementNS("http://www.w3.org/2000/svg", "rect");
var transform = rectElement.transform.baseVal;

debug("");
debug("Check passing invalid arguments to 'createSVGTransformFromMatrix'");
shouldThrow("transform.createSVGTransformFromMatrix()");
shouldThrow("transform.createSVGTransformFromMatrix(svgElement.createSVGTransform())");
shouldThrow("transform.createSVGTransformFromMatrix(svgElement)");
shouldThrow("transform.createSVGTransformFromMatrix('aString')");
shouldThrow("transform.createSVGTransformFromMatrix(1)");
shouldThrow("transform.createSVGTransformFromMatrix(true)");
shouldThrow("transform.createSVGTransformFromMatrix(undefined)");
shouldThrow("transform.createSVGTransformFromMatrix(null)");

successfullyParsed = true;

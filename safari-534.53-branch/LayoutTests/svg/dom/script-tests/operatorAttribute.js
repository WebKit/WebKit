description("This test checks that SVG operator attribute exists while _operator doesn't");

var feMorphologyElement = document.createElementNS("http://www.w3.org/2000/svg", "feMorphology");

feMorphologyElement.setAttribute("operator", "dilate");

// We need to check if we are on a filters enabled build:
if ('SVGFEMorphologyElement' in window) {
    shouldBe("feMorphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");

    feMorphologyElement.setAttribute("operator", "erode");
    shouldBe("feMorphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_ERODE");

    shouldBeUndefined("feMorphologyElement._operator");
}

successfullyParsed = true;

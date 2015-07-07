description("Test MorphologyOperatorType enumeration animations");
createSVGTestCase();

// Setup test document
var defs = createSVGElement("defs");
rootSVGElement.appendChild(defs);

var morphology = createSVGElement("feMorphology");
morphology.setAttribute("in", "SourceAlpha");
morphology.setAttribute("operator", "dilate");
morphology.setAttribute("radius", "4");

var filter = createSVGElement("filter");
filter.setAttribute("id", "filter");
filter.setAttribute("filterUnits", "userSpaceOnUse");
filter.setAttribute("x", "0");
filter.setAttribute("y", "0");
filter.setAttribute("width", "700");
filter.setAttribute("height", "200");
filter.appendChild(morphology);
defs.appendChild(filter);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "#408067");
rect.setAttribute("filter", "url(#filter)");
rect.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(rect);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "operator");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "dilate");
animate.setAttribute("to", "erode");
morphology.appendChild(animate);

// Setup animation test
function sample1() {
    shouldBe("morphology.operator.animVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");
    shouldBe("morphology.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");
}

function sample2() {
    shouldBe("morphology.operator.animVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_ERODE");
    shouldBe("morphology.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.999, sample1],
        ["animation", 2.001, sample2],
        ["animation", 3.999, sample2],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

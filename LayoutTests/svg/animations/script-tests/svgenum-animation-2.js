description("Test EdgeModeType enumeration animations");
createSVGTestCase();

// Setup test document
var defs = createSVGElement("defs");
rootSVGElement.appendChild(defs);

var convolveMatrix = createSVGElement("feConvolveMatrix");
convolveMatrix.setAttribute("in", "SourceGraphic");
convolveMatrix.setAttribute("order", "3");
convolveMatrix.setAttribute("kernelMatrix", "3 0 3 0 0 0 3 0 3");
convolveMatrix.setAttribute("targetX", "0");
convolveMatrix.setAttribute("edgeMode", "wrap");

var filter = createSVGElement("filter");
filter.setAttribute("id", "filter");
filter.setAttribute("filterUnits", "userSpaceOnUse");
filter.setAttribute("x", "0");
filter.setAttribute("y", "0");
filter.setAttribute("width", "200");
filter.setAttribute("height", "200");
filter.appendChild(convolveMatrix);
defs.appendChild(filter);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("filter", "url(#filter)");
rect.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(rect);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "edgeMode");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("values", "duplicate;none");
convolveMatrix.appendChild(animate);

// Setup animation test
function sample1() {
    shouldBe("convolveMatrix.edgeMode.animVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_WRAP");
    shouldBe("convolveMatrix.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_WRAP");
}

function sample2() {
    shouldBe("convolveMatrix.edgeMode.animVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_DUPLICATE");
    shouldBe("convolveMatrix.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_WRAP");
}

function sample3() {
    shouldBe("convolveMatrix.edgeMode.animVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_NONE");
    shouldBe("convolveMatrix.edgeMode.baseVal", "SVGFEConvolveMatrixElement.SVG_EDGEMODE_WRAP");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 0.001, sample2],
        ["animation", 1.999, sample2],
        ["animation", 2.001, sample3],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

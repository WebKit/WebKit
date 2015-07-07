description("Test ColorMatrixType enumeration animations");
createSVGTestCase();

// Setup test document
var defs = createSVGElement("defs");
rootSVGElement.appendChild(defs);

var colorMatrix = createSVGElement("feColorMatrix");
colorMatrix.setAttribute("in", "SourceGraphic");
colorMatrix.setAttribute("type", "matrix");

var filter = createSVGElement("filter");
filter.setAttribute("id", "filter");
filter.setAttribute("filterUnits", "userSpaceOnUse");
filter.setAttribute("x", "0");
filter.setAttribute("y", "0");
filter.setAttribute("width", "700");
filter.setAttribute("height", "200");
filter.appendChild(colorMatrix);
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
animate.setAttribute("attributeName", "type");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("values", "matrix;saturate;hueRotate;luminanceToAlpha");
colorMatrix.appendChild(animate);

// Setup animation test
function sample1() {
    shouldBe("colorMatrix.type.animVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX");
    shouldBe("colorMatrix.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX");
}

function sample2() {
    shouldBe("colorMatrix.type.animVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_SATURATE");
    shouldBe("colorMatrix.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX");
}

function sample3() {
    shouldBe("colorMatrix.type.animVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_HUEROTATE");
    shouldBe("colorMatrix.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX");
}

function sample4() {
    shouldBe("colorMatrix.type.animVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA");
    shouldBe("colorMatrix.type.baseVal", "SVGFEColorMatrixElement.SVG_FECOLORMATRIX_TYPE_MATRIX");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 0.999, sample1],
        ["animation", 1.001, sample2],
        ["animation", 1.999, sample2],
        ["animation", 2.001, sample3],
        ["animation", 2.999, sample3],
        ["animation", 3.001, sample4],
        ["animation", 3.999, sample4],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

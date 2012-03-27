description("Test animation of SVGInteger.");
createSVGTestCase();

// Setup test document
var defs = createSVGElement("defs");
rootSVGElement.appendChild(defs);

var filter = createSVGElement("filter");
filter.setAttribute("id", "filter");
defs.appendChild(filter);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "200");
rect.setAttribute("height", "200");
rect.setAttribute("fill", "green");
rect.setAttribute("filter", "url(#filter)");
rect.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(rect);

var feConvolveMatrix = createSVGElement("feConvolveMatrix");
feConvolveMatrix.setAttribute("id", "feConvlveMatrix");
feConvolveMatrix.setAttribute("order", "3");
feConvolveMatrix.setAttribute("kernelMatrix", "0 0 0   0 1 0   0 0 0");
feConvolveMatrix.setAttribute("targetX", "0");
filter.appendChild(feConvolveMatrix);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "targetX");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "0");
animate.setAttribute("to", "2");
feConvlveMatrix.appendChild(animate);

// Setup animation test
function sample1() {
    shouldBeCloseEnough("feConvolveMatrix.targetX.animVal", "0");
    shouldBe("feConvolveMatrix.targetX.baseVal", "0");
}

function sample2() {
    shouldBeCloseEnough("feConvolveMatrix.targetX.animVal", "1");
    shouldBe("feConvolveMatrix.targetX.baseVal", "0");
}

function sample3() {
    shouldBeCloseEnough("feConvolveMatrix.targetX.animVal", "2");
    shouldBe("feConvolveMatrix.targetX.baseVal", "0");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 2.0,   sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

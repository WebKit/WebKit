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
feConvolveMatrix.setAttribute("order", "3 1");
feConvolveMatrix.setAttribute("kernelMatrix", "0 0 1");
filter.appendChild(feConvolveMatrix);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "order");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "3 1");
animate.setAttribute("to", "1 3");
feConvlveMatrix.appendChild(animate);

// Setup animation test
function sample1() {
    shouldBeCloseEnough("feConvolveMatrix.orderX.animVal", "3");
    shouldBeCloseEnough("feConvolveMatrix.orderY.animVal", "1");
    shouldBe("feConvolveMatrix.orderX.baseVal", "3");
    shouldBe("feConvolveMatrix.orderY.baseVal", "1");
}

function sample2() {
    shouldBeCloseEnough("feConvolveMatrix.orderX.animVal", "2");
    shouldBeCloseEnough("feConvolveMatrix.orderY.animVal", "2");
    shouldBe("feConvolveMatrix.orderX.baseVal", "3");
    shouldBe("feConvolveMatrix.orderY.baseVal", "1");
}

function sample3() {
    shouldBeCloseEnough("feConvolveMatrix.orderX.animVal", "1");
    shouldBeCloseEnough("feConvolveMatrix.orderY.animVal", "3");
    shouldBe("feConvolveMatrix.orderX.baseVal", "3");
    shouldBe("feConvolveMatrix.orderY.baseVal", "1");
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

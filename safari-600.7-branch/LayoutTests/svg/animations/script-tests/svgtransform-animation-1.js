description("Test accumulate=sum animation on SVGAnimateTransform.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animateTransform");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "transform");
animate.setAttribute("type", "scale");
animate.setAttribute("from", "0,0");
animate.setAttribute("to", "2,2");
animate.setAttribute("type", "scale");
animate.setAttribute("fill", "freeze");
animate.setAttribute("accumulate", "sum");
animate.setAttribute("repeatCount", "3");
animate.setAttribute("additive", "sum");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "0");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "0");
    shouldBe("rect.transform.baseVal.numberOfItems", "0");
}

function sample2() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "1");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "1");
    shouldBe("rect.transform.baseVal.numberOfItems", "0");
}

function sample3() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "2");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "2");
    shouldBe("rect.transform.baseVal.numberOfItems", "0");
}

function sample4() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "3");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "3");
    shouldBe("rect.transform.baseVal.numberOfItems", "0");
}

function sample5() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "4");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "4");
    shouldBe("rect.transform.baseVal.numberOfItems", "0");
}

function sample6() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "5");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "5");
    shouldBe("rect.transform.baseVal.numberOfItems", "0");
}

function sample7() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "6");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "6");
    shouldBe("rect.transform.baseVal.numberOfItems", "0");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.001,  sample1],
        ["animation", 2.0,    sample2],
        ["animation", 3.999,  sample3],
        ["animation", 4.001,  sample3],
        ["animation", 6.0,    sample4],
        ["animation", 7.999,  sample5],
        ["animation", 8.001,  sample5],
        ["animation", 10.0,   sample6],
        ["animation", 11.999, sample7],
        ["animation", 12.001, sample7],
        ["animation", 14.0,   sample7],
        ["animation", 60.0,   sample7]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

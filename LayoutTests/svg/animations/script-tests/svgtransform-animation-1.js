description("Test accumulate=sum animation on SVGAnimateTransform.");
createSVGTestCase();
// FIXME: We should move to animatePathSegList, once it is implemented.

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
animate.setAttribute("accumulate", "sum");
animate.setAttribute("repeatCount", "2");
animate.setAttribute("additive", "sum");
animate.setAttribute("dur", "4s");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "0.01", 0.01);
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "0.01", 0.01);
}

function sample2() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "1", 0.01);
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "1", 0.01);
}

function sample3() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "2", 0.01);
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.d", "2", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "rect", sample1],
        ["animation", 2.0,    "rect", sample2],
        ["animation", 3.9999, "rect", sample3]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;

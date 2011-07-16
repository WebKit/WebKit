description("Test calcMode=discrete animation on SVGAnimateTransform.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("x", "0");
rect.setAttribute("y", "0");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animateTransform");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "transform");
animate.setAttribute("type", "translate");
animate.setAttribute("from", "100,100");
animate.setAttribute("to", "0,0");
animate.setAttribute("type", "translate");
animate.setAttribute("calcMode", "discrete");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test

function sample1() {
    // Check initial/end conditions
    shouldBe("rect.transform.animVal.numberOfItems", "0");
    shouldBeCloseEnough("document.defaultView.getComputedStyle(rect).getPropertyValue('x')", "0", 0.01);
    shouldBeCloseEnough("document.defaultView.getComputedStyle(rect).getPropertyValue('y')", "0", 0.01);
}

function sample2() {
    // Check initial/end conditions
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.e", "100", 0.01);
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.f", "100", 0.01);
}

function sample3() {
    shouldBe("rect.transform.animVal.numberOfItems", "1");
    shouldBe("rect.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.e", "0", 0.01);
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.f", "0", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,  "rect", sample1],
        ["animation", 0.001,  "rect", sample2],
        ["animation", 1.0,    "rect", sample2],
        ["animation", 3.0,    "rect", sample3],
        ["animation", 3.9999, "rect", sample3],
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;

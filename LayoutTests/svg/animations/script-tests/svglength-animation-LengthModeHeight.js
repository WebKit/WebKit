description("Test SVGLength animation on LengthModeHeight.");
createSVGTestCase();

// Setup test document
rootSVGElement.setAttribute("width", "600");
rootSVGElement.setAttribute("height", "400");

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "0");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("font-size", "10px");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "height");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "100");
animate.setAttribute("to", "50%");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.height.animVal.value", "100");
    shouldBe("rect.height.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.height.animVal.value", "150", 0.01);
    shouldBeCloseEnough("rect.height.baseVal.value", "150", 0.01);
}

function sample3() {
    shouldBeCloseEnough("rect.height.animVal.value", "200", 0.01);
    shouldBeCloseEnough("rect.height.baseVal.value", "200", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "rect", sample1],
        ["animation", 2.0,    "rect", sample2],
        ["animation", 3.9999, "rect", sample3],
        ["animation", 4.0 ,   "rect", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 30)", 0);
var successfullyParsed = true;
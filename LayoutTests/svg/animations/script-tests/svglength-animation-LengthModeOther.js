description("Test SVGLength animation on LengthModeOther.");
createSVGTestCase();

// Setup test document
rootSVGElement.setAttribute("width", "600");
rootSVGElement.setAttribute("height", "400");

// Setup test document
var circle = createSVGElement("circle");
circle.setAttribute("id", "circle");
circle.setAttribute("cx", "50");
circle.setAttribute("cy", "50");
circle.setAttribute("r", "10");
circle.setAttribute("fill", "green");
circle.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "r");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "10");
animate.setAttribute("to", "50%");
circle.appendChild(animate);
rootSVGElement.appendChild(circle);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("circle.r.animVal.value", "10");
    shouldBe("circle.r.baseVal.value", "10");
}

function sample2() {
    shouldBeCloseEnough("circle.r.animVal.value", "132.475", 0.01);
    shouldBeCloseEnough("circle.r.baseVal.value", "132.475", 0.01);
}

function sample3() {
    shouldBeCloseEnough("circle.r.animVal.value", "254.9", 0.1);
    shouldBeCloseEnough("circle.r.baseVal.value", "254.9", 0.1);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "circle", sample1],
        ["animation", 2.0,    "circle", sample2],
        ["animation", 3.9999, "circle", sample3],
        ["animation", 4.0 ,   "circle", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;
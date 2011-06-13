description("Test SVGLength animation from px to percentage.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "0");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "width");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "100px");
animate.setAttribute("to", "200%");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "350", 0.01);
    shouldBeCloseEnough("rect.width.baseVal.value", "350", 0.01);
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "600", 0.1);
    shouldBeCloseEnough("rect.width.baseVal.value", "600", 0.1);
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

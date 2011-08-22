createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "100");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "x");
animate.setAttribute("values", "0;300");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "3s");
animate.setAttribute("end", "2s");
animate.setAttribute("fill", "freeze");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    shouldBe("rect.x.baseVal.value", "100");
}

function sample2() {
    shouldBe("rect.x.baseVal.value", "50");
}

function sample3() {
    shouldBeCloseEnough("rect.x.baseVal.value", "200", 1);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "rect", sample1],
        ["animation", 0.5,    "rect", sample2],
        ["animation", 2.0,    "rect", sample3],
        ["animation", 3.0,    "rect", sample3]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(150, 30)", 0);
var successfullyParsed = true;

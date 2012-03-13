description("Test calcMode discrete with from-to animation on numbers. You should see a green 100x100 rect and only PASS messages");
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
animate.setAttribute("from", "100");
animate.setAttribute("to", "0");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("calcMode", "discrete");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.x.animVal.value", "100");
    shouldBe("rect.x.baseVal.value", "100");
}

function sample2() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBe("rect.x.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.999, sample1],
        ["animation", 2.001, sample2],
        ["animation", 3.999, sample2],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

window.clickX = 150;
var successfullyParsed = true;

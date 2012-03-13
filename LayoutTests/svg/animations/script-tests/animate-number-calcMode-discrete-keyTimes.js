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
animate.setAttribute("values", "100;200;300");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "3s");
animate.setAttribute("keyTimes", "0;0.5;1");
animate.setAttribute("calcMode", "discrete");
animate.setAttribute("fill", "freeze");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    shouldBe("rect.x.animVal.value", "100");
    shouldBe("rect.x.baseVal.value", "100");
}

function sample2() {
    shouldBe("rect.x.animVal.value", "200");
    shouldBe("rect.x.baseVal.value", "100");
}

function sample3() {
    shouldBe("rect.x.animVal.value", "300");
    shouldBe("rect.x.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0, sample1],
        ["animation", 1.499, sample1],
        ["animation", 1.501, sample2],
        ["animation", 2.999, sample2],
        ["animation", 3.001, sample3]
    ];

    runAnimationTest(expectedValues);
}

window.clickX = 150;
var successfullyParsed = true;

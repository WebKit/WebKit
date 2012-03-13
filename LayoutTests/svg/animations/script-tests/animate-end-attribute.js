description("Tests end conditions are respected properly");
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
animate.setAttribute("end", "click+2s");
animate.setAttribute("fill", "freeze");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.x.animVal.value", "100");
    shouldBe("rect.x.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.x.animVal.value", "50");
    shouldBe("rect.x.baseVal.value", "100");
}

function sample3() {
    shouldBeCloseEnough("rect.x.animVal.value", "200");
    shouldBe("rect.x.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0, sample1],
        ["animation", 0.5, sample2],
        ["animation", 2.0, sample3],
        ["animation", 3.0, sample3]
    ];

    runAnimationTest(expectedValues);
}

window.clickX = 150;
var successfullyParsed = true;

description("Tests end conditions are respected properly near the limits of float numeric precision");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "100");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "x");
animate.setAttribute("values", "0;300");
animate.setAttribute("begin", "0.333333333333333s");
animate.setAttribute("dur", "0.4256483205159505s");
animate.setAttribute("fill", "freeze");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.x.animVal.value", "100");
    shouldBe("rect.x.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.x.animVal.value", "300");
    shouldBe("rect.x.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0, sample1],
        ["animation", 1.0, sample2]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
executeTest();

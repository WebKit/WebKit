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
    shouldBeCloseEnough("rect.height.animVal.value", "100");
    shouldBe("rect.height.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.height.animVal.value", "150");
    shouldBe("rect.height.baseVal.value", "100");
}

function sample3() {
    shouldBeCloseEnough("rect.height.animVal.value", "200");
    shouldBe("rect.height.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 2.0,   sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

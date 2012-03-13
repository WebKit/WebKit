description("Test behavior of dynamically inserting animate without begin attribute");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "0");
rect.setAttribute("y", "45");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "x");
animate.setAttribute("from", "0");
animate.setAttribute("to", "90");
animate.setAttribute("dur", "3s");
animate.setAttribute("fill", "freeze");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.x.animVal.value", "0");
    shouldBe("rect.x.baseVal.value", "0");
}

function sample2() {
    shouldBeCloseEnough("rect.x.animVal.value", "90");
    shouldBe("rect.x.baseVal.value", "0");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0, sample1],
        ["animation", 3.0, sample2]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;

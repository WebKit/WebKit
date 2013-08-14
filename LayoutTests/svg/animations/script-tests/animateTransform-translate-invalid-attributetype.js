description("Tests that an animateTransform with invalid attributeType does not animate. Should result in a 100x100 rect and only PASS messages.");
createSVGTestCase();

// Setup test document

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "200");
rect.setAttribute("height", "200");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animateTransform");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "transform");
animate.setAttribute("attributeType", "CSS");
animate.setAttribute("type", "translate");
animate.setAttribute("from", "0, 0");
animate.setAttribute("to", "200, 200");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("fill", "freeze");
rect.appendChild(animate);

rootSVGElement.appendChild(rect);

// Setup animation test
function sample() {
    expectTranslationMatrix("rootSVGElement.getTransformToElement(rect)", "0", "0");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample],
        ["animation", 0.001, sample],
        ["animation", 2.0,   sample],
        ["animation", 3.999, sample],
        ["animation", 4.001, sample]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

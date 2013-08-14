description('Tests that an animateTransform with attributeType "auto" acts as "XML". Should result in a translated 100x100 rect and only PASS messages.');
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
animate.setAttribute("attributeType", "auto");
animate.setAttribute("type", "translate");
animate.setAttribute("from", "0, 0");
animate.setAttribute("to", "200, 200");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("fill", "freeze");
rect.appendChild(animate);

rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    expectTranslationMatrix("rootSVGElement.getTransformToElement(rect)", "0", "0");
}
function sample2() {
    expectTranslationMatrix("rootSVGElement.getTransformToElement(rect)", "-100", "-100");
}
function sample3() {
    expectTranslationMatrix("rootSVGElement.getTransformToElement(rect)", "-200", "-200");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 0.001, sample1],
        ["animation", 2.0,   sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

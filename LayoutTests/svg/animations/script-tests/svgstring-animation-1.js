description("Test animVal support for SVGAnimatedString animations.");
createSVGTestCase();

// Setup test document
var aElement = createSVGElement("a");
aElement.setAttribute("target", "base");

var textElement = createSVGElement("text");
textElement.setAttribute("id", "text");
textElement.setAttribute("y", "50");
textElement.textContent = "Test";
aElement.appendChild(textElement);
rootSVGElement.appendChild(aElement);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "target");
animate.setAttribute("begin", "0s");
animate.setAttribute("dur", "4s");
animate.setAttribute("values", "a;b");
aElement.appendChild(animate);

// Setup animation test
function sample1() {
    shouldBeEqualToString("aElement.target.animVal", "base");
    shouldBeEqualToString("aElement.target.baseVal", "base");
}

function sample2() {
    shouldBeEqualToString("aElement.target.animVal", "a");
    shouldBeEqualToString("aElement.target.baseVal", "base");
}

function sample3() {
    shouldBeEqualToString("aElement.target.animVal", "b");
    shouldBeEqualToString("aElement.target.baseVal", "base");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 0.001, sample2],
        ["animation", 1.999, sample2],
        ["animation", 2.001, sample3],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;

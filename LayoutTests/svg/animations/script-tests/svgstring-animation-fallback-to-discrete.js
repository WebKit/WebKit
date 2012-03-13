description("Tests fallback to calcMode='discrete' on animation of SVGString with 'values'.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "visibility");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "6s");
animate.setAttribute("calcMode", "linear");
animate.setAttribute("values", "visible ; hidden ; visible");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("getComputedStyle(rect).visibility", "'visible'");
    shouldBeEqualToString("rect.style.visibility", "");
}

function sample2() {
    shouldBe("getComputedStyle(rect).visibility", "'hidden'");
    shouldBeEqualToString("rect.style.visibility", "");
}

function sample3() {
    shouldBe("getComputedStyle(rect).visibility", "'visible'");
    shouldBeEqualToString("rect.style.visibility", "");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 1.999, sample1],
        ["animation", 2.001, sample2],
        ["animation", 5.999, sample3],
        ["animation", 6.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

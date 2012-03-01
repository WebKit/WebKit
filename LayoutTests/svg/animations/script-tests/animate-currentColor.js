description("Tests animation on 'currentColor'.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100px");
rect.setAttribute("height", "100px");
rect.setAttribute("fill", "red");
rect.setAttribute("color", "green");
rect.setAttribute("onclick", "executeTest()");

var animateCurrentColor = createSVGElement("animateColor");
animateCurrentColor.setAttribute("id", "animateCurrentColor");
animateCurrentColor.setAttribute("attributeName", "fill");
animateCurrentColor.setAttribute("from", "red");
animateCurrentColor.setAttribute("to", "currentColor");
animateCurrentColor.setAttribute("dur", "3s");
animateCurrentColor.setAttribute("begin", "click");
animateCurrentColor.setAttribute("fill", "freeze");
rect.appendChild(animateCurrentColor);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("getComputedStyle(rect).fill", "'#ff0000'");
    shouldBeEqualToString("rect.style.fill", "");
}

function sample2() {
    // Check half-time conditions
    shouldBe("getComputedStyle(rect).fill", "'#804000'");
    shouldBeEqualToString("rect.style.fill", "");
}

function sample3() {
    // Check half-time conditions
    shouldBe("getComputedStyle(rect).fill", "'#008000'");
    shouldBeEqualToString("rect.style.fill", "");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animateCurrentColor", 0.0, sample1],
        ["animateCurrentColor", 1.5, sample2],
        ["animateCurrentColor", 3.0, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;


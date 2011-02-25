description("Tests animation on 'currentColor'.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100px");
rect.setAttribute("height", "100px");
rect.setAttribute("fill", "red");
rect.setAttribute("color", "green");

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
    shouldBe("rect.style.fill", "'#ff0000'");
}

function sample2() {
    // Check half-time conditions
    shouldBe("rect.style.fill", "'#804000'");
}

function sample3() {
    // Check half-time conditions
    shouldBe("rect.style.fill", "'#008000'");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animateCurrentColor", 0.0,    "rect", sample1],
        ["animateCurrentColor", 1.5,    "rect", sample2],
        ["animateCurrentColor", 3.0,    "rect", sample3]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
rect.setAttribute("onclick", "executeTest()");
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;


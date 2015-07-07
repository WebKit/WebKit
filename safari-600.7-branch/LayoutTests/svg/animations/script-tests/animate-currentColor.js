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
    // Check initial conditions
    expectFillColor(rect, 255, 0, 0);
}

function sample2() {
    // Check half-time conditions
    expectFillColor(rect, 128, 64, 0);
}

function sample3() {
    // Check end condition
    expectFillColor(rect, 0, 128, 0);
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


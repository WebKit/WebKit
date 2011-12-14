description("Tests animation with 'inherit'.");
createSVGTestCase();

// Setup test document
var g = createSVGElement("g");
g.setAttribute("fill", "green");

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100px");
rect.setAttribute("height", "100px");
rect.setAttribute("fill", "red");
g.appendChild(rect);

var animateInherit = createSVGElement("animateColor");
animateInherit.setAttribute("id", "animateInherit");
animateInherit.setAttribute("attributeName", "fill");
animateInherit.setAttribute("from", "red");
animateInherit.setAttribute("to", "inherit");
animateInherit.setAttribute("dur", "3s");
animateInherit.setAttribute("begin", "click");
animateInherit.setAttribute("fill", "freeze");
rect.appendChild(animateInherit);
rootSVGElement.appendChild(g);

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
        ["animateInherit", 0.0,    "rect", sample1],
        ["animateInherit", 1.5,    "rect", sample2],
        ["animateInherit", 3.0,    "rect", sample3]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
rect.setAttribute("onclick", "executeTest()");
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;


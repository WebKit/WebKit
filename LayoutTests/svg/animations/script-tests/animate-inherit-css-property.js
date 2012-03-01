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
rect.setAttribute("onclick", "executeTest()");
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
        ["animateInherit", 0.0, sample1],
        ["animateInherit", 1.5, sample2],
        ["animateInherit", 3.0, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;

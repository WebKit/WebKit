description("Tests that 'transparent' is treated as a valid color.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "test");
rect.setAttribute("width", "100px");
rect.setAttribute("height", "100px");
rect.setAttribute("fill", "#00FF00");

var animate = createSVGElement("animateColor");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "fill");
animate.setAttribute("from", "transparent");
animate.setAttribute("by", "red");
animate.setAttribute("dur", "3s");
animate.setAttribute("begin", "click");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function expectTransparent() {
    // Check initial/end conditions
    shouldBe("rect.style.fill", "'#00FF00'");
}

function expectOtherColor() {
    // Check half-time conditions
    shouldBe("rect.style.fill", "'#7F0000'");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "test", expectTransparent],
        ["animation", 1.5,    "test", expectOtherColor]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
rect.setAttribute("onclick", "executeTest()");
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;


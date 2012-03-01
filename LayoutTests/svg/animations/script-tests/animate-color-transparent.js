description("Tests that 'transparent' is treated as a valid color.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "test");
rect.setAttribute("width", "100px");
rect.setAttribute("height", "100px");
rect.setAttribute("fill", "#00FF00");
rect.setAttribute("onclick", "executeTest()");

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
    shouldBe("getComputedStyle(rect).fill", "'#00ff00'");
    shouldBeEqualToString("rect.style.fill", "");
}

function expectOtherColor() {
    shouldBe("getComputedStyle(rect).fill", "'#7f0000'");
    shouldBeEqualToString("rect.style.fill", "");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0, expectTransparent],
        ["animation", 1.5, expectOtherColor]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;


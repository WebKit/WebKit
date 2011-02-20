description("Test behavior on dynamic-update of attributeName");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "200");
rect.setAttribute("height", "200");
rect.setAttribute("fill", "red");
rect.setAttribute("color", "red");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "color");
animate.setAttribute("from", "green");
animate.setAttribute("to", "green");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "3s");
animate.setAttribute("fill", "freeze");
animate.setAttribute("calcMode", "discrete");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    shouldBe("rect.style.color", "'rgb(0, 128, 0)'");
}

function sample2() {
    // Set 'attributeName' from 'color' to 'fill'
    animate.setAttribute("attributeName", "fill");
}

function sample3() {
    shouldBe("rect.style.fill", "'#008000'");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.1,    "rect", sample1],
        ["animation", 1.5,    "rect", sample2],
        ["animation", 3.0,    "rect", sample3],
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;

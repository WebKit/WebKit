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

var computedStyle = rect.ownerDocument.defaultView.getComputedStyle(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.style.visibility", "'visible'");
}

function sample2() {
    shouldBe("rect.style.visibility", "'hidden'");
}

function sample3() {
    shouldBe("rect.style.visibility", "'visible'");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 1.9999, "rect", sample1],
        ["animation", 2,      "rect", sample2],
        ["animation", 3.9999, "rect", sample3],
        ["animation", 4,      "rect", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 30)", 0);
var successfullyParsed = true;

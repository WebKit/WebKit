description("Tests animation of 'transform'. Should result in a 200x200 rect and only PASS messages.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "200");
rect.setAttribute("height", "200");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animateTransform");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "transform");
animate.setAttribute("type", "scale");
animate.setAttribute("from", "1");
animate.setAttribute("to", "2");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "1", 0.01);
    shouldBeCloseEnough("rect.transform.baseVal.getItem(0).matrix.a", "1", 0.01);
}

function sample2() {
    // Check half-time conditions
    shouldBe("rect.transform.animVal.getItem(0).matrix.a", "1.5");
    shouldBe("rect.transform.baseVal.getItem(0).matrix.a", "1.5");
}

function sample3() {
    shouldBeCloseEnough("rect.transform.animVal.getItem(0).matrix.a", "2", 0.01);
    shouldBeCloseEnough("rect.transform.baseVal.getItem(0).matrix.a", "2", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.001,  "rect", sample1],
        ["animation", 2.0,    "rect", sample2],
        ["animation", 3.9999, "rect", sample3],
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(15, 30)", 0);
var successfullyParsed = true;

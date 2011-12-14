description("Test calcMode spline with to animation. You should see a green 100x100 rect and only PASS messages");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "100");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "x");
animate.setAttribute("to", "0");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("keyTimes", "0;1");
animate.setAttribute("keySplines", "0.25 .5 .25 0.85");
animate.setAttribute("calcMode", "spline");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.x.animVal.value", "100");
    shouldBe("rect.x.baseVal.value", "100");
}

function sample2() {
    var ok = isCloseEnough(rect.x.animVal.value, 18.8, 0.01);
    if (ok)
        testPassed("rect.x.animVal.value is almost 18.8, after first half");
    else
        testFailed("rect.x.animVal.value is NOT almost 51, as expected");

    ok = isCloseEnough(rect.x.baseVal.value, 18.8, 0.01);
    if (ok)
        testPassed("rect.x.baseVal.value is almost 18.8, after first half");
    else
        testFailed("rect.x.baseVal.value is NOT almost 18.8, as expected");
}

function sample3() {
    // FIXME: Add animVal support. Animates baseVal at the moment.
    // shouldBe("rect.x.animVal.value", "0");
    // shouldBe("rect.x.baseVal.value", "200");

    // Check just before-end conditions
    var ok = isCloseEnough(rect.x.animVal.value, 0, 0.01);
    if (ok)
        testPassed("rect.x.animVal.value is almost 0, just before-end");
    else
        testFailed("rect.x.animVal.value is NOT almost 0, as expected");

    ok = isCloseEnough(rect.x.baseVal.value, 0, 0.01);
    if (ok)
        testPassed("rect.x.baseVal.value is almost 0, just before-end");
    else
        testFailed("rect.x.baseVal.value is NOT almost 0, as expected");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "rect", sample1],
        ["animation", 2,    "rect", sample2],
        ["animation", 3.9999, "rect", sample3],
        ["animation", 4.0 ,   "rect", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(150, 30)", 0);
var successfullyParsed = true;

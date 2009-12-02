// Inspired by Layoutests/animations/animation-test-helpers.js
// Modified to work with SVG and together with LayoutTests/svg/dynamic-updates/resources/SVGTestCase.js

function isCloseEnough(actual, desired, tolerance)
{
    var diff = Math.abs(actual - desired);
    return diff <= tolerance;
}

function moveAnimationTimelineAndSample(index)
{
    var animationId = expectedResults[index][0];
    var time = expectedResults[index][1];
    var elementId = expectedResults[index][2];
    var sampleCallback = expectedResults[index][3];

    if (!layoutTestController.sampleSVGAnimationForElementAtTime(animationId, time, elementId)) {
        testFailed("animation \"" + animationId + "\" is not running");
        return;
    }

    sampleCallback();
}

var currentTest = 0;
var expectedResults;

function startTest(callback) {
    if (currentTest > 0)
        throw("Not allowed to call startTest() twice");

    testCount = expectedResults.length;
    currentTest = 0;

    if (callback)
        callback();

    // Immediately sample, if the first time is 0
    if (expectedResults[0][1] == '0') {
        expectedResults[0][3]();
        ++currentTest;
    }

    // We may have just sampled on animation-begin, give the
    // document some time to invoke the SVG animation.
    // If we fix the animation to start with the SVGLoad event
    // not on implicitClose(), we can even avoid this hack.
    window.setTimeout(sampleAnimation, 0);
}

function sampleAnimation() {
    if (currentTest == expectedResults.length) {
        completeTest();
        return;
    }

    moveAnimationTimelineAndSample(currentTest);
    ++currentTest;

    sampleAnimation();
}

var hasPauseAnimationAPI;
var globalCallback;

function runAnimationTest(expected, callback)
{
    if (!expected)
        throw("Expected results are missing!");

    expectedResults = expected;

    hasPauseAnimationAPI = ('layoutTestController' in window) && ('sampleSVGAnimationForElementAtTime' in layoutTestController);
    if (!hasPauseAnimationAPI)
        throw("SVG animation pause API missing!");

    startTest(callback);
}

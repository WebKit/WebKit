// Inspired by Layoutests/animations/animation-test-helpers.js
// Modified to work with SVG and together with LayoutTests/svg/dynamic-updates/resources/SVGTestCase.js

function isCloseEnough(actual, desired, tolerance) {
    var diff = Math.abs(actual - desired);
    return diff <= tolerance;
}

function shouldBeCloseEnough(_a, _b, tolerance) {
    if (typeof tolerance != "number")
      tolerance = 0.1 // Default
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeCloseEnough() expects two string and one number arguments");
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }
    var _bv = eval(_b);
    
    if (exception)
        testFailed(_a + " should be " + _bv + ". Threw exception " + exception);
    else if (isCloseEnough(_av, _bv, tolerance))
        testPassed(_a + " is " + _b);
    else if (typeof(_av) == typeof(_bv))
        testFailed(_a + " should be close to " + _bv + ". Was " + stringify(_av) + ".");
    else
        testFailed(_a + " should be close to " + _bv + " (of type " + typeof _bv + "). Was " + _av + " (of type " + typeof _av + ").");
}

function expectMatrix(actualMatrix, expectedA, expectedB, expectedC, expectedD, expectedE, expectedF, tolerance) {
    shouldBeCloseEnough(actualMatrix + ".a", expectedA, tolerance);
    shouldBeCloseEnough(actualMatrix + ".b", expectedB, tolerance);
    shouldBeCloseEnough(actualMatrix + ".c", expectedC, tolerance);
    shouldBeCloseEnough(actualMatrix + ".d", expectedD, tolerance);
    shouldBeCloseEnough(actualMatrix + ".e", expectedE, tolerance);
    shouldBeCloseEnough(actualMatrix + ".f", expectedF, tolerance);
}

function expectTranslationMatrix(actualMatrix, expectedE, expectedF, tolerance) {
    shouldBeCloseEnough(actualMatrix + ".e", expectedE, tolerance);
    shouldBeCloseEnough(actualMatrix + ".f", expectedF, tolerance);
}

function expectColor(element, red, green, blue, property) {
    if (typeof property != "string")
        color = getComputedStyle(element).getPropertyCSSValue("color").getRGBColorValue();
    else {
        fillPaint = getComputedStyle(element).getPropertyCSSValue(property);
        color = getComputedStyle(element).getPropertyCSSValue(property).rgbColor;
    }

    // Allow a tolerance of 1 for color values, as they are integers.
    shouldBeCloseEnough("color.red.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + red, 1);
    shouldBeCloseEnough("color.green.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + green, 1);
    shouldBeCloseEnough("color.blue.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + blue, 1);
}

function expectFillColor(element, red, green, blue) {
    expectColor(element, red, green, blue, "fill");
}

function moveAnimationTimelineAndSample(index) {
    var animationId = expectedResults[index][0];
    var time = expectedResults[index][1];
    var sampleCallback = expectedResults[index][2];
    var animation = rootSVGElement.ownerDocument.getElementById(animationId);

    // If we want to sample the animation end, add a small delta, to reliable point past the end of the animation.
    newTime = time;

    try {
        newTime += animation.getStartTime();
    } catch(e) {
        debug('Exception thrown: ' + e);
    }

    // The sample time is relative to the start time of the animation, take that into account.
    rootSVGElement.setCurrentTime(newTime);
    sampleCallback();
}

var currentTest = 0;
var expectedResults;

function sampleAnimation() {
    if (currentTest == expectedResults.length) {
        completeTest();
        return;
    }

    moveAnimationTimelineAndSample(currentTest);
    ++currentTest;

    setTimeout(sampleAnimation, 0);
}

function runSMILTest() {
    // If eg. an animation is running with begin="0s", and we want to sample the first time, before the animation
    // starts, then we can't delay the testing by using an onclick event, as the animation would be past start time.
    if (window.animationStartsImmediately) {
        executeTest();
        return;
    }

    var useX = 50;
    var useY = 50;
    if (window.clickX)
        useX = window.clickX;
    if (window.clickY)
        useY = window.clickY;
    setTimeout("clickAt(" + useX + "," + useY + ")", 0);
}

function runAnimationTest(expected) {
    if (!expected)
        throw("Expected results are missing!");
    if (currentTest > 0)
        throw("Not allowed to call runAnimationTest() twice");

    if (window.testRunner)
        testRunner.waitUntilDone();

    expectedResults = expected;
    testCount = expectedResults.length;
    currentTest = 0;

    // Immediately sample, if the first time is zero.
    if (expectedResults[0][1] == 0) {
        expectedResults[0][2]();
        ++currentTest;
    }

    // Pause animations, we'll drive them manually.
    rootSVGElement.pauseAnimations();

    if (window.testRunner)
        setTimeout(sampleAnimation, 0);
    else
        setTimeout(sampleAnimation, 50);
}

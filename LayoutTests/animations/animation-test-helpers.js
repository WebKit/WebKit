/* This is the helper function to run animation tests:

Test page requirements:
- The body must contain an empty div with id "result"
- Call this function directly from the <script> inside the test page

Function parameters:
    expected [required]: an array of arrays defining a set of CSS properties that must have given values at specific times (see below)
    callback [optional]: a function to be executed just before the test starts (none by default)
    event [optional]: which DOM event to wait for before starting the test ("webkitAnimationStart" by default)

    Each sub-array must contain these items in this order:
    - the name of the CSS animation (may be null) [1]
    - the time in seconds at which to snapshot the CSS property
    - the id of the element on which to get the CSS property value
    - the name of the CSS property to get [2]
    - the expected value for the CSS property
    - the tolerance to use when comparing the effective CSS property value with its expected value

    [1] If null is passed, a regular setTimeout() will be used instead to snapshot the animated property in the future,
    instead of fast forwarding using the pauseAnimationAtTimeOnElementWithId() JS API from DRT 

    [2] If the CSS property name is "webkitTransform", expected value must be an array of 1 or more numbers corresponding to the matrix elements,
    or a string which will be compared directly (useful if the expected value is "none")
    If the CSS property name is "webkitTransform.N", expected value must be a number corresponding to the Nth element of the matrix

*/
function runAnimationTest(expected, callback, event)
{
    var result = "";
    var hasPauseAnimationAPI = window.layoutTestController && layoutTestController.pauseAnimationAtTimeOnElementWithId;

    function isCloseEnough(actual, desired, tolerance)
    {
        var diff = Math.abs(actual - desired);
        return diff <= tolerance;
    }

    function checkExpectedValue(expected, index)
    {
        var animationName = expected[index][0];
        var time = expected[index][1];
        var elementId = expected[index][2];
        var property = expected[index][3];
        var expectedValue = expected[index][4];
        var tolerance = expected[index][5];

        if (animationName && hasPauseAnimationAPI && !layoutTestController.pauseAnimationAtTimeOnElementWithId(animationName, time, elementId)) {
            result += "FAIL - animation \"" + animationName + "\" is not running" + "<br>";
            return;
        }

        var computedValue;
        var pass;
        if (!property.indexOf("webkitTransform")) {
            computedValue = window.getComputedStyle(document.getElementById(elementId)).webkitTransform;

            if (typeof expectedValue == "string")
                pass = (computedValue == expectedValue);
            else if (typeof expectedValue == "number") {
                var m = computedValue.split("(");
                var m = m[1].split(",");
                pass = isCloseEnough(parseFloat(m[parseInt(property.substring(16))]), expectedValue, tolerance);
            } else {
                var m = computedValue.split("(");
                var m = m[1].split(",");
                for (i = 0; i < expectedValue.length; ++i) {
                    pass = isCloseEnough(parseFloat(m[i]), expectedValue[i], tolerance);
                    if (!pass)
                        break;
                }
            }
        } else if (property == "lineHeight") {
            computedValue = parseInt(window.getComputedStyle(document.getElementById(elementId)).lineHeight);
            pass = isCloseEnough(computedValue, expectedValue, tolerance);
        } else {    
            var computedStyle = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property);
            computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
            pass = isCloseEnough(computedValue, expectedValue, tolerance);
        }

        if (pass)
            result += "PASS - \"" + property + "\" property for \"" + elementId + "\" element at " + time + "s saw something close to: " + expectedValue + "<br>";
        else
            result += "FAIL - \"" + property + "\" property for \"" + elementId + "\" element at " + time + "s expected: " + expectedValue + " but saw: " + computedValue + "<br>";
    }

    function endTest()
    {
        document.getElementById('result').innerHTML = result;

        if (window.layoutTestController)
            layoutTestController.notifyDone();
    }

    function checkExpectedValueCallback(expected, index)
    {
        return function() { checkExpectedValue(expected, index); };
    }

    var testStarted = false;
    function startTest(expected, callback)
    {
        if (testStarted) return;
        testStarted = true;

        if (callback)
            callback();

        var maxTime = 0;

        for (var i = 0; i < expected.length; ++i) {
            var animationName = expected[i][0];
            var time = expected[i][1];

            // We can only use the animation fast-forward mechanism if there's an animation name
            // and DRT implements pauseAnimationAtTimeOnElementWithId()
            if (animationName && hasPauseAnimationAPI)
                checkExpectedValue(expected, i);
            else {
                if (time > maxTime)
                    maxTime = time;

                window.setTimeout(checkExpectedValueCallback(expected, i), time * 1000);
            }
        }

        if (maxTime > 0)
            window.setTimeout(endTest, maxTime * 1000 + 50);
        else
            endTest();
    }
    
    if (window.layoutTestController) {
        layoutTestController.dumpAsText();
        layoutTestController.waitUntilDone();
    }
    
    if (!expected)
        throw("Expected results are missing!");
    
    var target = document;
    if (event == undefined)
        event = "webkitAnimationStart";
    else if (event == "load")
        target = window;
    target.addEventListener(event, function() { startTest(expected, callback); }, false);
}

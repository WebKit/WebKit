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
    - the id of the element on which to get the CSS property value [2]
    - the name of the CSS property to get [3]
    - the expected value for the CSS property
    - the tolerance to use when comparing the effective CSS property value with its expected value

    [1] If null is passed, a regular setTimeout() will be used instead to snapshot the animated property in the future,
    instead of fast forwarding using the pauseAnimationAtTimeOnElementWithId() JS API from DRT
    
    [2] If a single string is passed, it is the id of the element to test. If an array with 2 elements is passed they
    are the ids of 2 elements, whose values are compared for equality. In this case the expected value is ignored
    but the tolerance is used in the comparison.

    [3] If the CSS property name is "webkitTransform", expected value must be an array of 1 or more numbers corresponding to the matrix elements,
    or a string which will be compared directly (useful if the expected value is "none")
    If the CSS property name is "webkitTransform.N", expected value must be a number corresponding to the Nth element of the matrix

*/
function runAnimationTest(expected, callback, event)
{
    var result = "";
    var hasPauseAnimationAPI = ('layoutTestController' in window) && ('pauseAnimationAtTimeOnElementWithId' in layoutTestController);
    
    function isCloseEnough(actual, desired, tolerance)
    {
        var diff = Math.abs(actual - desired);
        return diff <= tolerance;
    }
    
    function matrixStringToArray(s)
    {
        var m = s.split("(");
        m = m[1].split(")");
        return m[0].split(",");
    }
    
    function checkExpectedValue(expected, index)
    {
        var animationName = expected[index][0];
        var time = expected[index][1];
        var elementId = expected[index][2];
        var property = expected[index][3];
        var expectedValue = expected[index][4];
        var tolerance = expected[index][5];

        // Check for a pair of element Ids
        var compareElements = false;
        var elementId2;
        if (typeof elementId != "string") {
            if (elementId.length != 2)
                return;
                
            elementId2 = elementId[1];
            elementId = elementId[0];
            compareElements = true;
        }

        if (animationName && hasPauseAnimationAPI && !layoutTestController.pauseAnimationAtTimeOnElementWithId(animationName, time, elementId)) {
            result += "FAIL - animation \"" + animationName + "\" is not running" + "<br>";
            return;
        }
        
        if (compareElements && animationName && hasPauseAnimationAPI && !layoutTestController.pauseAnimationAtTimeOnElementWithId(animationName, time, elementId2)) {
            result += "FAIL - animation \"" + animationName + "\" is not running" + "<br>";
            return;
        }
        
        var computedValue, computedValue2;
        var pass = true;
        if (!property.indexOf("webkitTransform")) {
            computedValue = window.getComputedStyle(document.getElementById(elementId)).webkitTransform;
            if (compareElements) {
                computedValue2 = window.getComputedStyle(document.getElementById(elementId2)).webkitTransform;
                var m1 = matrixStringToArray(computedValue);
                var m2 = matrixStringToArray(computedValue2);
                
                // for now we assume that both matrices are either both 2D or both 3D
                var count = (computedValue.substring(0, 7) == "matrix3d") ? 16 : 6;
                for (var i = 0; i < count; ++i) {
                    pass = isCloseEnough(parseFloat(m1[i]), m2[i], tolerance);
                    if (!pass)
                        break;
                }                
            } else {
                if (typeof expectedValue == "string")
                    pass = (computedValue == expectedValue);
                else if (typeof expectedValue == "number") {
                    var m = matrixStringToArray(computedValue);
                    pass = isCloseEnough(parseFloat(m[parseInt(property.substring(16))]), expectedValue, tolerance);
                } else {
                    var m = matrixStringToArray(computedValue);
                    for (i = 0; i < expectedValue.length; ++i) {
                        pass = isCloseEnough(parseFloat(m[i]), expectedValue[i], tolerance);
                        if (!pass)
                            break;
                    }
                }
            }
        } else if (property == "lineHeight") {
            computedValue = parseInt(window.getComputedStyle(document.getElementById(elementId)).lineHeight);
            if (compareElements) {
                computedValue2 = parseInt(window.getComputedStyle(document.getElementById(elementId2)).lineHeight);
                pass = isCloseEnough(computedValue, computedValue2, tolerance);
            }
            else
                pass = isCloseEnough(computedValue, expectedValue, tolerance);
        } else {
            var computedStyle = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property);
            computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
            if (compareElements) {
                var computedStyle2 = window.getComputedStyle(document.getElementById(elementId2)).getPropertyCSSValue(property);
                computedValue2 = computedStyle2.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
                pass = isCloseEnough(computedValue, computedValue2, tolerance);
            }
            else
                pass = isCloseEnough(computedValue, expectedValue, tolerance);
        }

        if (compareElements) {
            if (pass)
                result += "PASS - \"" + property + "\" property for \"" + elementId + "\" and \"" + elementId2 + "\" elements at " + time + "s are close enough to each other" + "<br>";
            else
                result += "FAIL - \"" + property + "\" property for \"" + elementId + "\" and \"" + elementId2 + "\" elements at " + time + "s saw: \"" + computedValue + "\" and \"" + computedValue2 + "\" which are not close enough to each other" + "<br>";
        } else {
            if (pass)
                result += "PASS - \"" + property + "\" property for \"" + elementId + "\" element at " + time + "s saw something close to: " + expectedValue + "<br>";
            else
                result += "FAIL - \"" + property + "\" property for \"" + elementId + "\" element at " + time + "s expected: " + expectedValue + " but saw: " + computedValue + "<br>";
        }
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

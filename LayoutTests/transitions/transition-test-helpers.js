/* This is the helper function to run transition tests:

Test page requirements:
- The body must contain an empty div with id "result"
- Call this function directly from the <script> inside the test page

Function parameters:
    expected [required]: an array of arrays defining a set of CSS properties that must have given values at specific times (see below)
    callback [optional]: a function to be executed just before the test starts (none by default)
    
    Each sub-array must contain these items in this order:
    - the time in seconds at which to snapshot the CSS property
    - the id of the element on which to get the CSS property value
    - the name of the CSS property to get [1]
    - the expected value for the CSS property
    - the tolerance to use when comparing the effective CSS property value with its expected value
    
    [1] If the CSS property name is "-webkit-transform", expected value must be an array of 1 or more numbers corresponding to the matrix elements,
    or a string which will be compared directly (useful if the expected value is "none")
    If the CSS property name is "-webkit-transform.N", expected value must be a number corresponding to the Nth element of the matrix

*/

function roundNumber(num, decimalPlaces)
{
  return Math.round(num * Math.pow(10, decimalPlaces)) / Math.pow(10, decimalPlaces);
}

function runTransitionTest(expected, callback)
{
    var result = "";
    var hasPauseTransitionAPI = window.layoutTestController && layoutTestController.pauseTransitionAtTimeOnElementWithId;

    function isCloseEnough(actual, desired, tolerance)
    {
        var diff = Math.abs(actual - desired);
        return diff <= tolerance;
    }

    function checkExpectedValue(expected, index)
    {
        var time = expected[index][0];
        var elementId = expected[index][1];
        var property = expected[index][2];
        var expectedValue = expected[index][3];
        var tolerance = expected[index][4];

        var computedValue;
        var pass;
        if (!property.indexOf("-webkit-transform")) {
            computedValue = window.getComputedStyle(document.getElementById(elementId)).webkitTransform;

            if (typeof expectedValue == "string")
                pass = (computedValue == expectedValue);
            else if (typeof expectedValue == "number") {
                var m = computedValue.split("(");
                var m = m[1].split(",");
                pass = isCloseEnough(parseFloat(m[parseInt(property.substring(18))]), expectedValue, tolerance);
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

    function runTest(expected)
    {
        var maxTime = 0;

        for (var i = 0; i < expected.length; ++i) {
            var time = expected[i][0];
            var elementId = expected[i][1];
            var property = expected[i][2];
            if (!property.indexOf("-webkit-transform"))
            property = "-webkit-transform";

            // We can only use the transition fast-forward mechanism if DRT implements pauseTransitionAtTimeOnElementWithId()
            if (hasPauseTransitionAPI) {
                layoutTestController.pauseTransitionAtTimeOnElementWithId(property, time, elementId);
                checkExpectedValue(expected, i);
            }
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
    
    function startTest(expected, callback)
    {
        if (callback)
            callback();

        window.setTimeout(function() { runTest(expected); }, 0);
    }
    
    if (window.layoutTestController) {
        layoutTestController.dumpAsText();
        layoutTestController.waitUntilDone();
    }
    
    if (!expected)
        throw("Expected results are missing!");
    
    window.addEventListener("load", function() { startTest(expected, callback); }, false);
}

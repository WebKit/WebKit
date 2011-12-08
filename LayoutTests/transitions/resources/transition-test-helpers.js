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

const usePauseAPI = true;
const dontUsePauseAPI = false;

const shouldBeTransitioning = true;
const shouldNotBeTransitioning = false;

function roundNumber(num, decimalPlaces)
{
  return Math.round(num * Math.pow(10, decimalPlaces)) / Math.pow(10, decimalPlaces);
}

function isCloseEnough(actual, desired, tolerance)
{
    var diff = Math.abs(actual - desired);
    return diff <= tolerance;
}

function isShadow(property)
{
  return (property == '-webkit-box-shadow' || property == 'text-shadow');
}

function getShadowXY(cssValue)
{
    var text = cssValue.cssText;
    // Shadow cssText looks like "rgb(0, 0, 255) 0px -3px 10px 0px"
    var shadowPositionRegExp = /\)\s*(-?\d+)px\s*(-?\d+)px/;
    var result = shadowPositionRegExp.exec(text);
    return [parseInt(result[1]), parseInt(result[2])];
}

function compareRGB(rgb, expected, tolerance)
{
    return (isCloseEnough(parseInt(rgb[0]), expected[0], tolerance) &&
            isCloseEnough(parseInt(rgb[1]), expected[1], tolerance) &&
            isCloseEnough(parseInt(rgb[2]), expected[2], tolerance));
}

function parseCrossFade(s)
{
    var matches = s.match("-webkit-cross-fade\\((.*)\\s*,\\s*(.*)\\s*,\\s*(.*)\\)");

    if (!matches)
        return null;

    return {"from": matches[1], "to": matches[2], "percent": parseFloat(matches[3])}
}

function checkExpectedValue(expected, index)
{
    var time = expected[index][0];
    var elementId = expected[index][1];
    var property = expected[index][2];
    var expectedValue = expected[index][3];
    var tolerance = expected[index][4];
    var postCompletionCallback = expected[index][5];

    var computedValue;
    var pass = false;
    var transformRegExp = /^-webkit-transform(\.\d+)?$/;
    if (transformRegExp.test(property)) {
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
    } else if (property == "fill" || property == "stroke") {
        computedValue = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property).rgbColor;
        if (compareRGB([computedValue.red.cssText, computedValue.green.cssText, computedValue.blue.cssText], expectedValue, tolerance))
            pass = true;
        else {
            // We failed. Make sure computed value is something we can read in the error message
            computedValue = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property).cssText;
        }
    } else if (property == "stop-color" || property == "flood-color" || property == "lighting-color") {
        computedValue = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property);
        // The computedValue cssText is rgb(num, num, num)
        var components = computedValue.cssText.split("(")[1].split(")")[0].split(",");
        if (compareRGB(components, expectedValue, tolerance))
            pass = true;
        else {
            // We failed. Make sure computed value is something we can read in the error message
            computedValue = computedValue.cssText;
        }
    } else if (property == "lineHeight") {
        computedValue = parseInt(window.getComputedStyle(document.getElementById(elementId)).lineHeight);
        pass = isCloseEnough(computedValue, expectedValue, tolerance);
    } else if (property == "background-image"
               || property == "border-image-source"
               || property == "border-image"
               || property == "list-style-image"
               || property == "-webkit-mask-image"
               || property == "-webkit-mask-box-image") {
        if (property == "border-image" || property == "-webkit-mask-image" || property == "-webkit-mask-box-image")
            property += "-source";
        
        computedValue = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property).cssText;
        computedCrossFade = parseCrossFade(computedValue);

        if (!computedCrossFade) {
            pass = false;
        } else {
            pass = isCloseEnough(computedCrossFade.percent, expectedValue, tolerance);
        }
    } else {
        var computedStyle = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property);
        if (computedStyle.cssValueType == CSSValue.CSS_VALUE_LIST) {
            var values = [];
            for (var i = 0; i < computedStyle.length; ++i) {
                switch (computedStyle[i].cssValueType) {
                  case CSSValue.CSS_PRIMITIVE_VALUE:
                    values.push(computedStyle[i].getFloatValue(CSSPrimitiveValue.CSS_NUMBER));
                    break;
                  case CSSValue.CSS_CUSTOM:
                    // arbitrarily pick shadow-x and shadow-y
                    if (isShadow) {
                      var shadowXY = getShadowXY(computedStyle[i]);
                      values.push(shadowXY[0]);
                      values.push(shadowXY[1]);
                    } else
                      values.push(computedStyle[i].cssText);
                    break;
                }
            }
            computedValue = values.join(',');
            pass = true;
            for (var i = 0; i < values.length; ++i)
                pass &= isCloseEnough(values[i], expectedValue[i], tolerance);
        } else if (computedStyle.cssValueType == CSSValue.CSS_PRIMITIVE_VALUE) {
            switch (computedStyle.primitiveType) {
                case CSSPrimitiveValue.CSS_STRING:
                    computedValue = computedStyle.getStringValue();
                    pass = computedValue == expectedValue;
                    break;
                case CSSPrimitiveValue.CSS_RGBCOLOR:
                    var rgbColor = computedStyle.getRGBColorValue();
                    computedValue = [rgbColor.red.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     rgbColor.green.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     rgbColor.blue.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)]; // alpha is not exposed to JS
                    pass = true;
                    for (var i = 0; i < 3; ++i)
                        pass &= isCloseEnough(computedValue[i], expectedValue[i], tolerance);
                    break;
                case CSSPrimitiveValue.CSS_RECT:
                    computedValue = computedStyle.getRectValue();
                    computedValue = [computedValue.top.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     computedValue.right.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     computedValue.bottom.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     computedValue.left.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)];
                     pass = true;
                     for (var i = 0; i < 4; ++i)
                         pass &= isCloseEnough(computedValue[i], expectedValue[i], tolerance);
                    break;
                case CSSPrimitiveValue.CSS_PERCENTAGE:
                    computedValue = parseFloat(computedStyle.cssText);
                    pass = isCloseEnough(computedValue, expectedValue, tolerance);
                    break;
                default:
                    computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
                    pass = isCloseEnough(computedValue, expectedValue, tolerance);
            }
        }
    }

    if (pass)
        result += "PASS - \"" + property + "\" property for \"" + elementId + "\" element at " + time + "s saw something close to: " + expectedValue + "<br>";
    else
        result += "FAIL - \"" + property + "\" property for \"" + elementId + "\" element at " + time + "s expected: " + expectedValue + " but saw: " + computedValue + "<br>";

    if (postCompletionCallback)
      result += postCompletionCallback();
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

function runTest(expected, usePauseAPI)
{
    var maxTime = 0;
    for (var i = 0; i < expected.length; ++i) {
        var time = expected[i][0];
        var elementId = expected[i][1];
        var property = expected[i][2];
        if (!property.indexOf("-webkit-transform."))
            property = "-webkit-transform";

        var tryToPauseTransition = expected[i][6];
        if (tryToPauseTransition === undefined)
          tryToPauseTransition = shouldBeTransitioning;

        // We can only use the transition fast-forward mechanism if DRT implements pauseTransitionAtTimeOnElementWithId()
        if (hasPauseTransitionAPI && usePauseAPI) {
            if (tryToPauseTransition) {
              if (!layoutTestController.pauseTransitionAtTimeOnElementWithId(property, time, elementId))
                window.console.log("Failed to pause '" + property + "' transition on element '" + elementId + "'");
            }
            checkExpectedValue(expected, i);
        } else {
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

function waitForAnimationStart(callback, delay)
{
    var delayTimeout = delay ? 1000 * delay + 10 : 0;
    // Why the two setTimeouts? Well, for hardware animations we need to ensure that the hardware animation
    // has started before we try to pause it, and timers fire before animations get committed in the runloop.
    window.setTimeout(function() {
        window.setTimeout(function() {
            callback();
        }, 0);
    }, delayTimeout);
}

function startTest(expected, usePauseAPI, callback)
{
    if (callback)
        callback();

    waitForAnimationStart(function() {
        runTest(expected, usePauseAPI);
    });
}

var result = "";
var hasPauseTransitionAPI;

function runTransitionTest(expected, callback, usePauseAPI, doPixelTest)
{
    hasPauseTransitionAPI = ('layoutTestController' in window) && ('pauseTransitionAtTimeOnElementWithId' in layoutTestController);
    
    if (window.layoutTestController) {
        if (!doPixelTest)
            layoutTestController.dumpAsText();
        layoutTestController.waitUntilDone();
    }
    
    if (!expected)
        throw("Expected results are missing!");
    
    window.addEventListener("load", function() { startTest(expected, usePauseAPI, callback); }, false);
}

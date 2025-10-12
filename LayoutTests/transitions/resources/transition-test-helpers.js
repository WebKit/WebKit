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

var usePauseAPI = true;
var dontUsePauseAPI = false;

var shouldBeTransitioning = true;
var shouldNotBeTransitioning = false;

function roundNumber(num, decimalPlaces)
{
  return Math.round(num * Math.pow(10, decimalPlaces)) / Math.pow(10, decimalPlaces);
}

function isCloseEnough(actual, desired, tolerance)
{
    var diff = Math.abs(actual - desired);
    return diff <= tolerance;
}

function parseCrossFade(s)
{
    var matches = s.match("(?:-webkit-)?cross-fade\\((.*)\\s*,\\s*(.*)\\s*,\\s*(.*)\\)");

    if (!matches)
        return null;

    return {"from": matches[1], "to": matches[2], "percent": parseFloat(matches[3])}
}

function extractPathValues(path)
{
    var components = path.split(' ');
    var result = [];
    for (component of components) {
        var compMatch;
        if (compMatch = component.match(/[0-9.-]+/)) {
            result.push(parseFloat(component))
        }
    }
    return result;
}

function parseClipPath(s)
{
    var pathMatch;
    if (pathMatch = s.match(/path\(((evenodd|nonzero), ?)?\'(.+)\'\)/))
        return extractPathValues(pathMatch[pathMatch.length - 1]);

    if (pathMatch = s.match(/path\(((evenodd|nonzero), ?)?\"(.+)\"\)/))
        return extractPathValues(pathMatch[pathMatch.length - 1]);

    // FIXME: This only matches a subset of the shape syntax, and the polygon expects 4 points.
    var patterns = [
        /inset\(([\d.]+)\w+ ([\d.]+)\w+\)/,
        /circle\(([\d.]+)\w+ at ([\d.]+)\w+ ([\d.]+)\w+\)/,
        /ellipse\(([\d.]+)\w+ ([\d.]+)\w+ at ([\d.]+)\w+ ([\d.]+)\w+\)/,
        /polygon\(([\d.]+)\w* ([\d.]+)\w*\, ([\d.]+)\w* ([\d.]+)\w*\, ([\d.]+)\w* ([\d.]+)\w*\, ([\d.]+)\w* ([\d.]+)\w*\)/,
    ];
    
    for (pattern of patterns) {
        var matchResult;
        if (matchResult = s.match(pattern)) {
            var result = [];
            for (var i = 1; i < matchResult.length; ++i)
                result.push(parseFloat(matchResult[i]));
            return result;
        }
    }

    console.log('failed to match ' + s);
    return null;
}

function parseLengthPair(s)
{
    var pathMatch;
    if (pathMatch = s.match(/^([\d.]+)px\s+([\d.]+)px$/))
        return [pathMatch[1], pathMatch[2]];

    // A pair can be coalesced if both lengths were the same.
    if (pathMatch = s.match(/^([\d.]+)px$/))
        return [pathMatch[1], pathMatch[1]];

    return null;
}

function isExpectedValue(value, expected, tolerance)
{
    if (!value)
        return false;
    if (typeof expected == "string")
        return value == expected;
    if (typeof expected == "number")
        return isCloseEnough(parseFloat(value), expected, tolerance);
    let values;
    if (/^\w+\(.+\)$/.test(value)) {
        // Split examples like "rgb(1, 2, 3)" into array [1, 2, 3].
        values = value.split("(")[1].split(")")[0].split(/, /);
    } else {
        // Split elements separated by spaces that are not inside parentheses.
        values = value.split(/(?!\(.*) (?![^(]*?\))/);
    }
    if (values.length != expected.length)
        return false;
    for (var i = 0; i < values.length; ++i) {
        if (!isExpectedValue(values[i], expected[i], tolerance))
            return false;
    }
    return true;
}

function checkExpectedValue(expected, index)
{
    const time = expected[index][0];
    const elementId = expected[index][1];
    let property = expected[index][2];
    const expectedValue = expected[index][3];
    const tolerance = expected[index][4];
    const postCompletionCallback = expected[index][5];

    if (property == "border-image" || property == "-webkit-mask-image" || property == "-webkit-mask-box-image")
        property += "-source";

    let computedValue = getComputedStyle(document.getElementById(elementId))[property.split(".")[0]];
    let matchResult = property.match(/\.(\d+)$/);
    if (matchResult)
        computedValue = computedValue.split("(")[1].split(")")[0].split(",")[matchResult[1]];

    let pass;
    if (property == "background-image"
               || property == "border-image-source"
               || property == "border-image"
               || property == "list-style-image"
               || property == "-webkit-mask-image"
               || property == "-webkit-mask-box-image") {
        const computedCrossFade = parseCrossFade(computedValue);
        pass = computedCrossFade && isExpectedValue(computedCrossFade.percent, expectedValue, tolerance);
    } else if (property == "-webkit-clip-path" || property == "-webkit-shape-outside") {
        const expectedValues = parseClipPath(expectedValue);
        const values = parseClipPath(computedValue);
        pass = values && values.length == expectedValues.length;
        if (pass) {
            for (var i = 0; i < values.length; ++i)
                pass &= isCloseEnough(values[i], expectedValues[i], tolerance);
        }
    } else if (/^(?:-webkit-)?border-(?:top|bottom)-(?:left|right)-radius$/.test(property))
        pass = isExpectedValue(parseLengthPair(computedValue), [expectedValue, expectedValue], tolerance);
    else
        pass = isExpectedValue(computedValue, expectedValue, tolerance);

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

    if (window.testRunner)
        testRunner.notifyDone();
}

function checkExpectedValueCallback(expected, index)
{
    return function() { checkExpectedValue(expected, index); };
}

const prefix = "-webkit-";
const propertiesRequiringPrefix = ["-webkit-text-stroke-color", "-webkit-text-fill-color"];

function pauseTransitionAtTimeOnElement(transitionProperty, time, element)
{
    if (transitionProperty.startsWith(prefix) && !propertiesRequiringPrefix.includes(transitionProperty))
        transitionProperty = transitionProperty.substr(prefix.length);

    const animations = element.getAnimations();
    for (let animation of animations) {
        if (animation instanceof CSSTransition && animation.transitionProperty == transitionProperty) {
            animation.pause();
            animation.currentTime = time * 1000;
            return true;
        }
    }
    console.log(`A transition for property ${transitionProperty} could not be found`);
    return false;
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

        if (hasPauseTransitionAPI && usePauseAPI) {
            if (tryToPauseTransition) {
              var element = document.getElementById(elementId);
              if (!pauseTransitionAtTimeOnElement(property, time, element))
                console.log("Failed to pause '" + property + "' transition on element '" + elementId + "'");
            }
            try {
                checkExpectedValue(expected, i);
            } catch (err) {
                result += "EXCEPTION for \"" + property + "\" - " + err + "<br>";
            }
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
var hasPauseTransitionAPI = true;

function runTransitionTest(expected, callback, usePauseAPI, doPixelTest)
{
    if (window.testRunner) {
        if (!doPixelTest)
            testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }
    
    if (!expected)
        throw("Expected results are missing!");
    
    window.addEventListener("load", function() { startTest(expected, usePauseAPI, callback); }, false);
}

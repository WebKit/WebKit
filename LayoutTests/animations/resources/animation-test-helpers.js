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
    instead of fast forwarding using the pauseAnimationAtTimeOnElement() function.
    
    [2] If a single string is passed, it is the id of the element to test. If an array with 2 elements is passed they
    are the ids of 2 elements, whose values are compared for equality. In this case the expected value is ignored
    but the tolerance is used in the comparison. If the second element is prefixed with "static:", no animation on that
    element is required, allowing comparison with an unanimated "expected value" element.
    
    If a string with a '.' is passed, this is an element in an iframe. The string before the dot is the iframe id
    and the string after the dot is the element name in that iframe.

    [3] If the CSS property name is "webkitTransform", expected value must be an array of 1 or more numbers corresponding to the matrix elements,
    or a string which will be compared directly (useful if the expected value is "none")
    If the CSS property name is "webkitTransform.N", expected value must be a number corresponding to the Nth element of the matrix

*/

function isCloseEnough(actual, expected, tolerance)
{
    if (isNaN(actual) || isNaN(expected))
        return false;
    var diff = Math.abs(actual - expected);
    return diff <= tolerance;
}

function matrixStringToArray(s)
{
    if (s == "none")
        return [ 1, 0, 0, 1, 0, 0 ];
    var m = s.split("(");
    m = m[1].split(")");
    return m[0].split(",");
}

function parseCSSImage(s)
{
    // Special case none.
    if (s == "none")
        return ["none"];

    // Separate function name from function value.
    var matches = s.match("([\\w\\-]+)\\((.*)\\)");
    if (!matches){
        console.error("Parsing error. Value not a CSS Image function ", s);
        return false;
    }

    var functionName = matches[1];
    var functionValue = matches[2];

    // Generator functions can have CSS images as values themself.
    // These functions will call parseCSSImage for each CSS Image.
    switch (functionName) {
    case "filter":
        return parseFilterImage(functionValue);
    case "cross-fade":
    case "-webkit-cross-fade":
        return parseCrossFade(functionValue);
    case "url":
        return ["url", functionValue];
    // FIXME: Add support for linear and redial gradient.
    default:
        // All supported filter functions must be listed above.
        return false;
    }
}

// This should just be called by parseCSSImage.
function parseCrossFade(s)
{
    var matches = s.match("(.*)\\s*,\\s*(.*)\\s*,\\s*(.*)\\s*");
    if (!matches) {
        console.error("Parsing error on 'cross-fade()'.");
        return null;
    }

    var from = parseCSSImage(matches[1]);
    var to = parseCSSImage(matches[2]);
    if (!from || !to) {
        console.error("Parsing error on images passed to 'cross-fade()' ", s);
        return null;
    }

    var fadeValue = matches[3];
    var percent;
    if (isNaN(fadeValue)) {
        // Check if last char is '%' and rip it off.
        // Normalize it to number.
        if (fadeValue.search('%') != fadeValue.length - 1) {
            console.error("Passed value to 'cross-fade()' is not a number or percentage ", fadeValue);
            return null;
        }
        fadeValue = fadeValue.slice(0, fadeValue.length - 1);
        if (isNaN(fadeValue)) {
            console.error("Passed value to 'cross-fade()' is not a number or percentage ", fadeValue);
            return null;
        }
        percent = parseFloat(fadeValue) / 100;
    } else
        percent = parseFloat(fadeValue);

    return ["cross-fade", from, to, percent];
}

// This should just be called by parseCSSImage.
function parseFilterImage(s)
{
    // Separate image value from filter function list.
    var matches = s.match("([\\-\\w]+\\(.*\\))\\s*,\\s*(.*)\\s*");
    if (!matches) {
        console.error("Parsing error on 'filter()' ", s);
        return false;
    }

    var image = parseCSSImage(matches[1]);
    if (!image) {
        console.error("Parsing error on image passed to 'filter()' ", s);
        return false;
    }

    var filterFunctionList = parseFilterFunctionList(matches[2]);
    if (!filterFunctionList) {
        console.error("Parsing error on filter function list passed to 'filter()' ", s);
        return false;
    }

    return ["-webkit-filter", image, filterFunctionList];
}

function parseFilterFunctionList(s)
{
    var reg = /\)*\s*(blur|brightness|contrast|drop\-shadow|grayscale|hue\-rotate|invert|opacity|saturate|sepia|url)\(/
    var matches = s.split(reg);

    // First item must be empty. All other items are of functionName, functionValue.
    if (!matches || matches.shift() != "")
        return null;

    // Odd items are the function name, even items the function value.
    if (!matches.length || matches.length % 2)
        return null;

    var functionList = [];
    for (var i = 0; i < matches.length; i += 2) {
        var functionName = matches[i];
        var functionValue = matches[i+1];
        functionList.push(functionName);
        if (functionName == "drop-shadow" || functionName == "url") {
            // FIXME: Support parsing of drop-shadow.
            functionList.push(functionValue);
            continue;
        }
        functionList.push(parseFloat(functionValue));
    }
    return functionList;
}

function parseBasicShape(s)
{
    var shapeFunction = s.match(/(\w+)\((.+)\)/);
    if (!shapeFunction)
        return null;

    var matches;
    switch (shapeFunction[1]) {
    case "inset":
        matches = s.match("inset\\(\\s*(.*)\\s*\\)");
        matches = matches[1].split(/\s+/);
        matches.unshift(s);
        break;
    case "circle":
        matches = s.match("circle\\((.*)\\s+at\\s+(.*)\\s+(.*)\\)");
        break;
    case "ellipse":
        matches = s.match("ellipse\\((.*)\\s+(.*)\\s+at\\s+(.*)\\s+(.*)\\)");
        break;
    case "polygon":
        matches = s.match("polygon\\(\\s*(.*)\\s*\\)");
        matches = matches[1].split(/\s*,\s*/);
        matches = matches.map(function(match) {
            return match.split(/\s+/);
        });
        matches = Array.prototype.concat.apply([s], matches);
        break;
    default:
        return null;
    }

    if (!matches)
        return null;

    matches.shift();
    var i = 0;
    if (shapeFunction[1] == "polygon")
        i++; // skip nonzero|evenodd below

    // Normalize percentage values.
    for (; i < matches.length; ++i) {
        var param = parseFloat(matches[i]);

        if (isNaN(param))
            continue;

        if (matches[i].indexOf('%') != -1)
            matches[i] = param / 100;
        else
            matches[i] = param;
    }

    return {"shape": shapeFunction[1], "params": matches};
}

function compareCSSImages(computedValue, expectedValue, tolerance)
{
    var actual = typeof computedValue === "string" ? parseCSSImage(computedValue) : computedValue;
    var expected = typeof expectedValue === "string" ? parseCSSImage(expectedValue) : expectedValue;
    if (!actual || !Array.isArray(actual)         // Check that: actual is an array,
        || !expected || !Array.isArray(expected)  // expected is an array,
        || actual[0] != expected[0]) {            // and image function names are the same.
        console.error("Unexpected mismatch between CSS Image functions.");
        return false;
    }
    switch (actual[0]) {
    case "none":
        return true;
    case "-webkit-filter":
        return compareCSSImages(actual[1], expected[1], tolerance)
            && compareFilterFunctions(actual[2], expected[2], tolerance);
    case "cross-fade":
    case "-webkit-cross-fade":
        return compareCSSImages(actual[1], expected[1], tolerance)
            && compareCSSImages(actual[2], expected[2], tolerance)
            && isCloseEnough(actual[3], expected[3], tolerance);
    case "url":
        return actual[1].search(expected[1]) >= 0;
    default:
        console.error("Unknown CSS Image function ", actual[0]);
        return false;
    }
}

function compareFontVariationSettings(computedValue, expectedValue, tolerance)
{
    if (!computedValue)
        return false;
    if (computedValue == "normal" || expectedValue == "normal")
        return computedValue == expectedValue;
    var computed = computedValue.split(", ");
    var expected = expectedValue.split(", ");
    if (computed.length != expected.length)
        return false;
    for (var i = 0; i < computed.length; ++i) {
        var computedPieces = computed[i].split(" ");
        var expectedPieces = expected[i].split(" ");
        if (computedPieces.length != 2 || expectedPieces.length != 2)
            return false;
        if (computedPieces[0] != expectedPieces[0])
            return false;
        var computedNumber = Number.parseFloat(computedPieces[1]);
        var expectedNumber = Number.parseFloat(expectedPieces[1]);
        if (Math.abs(computedNumber - expectedNumber) > tolerance)
            return false;
    }
    return true;
}

function compareFontStyle(computedValue, expectedValue, tolerance)
{
	var computed = computedValue.split(" ");
	var expected = expectedValue.split(" ");
	var computedAngle = computed[1].split("deg");
	var expectedAngle = expected[1].split("deg");
	return computed[0] == expected[0] && Math.abs(computedAngle[0] - expectedAngle[0]) <= tolerance;
}

// Called by CSS Image function filter() as well as filter property.
function compareFilterFunctions(computedValue, expectedValue, tolerance)
{
    var actual = typeof computedValue === "string" ? parseFilterFunctionList(computedValue) : computedValue;
    var expected = typeof expectedValue === "string" ? parseFilterFunctionList(expectedValue) : expectedValue;
    if (!actual || !Array.isArray(actual)         // Check that: actual is an array,
        || !expected || !Array.isArray(expected)  // expected is an array,
        || !actual.length                         // actual array has entries,
        || actual.length != expected.length       // actual and expected have same length
        || actual.length % 2 == 1)                // and image function names are the same.
        return false;

    for (var i = 0; i < actual.length; i += 2) {
        if (actual[i] != expected[i]) {
            console.error("Filter functions do not match.");
            return false;
        }
        if (!isCloseEnough(actual[i+1], expected[i+1], tolerance)) {
            console.error("Filter function values do not match.");
            return false;
        }
    }
    return true;
}

function basicShapeParametersMatch(paramList1, paramList2, tolerance)
{
    if (paramList1.shape != paramList2.shape
        || paramList1.params.length != paramList2.params.length)
        return false;
    var i = 0;
    for (; i < paramList1.params.length; ++i) {
        var param1 = paramList1.params[i], 
            param2 = paramList2.params[i];
        if (param1 === param2)
            continue;
        var match = isCloseEnough(param1, param2, tolerance);
        if (!match)
            return false;
    }
    return true;
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
    var element2Static = false;
    var elementId2;
    if (typeof elementId != "string") {
        if (elementId.length != 2)
            return;
            
        elementId2 = elementId[1];
        elementId = elementId[0];

        if (elementId2.indexOf("static:") == 0) {
            elementId2 = elementId2.replace("static:", "");
            element2Static = true;
        }

        compareElements = true;
    }
    
    // Check for a dot separated string
    var iframeId;
    if (!compareElements) {
        var array = elementId.split('.');
        if (array.length == 2) {
            iframeId = array[0];
            elementId = array[1];
        }
    }

    if (animationName && hasPauseAnimationAPI && !pauseAnimationAtTimeOnElement(animationName, time, document.getElementById(elementId))) {
        result += "FAIL - animation \"" + animationName + "\" is not running" + "<br>";
        return;
    }
    
    if (compareElements && !element2Static && animationName && hasPauseAnimationAPI && !pauseAnimationAtTimeOnElement(animationName, time, document.getElementById(elementId2))) {
        result += "FAIL - animation \"" + animationName + "\" is not running" + "<br>";
        return;
    }
    
    var computedValue, computedValue2;
    if (compareElements) {
        computedValue = getPropertyValue(property, elementId, iframeId);
        computedValue2 = getPropertyValue(property, elementId2, iframeId);

        if (comparePropertyValue(property, computedValue, computedValue2, tolerance))
            result += "PASS - \"" + property + "\" property for \"" + elementId + "\" and \"" + elementId2 + 
                            "\" elements at " + time + "s are close enough to each other" + "<br>";
        else
            result += "FAIL - \"" + property + "\" property for \"" + elementId + "\" and \"" + elementId2 + 
                            "\" elements at " + time + "s saw: \"" + computedValue + "\" and \"" + computedValue2 + 
                                            "\" which are not close enough to each other" + "<br>";
    } else {
        var elementName;
        if (iframeId)
            elementName = iframeId + '.' + elementId;
        else
            elementName = elementId;

        computedValue = getPropertyValue(property, elementId, iframeId);

        if (comparePropertyValue(property, computedValue, expectedValue, tolerance))
            result += "PASS - \"" + property + "\" property for \"" + elementName + "\" element at " + time + 
                            "s saw something close to: " + expectedValue + "<br>";
        else
            result += "FAIL - \"" + property + "\" property for \"" + elementName + "\" element at " + time + 
                            "s expected: " + expectedValue + " but saw: " + computedValue + "<br>";
    }
}


function getPropertyValue(property, elementId, iframeId)
{
    var computedValue;
    var element;
    if (iframeId)
        element = document.getElementById(iframeId).contentDocument.getElementById(elementId);
    else
        element = document.getElementById(elementId);

    if (property == "lineHeight")
        computedValue = parseInt(window.getComputedStyle(element).lineHeight);
    else if (property == "backgroundImage"
               || property == "borderImageSource"
               || property == "listStyleImage"
               || property == "webkitMaskImage"
               || property == "webkitMaskBoxImage"
               || property == "filter"
               || property == "-apple-color-filter"
               || property == "webkitFilter"
               || property == "webkitBackdropFilter"
               || property == "webkitClipPath"
               || property == "webkitShapeInside"
               || property == "webkitShapeOutside"
               || property == "font-variation-settings"
               || property == "font-style"
               || !property.indexOf("webkitTransform")
               || !property.indexOf("transform")) {
        computedValue = window.getComputedStyle(element)[property.split(".")[0]];
    } else if (property == "font-stretch") {
        var computedStyle = window.getComputedStyle(element).getPropertyCSSValue(property);
        computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_PERCENTAGE);
    } else {
        var computedStyle = window.getComputedStyle(element).getPropertyCSSValue(property);
        computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
    }

    return computedValue;
}

function comparePropertyValue(property, computedValue, expectedValue, tolerance)
{
    var result = true;

    if (!property.indexOf("webkitTransform") || !property.indexOf("transform")) {
        if (typeof expectedValue == "string")
            result = (computedValue == expectedValue);
        else if (typeof expectedValue == "number") {
            var m = matrixStringToArray(computedValue);
            result = isCloseEnough(parseFloat(m[parseInt(property.substring(16))]), expectedValue, tolerance);
        } else {
            var m = matrixStringToArray(computedValue);
            for (i = 0; i < expectedValue.length; ++i) {
                result = isCloseEnough(parseFloat(m[i]), expectedValue[i], tolerance);
                if (!result)
                    break;
            }
        }
    } else if (property == "webkitFilter" || property == "webkitBackdropFilter" || property == "filter" || property == "-apple-color-filter") {
        var filterParameters = parseFilterFunctionList(computedValue);
        var filter2Parameters = parseFilterFunctionList(expectedValue);
        result = compareFilterFunctions(filterParameters, filter2Parameters, tolerance);
    } else if (property == "webkitClipPath" || property == "webkitShapeInside" || property == "webkitShapeOutside") {
        var clipPathParameters = parseBasicShape(computedValue);
        var clipPathParameters2 = parseBasicShape(expectedValue);
        if (!clipPathParameters || !clipPathParameters2)
            result = false;
        result = basicShapeParametersMatch(clipPathParameters, clipPathParameters2, tolerance);
    } else if (property == "backgroundImage"
               || property == "borderImageSource"
               || property == "listStyleImage"
               || property == "webkitMaskImage"
               || property == "webkitMaskBoxImage")
        result = compareCSSImages(computedValue, expectedValue, tolerance);
    else if (property == "font-variation-settings")
        result = compareFontVariationSettings(computedValue, expectedValue, tolerance);
    else if (property == "font-style")
        result = compareFontStyle(computedValue, expectedValue, tolerance);
    else
        result = isCloseEnough(computedValue, expectedValue, tolerance);
    return result;
}

function endTest(finishCallback)
{
    document.getElementById('result').innerHTML = result;

    if (finishCallback)
        finishCallback();

    if (window.testRunner)
        testRunner.notifyDone();
}

function checkExpectedValueCallback(expected, index)
{
    return function() { checkExpectedValue(expected, index); };
}

function pauseAnimationAtTimeOnElement(animationName, time, element)
{
    const animations = element.getAnimations();
    for (let animation of animations) {
        if (animation instanceof CSSAnimation && animation.animationName == animationName) {
            animation.pause();
            animation.currentTime = time * 1000;
            return true;
        }
    }
    return false;
}

var testStarted = false;
function startTest(expected, startCallback, finishCallback)
{
    if (testStarted) return;
    testStarted = true;

    if (startCallback)
        startCallback();

    var maxTime = 0;

    for (var i = 0; i < expected.length; ++i) {
        var animationName = expected[i][0];
        var time = expected[i][1];

        // We can only use the animation fast-forward mechanism if there's an animation name
        if (animationName && hasPauseAnimationAPI)
            checkExpectedValue(expected, i);
        else {
            if (time > maxTime)
                maxTime = time;

            window.setTimeout(checkExpectedValueCallback(expected, i), time * 1000);
        }
    }

    if (maxTime > 0)
        window.setTimeout(function () {
            endTest(finishCallback);
        }, maxTime * 1000 + 50);
    else
        endTest(finishCallback);
}

var result = "";
var hasPauseAnimationAPI = true;

if (window.testRunner)
    testRunner.waitUntilDone();

function runAnimationTest(expected, startCallback, event, disablePauseAnimationAPI, doPixelTest, finishCallback)
{
    if (disablePauseAnimationAPI)
        hasPauseAnimationAPI = false;

    if (window.testRunner) {
        if (!doPixelTest)
            testRunner.dumpAsText();
    }
    
    if (!expected)
        throw("Expected results are missing!");
    
    var target = document;
    if (event == undefined)
        waitForAnimationToStart(target, function() { startTest(expected, startCallback, finishCallback); });
    else if (event == "load")
        window.addEventListener(event, function() {
            startTest(expected, startCallback, finishCallback);
        }, false);
}

function waitForAnimationToStart(element, callback)
{
    element.addEventListener('webkitAnimationStart', function() {
        requestAnimationFrame(callback); // delay to give hardware animations a chance to start
    }, false);
}

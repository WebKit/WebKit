// Copyright 2014 Google Inc.  All rights reserved.
// Copyright 2023 Apple Inc.  All rights reserved.

if (self.testRunner) {
	// svg/dynamic-updates tests set enablePixelTesting=true, as we want to dump text + pixel results
    if (self.enablePixelTesting)
        testRunner.dumpAsTextWithPixelResults();
    else
        testRunner.dumpAsText();
}

var description, debug, didFailSomeTests, successfullyParsed;

didFailSomeTests = false;

var expectingError; // set by shouldHaveError()
var expectedErrorMessage; // set by onerror when expectingError is true
var unexpectedErrorMessage; // set by onerror when expectingError is not true

(function() {

    function createHTMLElement(tagName)
    {
        // FIXME: In an XML document, document.createElement() creates an element with a null namespace URI.
        // So, we need use document.createElementNS() to explicitly create an element with the specified
        // tag name in the HTML namespace. We can remove this function and use document.createElement()
        // directly once we fix <https://bugs.webkit.org/show_bug.cgi?id=131074>.
        if (document.createElementNS)
            return document.createElementNS("http://www.w3.org/1999/xhtml", tagName);
        return document.createElement(tagName);
    }

    var rootElement = null;
    function ensureRootElement()
    {
        if (!rootElement || !rootElement.isConnected) {
            rootElement = document.body || document.documentElement;
            if (document.documentElement.namespaceURI == 'http://www.w3.org/2000/svg') {
                // FIXME: Make the test harness use SVG elements naively.
                var foreignObject = document.createElementNS('http://www.w3.org/2000/svg', 'foreignObject');
                foreignObject.setAttribute('x', '0px');
                foreignObject.setAttribute('y', '0px');
                foreignObject.setAttribute('width', '100%');
                foreignObject.setAttribute('height', '100%');
                foreignObject.setAttribute('style', 'padding: 10px; background-color: rgba(255, 255, 255, 0.5)');
                document.documentElement.appendChild(foreignObject);
                rootElement = foreignObject;
            }
        }
        return rootElement;
    }

    moveForeignObjectToTopIfNeeded = function () {
        if (rootElement && rootElement.localName == 'foreignObject')
            document.documentElement.appendChild(rootElement);
    }

    getOrCreate = function getOrCreate(id, tagName)
    {
        var element = document.getElementById(id);
        if (element)
            return element;

        element = createHTMLElement(tagName);
        element.id = id;
        var refNode;
        var parent = ensureRootElement();

        if (id == "description")
            refNode = getOrCreate("console", "div");
        else
            refNode = parent.firstChild;

        parent.insertBefore(element, refNode);
        return element;
    }

    description = function description(msg, quiet)
    {
        // For MSIE 6 compatibility
        var span = createHTMLElement("span");
        if (quiet)
            span.innerHTML = '<p>' + msg + '</p><p>On success, you will see no "<span class="fail">FAIL</span>" messages, followed by "<span class="pass">TEST COMPLETE</span>".</p>';
        else
            span.innerHTML = '<p>' + msg + '</p><p>On success, you will see a series of "<span class="pass">PASS</span>" messages, followed by "<span class="pass">TEST COMPLETE</span>".</p>';

        var description = getOrCreate("description", "p");
        if (description.firstChild)
            description.replaceChild(span, description.firstChild);
        else
            description.appendChild(span);
    };

    debug = function debug(msg)
    {
        var span = createHTMLElement("span");
        span.innerHTML = msg + '<br />';
        getOrCreate("console", "div").appendChild(span);
    };

    var css =
        ".pass {" +
            "font-weight: bold;" +
            "color: green;" +
        "}" +
        ".fail {" +
            "font-weight: bold;" +
            "color: red;" +
        "}" +
        "#console {" +
            "white-space: pre-wrap;" +
            "font-family: monospace;" +
        "}";

    function insertStyleSheet()
    {
        var styleElement = createHTMLElement("style");
        styleElement.textContent = css;
        (document.head || ensureRootElement()).appendChild(styleElement);
    }

    function handleTestFinished()
    {
        // FIXME: Get rid of this boolean.
        wasPostTestScriptParsed = true;
        if (window.jsTestIsAsync) {
            if (window.testRunner)
                testRunner.waitUntilDone();
            if (window.wasFinishJSTestCalled)
                finishJSTest();
        } else
            finishJSTest();
    }

    if (!isWorker()) {
        window.addEventListener('DOMContentLoaded', function() {
            // Call waitUntilDone() as early as possible otherwise some tests may complete before
            // the load event has fired.
            if (window.jsTestIsAsync && window.testRunner)
                testRunner.waitUntilDone();

            // Some tests set jsTestIsAsync in load event handler. Adding the listener late
            // makes handleTestFinished() run after the test handles load events.
            window.addEventListener("load", handleTestFinished, false);
        }, false);
        insertStyleSheet();
    }

    if (!self.isOnErrorTest) {
        self.onerror = function(message)
        {
            if (self.expectingError) {
                self.expectedErrorMessage = message;
                self.expectingError = false;
                return;
            }
            self.unexpectedErrorMessage = message;
            if (self.jsTestIsAsync) {
                self.testFailed("Unexpected error: " + message);
                finishJSTest();
            }
        };
    }
})();

function isWorker()
{
    // It's conceivable that someone would stub out 'document' in a worker so
    // also check for childNodes, an arbitrary DOM-related object that is
    // meaningless in a WorkerContext.
    return (typeof document === 'undefined' || typeof document.childNodes === 'undefined') && !!self.importScripts;
}

function descriptionQuiet(msg) { description(msg, true); }

function escapeHTML(text)
{
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/\0/g, "\\0");
}

function testPassed(msg)
{
    debug('<span><span class="pass">PASS</span> ' + escapeHTML(msg) + '</span>');
}

function testFailed(msg)
{
    didFailSomeTests = true;
    debug('<span><span class="fail">FAIL</span> ' + escapeHTML(msg) + '</span>');
}

function areArraysEqual(a, b)
{
    try {
        if (a.length !== b.length)
            return false;
        for (var i = 0; i < a.length; i++)
            if (a[i] !== b[i])
                return false;
    } catch (ex) {
        return false;
    }
    return true;
}

function isMinusZero(n)
{
    // the only way to tell 0 from -0 in JS is the fact that 1/-0 is
    // -Infinity instead of Infinity
    return n === 0 && 1/n < 0;
}

function isNewSVGTearOffType(v)
{
    return ['[object SVGLength]', '[object SVGLengthList]', '[object SVGPoint]', '[object SVGPointList]', '[object SVGNumber]'].indexOf(""+v) != -1;
}

function isResultCorrect(actual, expected)
{
    if (expected === 0)
        return actual === expected && (1/actual) === (1/expected);
    if (actual === expected)
        return true;
    // http://crbug.com/308818 : The new implementation of SVGListProperties do not necessary return the same wrapper object, so === operator would not work. We compare for their string representation instead.
    if (isNewSVGTearOffType(expected) && typeof(expected) == typeof(actual) && actual.valueAsString == expected.valueAsString)
        return true;
    if (typeof(expected) == "number" && isNaN(expected))
        return typeof(actual) == "number" && isNaN(actual);
    if (expected && (Object.prototype.toString.call(expected) == Object.prototype.toString.call([])))
        return areArraysEqual(actual, expected);
    return false;
}

function stringify(v)
{
    if (isNewSVGTearOffType(v))
        return v.valueAsString;
    if (v === 0 && 1/v < 0)
        return "-0";
    else return "" + v;
}

function evalAndLog(_a, _quiet)
{
  if (typeof _a != "string")
    debug("WARN: tryAndLog() expects a string argument");

  // Log first in case things go horribly wrong or this causes a sync event.
  if (!_quiet)
    debug(_a);

  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
    testFailed(_a + " threw exception " + e);
  }
  return _av;
}

function shouldBe(_a, _b, _quiet)
{
    if ((typeof _a != "function" && typeof _a != "string") || (typeof _b != "function" && typeof _b != "string"))
        debug("WARN: shouldBe() expects function or string arguments");
    var _exception;
    var _av;
    try {
        _av = (typeof _a == "function" ? _a() : eval(_a));
    } catch (e) {
        _exception = e;
    }
    var _bv = (typeof _b == "function" ? _b() : eval(_b));

    if (_exception)
        testFailed(_a + " should be " + stringify(_bv) + ". Threw exception " + _exception);
    else if (isResultCorrect(_av, _bv)) {
        if (!_quiet) {
            testPassed(_a + " is " + (typeof _b == "function" ? _bv : _b));
        }
    } else if (typeof(_av) == typeof(_bv))
        testFailed(_a + " should be " + stringify(_bv) + ". Was " + stringify(_av) + ".");
    else
        testFailed(_a + " should be " + stringify(_bv) + " (of type " + typeof _bv + "). Was " + _av + " (of type " + typeof _av + ").");
}

function shouldBeOneOfValues(_a, _values)
{
    if ((typeof _a != "function" && typeof _a != "string"))
        debug("WARN: shouldBeOneOfValues() expects the first argument to be a function or a string");
    if (!Array.isArray(_values)) {
        testFailed("The second argument to shouldBeOneOfValues() must be an array of values");
        return;
    }

    var _exception;
    var _av;
    try {
        _av = (typeof _a == "function" ? _a() : eval(_a));
    } catch (e) {
        _exception = e;
    }

    var stringifiedValues = '';
    for (var i = 0; i < _values.length; ++i) {
        if (i) {
            if (i + 1 == _values.length)
                stringifiedValues += ', and ';
            else
                stringifiedValues += ','
        }
        stringifiedValues += "`" + stringify(_values[i]) + "`";
    }
    if (_exception)
        testFailed(_a + " should be one of " + stringifiedValues + ". Threw exception " + _exception);
    else {
        var matchedValue = _values.find(function (value) { return isResultCorrect(_av, value); });
        if (matchedValue !== undefined) {
            testPassed(_a + " is one of " + stringifiedValues);
        } else {
            testFailed(_a + " should be one of " + stringifiedValues + ". Was " + stringify(_av) + ".");
        }
    }
}

// Execute condition every 5 milliseconds until it succeeds.
function _waitForCondition(condition, completionHandler)
{
  if (condition())
    completionHandler();
  else
    setTimeout(_waitForCondition, 5, condition, completionHandler);
}

function shouldBecomeEqual(_a, _b, completionHandler)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldBecomeEqual() expects string arguments");

  function condition() {
    var exception;
    var _av;
    try {
      _av = eval(_a);
    } catch (e) {
      exception = e;
    }
    var _bv = eval(_b);
    if (exception)
      testFailed(_a + " should become " + _bv + ". Threw exception " + exception);
    if (isResultCorrect(_av, _bv)) {
      testPassed(_a + " became " + _b);
      return true;
    }
    return false;
  }

  if (!completionHandler)
    return new Promise(resolve => setTimeout(_waitForCondition, 0, condition, resolve));

  setTimeout(_waitForCondition, 0, condition, completionHandler);
}

function shouldBecomeEqualToString(value, reference, completionHandler)
{
  if (typeof value !== "string" || typeof reference !== "string")
    debug("WARN: shouldBecomeEqualToString() expects string arguments");
  var unevaledString = JSON.stringify(reference);
  shouldBecomeEqual(value, unevaledString, completionHandler);
}

function shouldBeType(_a, _type) {
  var _exception;
  var _av;
  try {
    _av = eval(_a);
  } catch (e) {
    _exception = e;
  }

  var _typev = eval(_type);
  if (_av instanceof _typev) {
    testPassed(_a + " is an instance of " + _type);
  } else {
    testFailed(_a + " is not an instance of " + _type);
  }
}

// Variant of shouldBe()--confirms that result of eval(_to_eval) is within
// numeric _tolerance of numeric _target.
function shouldBeCloseTo(_to_eval, _target, _tolerance, _quiet)
{
  if (typeof _to_eval != "string") {
    testFailed("shouldBeCloseTo() requires string argument _to_eval. was type " + typeof _to_eval);
    return;
  }
  if (typeof _target != "number") {
    testFailed("shouldBeCloseTo() requires numeric argument _target. was type " + typeof _target);
    return;
  }
  if (typeof _tolerance != "number") {
    testFailed("shouldBeCloseTo() requires numeric argument _tolerance. was type " + typeof _tolerance);
    return;
  }

  var _result;
  try {
     _result = eval(_to_eval);
  } catch (e) {
    testFailed(_to_eval + " should be within " + _tolerance + " of "
               + _target + ". Threw exception " + e);
    return;
  }

  if (typeof(_result) != typeof(_target)) {
    testFailed(_to_eval + " should be of type " + typeof _target
               + " but was of type " + typeof _result);
  } else if (Math.abs(_result - _target) <= _tolerance) {
    if (!_quiet) {
        testPassed(_to_eval + " is within " + _tolerance + " of " + _target);
    }
  } else {
    testFailed(_to_eval + " should be within " + _tolerance + " of " + _target
               + ". Was " + _result + ".");
  }
}

function shouldNotBe(_a, _b, _quiet)
{
    if ((typeof _a != "function" && typeof _a != "string") || (typeof _b != "function" && typeof _b != "string"))
        debug("WARN: shouldNotBe() expects function or string arguments");
    var _exception;
    var _av;
    try {
        _av = (typeof _a == "function" ? _a() : eval(_a));
    } catch (e) {
        _exception = e;
    }
    var _bv = (typeof _b == "function" ? _b() : eval(_b));

    if (_exception)
        testFailed(_a + " should not be " + _bv + ". Threw exception " + _exception);
    else if (!isResultCorrect(_av, _bv)) {
        if (!_quiet) {
            testPassed(_a + " is not " + (typeof _b == "function" ? _bv : _b));
        }
    } else
        testFailed(_a + " should not be " + _bv + ".");
}

function shouldBecomeDifferent(_a, _b, completionHandler)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldBecomeDifferent() expects string arguments");

  function condition() {
    var exception;
    var _av;
    try {
      _av = eval(_a);
    } catch (e) {
      exception = e;
    }
    var _bv = eval(_b);
    if (exception)
      testFailed(_a + " should became not equal to " + _bv + ". Threw exception " + exception);
    if (!isResultCorrect(_av, _bv)) {
      testPassed(_a + " became different from " + _b);
      return true;
    }
    return false;
  }

  if (!completionHandler)
    return new Promise(resolve => setTimeout(_waitForCondition, 0, condition, resolve));

  setTimeout(_waitForCondition, 0, condition, completionHandler);
}

function shouldBeTrue(a, quiet) { shouldBe(a, "true", quiet); }
function shouldBeTrueQuiet(a) { shouldBe(a, "true", true); }
function shouldBeFalse(a, quiet) { shouldBe(a, "false", quiet); }
function shouldBeNaN(a, quiet) { shouldBe(a, "NaN", quiet); }
function shouldBeNull(a, quiet) { shouldBe(a, "null", quiet); }
function shouldBeZero(a, quiet) { shouldBe(a, "0", quiet); }

function shouldBeEqualToString(a, b)
{
  if (typeof a !== "string" || typeof b !== "string")
    debug("WARN: shouldBeEqualToString() expects string arguments");
  var unevaledString = JSON.stringify(b);
  shouldBe(a, unevaledString);
}

function shouldBeEqualToNumber(a, b)
{
  if (typeof a !== "string" || typeof b !== "number")
    debug("WARN: shouldBeEqualToNumber() expects a string and a number arguments");
  var unevaledString = JSON.stringify(b);
  shouldBe(a, unevaledString);
}

function shouldBeEmptyString(a) { shouldBeEqualToString(a, ""); }

function shouldEvaluateTo(actual, expected) {
  // A general-purpose comparator.  'actual' should be a string to be
  // evaluated, as for shouldBe(). 'expected' may be any type and will be
  // used without being eval'ed.
  if (expected == null) {
    // Do this before the object test, since null is of type 'object'.
    shouldBeNull(actual);
  } else if (typeof expected == "undefined") {
    shouldBeUndefined(actual);
  } else if (typeof expected == "function") {
    // All this fuss is to avoid the string-arg warning from shouldBe().
    try {
      var actualValue = eval(actual);
    } catch (e) {
      testFailed("Evaluating " + actual + ": Threw exception " + e);
      return;
    }
    shouldBe("'" + actualValue.toString().replace(/\n/g, "") + "'",
             "'" + expected.toString().replace(/\n/g, "") + "'");
  } else if (typeof expected == "object") {
    shouldBeTrue(actual + " == '" + expected + "'");
  } else if (typeof expected == "string") {
    shouldBe(actual, expected);
  } else if (typeof expected == "boolean") {
    shouldBe("typeof " + actual, "'boolean'");
    if (expected)
      shouldBeTrue(actual);
    else
      shouldBeFalse(actual);
  } else if (typeof expected == "number") {
    shouldBe(actual, stringify(expected));
  } else {
    debug(expected + " is unknown type " + typeof expected);
    shouldBeTrue(actual, "'"  +expected.toString() + "'");
  }
}

function shouldBeNonZero(_a)
{
  var _exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     _exception = e;
  }

  if (_exception)
    testFailed(_a + " should be non-zero. Threw exception " + _exception);
  else if (_av != 0)
    testPassed(_a + " is non-zero.");
  else
    testFailed(_a + " should be non-zero. Was " + _av);
}

function shouldBeNonNull(_a)
{
  var _exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     _exception = e;
  }

  if (_exception)
    testFailed(_a + " should be non-null. Threw exception " + _exception);
  else if (_av != null)
    testPassed(_a + " is non-null.");
  else
    testFailed(_a + " should be non-null. Was " + _av);
}

function shouldBeUndefined(_a)
{
  var _exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
      _exception = e;
  }

  if (_exception)
    testFailed(_a + " should be undefined. Threw exception " + _exception);
  else if (typeof _av == "undefined")
    testPassed(_a + " is undefined.");
  else
    testFailed(_a + " should be undefined. Was " + _av);
}

function shouldBeDefined(_a)
{
  var _exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     _exception = e;
  }

  if (_exception)
    testFailed(_a + " should be defined. Threw exception " + _exception);
  else if (_av !== undefined)
    testPassed(_a + " is defined.");
  else
    testFailed(_a + " should be defined. Was " + _av);
}

function shouldBeGreaterThan(_a, _b) {
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeGreaterThan expects string arguments");

    var _exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        _exception = e;
    }
    var _bv = eval(_b);

    if (_exception)
        testFailed(_a + " should be > " + _b + ". Threw exception " + _exception);
    else if (typeof _av == "undefined" || _av <= _bv)
        testFailed(_a + " should be > " + _b + ". Was " + _av + " (of type " + typeof _av + ").");
    else
        testPassed(_a + " is > " + _b);
}

function shouldBeGreaterThanOrEqual(_a, _b) {
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeGreaterThanOrEqual expects string arguments");

    var _exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        _exception = e;
    }
    var _bv = eval(_b);

    if (_exception)
        testFailed(_a + " should be >= " + _b + ". Threw exception " + _exception);
    else if (typeof _av == "undefined" || _av < _bv)
        testFailed(_a + " should be >= " + _b + ". Was " + _av + " (of type " + typeof _av + ").");
    else
        testPassed(_a + " is >= " + _b);
}

function shouldBeLessThanOrEqual(_a, _b) {
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeLessThanOrEqual expects string arguments");

    var _exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        _exception = e;
    }
    var _bv = eval(_b);

    if (_exception)
        testFailed(_a + " should be <= " + _b + ". Threw exception " + _exception);
    else if (typeof _av == "undefined" || _av > _bv)
        testFailed(_a + " should be <= " + _b + ". Was " + _av + " (of type " + typeof _av + ").");
    else
        testPassed(_a + " is <= " + _b);
}

function expectTrue(v, msg) {
  if (v) {
    testPassed(msg);
  } else {
    testFailed(msg);
  }
}

function shouldNotThrow(_a, _message) {
    try {
        typeof _a == "function" ? _a() : eval(_a);
        testPassed((_message ? _message : _a) + " did not throw exception.");
    } catch (e) {
        testFailed((_message ? _message : _a) + " should not throw exception. Threw exception " + e + ".");
    }
}

function shouldThrow(_a, _e, _message)
{
    var _exception;
    var _av;
    try {
        _av = typeof _a == "function" ? _a() : eval(_a);
    } catch (e) {
        _exception = e;
    }

    var _ev;
    if (_e)
        _ev = eval(_e);

    if (_exception) {
        if (typeof _e == "undefined" || _exception == _ev)
            testPassed((_message ? _message : _a) + " threw exception " + _exception + ".");
        else
            testFailed((_message ? _message : _a) + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Threw exception " + _exception + ".");
    } else if (typeof _av == "undefined")
        testFailed((_message ? _message : _a) + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was undefined.");
    else
        testFailed((_message ? _message : _a) + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was " + _av + ".");
}

function shouldBeNow(a, delta)
{
    // Right now, V8 and Chromium / Blink use two different clock
    // implementations. On Windows, the implementations are non-trivial and can
    // be slightly out of sync. The delta is intended to compensate for that.
    //
    // FIXME: reconsider this when the V8 and Blink clocks get unified, see http://crbug.com/324110
    if (delta === undefined)
        delta = 1000;

    for (var i = 0; i < 1000; ++i) {
        var startDate = Date.now();
        var av = eval(a);
        var date = av.valueOf();
        var endDate = Date.now();

        // On some occasions such as NTP updates, the current time can go
        // backwards. This should only happen rarely, so we can get away with
        // retrying the test a few times if we detect the time going backwards.
        if (startDate > endDate)
            continue;

        if (typeof date !== "number") {
            testFailed(a + " is not a number or a Date. Got " + av);
            return;
        }
        if (date < startDate - delta) {
            testFailed(a + " is not the curent time. Got " + av + " which is " + (startDate - date) / 1000 + " seconds in the past.");
            return;
        }
        if (date > endDate + delta) {
            testFailed(a + " is not the current time. Got " + av + " which is " + (date - endDate) / 1000 + " seconds in the future.");
            return;
        }

        testPassed(a + " is equivalent to Date.now().");
        return;
    }
    testFailed(a + " cannot be tested against the current time. The clock is going backwards too often.");
}

function expectError()
{
    if (expectingError) {
        testFailed("shouldHaveError() called twice before an error occurred!");
    }
    expectingError = true;
}

function shouldReject(_a, _message)
{
    return shouldRejectWithErrorName(_a, undefined, _message);
}

function shouldRejectWithErrorName(_a, _name, _message)
{
    var _exception;
    var _av;
    try {
        _av = typeof _a == "function" ? _a() : eval(_a);
    } catch (e) {
        testFailed((_message ? _message : _a) + " should not throw exception. Threw exception " + e + ".");
        return Promise.resolve();
    }

    return _av.then(function(result) {
        testFailed((_message ? _message : _a) + " should reject promise. Resolved with " + result + ".");
    }, function(error) {
        if (_name === undefined) {
            testPassed((_message ? _message : _a) + " rejected promise.");
        } else if (error['name'] === _name) {
            // FIXME: Remove the extra space and '.' (DOMException descriptions already end with periods) then rebase tests.
            testPassed((_message ? _message : _a) + " rejected promise  with " + error + ".");
        } else
            testFailed((_message ? _message : _a) + " should reject promise with " + _name + ". Rejected with " + error['name'] + " instead.");
    });
}

function shouldThrowErrorName(_a, _name)
{
    var _exception;
    try {
        typeof _a == "function" ? _a() : eval(_a);
    } catch (e) {
        _exception = e;
    }

    if (_exception) {
        if (_exception.name == _name)
            testPassed(_a + " threw exception " + _exception + ".");
        else
            testFailed(_a + " should throw a " + _name + ". Threw a " + _exception.name + ".");
    } else
        testFailed(_a + " should throw a " + _name + ". Did not throw.");
}

function shouldHaveHadError(message)
{
    if (expectingError) {
        testFailed("No error thrown between expectError() and shouldHaveHadError()");
        return;
    }

    if (expectedErrorMessage) {
        if (!message)
            testPassed("Got expected error");
        else if (expectedErrorMessage.indexOf(message) !== -1)
            testPassed("Got expected error: '" + message + "'");
        else
            testFailed("Unexpected error '" + message + "'");
        expectedErrorMessage = undefined;
        return;
    }

    testFailed("expectError() not called before shouldHaveHadError()");
}

function gc() {
    if (typeof GCController !== "undefined")
        GCController.collect();
    else {
        var gcRec = function (n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        };
        for (var i = 0; i < 1000; i++)
            gcRec(10);
    }
}

function minorGC() {
    if (typeof GCController !== "undefined")
        GCController.minorCollect();
    else
        testFailed("Minor GC is available only when you enable the --expose-gc option in V8.");
}

function isSuccessfullyParsed()
{
    // FIXME: Remove this and only report unexpected syntax errors.
    successfullyParsed = !unexpectedErrorMessage;
    shouldBeTrue("successfullyParsed");
    if (didFailSomeTests)
        debug("Some tests failed.");
    debug('<br /><span class="pass">TEST COMPLETE</span>');
}

// It's possible for an async test to call finishJSTest() before js-test-post.js
// has been parsed.
function finishJSTest()
{
    wasFinishJSTestCalled = true;
    if (!self.wasPostTestScriptParsed)
        return;
    isSuccessfullyParsed();
    moveForeignObjectToTopIfNeeded();
    if (self.jsTestIsAsync && self.testRunner)
        testRunner.notifyDone();
}

function areObjectsEqual(a, b) {
	for (var property in a) {
		if (!b.hasOwnProperty(property))
			return false;

		switch (typeof (a[property])) {
		case 'function':
			if (typeof b[property] == 'undefined' || a[property].toString() != b[property].toString())
				return false;
			break;
		case 'object':
			if (!areObjectsEqual(a, b))
				return false;
			break;
		default:
			if (a[property] != b[property])
				return false;
		}
	}
 
	for (var property in b) {
		if (!a.hasOwnProperty(property))
			return false;
	}
	
	return true;
};

function startWorker(testScriptURL)
{
    self.jsTestIsAsync = true;
    debug('Starting worker: ' + testScriptURL);
    var worker = new Worker(testScriptURL);
    worker.onmessage = function(event)
    {
        var workerPrefix = "[Worker] ";
        if (event.data.length < 5 || event.data.charAt(4) != ':') {
          debug(workerPrefix + event.data);
          return;
        }
        var code = event.data.substring(0, 4);
        var payload = workerPrefix + event.data.substring(5);
        if (code == "PASS")
            testPassed(payload);
        else if (code == "FAIL")
            testFailed(payload);
        else if (code == "DESC")
            description(payload);
        else if (code == "DONE")
            finishJSTest();
        else
            debug(workerPrefix + event.data);
    };

    worker.onerror = function(event)
    {
        debug('Got error from worker: ' + event.message);
        finishJSTest();
    };

    return worker;
}

if (isWorker()) {
    var workerPort = self;
    description = function(msg, quiet) {
        workerPort.postMessage('DESC:' + msg);
    };
    testFailed = function(msg) {
        workerPort.postMessage('FAIL:' + msg);
    };
    testPassed = function(msg) {
        workerPort.postMessage('PASS:' + msg);
    };
    finishJSTest = function() {
        workerPort.postMessage('DONE:');
    };
    debug = function(msg) {
        workerPort.postMessage(msg);
    };
}

function downgradeReferrerCallback(policy, host) {
    let scriptElement = document.createElement("script");
    scriptElement.src = "http://".concat(host, ":8000/referrer-policy/resources/script.py");
    scriptElement.referrerPolicy = policy;
    document.body.appendChild(scriptElement);
}

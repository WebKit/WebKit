// svg/dynamic-updates tests set enablePixelTesting=true, as we want to dump text + pixel results
if (self.testRunner)
    testRunner.dumpAsText(self.enablePixelTesting);

var description, debug, successfullyParsed, errorMessage, silentTestPass, didPassSomeTestsSilently, didFailSomeTests;

silentTestPass = false;
didPassSomeTestsSilently = false;
didFailSomeTests = false;

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

    function getOrCreate(id, tagName)
    {
        var element = document.getElementById(id);
        if (element)
            return element;

        element = createHTMLElement(tagName);
        element.id = id;
        var refNode;
        var parent = document.body || document.documentElement;
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
        getOrCreate("console", "div").appendChild(span); // insert it first so XHTML knows the namespace
        span.innerHTML = msg + '<br />';
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
        (document.head || document.documentElement).appendChild(styleElement);
    }

    if (!isWorker())
        insertStyleSheet();

    self.onerror = function(message)
    {
        errorMessage = message;
    };

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

function escapeHTMLAndStripFileURLs(text)
{
    return escapeHTML(text).replace(/file:\/\/.*LayoutTests/g, "file:///LayoutTests");
}

function testPassed(msg)
{
    if (silentTestPass)
        didPassSomeTestsSilently = true;
    else
        debug('<span><span class="pass">PASS</span> ' + escapeHTMLAndStripFileURLs(msg) + '</span>');
}

function testFailed(msg)
{
    didFailSomeTests = true;
    debug('<span><span class="fail">FAIL</span> ' + escapeHTMLAndStripFileURLs(msg) + '</span>');
}

function areNumbersEqual(_actual, _expected)
{
    if (_expected === 0)
        return _actual === _expected && (1/_actual) === (1/_expected);
    if (_actual === _expected)
        return true;
    if (typeof(_expected) == "number" && isNaN(_expected))
        return typeof(_actual) == "number" && isNaN(_actual);
    return false;
}

function areArraysEqual(_a, _b)
{
    try {
        if (_a.length !== _b.length)
            return false;
        for (var i = 0; i < _a.length; i++)
            if (!areNumbersEqual(_a[i], _b[i]))
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

function isTypedArray(array)
{
    return array instanceof Int8Array
        || array instanceof Int16Array
        || array instanceof Int32Array
        || array instanceof Uint8Array
        || array instanceof Uint8ClampedArray
        || array instanceof Uint16Array
        || array instanceof Uint32Array
        || array instanceof Float32Array
        || array instanceof Float64Array;
}

function isResultCorrect(_actual, _expected)
{
    if (areNumbersEqual(_actual, _expected))
        return true;
    if (_expected
        && (Object.prototype.toString.call(_expected) ==
            Object.prototype.toString.call([])
            || isTypedArray(_expected)))
        return areArraysEqual(_actual, _expected);
    return false;
}

function stringify(v)
{
    if (v === 0 && 1/v < 0)
        return "-0";
    else if (isTypedArray(v))
        return v.__proto__.constructor.name + ":[" + Array.prototype.join.call(v, ",") + "]";
    else
        return "" + v;
}

function evalAndLog(_a, _quiet)
{
    if (typeof _a != "string")
        debug("WARN: evalAndLog() expects a string argument");

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

function evalAndLogResult(_a)
{
    if (typeof _a != "string")
        debug("WARN: evalAndLogResult() expects a string argument");

    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        testFailed(_a + " threw exception " + e);
    }

    debug('<span>' + _a + " is " + escapeHTMLAndStripFileURLs(_av) + '</span>');
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

function dfgShouldBe(theFunction, _a, _b)
{
    if (typeof theFunction != "function" || typeof _a != "string" || typeof _b != "string")
        debug("WARN: dfgShouldBe() expects a function and two strings");
    noInline(theFunction);
    var exception;
    var values = [];

    // Defend against tests that muck with numeric properties on array.prototype.
    values.__proto__ = null;
    values.push = Array.prototype.push;

    try {
        while (!dfgCompiled({f:theFunction}))
            values.push(eval(_a));
        values.push(eval(_a));
    } catch (e) {
        exception = e;
    }

    var _bv = eval(_b);
    if (exception)
        testFailed(_a + " should be " + stringify(_bv) + ". On iteration " + (values.length + 1) + ", threw exception " + exception);
    else {
        var allPassed = true;
        for (var i = 0; i < values.length; ++i) {
            var _av = values[i];
            if (isResultCorrect(_av, _bv))
                continue;
            if (typeof(_av) == typeof(_bv))
                testFailed(_a + " should be " + stringify(_bv) + ". On iteration " + (i + 1) + ", was " + stringify(_av) + ".");
            else
                testFailed(_a + " should be " + stringify(_bv) + " (of type " + typeof _bv + "). On iteration " + (i + 1) + ", was " + _av + " (of type " + typeof _av + ").");
            allPassed = false;
        }
        if (allPassed)
            testPassed(_a + " is " + _b + " on all iterations including after DFG tier-up.");
    }

    return values.length;
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
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
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
function shouldBeCloseTo(_to_eval, _target, _tolerance, quiet)
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
        if (!quiet) {
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

function shouldBeTrue(_a) { shouldBe(_a, "true"); }
function shouldBeTrueQuiet(_a) { shouldBe(_a, "true", true); }
function shouldBeFalse(_a) { shouldBe(_a, "false"); }
function shouldBeNaN(_a) { shouldBe(_a, "NaN"); }
function shouldBeNull(_a) { shouldBe(_a, "null"); }
function shouldBeZero(_a) { shouldBe(_a, "0"); }

function shouldBeEqualToString(a, b)
{
    if (typeof a !== "string" || typeof b !== "string")
        debug("WARN: shouldBeEqualToString() expects string arguments");
    var unevaledString = JSON.stringify(b);
    shouldBe(a, unevaledString);
}

function shouldNotBeEqualToString(a, b)
{
    if (typeof a !== "string" || typeof b !== "string")
        debug("WARN: shouldBeEqualToString() expects string arguments");
    var unevaledString = JSON.stringify(b);
    shouldNotBe(a, unevaledString);
}
function shouldBeEmptyString(_a) { shouldBeEqualToString(_a, ""); }

function shouldEvaluateTo(actual, expected)
{
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
            actualValue = eval(actual);
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
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }

    if (exception)
        testFailed(_a + " should be non-zero. Threw exception " + exception);
    else if (_av != 0)
        testPassed(_a + " is non-zero.");
    else
        testFailed(_a + " should be non-zero. Was " + _av);
}

function shouldBeNonNull(_a)
{
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }

    if (exception)
        testFailed(_a + " should be non-null. Threw exception " + exception);
    else if (_av != null)
        testPassed(_a + " is non-null.");
    else
        testFailed(_a + " should be non-null. Was " + _av);
}

function shouldBeUndefined(_a)
{
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }

    if (exception)
        testFailed(_a + " should be undefined. Threw exception " + exception);
    else if (typeof _av == "undefined")
        testPassed(_a + " is undefined.");
    else
        testFailed(_a + " should be undefined. Was " + _av);
}

function shouldBeDefined(_a)
{
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }

    if (exception)
        testFailed(_a + " should be defined. Threw exception " + exception);
    else if (_av !== undefined)
        testPassed(_a + " is defined.");
    else
        testFailed(_a + " should be defined. Was " + _av);
}

function shouldBeGreaterThan(_a, _b) {
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeGreaterThan expects string arguments");

    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }
    var _bv = eval(_b);

    if (exception)
        testFailed(_a + " should be > " + _b + ". Threw exception " + exception);
    else if (typeof _av == "undefined" || _av <= _bv)
        testFailed(_a + " should be > " + _b + ". Was " + _av + " (of type " + typeof _av + ").");
    else
        testPassed(_a + " is > " + _b);
}

function shouldBeGreaterThanOrEqual(_a, _b) {
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeGreaterThanOrEqual expects string arguments");

    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }
    var _bv = eval(_b);

    if (exception)
        testFailed(_a + " should be >= " + _b + ". Threw exception " + exception);
    else if (typeof _av == "undefined" || _av < _bv)
        testFailed(_a + " should be >= " + _b + ". Was " + _av + " (of type " + typeof _av + ").");
    else
        testPassed(_a + " is >= " + _b);
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

function shouldReject(_a, _message)
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
        testPassed((_message ? _message : _a) + " rejected promise  with " + error + ".");
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
    if (errorMessage) {
        if (!message)
            testPassed("Got expected error");
        else if (errorMessage.indexOf(message) !== -1)
            testPassed("Got expected error: '" + message + "'");
        else
            testFailed("Unexpexted error '" + message + "'");
    } else
        testFailed("Missing expexted error");
    errorMessage = undefined;
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
            gcRec(10)
    }
}

function dfgCompiled(argument)
{
    var numberOfCompiles = "compiles" in argument ? argument.compiles : 1;

    if (!("f" in argument))
        throw new Error("dfgCompiled called with invalid argument.");

    if (argument.f instanceof Array) {
        for (var i = 0; i < argument.f.length; ++i) {
            if (testRunner.numberOfDFGCompiles(argument.f[i]) < numberOfCompiles)
                return false;
        }
    } else {
        if (testRunner.numberOfDFGCompiles(argument.f) < numberOfCompiles)
            return false;
    }

    return true;
}

function dfgIncrement(argument)
{
    if (!self.testRunner)
        return argument.i;

    if (argument.i < argument.n)
        return argument.i;

    if (didFailSomeTests)
        return argument.i;

    if (!dfgCompiled(argument))
        return "start" in argument ? argument.start : 0;

    return argument.i;
}

function noInline(theFunction)
{
    if (!self.testRunner)
        return;

    testRunner.neverInlineFunction(theFunction);
}

function isSuccessfullyParsed()
{
    // FIXME: Remove this and only report unexpected syntax errors.
    if (!errorMessage)
        successfullyParsed = true;
    shouldBeTrue("successfullyParsed");
    if (silentTestPass && didPassSomeTestsSilently)
        debug("Passed some tests silently.");
    if (silentTestPass && didFailSomeTests)
        debug("Some tests failed.");
    debug('<br /><span class="pass">TEST COMPLETE</span>');
}

function asyncTestStart() {
    if (self.testRunner)
        testRunner.waitUntilDone();
}

function asyncTestPassed() {
    if (self.testRunner)
        testRunner.notifyDone();
}

// It's possible for an async test to call finishJSTest() before js-test-post.js
// has been parsed.
function finishJSTest()
{
    wasFinishJSTestCalled = true;
    if (!self.wasPostTestScriptParsed)
        return;
    isSuccessfullyParsed();
    if (self.jsTestIsAsync && self.testRunner)
        testRunner.notifyDone();
}

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
    }

    return worker;
}

if (isWorker()) {
    var workerPort = self;
    description = function(msg, quiet) {
        workerPort.postMessage('DESC:' + msg);
    }
    testFailed = function(msg) {
        workerPort.postMessage('FAIL:' + msg);
    }
    testPassed = function(msg) {
        workerPort.postMessage('PASS:' + msg);
    }
    finishJSTest = function() {
        workerPort.postMessage('DONE:');
    }
    debug = function(msg) {
        workerPort.postMessage(msg);
    }
}

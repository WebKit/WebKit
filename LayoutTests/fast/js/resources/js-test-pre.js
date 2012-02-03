// svg/dynamic-updates tests set enablePixelTesting=true, as we want to dump text + pixel results
if (self.layoutTestController)
    layoutTestController.dumpAsText(self.enablePixelTesting);

var description, debug, successfullyParsed, errorMessage;

(function() {

    function getOrCreate(id, tagName)
    {
        var element = document.getElementById(id);
        if (element)
            return element;

        element = document.createElement(tagName);
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
        var span = document.createElement("span");
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
        var span = document.createElement("span");
        getOrCreate("console", "div").appendChild(span); // insert it first so XHTML knows the namespace
        span.innerHTML = msg + '<br />';
    };

    function isWorker()
    {
        return typeof document === 'undefined';
    }

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
        var styleElement = document.createElement("style");
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
    debug('<span><span class="fail">FAIL</span> ' + escapeHTML(msg) + '</span>');
}

function areArraysEqual(_a, _b)
{
    try {
        if (_a.length !== _b.length)
            return false;
        for (var i = 0; i < _a.length; i++)
            if (_a[i] !== _b[i])
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

function isResultCorrect(_actual, _expected)
{
    if (_expected === 0)
        return _actual === _expected && (1/_actual) === (1/_expected);
    if (_actual === _expected)
        return true;
    if (typeof(_expected) == "number" && isNaN(_expected))
        return typeof(_actual) == "number" && isNaN(_actual);
    if (_expected && (Object.prototype.toString.call(_expected) == Object.prototype.toString.call([])))
        return areArraysEqual(_actual, _expected);
    return false;
}

function stringify(v)
{
    if (v === 0 && 1/v < 0)
        return "-0";
    else return "" + v;
}

function evalAndLog(_a)
{
  if (typeof _a != "string")
    debug("WARN: tryAndLog() expects a string argument");

  // Log first in case things go horribly wrong or this causes a sync event.
  debug(_a);

  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
    testFailed(_a + " threw exception " + e);
  }
  return _av;
}

function shouldBe(_a, _b, quiet)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldBe() expects string arguments");
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
  else if (isResultCorrect(_av, _bv)) {
    if (!quiet) {
        testPassed(_a + " is " + _b);
    }
  } else if (typeof(_av) == typeof(_bv))
    testFailed(_a + " should be " + _bv + ". Was " + stringify(_av) + ".");
  else
    testFailed(_a + " should be " + _bv + " (of type " + typeof _bv + "). Was " + _av + " (of type " + typeof _av + ").");
}

function shouldNotBe(_a, _b, quiet)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldNotBe() expects string arguments");
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }
  var _bv = eval(_b);

  if (exception)
    testFailed(_a + " should not be " + _bv + ". Threw exception " + exception);
  else if (!isResultCorrect(_av, _bv)) {
    if (!quiet) {
        testPassed(_a + " is not " + _b);
    }
  } else
    testFailed(_a + " should not be " + _bv + ".");
}

function shouldBeTrue(_a) { shouldBe(_a, "true"); }
function shouldBeTrueQuiet(_a) { shouldBe(_a, "true", true); }
function shouldBeFalse(_a) { shouldBe(_a, "false"); }
function shouldBeNaN(_a) { shouldBe(_a, "NaN"); }
function shouldBeNull(_a) { shouldBe(_a, "null"); }

function shouldBeEqualToString(a, b)
{
  var unevaledString = '"' + b.replace(/\\/g, "\\\\").replace(/"/g, "\"").replace(/\n/g, "\\n").replace(/\r/g, "\\r") + '"';
  shouldBe(a, unevaledString);
}

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

function shouldThrow(_a, _e)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  var _ev;
  if (_e)
      _ev =  eval(_e);

  if (exception) {
    if (typeof _e == "undefined" || exception == _ev)
      testPassed(_a + " threw exception " + exception + ".");
    else
      testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Threw exception " + exception + ".");
  } else if (typeof _av == "undefined")
    testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was undefined.");
  else
    testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was " + _av + ".");
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
        function gcRec(n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        }
        for (var i = 0; i < 1000; i++)
            gcRec(10)
    }
}

function isSuccessfullyParsed()
{
    // FIXME: Remove this and only report unexpected syntax errors.
    if (!errorMessage)
        successfullyParsed = true;
    shouldBeTrue("successfullyParsed");
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
    if (self.jsTestIsAsync && self.layoutTestController)
        layoutTestController.notifyDone();
}

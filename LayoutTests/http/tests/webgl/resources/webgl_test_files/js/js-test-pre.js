/*
Copyright (c) 2019 The Khronos Group Inc.
Use of this source code is governed by an MIT-style license that can be
found in the LICENSE.txt file.
*/

(function() {
  var testHarnessInitialized = false;

  var initNonKhronosFramework = function() {
    if (testHarnessInitialized) {
      return;
    }
    testHarnessInitialized = true;

    /* -- plaform specific code -- */

    // WebKit Specific code. Add your code here.
    if (window.testRunner && !window.layoutTestController) {
      window.layoutTestController = window.testRunner;
    }

    if (window.layoutTestController) {
      window.layoutTestController.dumpAsText();
      window.layoutTestController.waitUntilDone();
    }
    if (window.internals) {
      // The WebKit testing system compares console output.
      // Because the output of the WebGL Tests is GPU dependent
      // we turn off console messages.
      window.console.log = function() { };
      window.console.error = function() { };
      window.internals.settings.setWebGLErrorsToConsoleEnabled(false);
    }

    /* -- end platform specific code --*/
  }

  this.initTestingHarness = function() {
    initNonKhronosFramework();
  }
}());

var getUrlOptions = (function() {
  var _urlOptionsParsed = false;
  var _urlOptions = {};
  return function() {
    if (!_urlOptionsParsed) {
      var s = window.location.href;
      var q = s.indexOf("?");
      var e = s.indexOf("#");
      if (e < 0) {
        e = s.length;
      }
      var query = s.substring(q + 1, e);
      var pairs = query.split("&");
      for (var ii = 0; ii < pairs.length; ++ii) {
        var keyValue = pairs[ii].split("=");
        var key = keyValue[0];
        var value = decodeURIComponent(keyValue[1]);
        _urlOptions[key] = value;
      }
      _urlOptionsParsed = true;
    }

    return _urlOptions;
  }
})();

if (typeof quietMode == 'undefined') {
  var quietMode = (function() {
    var _quietModeChecked = false;
    var _isQuiet = false;
    return function() {
      if (!_quietModeChecked) {
        _isQuiet = (getUrlOptions().quiet == 1);
        _quietModeChecked = true;
      }
      return _isQuiet;
    }
  })();
}

function nonKhronosFrameworkNotifyDone() {
  // WebKit Specific code. Add your code here.
  if (window.layoutTestController) {
    window.layoutTestController.notifyDone();
  }
}

const RESULTS = {
  pass: 0,
  fail: 0,
};

// We cache these values since they will potentially be accessed many (100k+)
// times and accessing window can be significantly slower than a local variable.
const locationPathname = window.location.pathname;
const webglTestHarness = window.parent.webglTestHarness;

function reportTestResultsToHarness(success, msg) {
  if (success) {
    RESULTS.pass += 1;
  } else {
    RESULTS.fail += 1;
  }
  if (webglTestHarness) {
    webglTestHarness.reportResults(locationPathname, success, msg);
  }
}

function reportSkippedTestResultsToHarness(success, msg) {
  if (webglTestHarness) {
    webglTestHarness.reportResults(locationPathname, success, msg, true);
  }
}

function notifyFinishedToHarness() {
  if (window._didNotifyFinishedToHarness) {
    testFailed("Duplicate notifyFinishedToHarness()");
  }
  window._didNotifyFinishedToHarness = true;

  if (webglTestHarness) {
    webglTestHarness.notifyFinished(locationPathname);
  }
  if (window.nonKhronosFrameworkNotifyDone) {
    window.nonKhronosFrameworkNotifyDone();
  }
}

// Start buffered, so that our thousands of passing subtests can be buffered.
// We flush the buffered logs on testFailed and/or finishTest.
var _bufferedConsoleLogs = [];

function _bufferedLogToConsole(msg)
{
  if (_bufferedConsoleLogs) {
    _bufferedConsoleLogs.push('[buffered] ' + msg);
  } else if (window.console) {
    window.console.log(msg);
  }
}

// Public entry point exposed to many other files.
function bufferedLogToConsole(msg)
{
  _bufferedLogToConsole(msg);
}

// Called implicitly by testFailed().
function _flushBufferedLogsToConsole()
{
  if (_bufferedConsoleLogs) {
    if (window.console) {
      for (var ii = 0; ii < _bufferedConsoleLogs.length; ++ii) {
        window.console.log(_bufferedConsoleLogs[ii]);
      }
    }
    _bufferedConsoleLogs = null;
  }
}

var _jsTestPreVerboseLogging = false;

function enableJSTestPreVerboseLogging()
{
    _jsTestPreVerboseLogging = true;
}

function description(msg)
{
    initTestingHarness();
    if (msg === undefined) {
      msg = document.title;
    }
    // For MSIE 6 compatibility
    var span = document.createElement("span");
    span.innerHTML = '<p>' + msg + '</p><p>On success, you will see a series of "<span class="pass">PASS</span>" messages, followed by "<span class="pass">TEST COMPLETE</span>".</p>';
    var description = document.getElementById("description");
    if (description.firstChild)
        description.replaceChild(span, description.firstChild);
    else
        description.appendChild(span);
    if (_jsTestPreVerboseLogging) {
        _bufferedLogToConsole(msg);
    }
}

function _addSpan(contents)
{
    var span = document.createElement("span");
    document.getElementById("console").appendChild(span); // insert it first so XHTML knows the namespace
    span.innerHTML = contents + '<br />';
}

function debug(msg)
{
    if (!quietMode())
      _addSpan(msg);
    if (_jsTestPreVerboseLogging) {
        _bufferedLogToConsole(msg);
    }
}

function escapeHTML(text)
{
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;");
}
/**
 * Defines the exception type for a test failure.
 * @constructor
 * @param {string} message The error message.
 */
var TestFailedException = function (message) {
   this.message = message;
   this.name = "TestFailedException";
};

/**
 * @param  {string=} msg
 */
function testPassed(msg) {
    msg = msg || 'Passed';
    if (_currentTestName)
      msg = _currentTestName + ': ' + msg;

    reportTestResultsToHarness(true, msg);

    if (!quietMode())
      _addSpan('<span><span class="pass">PASS</span> ' + escapeHTML(msg) + '</span>');
    if (_jsTestPreVerboseLogging) {
        _bufferedLogToConsole('PASS ' + msg);
    }
}

/**
 * @param  {string=} msg
 */
function testFailed(msg) {
    msg = msg || 'Failed';
    if (_currentTestName)
      msg = _currentTestName + ': ' + msg;

    reportTestResultsToHarness(false, msg);
    _addSpan('<span><span class="fail">FAIL</span> ' + escapeHTML(msg) + '</span>');
    _bufferedLogToConsole('FAIL ' + msg);
    _flushBufferedLogsToConsole();
}

var _currentTestName;

/**
 * Sets the current test name for usage within testPassedOptions/testFailedOptions.
 * @param {string=} name The name to set as the current test name.
 */
function setCurrentTestName(name)
{
    _currentTestName = name;
}

/**
 * Gets the current test name in use within testPassedOptions/testFailedOptions.
 * @return {string} The name of the current test.
 */
function getCurrentTestName()
{
    return _currentTestName;
}

/**
 * Variation of the testPassed function, with the option to not show (and thus not count) the test's pass result.
 * @param {string} msg The message to be shown in the pass result.
 * @param {boolean} addSpan Indicates whether the message will be visible (thus counted in the results) or not.
 */
function testPassedOptions(msg, addSpan)
{
    reportTestResultsToHarness(true, _currentTestName + ": " + msg);
    if (addSpan && !quietMode())
    {
        _addSpan('<span><span class="pass">PASS</span> ' + escapeHTML(_currentTestName) + ": " + escapeHTML(msg) + '</span>');
    }
    if (_jsTestPreVerboseLogging) {
        _bufferedLogToConsole('PASS ' + msg);
    }
}

/**
 * Report skipped tests.
 * @param {string} msg The message to be shown in the skip result.
 * @param {boolean} addSpan Indicates whether the message will be visible (thus counted in the results) or not.
 */
function testSkippedOptions(msg, addSpan)
{
    reportSkippedTestResultsToHarness(true, _currentTestName + ": " + msg);
    if (addSpan && !quietMode())
    {
        _addSpan('<span><span class="warn">SKIP</span> ' + escapeHTML(_currentTestName) + ": " + escapeHTML(msg) + '</span>');
    }
    if (_jsTestPreVerboseLogging) {
        _bufferedLogToConsole('SKIP' + msg);
    }
}

/**
 * Variation of the testFailed function, with the option to throw an exception or not.
 * @param {string} msg The message to be shown in the fail result.
 * @param {boolean} exthrow Indicates whether the function will throw a TestFailedException or not.
 */
function testFailedOptions(msg, exthrow)
{
    reportTestResultsToHarness(false, _currentTestName + ": " + msg);
    _addSpan('<span><span class="fail">FAIL</span> ' + escapeHTML(_currentTestName) + ": " + escapeHTML(msg) + '</span>');
    _bufferedLogToConsole('FAIL ' + msg);
    _flushBufferedLogsToConsole();
    if (exthrow) {
        _currentTestName = ""; //Remembering to set the name of current testcase to empty string.
        throw new TestFailedException(msg);
    }
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
    if (Object.prototype.toString.call(_expected) == Object.prototype.toString.call([]))
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

function shouldBeString(evalable, expected) {
    const val = eval(evalable);
    const text = evalable + " should be " + expected + ".";
    if (val == expected) {
        testPassed(text);
    } else {
        testFailed(text + " (was " + val + ")");
    }
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
function shouldBeFalse(_a) { shouldBe(_a, "false"); }
function shouldBeNaN(_a) { shouldBe(_a, "NaN"); }
function shouldBeNull(_a) { shouldBe(_a, "null"); }

function shouldBeEqualToString(a, b)
{
  var unevaledString = '"' + b.replace(/"/g, "\"") + '"';
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

function shouldBeLessThanOrEqual(_a, _b) {
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeLessThanOrEqual expects string arguments");

    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }
    var _bv = eval(_b);

    if (exception)
        testFailed(_a + " should be <= " + _b + ". Threw exception " + exception);
    else if (typeof _av == "undefined" || _av > _bv)
        testFailed(_a + " should be >= " + _b + ". Was " + _av + " (of type " + typeof _av + ").");
    else
        testPassed(_a + " is <= " + _b);
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

function maxArrayDiff(a, b) {
    if (a.length != b.length)
        throw new Error(`a and b have different lengths: ${a.length} vs ${b.length}`);

    let diff = 0;
    for (const i in a) {
        diff = Math.max(diff, Math.abs(a[i] - b[i]));
    }
    return diff;
}

function expectArray(was, expected, maxDiff=0) {
    const diff = maxArrayDiff(expected, was);
    let str = `Expected [${expected.toString()}]`;
    let fn = testPassed;
    if (maxDiff) {
        str += ' +/- ' + maxDiff;
    }
    if (diff > maxDiff) {
        fn = testFailed;
        str += `, was [${was.toString()}]`;
    }
    fn(str);
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

function shouldNotThrow(evalStr, desc) {
  desc = desc || `\`${evalStr}\``;
  try {
    eval(evalStr);
    testPassed(`${desc} should not throw.`);
  } catch (e) {
    testFailed(`${desc} should not throw, but threw exception ${e}.`);
  }
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

    if(_typev === Number){
        if(_av instanceof Number){
            testPassed(_a + " is an instance of Number");
        }
        else if(typeof(_av) === 'number'){
            testPassed(_a + " is an instance of Number");
        }
        else{
            testFailed(_a + " is not an instance of Number");
        }
    }
    else if (_av instanceof _typev) {
        testPassed(_a + " is an instance of " + _type);
    } else {
        testFailed(_a + " is not an instance of " + _type);
    }
}

/**
 * Shows a message in case expression test fails.
 * @param {boolean} exp
 * @param {straing} message
 */
function checkMessage(exp, message) {
    if ( !exp )
        _addSpan('<span><span class="warn">WARNING</span> ' + escapeHTML(_currentTestName) + ": " + escapeHTML(message) + '</span>');
}

function assertMsg(assertion, msg) {
    if (assertion) {
        testPassed(msg);
    } else {
        testFailed(msg);
    }
}

/**
 * Variation of the assertMsg function, with the option to not show (and thus not count) the test's pass result,
 * and throw or not a TestFailedException in case of failure.
 * @param {boolean} assertion If this is true, means success, else failure.
 * @param {?string} msg The message to be shown in the result.
 * @param {boolean} verbose In case of success, determines if the test will show it's result and count in the results.
 * @param {boolean} exthrow In case of failure, determines if the function will throw a TestFailedException.
 */
function assertMsgOptions(assertion, msg, verbose, exthrow) {
    if (assertion) {
        testPassedOptions(msg, verbose);
    } else {
        testFailedOptions(msg, exthrow);
    }
}


function webglHarnessCollectGarbage() {
    if (window.GCController) {
        window.GCController.collect();
        return;
    }

    if (window.opera && window.opera.collect) {
        window.opera.collect();
        return;
    }

    try {
        window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
              .getInterface(Components.interfaces.nsIDOMWindowUtils)
              .garbageCollect();
        return;
    } catch(e) {}

    if (window.gc) {
        window.gc();
        return;
    }

    if (window.CollectGarbage) {
        CollectGarbage();
        return;
    }

    // WebKit's MiniBrowser.
    if (window.$vm) {
        window.$vm.gc();
        return;
    }

    function gcRec(n) {
        if (n < 1)
            return {};
        var temp = {i: "ab" + i + (i / 100000)};
        temp += "foo";
        gcRec(n-1);
    }
    for (var i = 0; i < 1000; i++)
        gcRec(10);
}

function finishTest() {
  _flushBufferedLogsToConsole();
  successfullyParsed = true;
  var epilogue = document.createElement("script");
  var basePath = "";
  var expectedBase = "js-test-pre.js";
  var scripts = document.getElementsByTagName('script');
  for (var script, i = 0; script = scripts[i]; i++) {
    var src = script.src;
    var l = src.length;
    if (src.substr(l - expectedBase.length) == expectedBase) {
      basePath = src.substr(0, l - expectedBase.length);
      break;
    }
  }
  epilogue.src = basePath + "js-test-post.js";
  document.body.appendChild(epilogue);
}

// -

/**
 * `=> { return fn(); }`
 *
 * To be clear up front that we're calling (instead of defining):
 * ```
 * call(() => {
 *    ...
 * });
 * ```
 *
 * As opposed to:
 * ```
 * (() => {
 *    ...
 * })();
 * ```
 *
 * @param {function(): any} fn
 */
function call(fn) {
    return fn();
}

// -

/**
 * A la python:
 * * range(3) => [0,1,2]
 * * range(1,3) => [1,2]
 * @param {number} a
 * @param {number} [b]
 * @returns {number[]} [min(a,b), ... , max(a,b)-1]
 */
function range(a, b) {
  b = b || 0;
  const begin = Math.min(a, b);
  const end = Math.max(a, b);
  return new Array(end-begin).fill().map((_,i) => begin+i);
}
{
  let was;
  console.assert((was = range(0)).toString() == [].toString(), {was});
  console.assert((was = range(1)).toString() == [0].toString(), {was});
  console.assert((was = range(3)).toString() == [0,1,2].toString(), {was});
  console.assert((was = range(1,3)).toString() == [1,2].toString(), {was});
}

// -

/**
 * `=> { throw v; }`
 *
 * Like `throw`, but usable as an expression not just a statement.\
 * E.g. `let found = foo.bar || throwv({foo, msg: 'foo must have .bar!'});`
 * @param {any} v
 * @throws Always throws `v`!
 */
function throwv(v) {
  throw v;
}

// -

/**
 * @typedef {object} ConfigDict
 * @property {any=} key
 */

/**
 * @typedef {ConfigDict[]} ConfigDictList
 */

/**
 * @param {...ConfigDictList} comboDimensions
 * @returns {ConfigDictList}  N-dim Cartesian Product of combinations of the key-value-map objects from each list.
 */
function crossCombine(...comboDimensions) {
  function crossCombine2(listA, listB) {
    const listC = [];
    for (const a of listA) {
      for (const b of listB) {
        const c = Object.assign({}, a, b);
        listC.push(c);
      }
    }
    return listC;
  }

  let res = [{}];
  for (const i in comboDimensions) {
    const next = comboDimensions[i] || throwv({i, comboDimensions});
    res = crossCombine2(res, next);
  }
  return res;
}

// -

/**
 * @typedef {number} U32 range: `[0, 0xffff_ffff]`
 * @typedef {number} I32 range: `[-0x8000_0000, 0x7fff_ffff]`, `0xffff_ffff|0 => -1`
 * @typedef {number} F32
 */

/**
 * "SHift Right as Uint32"
 * @param {U32} val
 * @param {number} n
 * @returns {U32}
 */
function shr_u32(val, n) {
  val >>= n; // In JS this is shr_i32, with sign-extension for negative lhs.

  if (n > 0) {
      const result_mask = (1 << (32-n)) - 1;
      val &= result_mask;
  }

  return val;
}
console.assert((0xffff_ffff | 0) == -1);
console.assert(0xffff_ff00 >> 4 == (0xffff_fff0 | 0));
console.assert(shr_u32(0xffff_ff00, 4) != (0xffff_fff0 | 0));
console.assert(shr_u32(0xffff_ff00, 4) == (0x0fff_fff0 | 0));
console.assert(shr_u32(0xffff_ff00, 4) == 0x0fff_fff0);
console.assert(shr_u32(0xffff_ff00|0, 4) == 0x0fff_fff0);

/**
 * @type {(val: number) => U32}
 */
const bitcast_u32 = call(() => {
  const u32View = new Uint32Array(1);
  return function bitcast_u32(val) {
    u32View[0] = val;
    return u32View[0];
  };
});

/**
 * @type {(u32: U32) => F32}
 */
const bitcast_f32_from_u32 = call(() => {
  const u32 = new Uint32Array(1);
  const f32 = new Float32Array(u32.buffer);
  return function bitcast_f32_from_u32(v) {
      u32[0] = v;
      return f32[0];
  };
});

// -

class PrngXorwow {
  /** @type {U32[]} */
  actual_seed;

  /** @type {U32[]} */
  state = new Uint32Array(6); // 5 u32 shuffler + 1 u32 counter.

  /**
   * @param {U32[] | U32 | undefined} seed
   */
  constructor(seed) {
    if (typeof(seed) == 'string') {
      seed = parseInt(seed);
    }
    if (typeof(seed) == 'object' && seed.length !== undefined) {
      // array-ish
      if (!seed.length) {
        seed = new Uint32Array(state.length);
        crypto.getRandomValues(seed);
      } else {
        seed = new Uint32Array(seed);
      }
    } else {
      // number?
      if (!seed) {
        seed = new Uint32Array(1);
        crypto.getRandomValues(seed);
      } else {
        seed = new Uint32Array([seed]);
      }
    }

    // Elide zeros from seed for compactness:
    while (seed[seed.length-1] == 0) {
        seed = seed.slice(0, seed.length-1);
    }
    this.actual_seed = seed.slice();

    // Seed the state:
    const state = this.state;
    for (const i in state) {
      state[i] = this.actual_seed[i] || 0;
    }
    console.assert(state[0] || state[1] || state[2] || state[3], "The first four words of seeded state must not all be 0:", state)

  }

  /**
   * (n>=2)
   * @returns {U32 | U32[n]}
   */
  seed() {
    const seed = this.actual_seed;
    if (seed.length == 1) return seed[0];
    return seed.slice();
  }

  // -

  /**
   * @returns {U32[6]}
   */
  state() {
    return this.state;
  }

  /**
   * @returns {U32}
   */
  next_u32() {
      /* Algorithm "xorwow" from p. 5 of Marsaglia, "Xorshift RNGs" */
      const state = this.state;
      let t = state[4];

      const s = state[0];
      state[4] = state[3];
      state[3] = state[2];
      state[2] = state[1];
      state[1] = s;

      t ^= shr_u32(t, 2);
      t ^= t << 1;
      t ^= s ^ (s << 4);
      state[0] = t;
      state[5] += 362437;

      let ret = state[0] + state[5];
      ret = bitcast_u32(ret);
      return ret;
  }

  /**
   * @returns {number} range: [0.0, 1.0)
   */
  next_unorm() {
      let ret = this.next_u32();
      const U32_MAX = 0xffff_ffff;
      ret /= (U32_MAX + 1);
      return ret; // [0,1)
  }

  /**
   * A la crypto.getRandomValues()
   * @param {ArrayBufferView} dest
   * @returns {ArrayBufferView}
   */
  getRandomValues(dest) {
    const u8s = abv_cast(Uint8Array, dest);
    const len_in_u32 = Math.floor(u8s.length / 4);
    const u32s = abv_cast(Uint32Array, u8s.subarray(0, 4*len_in_u32));
    for (const i in u32s) {
      u32s[i] = this.next_u32();
    }
    for (const i of range(u32s.byteLength, u8s.byteLength)) {
      u8s[i] = this.next_u32(); // Truncates u32 to u8.
    }

    return dest;
  }
}

// -

/**
 * @template {ArrayBufferView} T
 * @param {T.constructor} ctor
 * @param {ArrayBufferView | ArrayBuffer} abv
 * @returns {T}
 */
function abv_cast(ctor, abv) {
  if (abv instanceof ArrayBuffer) return new ctor(abv);
  const ctor_bytes_per_element = ctor.BYTES_PER_ELEMENT || 1; // DataView doesn't have BYTES_PER_ELEMENT.
  return new ctor(abv.buffer, abv.byteOffset, abv.byteLength / ctor_bytes_per_element);
}

// -

/**
 * @returns {PrngXorwow}
 */
function getDrng(defaultSeed=1) {
  if (globalThis._DRNG) return globalThis._DRNG;

  const seedKeyName = `seed`;

  const url = new URL(window.location);
  let requestedSeed = url.searchParams.get(seedKeyName);
  if (requestedSeed === null) {
    requestedSeed = defaultSeed;
  }

  const drng = globalThis._DRNG = new PrngXorwow(requestedSeed);
  const seed = drng.seed();

  // Run it a few times to avoid seed=1 giving similar values at first.
  for (const _ of range(100)) {
    drng.next_u32();
  }

  url.searchParams.set(seedKeyName, seed);
  let linkText = `Link to this run's seed: ${url}`;
  if (seed != requestedSeed) {
    linkText += ' (autogenerated)';
  }

  globalThis.debug && debug(linkText);
  console.log(linkText);

  return drng;
}

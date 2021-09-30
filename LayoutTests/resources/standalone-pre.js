var wasPostTestScriptParsed = false;
var errorMessage;
var self = this;

self.testRunner = {
    neverInlineFunction: neverInlineFunction,
    numberOfDFGCompiles: numberOfDFGCompiles,
    failNextNewCodeBlock: failNextNewCodeBlock
};

var silentTestPass, didPassSomeTestsSilently, didFailSomeTests, successfullyParsed;
silentTestPass = false;
didPassSomeTestsSilently = false;
didFailSomeTests = false;

print = legacyPrint;

function description(msg)
{
    print(msg);
    print("\nOn success, you will see a series of \"PASS\" messages, followed by \"TEST COMPLETE\".\n");
    print();
}

function debug(msg)
{
    print(msg);
}

function escapeString(text)
{
    return text.replace(/\0/g, "");
}

function testPassed(msg)
{
    if (silentTestPass)
        didPassSomeTestsSilently = true;
    else
        print("PASS", escapeString(msg));
}

function testFailed(msg)
{
    didFailSomeTests = true;
    print("FAIL", escapeString(msg));
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

function shouldBeTrue(_a) { shouldBe(_a, "true"); }
function shouldBeFalse(_a) { shouldBe(_a, "false"); }
function shouldBeNaN(_a) { shouldBe(_a, "NaN"); }
function shouldBeNull(_a) { shouldBe(_a, "null"); }

function shouldBeEqualToString(a, b)
{
  if (typeof a !== "string" || typeof b !== "string")
    debug("WARN: shouldBeEqualToString() expects string arguments");
  var unevaledString = JSON.stringify(b);
  shouldBe(a, unevaledString);
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
    debug("\nTEST COMPLETE\n");
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

// It's possible for an async test to call finishJSTest() before js-test-post.js
// has been parsed.
function finishJSTest()
{
    wasFinishJSTestCalled = true;
    if (!wasPostTestScriptParsed)
        return;
    isSuccessfullyParsed();
}

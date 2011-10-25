description("Tests the acceptable types for arguments to navigator.getUserMedia methods.");

function shouldNotThrow(expression)
{
  try {
    eval(expression);
    testPassed(expression + " did not throw exception.");
  } catch(e) {
    testFailed(expression + " should not throw exception. Threw exception " + e);
  }
}

function test(expression, expressionShouldThrow, expectedException) {
    if (expressionShouldThrow) {
        if (expectedException)
            shouldThrow(expression, '(function() { return "' + expectedException + '"; })();');
        else
            shouldThrow(expression, '(function() { return "Error: TYPE_MISMATCH_ERR: DOM Exception 17"; })();');
    } else {
        shouldNotThrow(expression);
    }
}

var toStringError = new Error('toString threw exception');
var notSupportedError = new Error('NOT_SUPPORTED_ERR: DOM Exception 9');
var emptyFunction = function() {};

function ObjectThrowingException() {};
ObjectThrowingException.prototype.toString = function() {
    throw toStringError;
}
var objectThrowingException = new ObjectThrowingException();

// No arguments
test('navigator.webkitGetUserMedia()', true);

// 1 Argument (getUserMedia requires at least 2 arguments).
test('navigator.webkitGetUserMedia(undefined)', true);
test('navigator.webkitGetUserMedia(null)', true);
test('navigator.webkitGetUserMedia({})', true);
test('navigator.webkitGetUserMedia(objectThrowingException)', true, toStringError);
test('navigator.webkitGetUserMedia("video")', true);
test('navigator.webkitGetUserMedia(true)', true);
test('navigator.webkitGetUserMedia(42)', true);
test('navigator.webkitGetUserMedia(Infinity)', true);
test('navigator.webkitGetUserMedia(-Infinity)', true);
test('navigator.webkitGetUserMedia(emptyFunction)', true);

// 2 Arguments.
test('navigator.webkitGetUserMedia("video", emptyFunction)', false);
test('navigator.webkitGetUserMedia(undefined, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia(null, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia({}, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia(objectThrowingException, emptyFunction)', true, toStringError);
test('navigator.webkitGetUserMedia(true, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia(42, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia(Infinity, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia(-Infinity, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia(emptyFunction, emptyFunction)', true, notSupportedError);

test('navigator.webkitGetUserMedia("video", "video")', true);
test('navigator.webkitGetUserMedia("video", undefined)', true);
test('navigator.webkitGetUserMedia("video", null)', true);
test('navigator.webkitGetUserMedia("video", {})', true);
test('navigator.webkitGetUserMedia("video", objectThrowingException)', true);
test('navigator.webkitGetUserMedia("video", true)', true);
test('navigator.webkitGetUserMedia("video", 42)', true);
test('navigator.webkitGetUserMedia("video", Infinity)', true);
test('navigator.webkitGetUserMedia("video", -Infinity)', true);

// 3 Arguments.
test('navigator.webkitGetUserMedia("video", emptyFunction, emptyFunction)', false);
test('navigator.webkitGetUserMedia("video", emptyFunction, undefined)', false);
test('navigator.webkitGetUserMedia("audio, video", emptyFunction, undefined)', false);
test('navigator.webkitGetUserMedia("audio, somethingelse,,video", emptyFunction, undefined)', false);
test('navigator.webkitGetUserMedia("audio, video user", emptyFunction, undefined)', false);
test('navigator.webkitGetUserMedia("audio, video environment", emptyFunction, undefined)', false);
test('navigator.webkitGetUserMedia("video", emptyFunction, "video")', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, null)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, {})', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, objectThrowingException)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, true)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, 42)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, Infinity)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, -Infinity)', true);

window.jsTestIsAsync = false;

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

var emptyFunction = function() {};

function ObjectThrowingException() {};
ObjectThrowingException.prototype.valueOf = function() {
    throw new Error('valueOf threw exception');
}
ObjectThrowingException.prototype.toString = function() {
    throw new Error('toString threw exception');
}
var objectThrowingException = new ObjectThrowingException();

// No arguments
test('navigator.webkitGetUserMedia()', true);

// 1 Argument (getUserMedia requires at least 2 arguments).
test('navigator.webkitGetUserMedia(undefined)', true);
test('navigator.webkitGetUserMedia(null)', true);
test('navigator.webkitGetUserMedia({})', true);
test('navigator.webkitGetUserMedia(objectThrowingException)', true);
test('navigator.webkitGetUserMedia("video")', true);
test('navigator.webkitGetUserMedia(true)', true);
test('navigator.webkitGetUserMedia(42)', true);
test('navigator.webkitGetUserMedia(Infinity)', true);
test('navigator.webkitGetUserMedia(-Infinity)', true);
test('navigator.webkitGetUserMedia(emptyFunction)', true);

// 2 Arguments.
test('navigator.webkitGetUserMedia("video", emptyFunction)', false);
test('navigator.webkitGetUserMedia(undefined, emptyFunction)', false);
test('navigator.webkitGetUserMedia(null, emptyFunction)', false);
test('navigator.webkitGetUserMedia({}, emptyFunction)', false);
test('navigator.webkitGetUserMedia(objectThrowingException, emptyFunction)', true, new Error('toString threw exception'));
test('navigator.webkitGetUserMedia(true, emptyFunction)', false);
test('navigator.webkitGetUserMedia(42, emptyFunction)', false);
test('navigator.webkitGetUserMedia(Infinity, emptyFunction)', false);
test('navigator.webkitGetUserMedia(-Infinity, emptyFunction)', false);
test('navigator.webkitGetUserMedia(emptyFunction, emptyFunction)', false);

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
test('navigator.webkitGetUserMedia("video", emptyFunction, "video")', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, null)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, {})', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, objectThrowingException)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, true)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, 42)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, Infinity)', true);
test('navigator.webkitGetUserMedia("video", emptyFunction, -Infinity)', true);

window.jsTestIsAsync = false;
window.successfullyParsed = true;

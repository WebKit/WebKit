description("Tests the acceptable types for arguments to navigator.getUserMedia methods.");

function test(expression, expressionShouldThrow, expectedException) {
    if (expressionShouldThrow) {
        if (expectedException)
            shouldThrow(expression, '"' + expectedException + '"');
        else
            shouldThrow(expression, '"TypeError: Not enough arguments"');
    } else {
        shouldNotThrow(expression);
    }
}

var errorCallbackError = new TypeError("Argument 3 ('errorCallback') to Navigator.getUserMedia must be a function")
var invalidDictionaryError = new TypeError('First argument of getUserMedia must be a valid Dictionary')
var notSupportedError = new Error('NotSupportedError: DOM Exception 9');
var successCallbackError = new TypeError("Argument 2 ('successCallback') to Navigator.getUserMedia must be a function")
var typeError = new TypeError('Type error');
var typeNotAnObjectError = new TypeError('Not an object.');

var emptyFunction = function() {};

// No arguments
test('navigator.getUserMedia()', true);

// 1 Argument (Navigtor.getUserMedia requires at least 3 arguments).
test('navigator.getUserMedia(undefined)', true);
test('navigator.getUserMedia(null)', true);
test('navigator.getUserMedia({ })', true);
test('navigator.getUserMedia({video: true})', true);
test('navigator.getUserMedia(true)', true);
test('navigator.getUserMedia(42)', true);
test('navigator.getUserMedia(Infinity)', true);
test('navigator.getUserMedia(-Infinity)', true);
test('navigator.getUserMedia(emptyFunction)', true);

// 2 Arguments.
test('navigator.getUserMedia({video: true}, emptyFunction)', true);
test('navigator.getUserMedia(undefined, emptyFunction)', true);
test('navigator.getUserMedia(null, emptyFunction)', true);
test('navigator.getUserMedia({ }, emptyFunction)', true);
test('navigator.getUserMedia(true, emptyFunction)', true);
test('navigator.getUserMedia(42, emptyFunction)', true);
test('navigator.getUserMedia(Infinity, emptyFunction)', true);
test('navigator.getUserMedia(-Infinity, emptyFunction)', true);
test('navigator.getUserMedia(emptyFunction, emptyFunction)', true);
test('navigator.getUserMedia({video: true}, "foobar")', true);
test('navigator.getUserMedia({video: true}, undefined)', true);
test('navigator.getUserMedia({video: true}, null)', true);
test('navigator.getUserMedia({video: true}, {})', true);
test('navigator.getUserMedia({video: true}, true)', true);
test('navigator.getUserMedia({video: true}, 42)', true);
test('navigator.getUserMedia({video: true}, Infinity)', true);
test('navigator.getUserMedia({video: true}, -Infinity)', true);

// 3 Arguments.
test('navigator.getUserMedia({ }, emptyFunction, emptyFunction)', false);
test('navigator.getUserMedia({video: true}, emptyFunction, emptyFunction)', false);
test('navigator.getUserMedia({video: true}, emptyFunction, undefined)', true, errorCallbackError);
test('navigator.getUserMedia({audio:true, video:true}, emptyFunction, undefined)', true, errorCallbackError);
test('navigator.getUserMedia({audio:true}, emptyFunction, undefined)', true, errorCallbackError);
test('navigator.getUserMedia({video: true}, emptyFunction, "video")', true, errorCallbackError);
test('navigator.getUserMedia({video: true}, emptyFunction, null)', true, errorCallbackError );
test('navigator.getUserMedia({video: true}, emptyFunction, {})', true, errorCallbackError);
test('navigator.getUserMedia({video: true}, emptyFunction, true)', true, errorCallbackError);
test('navigator.getUserMedia({video: true}, emptyFunction, 42)', true, errorCallbackError);
test('navigator.getUserMedia({video: true}, emptyFunction, Infinity)', true, errorCallbackError);
test('navigator.getUserMedia({video: true}, emptyFunction, -Infinity)', true, errorCallbackError);

window.jsTestIsAsync = false;

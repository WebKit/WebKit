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

var errorCallbackError = new TypeError("Argument 3 ('errorCallback') to Navigator.webkitGetUserMedia must be a function")
var invalidDictionaryError = new TypeError('First argument of webkitGetUserMedia must be a valid Dictionary')
var notSupportedError = new Error('NotSupportedError: DOM Exception 9');
var successCallbackError = new TypeError("Argument 2 ('successCallback') to Navigator.webkitGetUserMedia must be a function")
var typeError = new TypeError('Type error');
var typeNotAnObjectError = new TypeError('Not an object.');

var emptyFunction = function() {};

// No arguments
test('navigator.webkitGetUserMedia()', true);

// 1 Argument (Navigtor.webkitGetUserMedia requires at least 3 arguments).
test('navigator.webkitGetUserMedia(undefined)', true);
test('navigator.webkitGetUserMedia(null)', true);
test('navigator.webkitGetUserMedia({ })', true);
test('navigator.webkitGetUserMedia({video: true})', true);
test('navigator.webkitGetUserMedia(true)', true);
test('navigator.webkitGetUserMedia(42)', true);
test('navigator.webkitGetUserMedia(Infinity)', true);
test('navigator.webkitGetUserMedia(-Infinity)', true);
test('navigator.webkitGetUserMedia(emptyFunction)', true);

// 2 Arguments.
test('navigator.webkitGetUserMedia({video: true}, emptyFunction)', true);
test('navigator.webkitGetUserMedia(undefined, emptyFunction)', true);
test('navigator.webkitGetUserMedia(null, emptyFunction)', true);
test('navigator.webkitGetUserMedia({ }, emptyFunction)', true);
test('navigator.webkitGetUserMedia(true, emptyFunction)', true);
test('navigator.webkitGetUserMedia(42, emptyFunction)', true);
test('navigator.webkitGetUserMedia(Infinity, emptyFunction)', true);
test('navigator.webkitGetUserMedia(-Infinity, emptyFunction)', true);
test('navigator.webkitGetUserMedia(emptyFunction, emptyFunction)', true);
test('navigator.webkitGetUserMedia({video: true}, "foobar")', true);
test('navigator.webkitGetUserMedia({video: true}, undefined)', true);
test('navigator.webkitGetUserMedia({video: true}, null)', true);
test('navigator.webkitGetUserMedia({video: true}, {})', true);
test('navigator.webkitGetUserMedia({video: true}, true)', true);
test('navigator.webkitGetUserMedia({video: true}, 42)', true);
test('navigator.webkitGetUserMedia({video: true}, Infinity)', true);
test('navigator.webkitGetUserMedia({video: true}, -Infinity)', true);

// 3 Arguments.
test('navigator.webkitGetUserMedia({ }, emptyFunction, emptyFunction)', false);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, emptyFunction)', false);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, undefined)', true, errorCallbackError);
test('navigator.webkitGetUserMedia({audio:true, video:true}, emptyFunction, undefined)', true, errorCallbackError);
test('navigator.webkitGetUserMedia({audio:true}, emptyFunction, undefined)', true, errorCallbackError);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, "video")', true, errorCallbackError);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, null)', true, errorCallbackError );
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, {})', true, errorCallbackError);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, true)', true, errorCallbackError);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, 42)', true, errorCallbackError);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, Infinity)', true, errorCallbackError);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, -Infinity)', true, errorCallbackError);

window.jsTestIsAsync = false;

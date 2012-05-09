description("Tests the acceptable types for arguments to method for DeprecatedPeerConnection defination.");

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

shouldBeTrue("typeof webkitDeprecatedPeerConnection === 'function'");

// 0 Argument
test('new webkitDeprecatedPeerConnection()', true,'TypeError: Not enough arguments');

// 1 Argument (new webkitDeprecatedPeerConnection requires at least 2 arguments).
test('new webkitDeprecatedPeerConnection("STUN 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("STUN relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("STUN example.net")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("STUNS 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("STUNS relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("STUNS example.net")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("TURN 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("TURN relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("TURN example.net")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("TURNS 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("TURNS relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("TURNS example.net")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("TURN NONE")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("TURNS NONE")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("STUN NONE")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("STUNS NONE")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("undefined")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection("null")', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection({})', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection(42)', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection(Infinity)', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection(-Infinity)', true, 'TypeError: Not enough arguments');
test('new webkitDeprecatedPeerConnection(emptyFunction)', true, 'TypeError: Not enough arguments');

//2 Argument
test('new webkitDeprecatedPeerConnection("STUN 203.0.113.2:2478",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("STUN relay.example.net:3478",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("STUN example.net",emptyFunction)',false);
test('new webkitDeprecatedPeerConnection("STUNS 203.0.113.2:2478",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("STUNS relay.example.net:3478",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("STUNS example.net",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURN 203.0.113.2:2478",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURN relay.example.net:3478",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURN example.net",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURNS 203.0.113.2:2478",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURNS relay.example.net:3478",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURNS example.net",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURN NONE",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURNS NONE",emptyFunction)',false);
test('new webkitDeprecatedPeerConnection("STUN NONE",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("STUNS NONE",emptyFunction)', false);
test('new webkitDeprecatedPeerConnection("TURN NONE",undefined)',  true);
test('new webkitDeprecatedPeerConnection("TURNS NONE",{})', true);
test('new webkitDeprecatedPeerConnection("STUN NONE",42)',  true);
test('new webkitDeprecatedPeerConnection("STUNS NONE",Infinity)', true);
test('new webkitDeprecatedPeerConnection("STUNS NONE",-Infinity)', true);



window.jsTestIsAsync = false;

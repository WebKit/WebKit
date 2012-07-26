description("Tests the acceptable types for arguments to method for PeerConnection00 defination.");

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

shouldBeTrue("typeof webkitPeerConnection00 === 'function'");

// 0 Argument
test('new webkitPeerConnection00()', true,'TypeError: Not enough arguments');

// 1 Argument (new webkitPeerConnection00 requires at least 2 arguments).
test('new webkitPeerConnection00("STUN 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("STUN relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("STUN example.net")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("STUNS 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("STUNS relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("STUNS example.net")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("TURN 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("TURN relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("TURN example.net")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("TURNS 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("TURNS relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("TURNS example.net")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("TURN NONE")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("TURNS NONE")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("STUN NONE")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("STUNS NONE")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("undefined")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00("null")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00({})', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00(42)', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00(Infinity)', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00(-Infinity)', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection00(emptyFunction)', true, 'TypeError: Not enough arguments');

//2 Argument
test('new webkitPeerConnection00("STUN 203.0.113.2:2478",emptyFunction)', false);
test('new webkitPeerConnection00("STUN relay.example.net:3478",emptyFunction)', false);
test('new webkitPeerConnection00("STUN example.net",emptyFunction)',false);
test('new webkitPeerConnection00("STUNS 203.0.113.2:2478",emptyFunction)', false);
test('new webkitPeerConnection00("STUNS relay.example.net:3478",emptyFunction)', false);
test('new webkitPeerConnection00("STUNS example.net",emptyFunction)', false);
test('new webkitPeerConnection00("TURN 203.0.113.2:2478",emptyFunction)', false);
test('new webkitPeerConnection00("TURN relay.example.net:3478",emptyFunction)', false);
test('new webkitPeerConnection00("TURN example.net",emptyFunction)', false);
test('new webkitPeerConnection00("TURNS 203.0.113.2:2478",emptyFunction)', false);
test('new webkitPeerConnection00("TURNS relay.example.net:3478",emptyFunction)', false);
test('new webkitPeerConnection00("TURNS example.net",emptyFunction)', false);
test('new webkitPeerConnection00("TURN NONE",emptyFunction)', false);
test('new webkitPeerConnection00("TURNS NONE",emptyFunction)',false);
test('new webkitPeerConnection00("STUN NONE",emptyFunction)', false);
test('new webkitPeerConnection00("STUNS NONE",emptyFunction)', false);
test('new webkitPeerConnection00("TURN NONE",undefined)',  true);
test('new webkitPeerConnection00("TURNS NONE",{})', true);
test('new webkitPeerConnection00("STUN NONE",42)',  true);
test('new webkitPeerConnection00("STUNS NONE",Infinity)', true);
test('new webkitPeerConnection00("STUNS NONE",-Infinity)', true);

window.jsTestIsAsync = false;

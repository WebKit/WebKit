description("Tests the acceptable types for arguments to method for PeerToPeerConnection defination.");

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

shouldBeTrue("typeof webkitPeerConnection== 'function'");

// 0 Argument
test('new webkitPeerConnection()', true,'TypeError: Not enough arguments');

// 1 Argument (new webkitPeerConnection requires at least 2 arguments).
test('new webkitPeerConnection("STUN 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("STUN relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("STUN example.net")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("STUNS 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("STUNS relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("STUNS example.net")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("TURN 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("TURN relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("TURN example.net")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("TURNS 203.0.113.2:2478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("TURNS relay.example.net:3478")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("TURNS example.net")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("TURN NONE")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("TURNS NONE")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("STUN NONE")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("STUNS NONE")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("undefined")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection("null")', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection({})', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection(42)', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection(Infinity)', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection(-Infinity)', true, 'TypeError: Not enough arguments');
test('new webkitPeerConnection(emptyFunction)', true, 'TypeError: Not enough arguments');

//2 Argument
test('new webkitPeerConnection("STUN 203.0.113.2:2478",emptyFunction)', false);
test('new webkitPeerConnection("STUN relay.example.net:3478",emptyFunction)', false);
test('new webkitPeerConnection("STUN example.net",emptyFunction)',false);
test('new webkitPeerConnection("STUNS 203.0.113.2:2478",emptyFunction)', false);
test('new webkitPeerConnection("STUNS relay.example.net:3478",emptyFunction)', false);
test('new webkitPeerConnection("STUNS example.net",emptyFunction)', false);
test('new webkitPeerConnection("TURN 203.0.113.2:2478",emptyFunction)', false);
test('new webkitPeerConnection("TURN relay.example.net:3478",emptyFunction)', false);
test('new webkitPeerConnection("TURN example.net",emptyFunction)', false);
test('new webkitPeerConnection("TURNS 203.0.113.2:2478",emptyFunction)', false);
test('new webkitPeerConnection("TURNS relay.example.net:3478",emptyFunction)', false);
test('new webkitPeerConnection("TURNS example.net",emptyFunction)', false);
test('new webkitPeerConnection("TURN NONE",emptyFunction)', false);
test('new webkitPeerConnection("TURNS NONE",emptyFunction)',false);
test('new webkitPeerConnection("STUN NONE",emptyFunction)', false);
test('new webkitPeerConnection("STUNS NONE",emptyFunction)', false);
test('new webkitPeerConnection("TURN NONE",undefined)',  true);
test('new webkitPeerConnection("TURNS NONE",{})', false);
test('new webkitPeerConnection("STUN NONE",42)',  true);
test('new webkitPeerConnection("STUNS NONE",Infinity)', true);
test('new webkitPeerConnection("STUNS NONE",-Infinity)', true);



window.jsTestIsAsync = false;

description("Tests DeprecatedPeerConnection related Attributes according to http://www.w3.org/TR/webrtc/");
var stream;
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
            shouldThrow(expression, '(function() { return "TypeError: Type error"; })();');
    } else {
        shouldNotThrow(expression);
    }
}
function shouldBeA(_a, _b, quiet)
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

function shouldBeTrueA(_a) { shouldBeA(_a, "true"); }

var toStringError = new Error('toString threw exception');
var notSupportedError = new Error('NOT_SUPPORTED_ERR: DOM Exception 9');
var emptyFunction = function() {};


function ObjectThrowingException() {};
ObjectThrowingException.prototype.toString = function() {
    throw toStringError;
}
var objectThrowingException = new ObjectThrowingException();


navigator.webkitGetUserMedia("video", gotStream, gotStreamFailed);
function gotStream(s) {
stream = s;
}

function gotStreamFailed(error) {
  alert("Failed to get access to webcam. Error code was " + error.code);
}


var pc=new webkitDeprecatedPeerConnection("STUN NONE", emptyFunction);
//method
shouldBeTrueA("typeof pc.addStream == 'function'");
shouldBeTrueA("typeof pc.removeStream == 'function'");
shouldBeTrueA("typeof pc.close == 'function'");

//peerconnection readyState 
shouldBeTrueA("pc.NEW ==0");
shouldBeTrueA("pc.NEGOTIATING == 1");
shouldBeTrueA("pc.ACTIVE == 2");
shouldBeTrueA("pc.CLOSED == 3");

//iceState and SDP state
shouldBeTrueA("pc.ICE_GATHERING == 0x100 ");
shouldBeTrueA("pc.ICE_WAITING == 0x200");
shouldBeTrueA("pc.ICE_CHECKING == 0x300");
shouldBeTrueA("pc.ICE_CONNECTED == 0x400");
shouldBeTrueA("pc.ICE_COMPLETED == 0x500");
shouldBeTrueA("pc.ICE_FAILED == 0x600");
shouldBeTrueA("pc.ICE_CLOSED == 0x700");
shouldBeTrueA("pc.SDP_IDLE == 0x1000");
shouldBeTrueA("pc.SDP_WAITING == 0x2000");
shouldBeTrueA("pc.SDP_GLARE ==0x3000");

//MediaStream[] attribute
shouldBeTrueA("typeof pc.localStreams == 'object'");
shouldBeTrueA("typeof pc.remoteStreams == 'object'");

//callback function definition 
shouldBeTrueA("typeof pc.onaddstream == 'object'");
shouldBeTrueA("typeof pc.onremovestream == 'object'");
shouldBeTrueA("typeof pc.onconnecting == 'object'");
shouldBeTrueA("typeof pc.onopen == 'object'");

window.jsTestIsAsync = false;

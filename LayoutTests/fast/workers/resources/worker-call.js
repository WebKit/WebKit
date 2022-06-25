postMessage("SUCCESS: postMessage() called directly");
postMessage.call(null, "SUCCESS: postMessage() invoked via postMessage.call()");
var saved = postMessage;
saved("SUCCESS: postMessage() called via intermediate variable");
postMessage("DONE");

postMessage("SUCCESS: postMessage() called directly");
webkitPostMessage("SUCCESS: webkitPostMessage() called directly");
postMessage.call(null, "SUCCESS: postMessage() invoked via postMessage.call()");
webkitPostMessage.call(null, "SUCCESS: webkitPostMessage() invoked via webkitPostMessage.call()");
var saved = postMessage;
saved("SUCCESS: postMessage() called via intermediate variable");
var saved1 = webkitPostMessage;
saved1("SUCCESS: webkitPostMessage() called via intermediate variable");
postMessage("DONE");

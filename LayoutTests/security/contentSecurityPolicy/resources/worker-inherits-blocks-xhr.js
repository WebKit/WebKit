var exception;
try {
    var xhr = new XMLHttpRequest;
    var isAsynchronous = false;
    xhr.open("GET", "non-existent-file", isAsynchronous);
    xhr.send();
} catch (e) {
    exception = e;
}

var expectedExceptionCode = 19; // DOMException.NETWORK_ERR
if (!exception)
    self.postMessage("FAIL should throw " + expectedExceptionCode + ". But did not throw an exception.");
else {
    if (exception.code === expectedExceptionCode)
        self.postMessage("PASS threw exception " + exception + ".");
    else
        self.postMessage("FAIL should throw " + expectedExceptionCode + ". Threw exception " + exception + ".");
}

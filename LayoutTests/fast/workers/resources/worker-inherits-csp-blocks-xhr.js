var exception;
try {
    var xhr = new XMLHttpRequest;
    var isAsynchronous = false;
    xhr.open("GET", "non-existent-file", isAsynchronous);
    xhr.send();
} catch (e) {
    exception = e;
}
// FIXME: We should be throwing a DOMException.NETWORK_ERR. See <https://bugs.webkit.org/show_bug.cgi?id=153598>.
var expectedExceptionCode = 18; // DOMException.SECURITY_ERR
if (!exception)
    self.postMessage("FAIL should throw " + expectedExceptionCode + ". But did not throw an exception.");
else {
    if (exception.code === expectedExceptionCode)
        self.postMessage("PASS threw exception " + exception + ".");
    else
        self.postMessage("FAIL should throw " + expectedExceptionCode + ". Threw exception " + exception + ".");
}

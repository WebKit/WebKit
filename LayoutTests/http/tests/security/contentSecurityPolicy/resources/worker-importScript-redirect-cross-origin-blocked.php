<?php
    require "determine-content-security-policy-header.php";
?>
var exception;
try {
    importScripts("http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.php?url=http://localhost:8000/security/contentSecurityPolicy/resources/script-set-value.js");
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

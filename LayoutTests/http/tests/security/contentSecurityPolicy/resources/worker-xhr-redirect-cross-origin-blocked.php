<?php
    require "determine-content-security-policy-header.php";
?>
var expectedExceptionCode = 19; // DOMException.NETWORK_ERR
var isAsynchronous = false;
var xhr = new XMLHttpRequest;
try {
    xhr.open("GET", "http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.php?url=http://localhost:8000/xmlhttprequest/resources/access-control-basic-allow.cgi", isAsynchronous);
    xhr.send();
    self.postMessage("FAIL should throw " + expectedExceptionCode + ". But did not throw an exception.");
} catch (exception) {
    if (exception.code === expectedExceptionCode)
        self.postMessage("PASS threw exception " + exception + ".");
    else
        self.postMessage("FAIL should throw " + expectedExceptionCode + ". Threw exception " + exception + ".");
}

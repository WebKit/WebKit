<?php
    require "determine-content-security-policy-header.php";
?>
self.result = false;
var exception;
try {
    importScripts("http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.php?url=http://localhost:8000/security/contentSecurityPolicy/resources/script-set-value.js");
} catch (e) {
    exception = e;
}
if (exception)
    self.postMessage("FAIL should not have thrown an exception. Threw exception " + exception + ".");
else {
    if (self.result)
        self.postMessage("PASS did import script from different origin.");
    else
        self.postMessage("FAIL did not import script from different origin.");
}

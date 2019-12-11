<?php
header("Content-type:application/javascript");

if (isset($_COOKIE["foo"])) {
    echo "var cookieVal = '" . $_COOKIE["foo"] . "';";
} else {
    echo "var cookieVal = '<null>';";
}

?>

if (cookieVal == "bar")
	document.getElementById("result").innerHTML = "PASSED: Cookie is set to 'bar'";
else
	document.getElementById("result").innerHTML = "FAILED: Cookie should be 'bar', is set to '" + cookieVal + "'";

if (window.testRunner)
	testRunner.notifyDone();

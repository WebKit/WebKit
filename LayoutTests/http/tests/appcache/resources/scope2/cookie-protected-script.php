<?php
header("Content-type:application/javascript");

if (isset($_COOKIE["scope"])) {
    echo "var cookieVal = '" . $_COOKIE["scope"] . "';";
} else {
    echo "var cookieVal = '<null>';";
}

?>

if (cookieVal === "script")
    document.getElementById("result1").innerHTML = "PASSED: Script Cookie is set to 'script'";
else
    document.getElementById("result1").innerHTML = "FAILED: Script Cookie should be 'script', is set to '" + cookieVal + "'";

<!DOCTYPE html>
<html>
<head>
<script src="cookie-utilities.js"></script>
<script>
function logDOMCookie(name, value)
{
    document.getElementById("dom-cookies-output").appendChild(document.createTextNode(`${name} = ${value}\n`));
}

window.onload = () => {
    let domCookies = getDOMCookies();
    for (let name of Object.keys(domCookies).sort())
        logDOMCookie(name, domCookies[name]);

    if (window.testRunner)
        testRunner.notifyDone();
};
</script>
</head>
<body>
<p>HTTP sent cookies:</p>
<pre>
<?php
$sortedCookieNames = array_keys($_COOKIE);
sort($sortedCookieNames);

foreach ($sortedCookieNames as $name)
    echo "$name = $_COOKIE[$name]\n";
?>
</pre>
<p>DOM cookies:</p>
<pre id="dom-cookies-output"></pre>
</body>
</html>

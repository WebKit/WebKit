<?php
header("Content-Type: text/html; charset=UTF-8");
?>
<html>
<head>
<script>

function runTest()
{
    var r = document.getElementById('result');
    var o = document.getElementById('output').firstChild;
    if (o.nodeValue == 'spkac exists') 
        r.innerHTML = "SUCCESS: keygen was parsed correctly";
    else
        r.innerHTML = "FAILURE: keygen was not parsed correctly. value=" +
        o.nodeValue;
        
    if (window.testRunner)
        testRunner.notifyDone();
}

</script>
</head>
<body onload="runTest()">
<p>
This is a regression test for keygen tag POST processing: https://bugs.webkit.org/show_bug.cgi?id=70617.
</p>
<div style='display: none;' id='output'><?php
if (array_key_exists('spkac', $_REQUEST)) {
    echo "spkac exists";
} else {
    echo "spkac does not exist";
}
?>
</div>
<div id="result"></div>
</body>
</html>

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
    if (o.nodeValue == 'MIHFMHEwXDANBgkqhkiG9w0BAQEFAANLADBIAkEAnX0TILJrOMUue%2BPtwBRE6XfV%0AWtKQbsshxk5ZhcUwcwyvcnIq9b82QhJdoACdD34rqfCAIND46fXKQUnb0mvKzQID%0AAQABFhFNb3ppbGxhSXNNeUZyaWVuZDANBgkqhkiG9w0BAQQFAANBAAKv2Eex2n%2FS%0Ar%2F7iJNroWlSzSMtTiQTEB%2BADWHGj9u1xrUrOilq%2Fo2cuQxIfZcNZkYAkWP4DubqW%0Ai0%2F%2FrgBvmco%3D')
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
    echo $_REQUEST['spkac'];
} else {
    echo "spkac does not exist";
}
?>
</div>
<div id="result"></div>
</body>
</html>

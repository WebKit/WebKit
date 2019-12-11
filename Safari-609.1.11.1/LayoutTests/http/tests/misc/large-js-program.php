<html>
<head>
<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>
</head>
<body>
<h1>This tests verifies that a large program doesn&#39;t crash JavaScript.</h1>
<p>This test should generate an out of stack exception, but have no other output.
<br>
<pre id="console"></pre>
<script src="/js-test-resources/js-test-pre.js"></script>
<script>
function print(m)
{
    document.getElementById("console").innerHTML += m + "<br>";
}

function foo(o)
{
    // We should not get to this code, we should throw an out of stack exception calling foo().
    testFailed("We should never get here!");
}


foo({"x": 1,
     "a": [
<?php
for ($i = 0; $i < 1000000; $i++) {
    if ($i != 0)
        echo ",\n";
    echo "[0, $i]";
}
?>
]});
</script>
</body>
</html>

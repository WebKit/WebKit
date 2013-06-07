<?php

@apache_setenv('no-gzip', 1);
@ini_set('zlib.output_compression', 0);
@ini_set('implicit_flush', 1);
for ($i = 0; $i < ob_get_level(); $i++) { ob_end_flush(); }
ob_implicit_flush(1);

header("HTTP/1.1 200 OK"); flush();

echo str_repeat(" ", 1024)."<pre>"; flush();
?>

<script>

if (window.testRunner) {
	testRunner.dumpAsText();
	testRunner.waitUntilDone();
}

</script>

This page takes forever to load.<br>
It also blocks on an external script that takes forever to load.<br>
Without the fix for http://webkit.org/b/117278, navigating away from this page will crash.<br>
So... navigating away should not crash!<br>

<script>

setTimeout("location.href = 'resources/notify-done.html'", 0);

</script>

<script src='resources/slow-loading-script.php'></script>

<?php
while(true)
{
    echo "Still loading...<br>\r\n"; flush(); sleep(1);
}
?>

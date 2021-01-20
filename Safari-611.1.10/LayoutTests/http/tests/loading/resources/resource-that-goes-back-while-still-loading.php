<?php

@apache_setenv('no-gzip', 1);
@ini_set('zlib.output_compression', 0);
@ini_set('implicit_flush', 1);
for ($i = 0; $i < ob_get_level(); $i++) { ob_end_flush(); }
ob_implicit_flush(1);

header("HTTP/1.1 200 OK"); flush();

echo str_repeat(" ", 1024)."<pre>"; flush();
echo "This page should trigger a back navigation while it's still loading.<br>\r\n";
echo "<script>setTimeout('window.history.back()', 0);</script>";

while(true)
{
    echo "Still loading...<br>\r\n"; flush(); sleep(1);
}

?>

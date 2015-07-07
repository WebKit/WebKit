<?php
header('HTTP/1.1 200 OK');
header('Ã: text/html');
echo '<script>';
echo '   if (window.testRunner)';
echo '       testRunner.dumpAsText();';
echo '</script>';
echo '<p>Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=96284">bug 96284</a>: Non UTF-8 HTTP headers do not cause a crash.</p>';
?>

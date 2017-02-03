#!/usr/bin/perl -wT
use strict;

print <<"EOL";
HTTP/1.1 200 OK
Ã: text/html
Content-Type: text/html

<script>
   if (window.testRunner)
       testRunner.dumpAsText();
</script>
<p>Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=96284">bug 96284</a>: Non UTF-8 HTTP headers do not cause a crash.</p>
EOL

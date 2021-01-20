<?php
 header("Content-Type: text/plain");
 header("X-Custom-Header: test");
 header("Set-Cookie: test");
 header("Set-Cookie2: test");
 header("X-Custom-Header-Empty:");
 header("X-Custom-Header-Comma: 1");
 header("X-Custom-Header-Comma: 2", false);
 header("X-Custom-Header-Bytes: …");
 echo "TEST";
?>

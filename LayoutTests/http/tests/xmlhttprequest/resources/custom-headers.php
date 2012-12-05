<?php
 header("Content-Type: text/plain");
 header("X-Custom-Header-Single: single");
 header("X-Custom-Header-Empty:");
 header("X-Custom-Header-List: one");
 header("X-Custom-Header-List: two", false);
?>
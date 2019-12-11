<?php
    header("Access-control-max-age: 0");
    header("Access-control-allow-origin: *");
    header("X-foo: BAR");
    header("X-TEST: TEST");
    header("Access-Control-Expose-Headers: x-Foo");
    header("Content-Type: text/html");

    print "echo"
?>

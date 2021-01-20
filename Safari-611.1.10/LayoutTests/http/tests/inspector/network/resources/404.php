<?php
header("HTTP/1.1 404 Not Found");

# Non-empty response.
$str = "12345678";
for ($i = 0; $i < 6; ++$i)
    $str .= $str;
echo $str;

?>

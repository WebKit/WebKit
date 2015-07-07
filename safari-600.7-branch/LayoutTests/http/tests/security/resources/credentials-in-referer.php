<?php
header("Cache: no-cache, no-store");

$refer = $_SERVER['HTTP_REFERER'];
if ($refer && stristr($refer, "login"))
    print("log('External script: FAIL')");
else 
    print("log('External script: PASS')");
?>

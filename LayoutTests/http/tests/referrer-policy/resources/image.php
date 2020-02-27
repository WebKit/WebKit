<?php
$refer = $_SERVER['HTTP_REFERER'];
$expected = $_GET['expected'];
if ($refer != $expected) {
    header("Content-type: text/plain");
    echo $refer;
    exit;
}
header("Content-type: image/png");
echo file_get_contents("../../security/contentSecurityPolicy/block-all-mixed-content/resources/red-square.png");
exit;
?>

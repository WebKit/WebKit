<?php
$referrer = $_SERVER['HTTP_REFERER'];
if ($referrer == "") {
    echo "HTTP Referer header is empty";
} else {
    echo $referrer;
}
?>
<?php
if($_GET["fromOrigin"] == "same") {
    header("From-Origin: Same");
} elseif($_GET["fromOrigin"] == "same-site") {
    header("From-Origin: Same-Site");
}

if (isset($_SERVER['HTTP_ORIGIN'])) {
    header("Access-Control-Allow-Origin: {$_SERVER['HTTP_ORIGIN']}");
}
?>
xhr
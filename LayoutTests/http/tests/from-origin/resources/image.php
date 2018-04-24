<?php
if($_GET["fromOrigin"] == "same") {
    header("From-Origin: Same");
} elseif($_GET["fromOrigin"] == "same-site") {
    header("From-Origin: Same-Site");
}
$fp = fopen("../../resources/square20.jpg", "rb");
header("Content-Type: image/jpeg");
header("Content-Length: " . filesize($name));
fpassthru($fp);
exit;
?>
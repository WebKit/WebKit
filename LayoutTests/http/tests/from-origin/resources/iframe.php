<?php
if($_GET["fromOrigin"] == "same") {
    header("From-Origin: Same");
} elseif($_GET["fromOrigin"] == "same-site") {
    header("From-Origin: Same-Site");
}
?>
<h3>The iframe</h3>

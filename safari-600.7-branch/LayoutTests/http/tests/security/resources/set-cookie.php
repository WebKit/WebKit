<?php
setcookie($_GET["name"], $_GET["value"], 0, "/");
?>
Set <?= $_GET["name"] ?>=<?= $_GET["value"] ?>

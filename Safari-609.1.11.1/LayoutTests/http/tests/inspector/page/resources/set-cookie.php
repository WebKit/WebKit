<?php
    setcookie($_GET["name"], $_GET["value"], time() + 3600);
?>
Set <?= $_GET["name"] ?>=<?= $_GET["value"] ?>

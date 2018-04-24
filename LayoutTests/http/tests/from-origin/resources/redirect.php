<?php
$redirectURL = $_GET["redirectTo"];
header('Location: ' . $redirectURL);
die();
?>

<?php
$redirectURL = $_GET["redirectTo"];
if (isset($_GET["name2"])) {
    $redirectURL = $redirectURL . "&name2=" . $_GET["name2"];
}
if (isset($_GET["name3"])) {
    $redirectURL = $redirectURL . "&name3=" . $_GET["name3"];
}
if (isset($_GET["message"])) {
    $redirectURL = $redirectURL . "&message=" . $_GET["message"];
}
header('Location: ' . $redirectURL);
die();
?>

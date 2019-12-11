<?php
echo $_GET["message"] . "<br>";
if(!isset($_COOKIE[$_GET["name1"]])) {
    echo "Did not receive cookie named '" . $_GET["name1"] . "'.<br>";
} else {
    echo "Received cookie named '" . $_GET["name1"] . "'.<br>";
}
if(!empty($_GET["name2"])) {
    if(!isset($_COOKIE[$_GET["name2"]])) {
        echo "Did not receive cookie named '" . $_GET["name2"] . "'.<br>";
    } else {
        echo "Received cookie named '" . $_GET["name2"] . "'.<br>";
    }
}
if(!empty($_GET["name3"])) {
    if(!isset($_COOKIE[$_GET["name3"]])) {
        echo "Did not receive cookie named '" . $_GET["name3"] . "'.<br>";
    } else {
        echo "Received cookie named '" . $_GET["name3"] . "'.<br>";
    }
}
?>
<p id="output"></p>
<script>
    document.getElementById("output").textContent = "Client-side document.cookie: " + document.cookie.replace(/ /g,'').split(';').sort();
</script>
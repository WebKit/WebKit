<?php
echo $_GET["message"] . "<br>";
if(!isset($_COOKIE[$_GET["name1"]])) {
    echo "Did not receive cookie named '" . $_GET["name1"] . "'.<br>";
} else {
    echo "Received cookie named '" . $_GET["name1"] . "'.<br>";
}
if(!isset($_COOKIE[$_GET["name2"]])) {
    echo "Did not receive cookie named '" . $_GET["name2"] . "'.<br>";
} else {
    echo "Received cookie named '" . $_GET["name2"] . "'.<br>";
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
    document.getElementById("output").textContent = "Client-side document.cookie: " + document.cookie;

    function messageToTop(messagePrefix, fetchData) {
        top.postMessage(messagePrefix + " document.cookie == " + document.cookie +
            (fetchData ? ", cookies seen server-side == " + JSON.stringify(fetchData) : ""), "http://127.0.0.1:8000");
    }

    function receiveMessage(event) {
        if (event.origin === "http://127.0.0.1:8000") {
            if (event.data.indexOf("reportBackCookies") !== -1) {
                fetch("echo-incoming-cookies-as-json.php", { credentials: "same-origin" }).then(function(response) {
                    return response.json();
                }).then(function(data) {
                    messageToTop("PASS", data);
                }).catch(function(error) {
                    console.log(error.message);
                });
            } else {
                messageToTop("FAIL Unknown request.");
            }
        } else {
            messageToTop("Fail Received a message from an unexpected origin: " + event.origin);
        }
    }

    window.addEventListener("message", receiveMessage, false);
</script>
<?php
header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Content-Type: application/javascript");

if (isset($_GET["credentials"])) {
    if (strtolower($_GET["credentials"]) == "true") {
        header("Access-Control-Allow-Credentials: true");
    } else {
        header("Access-Control-Allow-Credentials: false");
    }
}

if (strtolower($_GET["fail"]) == "true")
    echo "throw({toString: function(){ return 'SomeError' }});";
else
    echo "alert('script ran.');";
?>

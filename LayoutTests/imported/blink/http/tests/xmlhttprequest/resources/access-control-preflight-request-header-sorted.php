<?php
if ($_SERVER["REQUEST_METHOD"] == "OPTIONS") {
    // Check that the names in Access-Control-Request-Headers are
    // "sorted lexicographically, and byte lowercased".
    // Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch-0
    if ($_SERVER["HTTP_ACCESS_CONTROL_REQUEST_HEADERS"] ==
        'x-custom-s, x-custom-test, x-custom-u, x-custom-ua, x-custom-v') {
        header("Access-Control-Allow-Headers: x-custom-s, x-custom-test, x-custom-u, x-custom-ua, x-custom-v");
    } else {
        header("HTTP/1.1 400");
    }
    header("Access-Control-Allow-Origin: *");
    header("Access-Control-Max-Age: 0");
} else if ($_SERVER["REQUEST_METHOD"] == "GET") {
    header("Access-Control-Allow-Origin: *");
    header("Access-Control-Max-Age: 0");
    if (isset($_SERVER["HTTP_X_CUSTOM_S"]))
        echo "PASS";
    else
        echo "FAIL";
}
?>

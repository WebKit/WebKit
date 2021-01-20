<?php
require_once '../../resources/portabilityLayer.php';

header("Access-Control-Allow-Origin: *");
header("Access-Control-Max-Age: 0");

if ($_SERVER["REQUEST_METHOD"] == "OPTIONS") {
    // Split the Access-Control-Request-header value based on the token.
    $accessControlRequestHeaderValues = explode(", ", $_SERVER["HTTP_ACCESS_CONTROL_REQUEST_HEADERS"]);
    if (in_array("x-custom-header", $accessControlRequestHeaderValues)) // Case-sensitive comparison to make sure that browser sends the value in lowercase.
        header("Access-Control-Allow-Headers: X-Custom-Header");  // Add "Access-Control-Allow-Headers: X-Custom-Header" to "OPTIONS" response.

} else if ($_SERVER["REQUEST_METHOD"] == "GET") {
    if (isset($_SERVER["HTTP_X_CUSTOM_HEADER"]))
        echo "PASS";
    else
        echo "FAIL";
}
?>

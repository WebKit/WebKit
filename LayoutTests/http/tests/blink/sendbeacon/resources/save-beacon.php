<?php
require_once '../../../resources/portabilityLayer.php';

function prettify($name) {
   return str_replace(' ', '-', ucwords(str_replace('_', ' ', str_replace('http_', '', strtolower($name)))));
}

$beaconFilename = sys_get_temp_dir() . "/beacon" . (isset($_REQUEST['name']) ? $_REQUEST['name'] : "") . ".txt";
$beaconFile = fopen($beaconFilename . ".tmp", 'w');
$httpHeaders = $_SERVER;
ksort($httpHeaders, SORT_STRING);
$contentType = "";
foreach ($httpHeaders as $name => $value) {
    if ($name === "CONTENT_TYPE" || $name === "HTTP_REFERER" || $name === "REQUEST_METHOD" || $name === "HTTP_COOKIE" || $name === "HTTP_ORIGIN") {
        if ($name === "CONTENT_TYPE") {
            $contentType = $value;
            $value = preg_replace('/boundary=.*$/', '', $value);
        }
        $headerName = prettify($name);
        fwrite($beaconFile, "$headerName: $value\n");
    }
}
$postdata = file_get_contents("php://input");
if (strlen($postdata) == 0)
   $postdata = http_build_query($_POST);

fwrite($beaconFile, "Length: " . strlen($postdata) . "\n");
if (strpos($contentType, "application/") !== false) {
    $postdata = base64_encode($postdata);
}

fwrite($beaconFile, "Body: $postdata\n");
fclose($beaconFile);
rename($beaconFilename . ".tmp", $beaconFilename);

if (!array_key_exists('dontclearcookies', $_GET)) {
  foreach ($_COOKIE as $name => $value)
      setcookie($name, "deleted", time() - 60, "/");
}
?>

<?php
require_once '../../resources/portabilityLayer.php';

clearstatcache();

if ($_SERVER["HTTP_IF_MODIFIED_SINCE"]) {
    header("HTTP/1.0 304 Not Modified");
    exit();
}
$one_year = 12 * 31 * 24 * 60 * 60;
$last_modified = gmdate(DATE_RFC1123, time() - $one_year);
$expires = gmdate(DATE_RFC1123, time() + $one_year);


header('Cache-Control: no-cache, max-age=' . $one_year);
header('Expires: ' . $expires);
header('Content-Type: text/html');
header('Etag: 123456789');
header('Last-Modified: ' . $last_modified);
header('X-FRAME-OPTIONS: ALLOWALL');

echo "<body><script>\n";
echo "window.onload = function() { window.parent.test(); }\n";
echo "</script></body>\n";

?>

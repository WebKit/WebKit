<?php
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");

$query=$_GET['query'];
if ($query == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82") {
    print("PASS: XHR query was encoded in UTF-8: ");
    print($query);
} else {
    print("FAIL: XHR query was NOT encoded in UTF-8: ");
    print($query);
}
?>

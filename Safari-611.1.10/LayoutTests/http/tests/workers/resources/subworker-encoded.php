<?php
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");

print("postMessage('Sub: Original test string: ' + String.fromCharCode(0x41F, 0x440, 0x438, 0x432, 0x435, 0x442));");
print("postMessage('Sub: Test string encoded using koi8-r: \xF0\xD2\xC9\xD7\xC5\xD4.');");
print("postMessage('Sub: Test string encoded using Windows-1251: \xCF\xF0\xE8\xE2\xE5\xF2.');");
print("postMessage('Sub: Test string encoded using UTF-8: \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82.');");
?>

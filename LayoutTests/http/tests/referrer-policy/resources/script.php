<?php
header("Cache: no-cache, no-store");
$refer = $_SERVER['HTTP_REFERER'];
print("checkReferrer('$refer');");
?>

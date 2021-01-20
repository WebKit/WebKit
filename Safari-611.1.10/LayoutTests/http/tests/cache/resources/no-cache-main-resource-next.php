<?php
header("Cache-control: no-cache, max-age=0");
header("Last-Modified: Thu, 01 Jan 1970 00:00:00 GMT");
header('Content-Type: text/html');

print "<script>\n";
print "var randomNumber = " . rand() . ";\n";
?>

opener.postMessage(randomNumber, '*');
</script>

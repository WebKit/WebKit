<?php
header("Content-Disposition: attachment; filename=test.xhtml");
header("Content-Type: application/xhtml+xml");
echo "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
echo "<?xml-stylesheet href=\"data:text/css,body::after { content: 'FAIL'; }\" ?>\n";
echo "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
echo "<body></body>\n";
echo "</html>\n";
?>

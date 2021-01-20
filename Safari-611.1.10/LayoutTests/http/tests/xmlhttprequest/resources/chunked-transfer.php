<?php 
header("Transfer-encoding: chunked");
header("Cache-Control: no-cache, no-store");
flush();
printf("4\r\n<a/>\r\n");
flush();
printf("0\r\n\r\n"); 
?>

<?php 
header("Transfer-encoding: chunked");
flush();
printf("4\r\n<a/>\r\n");
flush();
printf("0\r\n\r\n"); 
?>

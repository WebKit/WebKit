<?
header("Content-Type: text/html; charset=utf-8");
?>
<!-- <?php
# Spam a bunch of As to make sure we blow past any buffers.
print str_repeat("A", 2048);
?> -->
<body>
<?php
print "<a ona";
print str_repeat("a", 2000);

flush();
usleep(200000);

print "click=alert(1) ttt>";
?>
Done.

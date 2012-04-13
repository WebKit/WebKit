<?php 
       header('Set-Cookie: db_cookie = https_cookie; expires=Thu, 12-Apr-2312 08:32:29 GMT;');
       echo 'Test updating of cookies\' database. It is for https://bugs.webkit.org/show_bug.cgi?id=83760.<br>';
       echo 'To run this test, http-cookie-database-set.php must be served over http and http-cookie-database-update.php must be served over https.<br>';
       echo 'Test steps:<br>';
       echo '1. Load http-cookie-database-set.php. (If you load http-cookie-database-update.php directly in this step, please clear cookies and load http-cookie-database-set.php to finish this step.<br>';
       echo '2. Restart the browser and load http-cookie-database-set.php again to see the result.<br>';

?>

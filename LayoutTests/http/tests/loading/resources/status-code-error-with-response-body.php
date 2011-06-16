<?php
header("Content-Type: text/html", TRUE, (int)$_REQUEST['statusCode']);
?>
<html>
<body>
<p>status code is <?php echo $_REQUEST['statusCode'];?></p>
</body>
</html>

<?php
header('Etag: 123456789');
echo file_get_contents('php://input');
?>

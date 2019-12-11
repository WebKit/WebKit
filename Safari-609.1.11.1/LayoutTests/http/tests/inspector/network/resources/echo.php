<?php
$mimeType = isset($_GET['mimetype']) ? $_GET['mimetype'] : 'text/plain';
$content = isset($_GET['content']) ? $_GET['content'] : 'Missing mimetype or content query parameter.';

header("Content-Type: $mimeType");
echo $content;
?>

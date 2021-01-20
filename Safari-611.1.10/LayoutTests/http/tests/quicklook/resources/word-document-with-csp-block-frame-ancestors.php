<?php
header("Content-Type: application/vnd.openxmlformats-officedocument.wordprocessingml.document");
header("Content-Security-Policy: frame-ancestors 'none'");
readfile("pass.docx");
?>

<?php

header("Content-Type: text/css");
header("Cache-Control: public, max-age=264");

echo ("@import \"imported-stylesheet-varying-according-domain.css.php?domain=127.0.0.1:8000\"");
exit;
?>

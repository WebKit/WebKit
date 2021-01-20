<?php
header("Content-Type: text/json");
echo "{";
for ($i = 0; $i < 13*1024*1024; $i++) {
    echo "\"v_$i\":$i,";
    if (!$i % 1000)
        flush();
}
echo "\"end\" : true}";
flush();
?>

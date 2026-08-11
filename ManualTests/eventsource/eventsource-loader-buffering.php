<?php
header("Content-Type: text/event-stream");

$data = "";
for ($i = 0; $i < 1000; $i++)
    $data .= "x";

for ($i = 0; $i < 30000; $i++) {
    echo "data: $data\n\n";
    flush();
    usleep(1000);
}
?>

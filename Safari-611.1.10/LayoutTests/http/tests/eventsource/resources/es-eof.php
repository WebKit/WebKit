<?php
header("Content-Type: text/event-stream");

$id = floatval($_SERVER['HTTP_LAST_EVENT_ID']) + 1;

echo "retry: 0\n";
echo "id: " . $id . "\n";
echo "data: DATA" . intval($id) . "\n\n";

echo "id: " . ($id + 0.1) . "\n";
echo "event: msg\n";
echo "data: DATA" . str_repeat("\n", $id - 1);
?>

<?php
for ($i = 0; $i < 1024 * 1024; $i++) {
    echo "Chunk $i";
    if (!$i % 200)
        flush();
}
?>

<?php
for ($i = 0; $i < 5; $i++) {
    echo "test";
    // Force content to be sent to the browser as is.
    flush();
    sleep(1);
}
?>

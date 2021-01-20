<?php
    // Prevent from being cached.
    header("Cache-Control: no-store, private, max-age=0");
    header("Content-Type: text/plain");
?>
<?php echo $_SERVER['HTTP_REFERER']; ?>

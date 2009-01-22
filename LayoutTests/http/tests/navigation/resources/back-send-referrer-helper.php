<?php
    // Prevent from being cached.
    header("Cache-Control: no-cache, private, max-age=0");
    header("Content-Type: text/html");
?>

<html>

<script>
    window.name = parseInt(window.name) + 1;
</script>

Referrer: <?php echo $_SERVER['HTTP_REFERER']; ?>
<br/>
window.name: <script>document.write(window.name)</script>

<form name=loopback action="" method=GET></form>

<script>
    if (window.name == 1) {
        // Navigate once more to add a history entry.
        document.loopback.submit();
    } else if (window.name == 2) {
        history.go(-1);
    } else {
        if (window.layoutTestController)
            window.layoutTestController.notifyDone();
    }
</script>

</html>

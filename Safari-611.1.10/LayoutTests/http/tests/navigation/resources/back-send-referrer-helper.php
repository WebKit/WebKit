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
        // Navigate once more (in a timeout) to add a history entry.
        setTimeout(function() {document.loopback.submit();}, 0);
    } else if (window.name == 2) {
        history.go(-1);
    } else {
        if (window.testRunner)
            window.testRunner.notifyDone();
    }
</script>

</html>

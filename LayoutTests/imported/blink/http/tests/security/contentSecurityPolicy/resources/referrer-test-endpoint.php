<html>
<head>
<script>
    var referrerHeader = "<?php echo $_SERVER['HTTP_REFERER'] ?>";
    window.top.postMessage(referrerHeader, "*");
</script>
</head>
</html>


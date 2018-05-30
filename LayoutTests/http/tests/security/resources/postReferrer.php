<!DOCTYPE html>
<html>
<body>
<script>
top.postMessage('<?php echo $_SERVER["HTTP_REFERER"]; ?>', '*');
</script>
</body>

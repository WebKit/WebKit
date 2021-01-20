<?php
header("Content-Disposition: attachment; filename=test.html");
header("Content-Type: text/html");
?>
<!DOCTYPE html>
<script>
document.write('FAIL');
</script>

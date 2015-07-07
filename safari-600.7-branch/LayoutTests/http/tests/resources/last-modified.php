<?php
    $date = $_GET['date'];
    header("Last-Modified: $date");
?>
<script>
document.write(document.lastModified);
</script>

<?php
    $date = $_GET['date'];
    header("Last-Modified: $date");
?>
<script>
document.write((new Date(document.lastModified)).toUTCString());
</script>

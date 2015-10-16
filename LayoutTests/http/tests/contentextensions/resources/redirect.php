<?php
if (isset($_GET['to'])) {
    echo("<script>location.href='" . $_GET['to'] . "';</script>");
}
echo "<body>Redirecting</body>";
?>

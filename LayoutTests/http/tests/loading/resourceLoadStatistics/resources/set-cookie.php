<?php
setcookie($_GET["name"], $_GET["value"], 0, "/");
echo $_GET["message"] . "<br>";
?>
<script>
if (document.location.hash) {
    setTimeout("document.location.href = document.location.hash.substring(1)", 10);
}
</script>
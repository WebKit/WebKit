<?php
setcookie($_GET["name"], $_GET["value"], (time()+60*60*24*30), "/");
echo $_GET["message"] . "<br>";
?>
<script>
if (document.location.hash) {
    setTimeout("document.location.href = document.location.hash.substring(1)", 10);
}
</script>
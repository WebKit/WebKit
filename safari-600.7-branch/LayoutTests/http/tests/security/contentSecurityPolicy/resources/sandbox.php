<?php
header("Content-Security-Policy: sandbox " . $_GET["sandbox"]);
?>
<!DOCTYPE html>
<p>Ready</p>
<script>
alert("Script executed in iframe.");
window.secret = "I am a secret";
</script>

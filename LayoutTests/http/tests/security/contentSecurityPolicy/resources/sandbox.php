<?php
if ($_GET["report-only"]) {
    header("Content-Security-Policy-Report-Only: sandbox " . $_GET["sandbox"]);
} else {
    header("Content-Security-Policy: sandbox " . $_GET["sandbox"]);
}
?>
<!DOCTYPE html>
<p>Ready</p>
<script>
console.log("Script executed in iframe.");
window.secret = "I am a secret";
</script>

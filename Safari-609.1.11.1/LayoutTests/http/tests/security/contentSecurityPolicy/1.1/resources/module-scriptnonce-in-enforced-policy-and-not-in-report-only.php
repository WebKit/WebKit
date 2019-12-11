<?
    header("Content-Security-Policy: script-src 'nonce-test'");
    header("Content-Security-Policy-Report-Only: script-src 'none'");
?>
<!DOCTYPE html>
<html>
<body>
<p id="script-with-nonce-result">FAIL did not execute script with nonce.</p>
<p id="script-without-nonce-result">PASS did not execute script without nonce.</p>
<script type="module" nonce="test">
document.getElementById("script-with-nonce-result").textContent = "PASS did execute script with nonce.";
if (window.testRunner)
    testRunner.notifyDone();
</script>
<script type="module" >
document.getElementById("script-without-nonce-result").textContent = "FAIL did execute script without nonce.";
</script>
</body>
</html>

<p>Referrer: <?php echo $_SERVER['HTTP_REFERER'] ?></p>
<script>
console.log("Referrer: <?php echo $_SERVER['HTTP_REFERER'] ?>");

var result = "window.opener: " + (window.opener ? "FAIL" : "PASS");
document.write(result);
console.log(result);
if (window.testRunner)
    testRunner.notifyDone();
</script>

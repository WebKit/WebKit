<?php
header('Content-type: multipart/x-mixed-replace; boundary=boundary');
header('Connection: keep-alive');
echo "--boundary\r\n";
echo "Content-Type: text/html\r\n\r\n";
echo str_pad('', 5000);
?>

<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    if (testRunner.setShouldDecideResponsePolicyAfterDelay)
        testRunner.setShouldDecideResponsePolicyAfterDelay(true);
}
</script>

<?php
for ($i = 0; $i <= 10; $i++) {
    echo "--boundary\r\n";
    echo "Content-Type: text/html\r\n\r\n";
    if ($i < 10) {
        echo "This is message {$i}.<br>";
        echo "FAIL: The message number should be 10.";
    } else {
        echo "PASS: This is message {$i}.";
    }
    echo str_pad('', 5000);
    echo "\r\n\r\n";
    flush();
    usleep(100000);
}
?>
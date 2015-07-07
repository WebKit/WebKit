<?php
header('Content-type: multipart/x-mixed-replace; boundary=boundary');
header('Connection: keep-alive');
echo "--boundary\r\n";
echo "Content-Type: text/html\r\n\r\n";
echo str_pad('', 5000);
?>

<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>

<?php
for ($i = 0; $i <= 10; $i++) {
    echo "--boundary\r\n";
    echo "Content-Type: text/plain\r\n\r\n";
    echo "This text should only appear once ";
    echo $i;
    echo str_pad('', 5000);
    echo "\r\n\r\n";
    flush();
    usleep(100000);
    $i++;
}
?>

<?php 
sleep(1);
header("Content-Type: text/html"); 
echo("<script>alert('PASS - iframe loaded'); if (window.testRunner) testRunner.notifyDone(); </script>"); 
?>

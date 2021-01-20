<?php

$expire = time() + 30;
$step = empty($_GET['step']) ? '' : $_GET['step'];
$cookie_name = empty($_GET['cookie_name']) ? md5(__FILE__ . time()) : $_GET['cookie_name'];

if (!$step) {
    // Step 0: Set cookie for following request.
    setcookie($cookie_name, 'not sure, but something', $expire);
    step0();
} elseif ($step == 1) {
    // Step 1: Request caused by JS. It is sent with Cookie header with value of step 0.
    setcookie($cookie_name, '42', $expire);
    redirect_to_step(2);
} elseif ($step == 2) {
    // Step 2: Redirected request should have only Cookie header with update value/
    step2($_COOKIE[$cookie_name]);
} else {
    die("Error: unknown step: {$step}");
}

exit(0);

function redirect_to_step($step) {
    header("HTTP/1.0 302 Found");
    header('Location: ' . redirect_url($step));
}

function redirect_url($step) {
    global $cookie_name;
    return "http://127.0.0.1:8000/cookies/" . basename(__FILE__) . "?step={$step}&cookie_name={$cookie_name}";
}

function step0() {
?>
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function gotoStep1() {
    window.location = "<?php echo redirect_url(1); ?>";
}
</script>

<body onload="gotoStep1()">
</body>
</html>
<?php
}

function step2($result) {
?>
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<script>
function finish() {
    if (window.testRunner)
        testRunner.notifyDone();
}
</script>

<body onload="finish()">
Cookie: <?php echo $result; ?>
</body>
</html>
<?php
}
?>

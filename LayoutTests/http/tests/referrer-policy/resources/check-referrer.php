<script src="/js-test-resources/js-test.js"></script>
<script>
description("Tests the behavior of <?php echo $_GET['policy']; ?> referrer policy.");

let referrer = "<?php echo $_SERVER['HTTP_REFERER']; ?>";
let expected = "<?php echo $_GET['expected']; ?>";
shouldBe("referrer", "expected");
if (window.testRunner)
    testRunner.notifyDone();
</script>

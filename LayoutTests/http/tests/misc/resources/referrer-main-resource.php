<html>
<head>
<script>
function runTest() {

    if (window.testRunner)
        testRunner.dumpAsText();

    <?php if (isset($_SERVER['HTTP_REFERER']))
              echo 'document.write("FAIL: The server should not receive a referrer which is not set by user agent.");';
          else
              echo 'document.write("PASS: The server didn\'t receive a referrer.");';
    ?>;

}
</script>
</head>
<body onload="runTest()">
</body>
</html>

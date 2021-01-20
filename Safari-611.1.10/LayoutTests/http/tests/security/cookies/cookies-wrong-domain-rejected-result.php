<html>
<head>
<script>
function runTest() {

    if (window.testRunner)
        testRunner.dumpAsText();

    <?php if (isset($_COOKIE['one_cookie']))
              echo 'document.write("FAIL: Cookies with a wrong domain should be rejected in user agent.");';
          else
              echo 'document.write("PASS: User agent rejected the cookie with a wrong domain.")';
    ?>;

}
</script>
</head>
<body onload="runTest()">
</body>
</html>


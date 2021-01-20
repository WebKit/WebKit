<html>
<body>

<p>Tests for user agent string template</p>

<script>
    if (window.testRunner) {
        testRunner.dumpAsText();
    }

    var userAgent = navigator.userAgent;

    // Validate the user agent string using the following template:
    var userAgentTemplate = "Mozilla/5.0 (%Platform%%Subplatform%) AppleWebKit/%WebKitVersion% (KHTML, like Gecko)"
    var userAgentTemplateRegExp = /^Mozilla\/5\.0 \(([^;]+; )*[^;]+\) AppleWebKit\/[0-9\.]+(\+)? \(KHTML, like Gecko\).*$/;
    document.write("UserAgent should match the " + userAgentTemplate + " template: " + !!userAgent.match(userAgentTemplateRegExp) + "<br>");

    // Validate navigator.appVersion and navigator.appCodeName
    document.write("UserAgent should be the same as the appVersion with appCodeName prefix: " + (userAgent == navigator.appCodeName + "/" + navigator.appVersion) + "<br>");

    // Validate HTTP User-Agent header
    var userAgentHeader = '<?php echo $_SERVER['HTTP_USER_AGENT']; ?>';
    document.write("HTTP User-Agent header should be the same as userAgent: " + (userAgentHeader == userAgent) + "<br>");

    // Make sure language tag is not present
    var languageTagRegExp = new RegExp("[ ;\(]" + navigator.language + "[ ;\)]");
    document.write("Language tag should not be present in the userAgent: " + !userAgent.match(languageTagRegExp) + "<br>");

</script>
</body>
</html>

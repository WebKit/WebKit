<body>
</body>
<script>
if (window.testRunner) {
	testRunner.dumpAsText();
} else {
	alert("Test can only be run in WebKitTestRunner");
}
</script>
<script src="resources/user-agent-script.php"></script>
<script>
window.mainResourceUserAgent = "<?php 
echo $_SERVER['HTTP_USER_AGENT'];
?>";

if (!sessionStorage.savedUserAgent) {
	sessionStorage.savedUserAgent = navigator.userAgent;
	testRunner.setCustomUserAgent("WebKitRules");
	testRunner.queueReload();
} else {
	errorFound = false;
	if (mainResourceUserAgent != subresourceUserAgent) {
		errorFound = true;
		document.body.innerHTML = "Error: Main resource and subresource were fetched with different user agent strings.";
    }
	if (mainResourceUserAgent == sessionStorage.savedUserAgent) {
		errorFound = true;
		document.body.innerHTML += "Error: Main resource was fetched with the same user agent string on the reload as was used on the first load.";
	}
    if (subresourceUserAgent == sessionStorage.savedUserAgent) {
		errorFound = true;
		document.body.innerHTML += "Error: Subresource was fetched with the same user agent string on the reload as was used on the first load.";
	}
	
	if (!errorFound)
		document.body.innerHTML = "Passed";
	
	testRunner.notifyDone();
}
</script>

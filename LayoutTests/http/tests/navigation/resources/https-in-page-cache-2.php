<?php
header("cache-control: no-cache");
?>
<script>

if (window.testRunner)
    testRunner.overridePreference("WebKitUsesPageCachePreferenceKey", 1);

function nextTest() {
	if (window.sessionStorage.https_in_page_cache_started)
		delete window.sessionStorage.https_in_page_cache_started;
	location = "https://127.0.0.1:8443/navigation/resources/https-in-page-cache-3.html";
}

window.onpageshow = function(evt) {
	if (!evt.persisted)
		return;

	alert("The page was restored from the page cache. Good job!. Running part 3 of the test.");
	nextTest();
}

window.onload = function() {
	alert("This page is https and has the no-cache cache-control directive. It should go in to the page cache.");
	window.sessionStorage.https_in_page_cache_started = true;
	setTimeout('window.location = "go-back.html"', 0);

}

</script>

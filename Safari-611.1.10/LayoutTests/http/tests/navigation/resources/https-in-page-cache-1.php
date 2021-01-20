<?php
header("cache-control: no-store");
?>
<script>

function nextTest() {
	if (window.sessionStorage.https_in_page_cache_started)
		delete window.sessionStorage.https_in_page_cache_started;
	location = "https://127.0.0.1:8443/navigation/resources/https-in-page-cache-2.php";
}

window.onpageshow = function(evt) {
	if (evt.persisted) {
		alert("The page was restored from the page cache. It should NOT have been. Running part 2 of the test.");
		nextTest();
	}
}

window.onload = function() {
	if (window.sessionStorage.https_in_page_cache_started) {
		alert("The page was reloaded on back, not from the page cache. Good job. Running part 2 of the test.");
		nextTest();
		return;
	}

	alert("This page is https and has the no-store cache-control directive. It should NOT go in to the page cache.");
	window.sessionStorage.https_in_page_cache_started = true;
	setTimeout('window.location = "go-back.html"', 0);

}

</script>

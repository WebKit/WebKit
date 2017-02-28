<?php 
    if (!isset($_SERVER['PHP_AUTH_USER'])) {
        header('WWW-Authenticate: Basic realm="WebKit test - credentials-in-main-resource"');
        header('HTTP/1.0 401 Unauthorized');
        exit;
    }
    echo "Main Resource Credentials: " . $_SERVER['PHP_AUTH_USER'] . ", " . $_SERVER['PHP_AUTH_PW'] . "<br/>";
?>
<script>

if (window.internals)
    internals.settings.setStorageBlockingPolicy('BlockThirdParty');

var request = new XMLHttpRequest();
request.onreadystatechange = function () {
    if (this.readyState == 4) {
        alert(this.responseText);
		if (window.testRunner)
			testRunner.notifyDone();
	}
};
request.open('GET', 'http://127.0.0.1:8000/security/resources/basic-auth.php?username=testuser&password=testpass', true);
request.send(null);
</script>

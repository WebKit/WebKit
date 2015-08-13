<?php
    function addCacheControl() {
        # Workaround for https://bugs.webkit.org/show_bug.cgi?id=77538
        # Caching redirects results in flakiness in tests that dump loader delegates.
        header("Cache-Control: no-store");
    }

    $url = $_GET['url'];
    $refresh = $_GET['refresh'];
    
    if (isset($refresh)) {
        header("HTTP/1.1 200");
        header("Refresh: $refresh; url=$url");
        addCacheControl();
        return;
    }

    $code = $_GET['code'];
    if (!isset($code))
        header("HTTP/1.1 302 Found");
    elseif ($code == 308) {
        # Apache 2.2 (and possibly some newer versions) cannot generate a reason string for code 308, and sends a 500 error instead.
        header("HTTP/1.1 308 Permanent Redirect");
    } else
        header("HTTP/1.1 $code");
    header("Location: $url");
    addCacheControl();
?>

<?php
    function addCacheControl() {
        # Workaround for https://bugs.webkit.org/show_bug.cgi?id=77538
        # Caching redirects results in flakiness in tests that dump loader delegates.
        header("Cache-Control: no-store");
    }

    $url = $_GET['url'];

    if (isset($_GET['refresh'])) {
        header("HTTP/1.1 200");
        header("Refresh: " . $_GET['refresh'] . "; url=$url");
        addCacheControl();
        return;
    }

    if (!isset($_GET['code']))
        header("HTTP/1.1 302 Found");
    elseif ($_GET['code'] == 308) {
        # Apache 2.2 (and possibly some newer versions) cannot generate a reason string for code 308, and sends a 500 error instead.
        header("HTTP/1.1 308 Permanent Redirect");
    } else
        header("HTTP/1.1 " . $_GET['code']);
    header("Location: $url");
    addCacheControl();
?>

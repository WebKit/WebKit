<?php
    $url = $_GET['url'];
    $refresh = $_GET['refresh'];
    
    if (isset($refresh)) {
        header("HTTP/1.1 200");
        header("Refresh: $refresh; url=$url");
        return;
    }

    $code = $_GET['code'];
    if (!isset($code))
        $code = 302;
    header("HTTP/1.1 $code");
    header("Location: $url");

    # Workaround for https://bugs.webkit.org/show_bug.cgi?id=77538
    # Caching redirects results in flakiness in tests that dump loader delegates.
    header("Cache-Control: no-store");
?>

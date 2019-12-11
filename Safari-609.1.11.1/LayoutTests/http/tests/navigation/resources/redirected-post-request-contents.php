<?php
function checkHeader($header) {
    if (array_key_exists($header, $_SERVER)) {
        echo $header . " is present. Its value is: " . $_SERVER[$header] . "<br>";
        return true;
    }
    return false;
}

if ($_GET["content"] == "true") {
    echo "headers CONTENT_TYPE and CONTENT_LENGTH should be present.<br>";
} else {
    echo "headers CONTENT_TYPE and CONTENT_LENGTH should not be present.<br>";
}

$content_type = checkHeader("CONTENT_TYPE");
$content_length = checkHeader("CONTENT_LENGTH");

if (!$content_type && !$content_length) {
    echo "headers CONTENT_TYPE and CONTENT_LENGTH are not present.<br>";
}

echo "<br>";

if ($_GET["content"] == "true") {
    echo "POST data should be present.<br>";
} else {
    echo "no POST data should be present.<br>";
}

if (sizeof($_POST) > 0 || sizeof($_FILES) > 0) {
    echo "POST data is present.<br>";
} else {
    echo "no POST data is present.<br>";
}

echo "<script>if (window.testRunner) testRunner.notifyDone();</script>"
?>

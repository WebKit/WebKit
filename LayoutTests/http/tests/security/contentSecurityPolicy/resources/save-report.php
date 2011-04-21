<?php
function undoMagicQuotes($value) {
    if (get_magic_quotes_gpc())
        return stripslashes($value);
    return $value;
}

$reportFile = fopen("csp-report.txt.tmp", 'w');
$httpHeaders = $_SERVER;
ksort($httpHeaders, SORT_STRING);
foreach ($httpHeaders as $name => $value) {
    if ($name === "CONTENT_TYPE" || $name === "HTTP_REFERER" || $name === "REQUEST_METHOD") {
        $value = undoMagicQuotes($value);
        fwrite($reportFile, "$name: $value\n");
    }
}
fwrite($reportFile, "=== POST DATA ===\n");
foreach ($_POST as $name => $value) {
    $name = undoMagicQuotes($name);
    $value = undoMagicQuotes($value);
    fwrite($reportFile, "$name: $value\n");
}
fclose($reportFile);
rename("csp-report.txt.tmp", "csp-report.txt");
?>

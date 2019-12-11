<?php
header('Content-Type: text/javascript');
header('Cache-Control: max-age=0');
header('Etag: 123456789');

?>
allRequestHeaders = [];
<?php
foreach (getallheaders() as $name => $value) {
    echo "allRequestHeaders['" . $name . "'] = '" . $value . "';";
}
?>

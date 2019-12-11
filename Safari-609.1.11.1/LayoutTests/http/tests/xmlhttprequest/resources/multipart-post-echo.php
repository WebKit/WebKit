<?php
if (strpos($_SERVER['CONTENT_TYPE'], 'multipart/form-data; boundary=') != 0) {
    echo 'Invalid Content-Types.';
    return;
}

$values = array();

foreach ($_POST as $key => $value) {
    $values[] = "$key=$value";
}

foreach ($_FILES as $key => $value) {
    $file = $_FILES[$key];
    if ($file['error']) {
        echo 'Upload file error: ' . $file['error'];
        return;
    } else {
        $fp = fopen($file['tmp_name'], 'r');
        if ($fp) {
            $content = fread($fp, $file['size']);
            fclose($fp);
        }
        $values[] = $key . '=' . $file['name'] . ':' . $file['type'] . ':' . $content;
    }
}

echo join('&', $values);
?>

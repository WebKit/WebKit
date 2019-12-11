<?
$length = $_GET['length'];
if ($length == 0) {
    header('Status: 200');
    header('Cache-Control: max-age=100');
    exit();
}
$length = $length - 1;

header('Status: 302');
header('Cache-Control: max-age=100');
header('Location: redirect-chain.php?length=' . $length . '&uniqueId=' . $_GET['uniqueId'] . '&random=' . rand());
?>

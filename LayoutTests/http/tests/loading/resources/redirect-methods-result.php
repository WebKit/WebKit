<?php
$status = (int)$_REQUEST['status'];
if ($status > 200 && !$_GET['redirected']) {
  header("Location: redirect-methods-result.php?redirected=true", TRUE, $status);
  exit();
}
?>
This page loaded using the <?php echo $_SERVER['REQUEST_METHOD'] ?> method.

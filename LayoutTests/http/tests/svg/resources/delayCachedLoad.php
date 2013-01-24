<?php
  // Delay load by 0.07s. This was found to be the lowest value
  // required for webkit.org/b/106733
  usleep(70000);
  header('Cache-Control: no-cache, must-revalidate');
  header('Location: http://127.0.0.1:8000/svg/resources/greenSquare.svg');
?>

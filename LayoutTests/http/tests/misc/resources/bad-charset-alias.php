<?php
echo '<meta charset="' . $_GET['charset'] . '">';
echo '<body onload="top.frameLoaded()">';
echo '<p id=charset>' . $_GET['charset'] . '</p>';
echo '<p id=test>SUσσεSS</p>'; // "σσε" are Cyrillic characters that look like "CCE".
echo '</body>';
?>

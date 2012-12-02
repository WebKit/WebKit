<!DOCTYPE HTML>
<html>
    <head>
        <meta content="text/html; charset=utf-8" http-equiv="Content-Type" />
        <title>Green Test Page</title>
        <?php
          $origin = $_GET["origin"] ? $_GET["origin"] : "";
          header("timing-allow-origin: $origin");
        ?>
    </head>
    <body style="background-color:#00FF00;">
        <h1>Placeholder</h1>
    </body>
</html>

<?php
    if (preg_match("/\*\/\*/", $_SERVER["HTTP_ACCEPT"])) {
?>
        p#target { position: relative; }
        /* This stylesheet is served as text/html */
<?php
    } else {
        header("Not acceptable", true, 406);
    }
?>

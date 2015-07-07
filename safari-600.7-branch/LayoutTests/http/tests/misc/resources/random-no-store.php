<?php
    header("Cache-Control: no-store");
?>
var p = document.createElement("p");
p.appendChild(document.createTextNode("<?php
    print rand();
?>
"));
document.body.appendChild(p);

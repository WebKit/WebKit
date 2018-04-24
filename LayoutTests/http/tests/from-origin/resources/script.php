<?php
if($_GET["fromOrigin"] == "same") {
    header("From-Origin: Same");
} elseif($_GET["fromOrigin"] == "same-site") {
    header("From-Origin: Same-Site");
}
?>
var divElement = document.createElement("div");
divElement.textContent = "Created by JavaScript.";
document.body.appendChild(divElement);

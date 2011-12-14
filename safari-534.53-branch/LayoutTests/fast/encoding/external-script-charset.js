if (window.layoutTestController)
    layoutTestController.dumpAsText();

document.getElementById("result1").innerHTML += ("Я" == "\u042F") ? "PASS" : "FAIL";

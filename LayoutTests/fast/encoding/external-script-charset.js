if (window.layoutTestController)
    layoutTestController.dumpAsText();

document.getElementById("result").innerHTML = ("Я" == "\u042F") ? "PASS" : "FAIL";
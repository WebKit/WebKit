if (window.testRunner)
    testRunner.dumpAsText();

document.getElementById("result1").innerHTML += ("Я" == "\u042F") ? "PASS" : "FAIL";

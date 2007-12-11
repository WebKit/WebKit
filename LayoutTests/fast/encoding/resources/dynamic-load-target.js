

if ('とうきょう' == tokyo)
    document.getElementById("target").innerHTML = "PASS: UTF-8 was correctly used for this script.";
else
    document.getElementById("target").innerHTML = "FAIL: Incorrect encoding used.  Expected '" + tokyo + "' but got '" + 'とうきょう' + "'.";

if (window.layoutTestController) 
    window.layoutTestController.notifyDone();

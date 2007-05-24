var maxTests = 4;
var currentTest = 1;
var iframe;

if (window.layoutTestController) {
    window.layoutTestController.waitUntilDone();
}

function testAndLoadNext() {
alert("Hey");
    iframe = document.getElementById("iframe");
    if (iframe.src.substring(0, 5) == "data:") {
alert("YOU");
        iframe.src = "resources/head-check-" + currentTest + ".html";
        return;
    }
alert("Get");
    debug("Testing: " + iframe.src.substring(iframe.src.lastIndexOf("/") + 1));
    shouldBe("iframe.contentWindow.document.getElementsByTagName('head').length", "1");
    shouldBe("iframe.contentWindow.document.firstChild.firstChild.nodeName.toLowerCase()", "'head'");
    debug("");
alert("hi");
    if (currentTest < maxTests) {
        currentTest++;
        iframe.src = "data:text/html,";
    }
    else {
        iframe.style.display = "none";
        debug('<span class="pass">TEST COMPLETE</span>');
        if (window.layoutTestController) {
            window.layoutTestController.notifyDone();
        }
    }
}

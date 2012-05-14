if (window.layoutTestController) {
  layoutTestController.waitUntilDone();
  layoutTestController.dumpAsText();
  layoutTestController.dumpChildFramesAsText();
}

function test() {
    if (tests.length === 0)
        return finishTesting();

    var current = tests.shift();
    var iframe = document.createElement('iframe');
    iframe.src = "http://127.0.0.1:8000/security/contentSecurityPolicy/resources/echo-script-src.pl?should_run=" + current[0] +
                 "&q=http://127.0.0.1:8000/security/contentSecurityPolicy/resources/script.js&csp=" + escape(current[1]);
    iframe.onload = test;
    document.body.appendChild(iframe);
}

function finishTesting() {
    if (window.layoutTestController) {
        setTimeout("layoutTestController.notifyDone()", 0);
    }
    return true;
}

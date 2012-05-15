if (window.layoutTestController) {
  layoutTestController.waitUntilDone();
  layoutTestController.dumpAsText();
  layoutTestController.dumpChildFramesAsText();
}

function test() {
    if (tests.length === 0)
        return finishTesting();

    var defaultScript = 'http://127.0.0.1:8000/security/contentSecurityPolicy/resources/script.js'
    var current = tests.shift();
    var iframe = document.createElement('iframe');
    iframe.src = "http://127.0.0.1:8000/security/contentSecurityPolicy/resources/echo-script-src.pl?" +
                 "should_run=" + escape(current[0]) +
                 "&csp=" + escape(current[1]) +
                 "&q=" + escape(current[2] ? current[2] : defaultScript);
    iframe.onload = test;
    document.body.appendChild(iframe);
}

function finishTesting() {
    if (window.layoutTestController) {
        setTimeout("layoutTestController.notifyDone()", 0);
    }
    return true;
}

console.log("Script is executing and is not blocked by CSP.");
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function sleep(ms = 0) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

window.no_csp = window.open("resources/no-csp.html", "no_csp");
window.no_csp.location.href = "javascript:console.log('This should not be logged');";
sleep(500).then(() => {testRunner.notifyDone();});
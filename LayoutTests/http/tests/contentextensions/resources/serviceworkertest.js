if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function testServiceWorker() {
    navigator.serviceWorker.addEventListener("message", function(event) {
        alert("Message from worker: " + event.data);
        if (window.testRunner)
            testRunner.notifyDone();
    });

    try {
        navigator.serviceWorker.register('resources/fetch-worker.js').then(function(reg) {
            worker = reg.installing ? reg.installing : reg.active;
            worker.postMessage("Hello from the web page");
        }).catch(function(error) {
            alert("Registration failed with: " + error);
            if (window.testRunner)
                testRunner.notifyDone();
        });
    } catch(e) {
        alert("Exception: " + e);
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

function test() {
    fetch("/resources/dummy.js").then(() => {
        alert("FAIL - should have blocked request to dummy.js");
        if (window.testRunner)
            testRunner.notifyDone();
    }).catch(() => {
        alert("PASS - blocked request to dummy.js");
        fetch("/resources/dummy.css").then(() => {
            testServiceWorker();
        }).catch(() => {
            alert("FAIL - should have allowed request to dummy.css");
            if (window.testRunner)
                testRunner.notifyDone();
        });
    });
}

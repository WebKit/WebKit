function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var worker = createWorker();
worker.postMessage("eval typeof navigator");
worker.postMessage("eval navigator");

// NavigatorConcurrentHardware
worker.postMessage("eval navigator.hardwareConcurrency >= 1 && navigator.hardwareConcurrency <= 8");

// NavigatorID
worker.postMessage("eval navigator.appCodeName === 'Mozilla'");
worker.postMessage("eval navigator.appName === 'Netscape'");
worker.postMessage("eval navigator.appVersion.indexOf('WebKit') != 0");
worker.postMessage("eval typeof navigator.platform == 'string'");
worker.postMessage("eval navigator.product === 'Gecko'");
worker.postMessage("eval navigator.productSub === undefined");
worker.postMessage("eval navigator.userAgent.indexOf('WebKit') != 0");
worker.postMessage("eval navigator.vendor === undefined");
worker.postMessage("eval navigator.vendorSub === undefined");

// NavigatorLanguage
worker.postMessage("eval typeof navigator.language == 'string'");

// NavigatorOnLine
worker.postMessage("eval typeof navigator.onLine == 'boolean'");


worker.postMessage("eval foo//bar");

worker.onmessage = function(evt) {
    if (!/foo\/\/bar/.test(evt.data))
        log(evt.data.replace(new RegExp("/.*LayoutTests"), "<...>"));
    else {
        log("DONE");
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

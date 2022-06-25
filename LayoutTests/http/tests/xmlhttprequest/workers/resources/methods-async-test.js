if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var console_messages = document.createElement("ol");
document.body.appendChild(console_messages);

function log(message)
{
    var item = document.createElement("li");
    item.appendChild(document.createTextNode(message));
    console_messages.appendChild(item);
}

var worker = createWorker('resources/methods-async.js');
worker.postMessage("START");

worker.onmessage = function(evt)
{
    if (/log .+/.test(evt.data))
        log(evt.data.substr(4));
    else if (/DONE/.test(evt.data)) {
        log("DONE");
        if (window.testRunner)
            testRunner.notifyDone();
    } else {
        log("ERROR");
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

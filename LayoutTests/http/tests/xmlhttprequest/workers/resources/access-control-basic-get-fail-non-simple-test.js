if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

var console_messages = document.createElement("ol");
document.body.appendChild(console_messages);

function log(message)
{
    var item = document.createElement("li");
    item.appendChild(document.createTextNode(message));
    console_messages.appendChild(item);
}

var worker = createWorker('resources/access-control-basic-get-fail-non-simple.js');
worker.postMessage("START");

worker.onmessage = function(event)
{
    if (/log .+/.test(event.data))
        log(event.data.substr(4));
    else if (/DONE/.test(event.data)) {
        log("DONE");
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    } else {
        log("ERROR");
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    }
}

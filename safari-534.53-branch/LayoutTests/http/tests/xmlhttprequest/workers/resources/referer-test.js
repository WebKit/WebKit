if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

var console_messages = document.createElement("ul");
document.body.appendChild(console_messages);

function log(message)
{
    var item = document.createElement("li");
    item.appendChild(document.createTextNode(message));
    console_messages.appendChild(item);
}

var workerUrl = location.protocol + "//MyUserName:MySecurePassword@" + location.host + location.pathname.replace(/\/[^\/]*$/, "") + '/resources/referer.js#ref';
var worker = createWorker(workerUrl);

worker.onmessage = function(evt)
{
    if (/log .+/.test(evt.data))
        log(evt.data.substr(4));
    else if (/DONE/.test(evt.data)) {
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    } else {
        log("ERROR");
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    }
}

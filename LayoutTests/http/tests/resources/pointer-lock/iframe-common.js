function thisFileName()
{
    return window.location.href.split("/").pop();
}

window.onmessage = function (messageEvent) {
    switch (messageEvent.data[0]) {
    case "eval":
        eval(messageEvent.data[1]);
        break;
    case "pass message down":
        iframe = document.getElementsByTagName("iframe")[0];
        iframe.contentWindow.postMessage(messageEvent.data.slice(1), "*");
        break;
    default:
        // Pass all other messages up to parent.
        parent.postMessage(messageEvent.data, "*");
    }
}

document.onpointerlockchange = function () {
    parent.postMessage(thisFileName() + " onpointerlockchange, document.pointerLockElement = " + document.pointerLockElement, "*");
}

document.onpointerlockerror = function () {
    parent.postMessage(thisFileName() + " onpointerlockerror", "*");
}

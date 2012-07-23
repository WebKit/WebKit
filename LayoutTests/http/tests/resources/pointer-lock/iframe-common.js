function thisFileName()
{
    return window.location.href.split("/").pop();
}

window.onmessage = function (messageEvent) {
    switch (messageEvent.data[0]) {
    case "eval":
        eval(messageEvent.data[1]);
        break;
    }
}


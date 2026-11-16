var es = new EventSource("../../resources/simple-event-stream.asis");

es.onmessage = function (evt) {
    postMessage(evt.data);
    es.close();
}

es.onerror = function () {
    postMessage("error");
    es.close();
}


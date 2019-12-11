function clearCookies()
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "block-cookies-worker.php?clear=1", false);
    xhr.send(null);
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/hybi/contentextensions/resources/echo-cookie");
ws.onopen = function() {
    postMessage("worker WebSocket open");
};
ws.onmessage = function(evt) {
    clearCookies();
    postMessage("PASS worker WebSocket message (" + evt.data + ")");
};

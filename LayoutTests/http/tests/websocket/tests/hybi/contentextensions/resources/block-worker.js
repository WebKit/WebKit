var ws = new WebSocket("ws://localhost:8880/websocket/tests/hybi/contentextensions/resources/echo");

ws.onopen = function() { 
    postMessage("FAIL worker onopen");
}
ws.onmessage = function(message) { 
    postMessage("FAIL worker onmessage " + message.data);
}
ws.onclose = function() {
    postMessage("FAIL worker onclose");
}
ws.onerror = function() { 
    postMessage("PASS worker onerror");
}

setTimeout(function() { 
    postMessage("FAIL worker timeout"); 
}, 3000);

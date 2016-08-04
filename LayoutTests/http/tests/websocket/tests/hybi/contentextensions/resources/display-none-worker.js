var ws = new WebSocket("ws://localhost:8880/websocket/tests/hybi/contentextensions/resources/echo");

ws.onopen = function() { 
    postMessage("worker onopen");
    ws.send("sent"); 
}
ws.onmessage = function(message) { 
    postMessage("worker onmessage " + message.data);
}
ws.onclose = function() {
    postMessage("PASS worker onclose");
}
ws.onerror = function() { 
    postMessage("FAIL worker onerror");
}

setTimeout(function() { 
    postMessage("FAIL worker timeout"); 
}, 3000);

var ws = new WebSocket("ws://127.0.0.1/websocket/tests/hybi/simple");

ws.onopen = function() { 
    postMessage("FAIL worker onopen");
}
ws.onmessage = function(message) { 
    postMessage("FAIL worker onmessage " + message.data);
}
ws.onclose = function() {
    postMessage("PASS worker onclose url " + ws.url);
}
ws.onerror = function() { 
    postMessage("FAIL worker onerror");
}

setTimeout(function() { 
    postMessage("FAIL worker timeout"); 
}, 3000);

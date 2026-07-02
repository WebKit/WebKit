// Opened from 127.0.0.1:8000; WebSocket connects to localhost:8880 (cross-origin).
// With ITP third-party cookie blocking enabled, the handshake must not carry cookies.
let ws = new WebSocket("ws://localhost:8880/websocket/tests/hybi/websocket-blocked-sending-cookie-as-third-party");

self.postMessage({ type: "debug", message: "Created a socket to '" + ws.url + "'; readyState " + ws.readyState + "." });

ws.onopen = () => {
    ws.close();
    self.postMessage({ type: "pass", message: "Connection was allowed (request did not contain cookies)." });
    self.postMessage({ type: "done", message: "" });
};

ws.onerror = () => {
    self.postMessage({ type: "fail", message: "Connection was rejected (request contained cookies)." });
    self.postMessage({ type: "done", message: "" });
};

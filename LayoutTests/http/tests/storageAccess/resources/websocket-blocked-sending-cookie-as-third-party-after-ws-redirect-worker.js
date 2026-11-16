// Runs in a DedicatedWorker created by a cross-site iframe that has been granted storage access.
// The worker does not inherit the grant, so the redirect target may not receive the iframe's cookies.
// The first hop is first-party to the top frame so that cookies are not blocked there; the cross-site
// redirect target is then evaluated by the redirect handling, where the grant must not apply.
const redirectURL = "wss://127.0.0.1:8443/resources/redirect.py?cookie=" + encodeURIComponent("setAsThirdPartyHTTP=value")
    + "&url=" + encodeURIComponent("wss://localhost:9323/websocket/tests/hybi/websocket-blocked-sending-cookie-as-third-party");

let ws = new WebSocket(redirectURL);
self.postMessage({ type: "debug", message: "<br>Created a socket to '" + ws.url + "'; readyState " + ws.readyState + "." });

ws.onopen = () => {
    ws.close();
    self.postMessage({ type: "pass", message: "Connection was allowed (request did not contain cookies)." });
    self.postMessage({ type: "done", message: "" });
};

ws.onerror = () => {
    self.postMessage({ type: "fail", message: "Connection was rejected (request contained cookies)." });
    self.postMessage({ type: "done", message: "" });
};

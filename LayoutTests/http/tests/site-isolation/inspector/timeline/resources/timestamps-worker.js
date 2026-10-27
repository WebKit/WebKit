// frame-target-timestamps.html sets a probe on the postMessage() line by line number; keep them in sync.
self.onmessage = function() {
    let now = performance.now();
    postMessage(now);
};
